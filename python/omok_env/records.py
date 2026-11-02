"""Versioned, atomic per-game JSON traces; replay always uses a separate environment."""
import json
import os
from pathlib import Path
import tempfile
from . import Environment, PROFILE, logical

SCHEMA = "omok-trace-v1"


def save(path, record):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(record, stream, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def begin(env, *, game_id, start, config, seed, code_version):
    return dict(schema=SCHEMA, profile=PROFILE, profile_version=1, game_id=game_id,
                config=config, seed=seed, code_version=code_version, start=start,
                initial=logical(env.snapshot()), actions=[], status="incomplete", diagnostic=None)


def apply(env, record, action, *, monitor=None, selection=None):
    s = env.snapshot()
    count = len(record["actions"])
    def append(decision, after):
        record["actions"].append(dict(action=action, row=action//15, column=action%15,
            color=s["next_color"], actor=s["next_actor"], decision=decision, after=logical(after)))
    try:
        if monitor is None:
            decision = env.step(action, actor=s["next_actor"], expected=s["token"])
        else:
            from omok_monitoring.checks import audited_step
            decision=audited_step(monitor,env,action,actor=s["next_actor"],expected=s["token"],selection=selection)
        append(decision, env.snapshot())
    except BaseException:
        # A signal can arrive just after the native transaction commits. Preserve
        # that committed action before recording an interruption of the runner.
        after = env.snapshot()
        if after["token"] != s["token"] and len(record["actions"]) == count:
            verdict = {"five":"win", "forbidden":"forbidden", "move_limit":"draw", "board_full":"draw"}.get(after["end_reason"],"legal")
            append(dict(verdict=verdict,reason=after["rule_reason"]),after)
        raise


def finish(env, record, status, diagnostic=None):
    if status == "interrupted" and env.snapshot()["terminated"]:
        status, diagnostic = "completed", None
    if status != "completed" and env.snapshot()["phase"] == "playing":
        env.abort()
    record.update(status=status, diagnostic=diagnostic, final=logical(env.snapshot()))
    return record


def replay(record, library=None, *, monitor=None):
    try:
        if monitor:
            from omok_monitoring.events import digest,file_hash
            if isinstance(record,(str,Path)):
                monitor.bind(trace_path=str(record));monitor.bind(trace_sha256=file_hash(record))
            else:monitor.bind(trace_game_id=record.get("game_id"),trace_sha256=digest(record))
        result=_replay(record,library,monitor=monitor)
    except Exception as error:
        if monitor:monitor.emit("LP-07","failure","record_validation_failed",dict(error=str(error)))
        raise
    if monitor:monitor.emit("LP-07","normal","logical_trace_replay_verified",dict(search_reexecution=False))
    return result


def _replay(record, library=None, *, monitor=None):
    if isinstance(record, (str, Path)):
        with open(record, encoding="utf-8") as stream:
            record = json.load(stream)
    if record["schema"] != SCHEMA or record["profile"] != PROFILE or record["profile_version"] != 1:
        raise ValueError("unsupported record/profile version")
    if record["status"] not in ("completed", "interrupted", "error", "blocked"):
        raise ValueError("incomplete/unknown record status")
    if not isinstance(record["game_id"], str) or not record["game_id"]:
        raise ValueError("invalid game id")
    start = record["start"]
    if set(start) != {"compact", "source_id", "black_owner", "profile"}:
        raise ValueError("invalid start specification")
    with Environment(library, **start) as env:
        if logical(env.snapshot()) != record["initial"]:
            raise ValueError("initial state mismatch")
        for event in record["actions"]:
            s = env.snapshot()
            if monitor and "search" in event.get("inference",{}):
                from omok_monitoring.checks import puct
                info=event["inference"]
                monitor.require("LP-07",info["action"]==event["action"],"search_action_trace_binding",{"action":event["action"]})
                puct(monitor,info,mask=env.mask(),rule="LP-07")
            if (event["actor"], event["color"]) != (s["next_actor"], s["next_color"]):
                raise ValueError("actor/color mismatch")
            if (event["row"], event["column"]) != divmod(event["action"],15):
                raise ValueError("coordinate mismatch")
            if env.step(event["action"], actor=event["actor"]) != event["decision"]:
                raise ValueError("decision mismatch")
            if logical(env.snapshot()) != event["after"]:
                raise ValueError("intermediate state mismatch")
        if record["status"] == "completed":
            if not env.snapshot()["terminated"] or record["diagnostic"] is not None:
                raise ValueError("completion without rule termination")
        else:
            if env.snapshot()["terminated"] or not isinstance(record["diagnostic"], dict):
                raise ValueError("invalid interruption/error")
            env.abort()
        final = logical(env.snapshot())
        if final != record["final"]:
            raise ValueError("final state mismatch")
        return final
