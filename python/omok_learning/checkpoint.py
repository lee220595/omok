"""Distinct inference/resume artifacts and explicit SL -> RL weight initialization."""
from dataclasses import asdict
import hashlib
import os
from pathlib import Path
import platform
import random
import tempfile
import torch
from .contracts import CONTRACT
from .model import ModelConfig,PolicyValueNet

SCHEMA="omok-checkpoint-v1"


def content_hash(value):
    h=hashlib.sha256()
    def visit(v):
        if isinstance(v,torch.Tensor):
            h.update(str(v.dtype).encode()); h.update(str(tuple(v.shape)).encode())
            h.update(bytes(v.detach().cpu().contiguous().reshape(-1).view(torch.uint8).tolist()))
        elif isinstance(v,dict):
            h.update(b"dict")
            for k in sorted(v,key=str): visit(k); visit(v[k])
        elif isinstance(v,(list,tuple)):
            h.update(type(v).__name__.encode()); h.update(str(len(v)).encode())
            for item in v: visit(item)
        elif v is None or isinstance(v,(str,int,float,bool)):
            h.update((type(v).__name__+":"+repr(v)).encode())
        else:
            raise ValueError(f"unsupported checkpoint content {type(v)}")
    visit(value)
    return h.hexdigest()


def runtime():
    return dict(torch=str(torch.__version__),python=platform.python_version(),os=platform.platform(),
        device="cpu",threads=torch.get_num_threads(),deterministic=torch.are_deterministic_algorithms_enabled())


def rng_state():
    return dict(python=random.getstate(),torch=torch.get_rng_state())


def restore_rng(state):
    random.setstate(state["python"]); torch.set_rng_state(state["torch"])


def atomic_save(path,payload):
    path=Path(path); path.parent.mkdir(parents=True,exist_ok=True)
    payload=dict(payload)
    payload.pop("integrity",None)
    payload["integrity"]=content_hash(payload)
    fd,name=tempfile.mkstemp(prefix=path.name+".",suffix=".tmp",dir=path.parent)
    try:
        with os.fdopen(fd,"wb") as stream:
            torch.save(payload,stream); stream.flush(); os.fsync(stream.fileno())
        os.replace(name,path)
    finally:
        if os.path.exists(name): os.unlink(name)


def payload(model,kind,metadata,**state):
    return dict(schema=SCHEMA,kind=kind,stage="sl",contract=CONTRACT,architecture="small-cnn-v1",
        model_config=asdict(model.config),weights=model.state_dict(),metadata=metadata,runtime=runtime(),**state)


def load(path,*,kind=None):
    try:
        data=torch.load(path,map_location="cpu",weights_only=True)
        if not isinstance(data,dict) or data.get("schema")!=SCHEMA or data.get("contract")!=CONTRACT:
            raise ValueError("checkpoint contract/schema mismatch")
        if data.get("architecture")!="small-cnn-v1" or data.get("stage")!="sl" or data.get("kind") not in ("inference","sl-resume"):
            raise ValueError("unsupported architecture/stage/artifact kind")
        if kind is not None and data["kind"]!=kind:
            raise ValueError(f"expected {kind}, got {data['kind']}")
        if data["integrity"]!=content_hash({k:v for k,v in data.items() if k!="integrity"}):
            raise ValueError("checkpoint content hash mismatch")
        model=PolicyValueNet(ModelConfig(**data["model_config"]))
        model.load_state_dict(data["weights"],strict=True)
        if not all(torch.isfinite(v).all() for v in model.state_dict().values()):
            raise ValueError("nonfinite checkpoint weights")
        if data["kind"]=="sl-resume":
            for key in ("optimizer","scheduler","rng","epoch","step","cursor","config","dataset_id","history","best_score","best_weights","best_epoch"):
                if key not in data: raise ValueError("missing resume field: "+key)
            if data["cursor"]!=0: raise ValueError("only completed-epoch resumes are supported")
            expected=model.state_dict()
            best=data["best_weights"]
            if set(best)!=set(expected) or any(best[k].shape!=expected[k].shape or best[k].dtype!=expected[k].dtype or not torch.isfinite(best[k]).all() for k in expected):
                raise ValueError("invalid selected-model weights in resume checkpoint")
        return model,data
    except Exception as exc:
        raise ValueError(f"cannot load compatible checkpoint: {exc}") from exc


def initialize_for_rl(path,*,optimizer_mode="reset"):
    """Load real shared weights; no experience generation or RL update is performed."""
    if optimizer_mode not in ("reset","preserve"):
        raise ValueError("optimizer_mode must be reset or preserve")
    model,artifact=load(path)
    if optimizer_mode=="preserve" and artifact["kind"]!="sl-resume":
        raise ValueError("preserving optimizer requires a complete SL resume artifact")
    optimizer=None
    if optimizer_mode=="preserve":
        optimizer=torch.optim.AdamW(model.parameters(),lr=artifact["config"]["lr"],weight_decay=artifact["config"]["weight_decay"])
        optimizer.load_state_dict(artifact["optimizer"])
    return model,optimizer,dict(stage="rl-initial-weights",parent_sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
        optimizer_mode=optimizer_mode,rl_step=0,experience=None,contract=CONTRACT)
