"""Append-only events and an end-of-scope report for a single sequential owner."""
from collections import Counter
from datetime import datetime,timezone
import hashlib
import json
import math
import os
from pathlib import Path
import time
import uuid

RULES={name:dict(version=1,owner="A" if name.startswith("GR") else "B") for name in
       ["GR-01","GR-02","GR-03","GR-04",*[f"LP-{i:02d}" for i in range(1,10)]]}
STATES=("normal","warning","failure","deferred")


def clean(value):
    if isinstance(value,float) and not math.isfinite(value):return repr(value)
    if isinstance(value,dict):return {str(k):clean(v) for k,v in value.items()}
    if isinstance(value,(list,tuple)):return [clean(v) for v in value]
    if isinstance(value,Path):return str(value)
    if value is None or isinstance(value,(str,int,float,bool)):return value
    raise TypeError(f"monitor evidence must be primitive, got {type(value).__name__}")


def digest(value):
    return hashlib.sha256(json.dumps(clean(value),sort_keys=True,separators=(",",":"),allow_nan=False).encode()).hexdigest()


def file_hash(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class MonitorViolation(ValueError):pass
class MonitoringStorageError(OSError):pass
class StopRequested(Exception):pass


class Monitor:
    def __init__(self,output,*,run_id=None,attempt_id="1",config=None,clock=time.monotonic,utc=None):
        self.output=Path(output)
        self.output.mkdir(parents=True,exist_ok=True)
        if any(self.output.iterdir()):raise ValueError("monitor requires an independent empty output directory")
        self.run_id=run_id or uuid.uuid4().hex
        self.attempt_id=str(attempt_id)
        self.config=clean(config or {})
        self.clock=clock;self.utc=utc or (lambda:datetime.now(timezone.utc).isoformat())
        self.started=clock();self.events=[];self.context={};self.seen=set()
        self.implementation_sha256=hashlib.sha256(b"".join(p.read_bytes() for p in sorted(Path(__file__).parent.glob("*.py")))).hexdigest()
        self.stop_requested=False;self.stop_accepted=False;self.execution_ended=False
        self.storage_failed=False;self.report_saved=False;self.saved={};self.required=set()
        self.quarantined=[];self.promotion_blocked=False;self.finalized=False
        self.path=self.output/"events.jsonl"
        self.path.touch(exist_ok=False)
        self.emit("LP-03","deferred","timing_thresholds_not_configured",{"watchdog":False,"periodic_producer":False})

    def __enter__(self):return self

    def __exit__(self,kind,error,traceback):
        if error is not None and not isinstance(error,StopRequested):
            self.emit("LP-09","failure","scope_exception",{"type":kind.__name__,"message":str(error)})
        self.finish(outcome="error" if error else "stopped" if self.stop_accepted else "completed")
        return isinstance(error,StopRequested)

    def bind(self,**context):
        self.context.update(clean(context))

    def _storage_failure(self,error,operation):
        self.storage_failed=True;self.stop_requested=True;self.promotion_blocked=True
        # Persistence is unavailable: retain explicit evidence in memory; never
        # recursively claim that this fallback event reached disk.
        event=self._event("LP-07","failure","monitor_storage_failure",{"operation":operation,"error":str(error)},None)
        event["persisted"]=False
        self.events.append(event)

    def _event(self,rule,state,reason,evidence,context):
        actions=["record"]
        if state=="warning":actions.append("warn")
        if state=="failure":actions += ["quarantine","block_promotion","request_stop"]
        if state=="deferred":actions.append("withhold_judgment")
        return dict(schema="omok-monitor-event-v1",run_id=self.run_id,attempt_id=self.attempt_id,
            sequence=len(self.events)+1,utc=self.utc(),elapsed_seconds=self.clock()-self.started,
            rule_id=rule,rule_version=RULES[rule]["version"],owner=RULES[rule]["owner"],
            state=state,severity=dict(normal="info",warning="warning",failure="error",deferred="undetermined")[state],
            reason=reason,evidence=clean(evidence),context=clean(dict(self.context,**(context or {}))),
            config_sha256=digest(self.config),implementation="sequential-monitor-v1",implementation_sha256=self.implementation_sha256,
            pid=os.getpid(),actions=actions,persisted=True)

    def emit(self,rule,state,reason,evidence=None,*,context=None):
        if self.finalized:raise RuntimeError("monitor already finalized")
        if rule not in RULES or state not in STATES:raise ValueError("unknown monitor rule/state")
        already_requested=self.stop_requested
        event=self._event(rule,state,reason,evidence or {},context)
        if state=="failure":
            self.stop_requested=True;self.promotion_blocked=True
            self.quarantined.append(dict(rule=rule,sequence=event["sequence"],context=event["context"]))
        self.events.append(event)
        try:
            with self.path.open("a",encoding="utf-8") as stream:
                stream.write(json.dumps(event,ensure_ascii=False,allow_nan=False,separators=(",",":"))+"\n")
                stream.flush()
                os.fsync(stream.fileno())
        except OSError as error:
            event["persisted"]=False;self._storage_failure(error,"append_event")
            raise MonitoringStorageError("monitor event persistence failed") from error
        if state=="failure" and not already_requested:
            self.emit("LP-03","normal","stop_requested",dict(source_rule=rule,source_sequence=event["sequence"],accepted=False))
        return event

    def check(self,rule,condition,reason,evidence=None,*,context=None):
        self.emit(rule,"normal" if condition else "failure",reason,evidence,context=context)
        return bool(condition)

    def require(self,rule,condition,reason,evidence=None,*,context=None):
        if not self.check(rule,condition,reason,evidence,context=context):raise MonitorViolation(reason)

    def progress(self,phase,cursor=None):
        self.bind(phase=phase)
        self.emit("LP-03","normal","boundary_progress_and_heartbeat",dict(cursor=cursor,heartbeat=True,
            meaning="this boundary was reached; no liveness or stagnation guarantee"))

    def request_stop(self,reason):
        self.stop_requested=True
        self.emit("LP-03","normal","stop_requested",dict(reason=reason,accepted=False))

    def boundary(self,phase,cursor=None):
        self.progress(phase,cursor)
        if self.stop_requested:
            self.stop_accepted=True
            self.emit("LP-03","normal","stop_accepted",dict(boundary=phase,cursor=cursor,stored=False,ended=False))
            return True
        return False

    def require_save(self,key):self.required.add(str(key))

    def save_artifact(self,key,path,writer):
        self.require_save(key)
        self.progress("saving",dict(key=str(key),path=str(path)))
        try:
            writer()
            h=file_hash(path)
        except Exception as error:
            self.storage_failed=True
            self.emit("LP-07","failure","artifact_save_failed",dict(key=str(key),path=str(path),error=str(error)))
            raise
        self.saved[str(key)]=dict(path=str(path),sha256=h)
        self.emit("LP-08","normal","artifact_save_confirmed",dict(key=str(key),**self.saved[str(key)]))

    def recovery_anchor(self,key,path):
        """Confirm a validated existing recovery file; do not claim a new save."""
        self.require_save(key)
        self.saved[str(key)]=dict(path=str(path),sha256=file_hash(path),origin="validated_recovery_file")
        self.emit("LP-08","normal","recovery_anchor_verified",dict(key=str(key),**self.saved[str(key)],new_save=False))

    def completed_unit(self,unit_id,*,resumed=False):
        key=str(unit_id)
        if key in self.seen:
            self.emit("LP-08","normal" if resumed else "failure","duplicate_unit_skipped" if resumed else "duplicate_result_inclusion",{"unit_id":key})
            return False
        self.seen.add(key)
        self.emit("LP-08","normal","completed_unit_registered",dict(unit_id=key,resumed=resumed))
        return True

    def finish(self,*,outcome="completed"):
        if self.finalized:return
        if outcome=="completed" and any(e["state"]=="failure" for e in self.events):outcome="failed_checks_scope_ended"
        self.execution_ended=True
        self.emit("LP-03","normal","execution_scope_ended",dict(outcome=outcome,process_exit_observed=False,
            stop_requested=self.stop_requested,stop_accepted=self.stop_accepted))
        coverage={rule:dict(states=dict(Counter(e["state"] for e in self.events if e["rule_id"]==rule)),
            checks=sum(e["rule_id"]==rule for e in self.events)) for rule in RULES}
        for value in coverage.values():
            if not value["checks"]:value.update(status="deferred",reason="not_executed")
            else:value["status"]=next(s for s in ("failure","warning","deferred","normal") if s in value["states"])
        try:events_hash=file_hash(self.path)
        except OSError as error:
            self._storage_failure(error,"hash_events")
            raise MonitoringStorageError("monitor event integrity read failed") from error
        report=dict(schema="omok-monitor-report-v1",run_id=self.run_id,attempt_id=self.attempt_id,config=self.config,
            event_count=len(self.events),events_sha256=events_hash,coverage=coverage,
            states=dict(Counter(e["state"] for e in self.events)),quarantined=self.quarantined,promotion_blocked=self.promotion_blocked,
            unpersisted_events=[e for e in self.events if not e["persisted"]],
            stop=dict(requested=self.stop_requested,accepted=self.stop_accepted,execution_scope_ended=True,
                required_saves=sorted(self.required),saved=self.saved,storage_failed=self.storage_failed,
                safe_stop_complete=self.stop_requested and self.stop_accepted and bool(self.required)
                    and self.required.issubset(self.saved) and not self.storage_failed),
            report_save_confirmed=True,outcome=outcome)
        try:
            from omok_env.records import save
            save(self.output/"report.json",report)
        except OSError as error:
            self._storage_failure(error,"save_report")
            raise MonitoringStorageError("monitor report persistence failed; safe stop unconfirmed") from error
        self.report_saved=True;self.finalized=True
