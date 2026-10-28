"""Short CPU supervised training with epoch-boundary exact resume and held-out selection."""
import argparse
import copy
import json
import math
from pathlib import Path
import random
import time
from dataclasses import asdict,dataclass
import torch
from torch.nn import functional as F
from omok_env.records import save
from omok_env.runner import code_version
from .contracts import augment
from .data import load_dataset
from .model import ModelConfig,PolicyValueNet,checked_outputs,masked_logits
from .checkpoint import payload,atomic_save,load,rng_state,restore_rng,runtime


@dataclass(frozen=True)
class TrainConfig:
    seed:int=17
    batch_size:int=16
    lr:float=0.01
    weight_decay:float=0.0001
    policy_weight:float=1.0
    value_weight:float=1.0
    gamma:float=0.98
    augment:bool=True
    threads:int=1

    def __post_init__(self):
        if (any(type(x) is not int for x in (self.seed,self.batch_size,self.threads)) or type(self.augment) is not bool
            or any(not math.isfinite(x) for x in (self.lr,self.weight_decay,self.policy_weight,self.value_weight,self.gamma))
            or self.batch_size<1 or self.threads<1 or self.lr<=0 or self.weight_decay<0 or self.policy_weight<0 or self.value_weight<0 or self.policy_weight+self.value_weight<=0 or not 0<self.gamma<=1):
            raise ValueError("invalid training configuration")


def batch(samples):
    obs=torch.tensor([s["observation"] for s in samples],dtype=torch.float32).reshape(-1,4,15,15)
    masks=torch.tensor([s["mask"] for s in samples],dtype=torch.bool)
    policy=[]; pe=[]; values=[]; ve=[]
    for s in samples:
        target=s["policy"]
        if target["kind"]=="action":
            a=target["action"]
            if type(a) is not int or not 0<=a<225: raise ValueError("invalid policy action")
            probs=[0.0]*225; probs[a]=1.0
        elif target["kind"]=="distribution":
            probs=target["probabilities"]
        else: raise ValueError("unknown policy target kind")
        t=torch.tensor(probs,dtype=torch.float32)
        if t.shape!=(225,) or not torch.isfinite(t).all() or (t<0).any() or abs(t.sum().item()-1)>1e-5:
            raise ValueError("policy target is not a probability distribution")
        if s["policy_eligible"] and any(p>0 and not valid for p,valid in zip(probs,s["mask"])):
            raise ValueError("policy target has illegal mass")
        if s["value_eligible"] and (s["value"] is None or not -1<=s["value"]<=1):
            raise ValueError("invalid eligible value target")
        policy.append(probs); pe.append(s["policy_eligible"])
        values.append(s["value"] if s["value_eligible"] else 0.0); ve.append(s["value_eligible"])
    return obs,masks,torch.tensor(policy),torch.tensor(pe,dtype=torch.bool),torch.tensor(values).float(),torch.tensor(ve,dtype=torch.bool)


def losses(model,samples):
    x,mask,target,pe,z,ve=batch(samples)
    logits,v=model(x); checked_outputs(logits,v,len(samples))
    masked=masked_logits(logits,mask)
    # Avoid 0 * -inf at illegal coordinates for both action and distribution targets.
    logp=F.log_softmax(masked,dim=1).masked_fill(~mask,0.0)
    p=(-(target*logp).sum(dim=1)[pe]).mean() if pe.any() else logits.sum()*0
    value=F.mse_loss(v[:,0][ve],z[ve]) if ve.any() else v.sum()*0
    with torch.no_grad():
        actual=target.argmax(dim=1)
        pred=masked.argmax(dim=1)
        metrics=dict(policy_n=int(pe.sum()),value_n=int(ve.sum()),
            policy_sum=float(p.detach())*int(pe.sum()),value_sum=float(value.detach())*int(ve.sum()),
            top1=int(((pred==actual)&pe).sum()),
            top5=int(((masked.topk(5,dim=1).indices==actual[:,None]).any(dim=1)&pe).sum()))
    return p,value,metrics


def metrics(model,samples,config):
    model.eval(); totals=dict(policy_n=0,value_n=0,policy_sum=0.,value_sum=0.,top1=0,top5=0)
    with torch.inference_mode():
        for i in range(0,len(samples),config.batch_size):
            _,_,m=losses(model,samples[i:i+config.batch_size])
            for k,v in m.items(): totals[k]+=v
    np,nv=totals["policy_n"],totals["value_n"]
    return dict(policy_loss=totals["policy_sum"]/np if np else None,
        value_mse=totals["value_sum"]/nv if nv else None,
        top1=totals["top1"]/np if np else None,top5=totals["top5"]/np if np else None,
        policy_n=np,value_n=nv)


def score(m,c):
    return c.policy_weight*(m["policy_loss"] or 0)+c.value_weight*(m["value_mse"] or 0)


