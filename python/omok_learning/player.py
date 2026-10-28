"""Greedy masked policy inference; value is returned for analysis only."""
from pathlib import Path
import hashlib
import time
import torch
from .checkpoint import load
from .model import checked_outputs,probabilities


class ModelPlayer:
    def __init__(self,path):
        self.path=str(Path(path).resolve())
        self.model=None
        self.artifact=None
        self.descriptor=dict(kind="policy-model-v1",path=self.path,
            sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest() if Path(path).is_file() else None,
            selection="masked-argmax-lowest-index-tie",value_usage="analysis-only")

    def select(self,env,mask):
        start=time.perf_counter()
        if self.model is None:
            if hashlib.sha256(Path(self.path).read_bytes()).hexdigest()!=self.descriptor["sha256"]:
                raise ValueError("model file changed after player configuration")
            self.model,self.artifact=load(self.path,kind="inference")
            self.model.eval()
        token=env.snapshot()["token"]
        if tuple(mask)!=env.mask():
            raise ValueError("mask is not current environment mask")
        obs=env.observation()
        if obs is None:
            raise ValueError("cannot infer in a terminal environment")
        with torch.inference_mode():
            forward_start=time.perf_counter()
            logits,value=self.model(torch.tensor(list(obs),dtype=torch.float32).reshape(1,4,15,15))
            checked_outputs(logits,value,1)
            probs=probabilities(logits,torch.tensor([mask],dtype=torch.bool))
            action=int(probs.argmax(dim=1)[0])
            forward_ms=(time.perf_counter()-forward_start)*1000
        if env.snapshot()["token"]!=token:
            raise ValueError("state changed during inference")
        return dict(action=action,value=float(value[0,0]),policy_probability=float(probs[0,action]),
            milliseconds=(time.perf_counter()-start)*1000,forward_ms=forward_ms,
            model_sha256=self.descriptor["sha256"],token=token)
