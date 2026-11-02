"""Greedy masked policy inference; value is returned for analysis only."""
from pathlib import Path
import hashlib
import time
import torch
from .checkpoint import load
from .model import checked_outputs,probabilities


class ModelPlayer:
    def __init__(self,path,*,monitor=None):
        self.path=str(Path(path).resolve())
        self.model=None
        self.artifact=None
        self.load_ms=0.0
        self.monitor=monitor
        self.descriptor=dict(kind="policy-model-v1",path=self.path,
            sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest() if Path(path).is_file() else None,
            selection="masked-argmax-lowest-index-tie",value_usage="analysis-only")

    def prepare(self):
        """Load once; callers can separate preparation from move timing."""
        if self.monitor:
            from omok_monitoring.checks import fixed_artifact
            fixed_artifact(self.monitor,self.path,self.descriptor["sha256"])
        if self.model is None:
            start=time.perf_counter()
            if hashlib.sha256(Path(self.path).read_bytes()).hexdigest()!=self.descriptor["sha256"]:
                raise ValueError("model file changed after player configuration")
            self.model,self.artifact=load(self.path,kind="inference",**({"monitor":self.monitor} if self.monitor else {}))
            self.model.eval()
            self.load_ms=(time.perf_counter()-start)*1000
        return self.load_ms

    def evaluate(self,env,mask):
        if self.monitor:
            self.last_output_meta=None
            from omok_monitoring.checks import context
            ctx=dict(context(env.snapshot()),model_sha256=self.descriptor["sha256"])
            self.monitor.progress("model_forward",{"token":ctx["token"]})
            try:
                result=self._evaluate(env,mask)
            except Exception as error:
                self.monitor.emit("LP-01","failure","model_evaluation_failed",{"error":str(error),"output":self.last_output_meta},context=ctx)
                raise
            self.monitor.emit("LP-01","normal","existing_model_output_checks_passed",{"shape_logits":[1,225],"shape_value":[1,1]},context=ctx)
            return result
        return self._evaluate(env,mask)

    def _evaluate(self,env,mask):
        """Same checked masked policy and current-actor value for policy/PUCT."""
        self.prepare()
        token=env.snapshot()["token"]
        if tuple(mask)!=env.mask():
            if self.monitor:self.monitor.emit("GR-01","failure","model_mask_snapshot_mismatch",{"token":token})
            raise ValueError("mask is not current environment mask")
        obs=env.observation()
        if obs is None:
            raise ValueError("cannot infer in a terminal environment")
        with torch.inference_mode():
            forward_start=time.perf_counter()
            logits,value=self.model(torch.tensor(list(obs),dtype=torch.float32).reshape(1,4,15,15))
            if self.monitor:
                self.last_output_meta=dict(logits_shape=list(logits.shape) if hasattr(logits,"shape") else None,
                    value_shape=list(value.shape) if hasattr(value,"shape") else None,
                    logits_finite=bool(torch.isfinite(logits).all()) if isinstance(logits,torch.Tensor) else False,
                    value_finite=bool(torch.isfinite(value).all()) if isinstance(value,torch.Tensor) else False)
            checked_outputs(logits,value,1)
            probs=probabilities(logits,torch.tensor([mask],dtype=torch.bool))
            if self.monitor:
                from omok_monitoring.checks import raw_policy_mass,context
                raw=torch.softmax(logits,dim=1)
                mass=float(raw[0][~torch.tensor(mask,dtype=torch.bool)].sum())
                raw_policy_mass(self.monitor,mass,ctx=dict(context(env.snapshot()),model_sha256=self.descriptor["sha256"]))
            forward_ms=(time.perf_counter()-forward_start)*1000
        if env.snapshot()["token"]!=token:
            if self.monitor:self.monitor.emit("GR-01","failure","model_result_stale",{"expected_token":token,"actual_token":env.snapshot()["token"]})
            raise ValueError("state changed during inference")
        return dict(priors=probs[0].tolist(),value=float(value[0,0]),forward_ms=forward_ms,token=token)

    def select(self,env,mask):
        start=time.perf_counter()
        needs_load=self.model is None
        result=self.evaluate(env,mask)
        priors=result["priors"]
        action=max((a for a,legal in enumerate(mask) if legal),key=lambda a:(priors[a],-a))
        return dict(action=action,value=result["value"],policy_probability=priors[action],
            milliseconds=(time.perf_counter()-start)*1000,forward_ms=result["forward_ms"],
            model_load_ms=self.load_ms if needs_load else 0.0,
            model_sha256=self.descriptor["sha256"],token=result["token"])