def train(dataset_path,output,*,epochs=5,config=TrainConfig(),model_config=ModelConfig(),resume=None):
    if type(epochs) is not int or epochs<1: raise ValueError("epochs must be positive")
    torch.set_num_threads(config.threads); torch.use_deterministic_algorithms(True)
    random.seed(config.seed); torch.manual_seed(config.seed)
    dataset=load_dataset(dataset_path)
    # Final split is deliberately never read during selection/training.
    training=dataset["samples"]["train"]; validation=dataset["samples"]["validation"]
    if not training or not validation: raise ValueError("train and validation required")
    output=Path(output); output.mkdir(parents=True,exist_ok=True)
    if (output/"latest.pt").exists() and resume is None: raise ValueError("output already has a training run")
    version=code_version()
    model=PolicyValueNet(model_config)
    optimizer=torch.optim.AdamW(model.parameters(),lr=config.lr,weight_decay=config.weight_decay)
    scheduler=torch.optim.lr_scheduler.StepLR(optimizer,step_size=1,gamma=config.gamma)
    epoch=step=0; history=[]; best=float("inf"); best_weights=None; best_epoch=0
    initial=metrics(model,training,config)
    if resume:
        model,old=load(resume,kind="sl-resume")
        if old["config"]!=asdict(config) or old["dataset_id"]!=dataset["id"] or model.config!=model_config:
            raise ValueError("resume config/data/model mismatch")
        if old["runtime"]!=runtime() or old["metadata"]["code"]["source_sha256"]!=version["source_sha256"]:
            raise ValueError("resume runtime/source mismatch")
        optimizer=torch.optim.AdamW(model.parameters(),lr=config.lr,weight_decay=config.weight_decay)
        scheduler=torch.optim.lr_scheduler.StepLR(optimizer,step_size=1,gamma=config.gamma)
        optimizer.load_state_dict(old["optimizer"]); scheduler.load_state_dict(old["scheduler"])
        epoch,step=old["epoch"],old["step"]
        history=old["history"]; best=old["best_score"]; best_weights=old["best_weights"]
        best_epoch=old["best_epoch"]
        initial=old["metadata"]["initial_train"]
        restore_rng(old["rng"])
    if epochs<=epoch: raise ValueError("target epochs must exceed completed epochs")
    started=time.perf_counter(); status="completed"
    try:
        while epoch<epochs:
            model.train()
            order=torch.randperm(len(training)).tolist()
            for offset in range(0,len(order),config.batch_size):
                samples=[training[i] for i in order[offset:offset+config.batch_size]]
                if config.augment: samples=[augment(s,random.randrange(8)) for s in samples]
                p,v,_=losses(model,samples)
                loss=config.policy_weight*p+config.value_weight*v
                if not torch.isfinite(loss): raise ValueError("nonfinite loss")
                optimizer.zero_grad(set_to_none=True); loss.backward(); optimizer.step(); step+=1
            scheduler.step(); epoch+=1
            tm=metrics(model,training,config); vm=metrics(model,validation,config)
            history.append(dict(epoch=epoch,step=step,train=tm,validation=vm,lr=scheduler.get_last_lr()[0]))
            if score(vm,config)<best:
                best=score(vm,config); best_weights=copy.deepcopy(model.state_dict()); best_epoch=epoch
            metadata=dict(code=version,dataset_id=dataset["id"],preprocess=dataset["manifest"]["preprocess"],
                split_id=dataset["id"],seed=config.seed,initial_train=initial,selection="validation weighted loss only",
                training_config=asdict(config),
                parent=str(resume) if resume else None)
            atomic_save(output/"latest.pt",payload(model,"sl-resume",metadata,optimizer=optimizer.state_dict(),
                scheduler=scheduler.state_dict(),rng=rng_state(),epoch=epoch,step=step,cursor=0,
                config=asdict(config),dataset_id=dataset["id"],history=history,best_score=best,best_weights=best_weights,best_epoch=best_epoch))
            # Model construction consumes RNG; restore it to keep resume identical.
            rng=rng_state()
            selected=PolicyValueNet(model_config); selected.load_state_dict(best_weights)
            atomic_save(output/"best.pt",payload(selected,"inference",dict(metadata,best_score=best,selected_epoch=best_epoch,
                selected_step=history[best_epoch-1]["step"],completed_epoch=epoch)))
            restore_rng(rng)
            save(output/"metrics.json",dict(status="running",history=history,initial_train=initial))
    except KeyboardInterrupt:
        status="interrupted_last_completed_epoch_only"
    report=dict(status=status,epochs=epoch,step=step,initial_train=initial,history=history,
        elapsed_seconds=time.perf_counter()-started,parameters=sum(p.numel() for p in model.parameters()),
        dataset_id=dataset["id"],runtime=runtime(),model_config=asdict(model_config),config=asdict(config),code=version)
    save(output/"metrics.json",report)
    return report


if __name__=="__main__":
    p=argparse.ArgumentParser(); p.add_argument("dataset");p.add_argument("output")
    p.add_argument("--epochs",type=int,default=5);p.add_argument("--resume");p.add_argument("--config")
    a=p.parse_args(); spec=json.loads(Path(a.config).read_text()) if a.config else {}
    print(json.dumps(train(a.dataset,a.output,epochs=a.epochs,resume=a.resume,
        config=TrainConfig(**spec.get("training",{})),model_config=ModelConfig(**spec.get("model",{}))),indent=2))
