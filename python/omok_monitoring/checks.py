"""Reuse adjudication/check results; never implement Renju or reward conversion here."""
import math
from pathlib import Path
from .events import MonitorViolation,digest,file_hash


def context(snapshot):
    return dict(token=snapshot["token"],ply=snapshot["placements"],profile=snapshot["profile"],actor=snapshot["next_actor"])


def audited_step(monitor,env,action,*,actor=None,expected=None,selection=None,intentional=False,allow_forbidden=False):
    before=env.snapshot();mask=env.mask()
    actor=before["next_actor"] if actor is None else actor
    expected=before["token"] if expected is None else expected
    evidence=dict(action=action,actor=actor,expected=expected,before_sha256=digest(before),intentional=intentional,
        mask_sha256=digest(mask),coordinate=list(divmod(action,15)) if type(action) is int and 0<=action<225 else None)
    if selection is not None:
        monitor.require("GR-01",selection["token"]==before["token"] and tuple(selection["mask"])==mask,
            "selection_snapshot_mask_binding",evidence,context=context(before))
        monitor.require("GR-01",type(action) is int and 0<=action<225 and mask[action],
            "post_mask_action_legal",evidence,context=context(before))
    try:
        decision=env.step(action,actor=actor,expected=expected)
    except ValueError as error:
        after=env.snapshot()
        unchanged=after==before
        monitor.emit("GR-02","normal" if intentional and unchanged else "warning" if unchanged else "failure",
            "request_rejected_preserved" if unchanged else "rejected_request_mutated_state",
            dict(evidence,error=str(error),after_sha256=digest(after)),context=context(before))
        raise
    after=env.snapshot()
    if decision["verdict"]=="forbidden":
        valid=(after["board"]==before["board"] and after["history"]==before["history"]
            and after["placements"]==before["placements"] and after["result"]=="won" and after["winner"]==1-actor
            and after["phase"]=="finished" and after["end_reason"]=="forbidden" and after["next_actor"] is None
            and after["token"][:2]==before["token"][:2] and after["token"][2]==before["token"][2]+1
            and after["event_count"]==before["event_count"]+1)
        monitor.require("GR-01",valid and allow_forbidden,"forbidden_adjudication_contract",
            dict(evidence,decision=decision,after_sha256=digest(after),successful_placement=False,
                learning_eligibility="unchanged; no value target generated"),context=context(before))
    else:
        monitor.require("GR-01",type(action) is int and mask[action] and decision["verdict"] in ("legal","win","draw"),
            "mask_core_apply_agreement",dict(evidence,decision=decision,independent_referee=False),context=context(before))
    return decision


def reviewed_fixture(monitor,*,fixture_id,actual,expected=None,source=None,reviewed=False):
    if not reviewed or expected is None or not source:
        return monitor.emit("GR-03","deferred","independent_expectation_unavailable",dict(fixture_id=fixture_id,source=source))
    return monitor.check("GR-03",actual==expected,"reviewed_fixed_expectation",
        dict(fixture_id=fixture_id,actual=actual,expected=expected,source=source,scope="fixed case, not universal referee verification"))


def episode(monitor,record):
    status=record["status"];final=record["final"]
    if status=="completed":
        return monitor.check("GR-04",final["terminated"] and final["result"] in ("won","draw"),
            "completed_episode_has_known_result",dict(status=status,result=final["result"],end_reason=final["end_reason"]))
    monitor.emit("GR-04","deferred","non_normal_episode_quarantined",
        dict(status=status,result=final["result"],diagnostic=record["diagnostic"],normal_wdl=False,learning_eligible=False))


def raw_policy_mass(monitor,mass,*,ctx=None):
    monitor.require("LP-01",math.isfinite(mass) and 0<=mass<=1.00001,"raw_policy_mass_finite",{"mass":mass},context=ctx)
    monitor.emit("GR-01","deferred","pre_mask_mass_observed_no_threshold",
        dict(raw_illegal_mass=mass,actual_illegal_placement=False,operating_threshold=None),context=ctx)


def finite_tree(value):
    if isinstance(value,dict):return all(finite_tree(v) for v in value.values())
    if isinstance(value,(list,tuple)):return all(finite_tree(v) for v in value)
    if hasattr(value,"isfinite"):return bool(value.isfinite().all().item())
    if isinstance(value,(int,float)):return math.isfinite(value)
    return value is None or isinstance(value,str)


def numeric(monitor,metrics,*,epoch=None,step=None,collected=("loss",)):
    def summary(v):
        if isinstance(v,dict):return {str(k):summary(x) for k,x in v.items()}
        if isinstance(v,(list,tuple)):return [summary(x) for x in v]
        if hasattr(v,"isfinite"):return dict(shape=list(v.shape),dtype=str(v.dtype),finite=finite_tree(v))
        return v
    valid=finite_tree(metrics) and all(v is None or (type(v) is int and v>=0) for v in (epoch,step))
    monitor.require("LP-02",valid,"collected_training_numerics_finite",
        dict(epoch=epoch,step=step,collected=list(collected),metrics=summary(metrics)))
    if not {"gradient","optimizer","weights"}.issubset(collected):
        monitor.emit("LP-02","deferred","uncollected_numerics",{"not_claimed":["complete gradient/optimizer audit"]})


def loss_observation(monitor,metrics):
    monitor.emit("LP-05","deferred","loss_trend_criteria_not_configured",dict(metrics=metrics,window=None,threshold=None))


def tactical(monitor,actual,*,expected=None,reviewed=False,required=True,spec=None):
    evidence=dict(actual=actual,expected=expected,reviewed=reviewed,required=required,spec=spec)
    if not reviewed or not expected or not spec:
        return monitor.emit("LP-04","deferred","tactical_gate_not_specified",evidence)
    return monitor.emit("LP-04","normal" if actual in expected else "failure" if required else "warning",
        "declared_tactical_case",evidence)


def comparison(monitor,metrics):
    monitor.emit("LP-06","deferred","strength_criteria_or_samples_not_established",dict(metrics=metrics,automatic_promotion=False))


def fixed_artifact(monitor,path,expected_hash):
    valid=Path(path).is_file() and ".tmp" not in Path(path).name
    actual=file_hash(path) if valid else None
    monitor.require("LP-08",valid and actual==expected_hash,"fixed_completed_artifact_hash",
        dict(path=str(path),expected=expected_hash,actual=actual))


def checkpoint_metadata(monitor,path,data):
    evidence=dict(path=str(path),kind=data["kind"],sha256=file_hash(path))
    if data["kind"]=="inference":
        meta=data["metadata"];selected=meta.get("selected_epoch");completed=meta.get("completed_epoch")
        if selected is None or completed is None:
            monitor.emit("LP-08","deferred","selection_epoch_evidence_missing",dict(path=str(path),kind=data["kind"]))
            return
        valid=type(selected) is int and type(completed) is int and 0<selected<=completed
        evidence.update(selected_epoch=selected,completed_epoch=completed)
    else:
        valid=all(type(data[k]) is int for k in ("best_epoch","epoch","step","cursor")) and 0<data["best_epoch"]<=data["epoch"] and data["cursor"]==0 and data["step"]>=0
        evidence.update(epoch=data["epoch"],best_epoch=data["best_epoch"],cursor=data["cursor"])
    monitor.require("LP-08",valid,"checkpoint_completed_epoch_consistency",evidence)


def puct_metadata_valid(info,mask=None):
    try:
        stats=info["search"];s=stats["simulations"];cfg=info["search_config"]
        from omok_learning.mcts import SearchConfig
        SearchConfig(**cfg)
        if type(s) is not int or s<1 or s!=cfg["simulations"]:return False
        for key in ("network_evaluations","evaluation_attempts","expanded_nodes","terminal_hits","transitions","clones","max_depth"):
            if type(stats[key]) is not int or stats[key]<0:return False
        if stats["network_evaluations"]+stats["terminal_hits"]!=s+1 or stats["clones"]!=s+1:return False
        if stats["expanded_nodes"]!=stats["network_evaluations"] or stats["evaluation_attempts"]!=stats["network_evaluations"]:return False
        if stats["transitions"]<s or not 1<=stats["max_depth"]<=stats["transitions"]:return False
        visits=info["root_visits"];dist=info["visit_distribution"];p=info["root_priors"];q=info["root_edge_q"]
        if any(len(v)!=225 for v in (visits,dist,p,q)):return False
        if any(type(n) is not int or n<0 for n in visits) or sum(visits)!=s:return False
        if not finite_tree((dist,p,q,info["root_q"],info["value"],info["milliseconds"],stats["network_ms"])):return False
        if abs(info["root_q"])>1.00001 or abs(info["value"])>1.00001 or info["milliseconds"]<0 or stats["network_ms"]<0:return False
        if any(x<0 for x in dist+p) or any(abs(x)>1.00001 for x in q):return False
        if abs(sum(p)-1)>1e-5 or abs(sum(dist)-1)>1e-5:return False
        if any(abs(d-n/s)>1e-5 for d,n in zip(dist,visits)):return False
        action=info["action"]
        if type(action) is not int or not 0<=action<225 or visits[action]!=max(visits):return False
        if p[action]!=max(p[a] for a,n in enumerate(visits) if n==max(visits)):return False
        if mask is not None:
            if len(mask)!=225 or info["root_legal_count"]!=sum(mask) or not mask[action]:return False
            if any((n or d or prior or quality) and not legal for n,d,prior,quality,legal in zip(visits,dist,p,q,mask)):return False
        return True
    except (KeyError,TypeError,ValueError,IndexError):return False


def puct(monitor,info,*,outcome="completed",mask=None,rule="LP-09"):
    if outcome!="completed":
        monitor.emit("LP-09","deferred","partial_search_not_success_checked",dict(outcome=outcome,partial=info))
        return
    monitor.require(rule,puct_metadata_valid(info,mask),"completed_search_metadata_invariants",
        dict(simulations=info.get("search",{}).get("simulations"),scope="structural metadata; not search reexecution"))
    if rule=="LP-09":
        monitor.emit("LP-09","deferred","cost_observed_threshold_unset",
            dict(milliseconds=info["milliseconds"],network_evaluations=info["search"]["network_evaluations"],threshold=None,monitoring_overhead_included=True))
