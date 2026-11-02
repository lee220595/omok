"""No-update model/checkpoint/SL-entry checks on copies, never a training run."""
import argparse
import json
from pathlib import Path
import shutil
from unittest.mock import patch
import torch
from omok_env import Environment
from omok_env.records import save
from omok_learning.player import ModelPlayer
from omok_learning.mcts import PUCTPlayer,SearchConfig
from omok_learning.checkpoint import load,atomic_save
from omok_learning.train import train,losses
from . import Monitor
from .checks import numeric,loss_observation
from .events import file_hash
from .acceptance import WIN


def execute(checkpoint,resume_checkpoint,output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):raise ValueError("independent empty output required")
    plan=dict(purpose="copied completed checkpoints, model forward/checks only; no learning",
        checkpoint_sha256=file_hash(checkpoint),resume_sha256=file_hash(resume_checkpoint),
        fixed_start=WIN,max_real_actions=0,max_simulations=2,optimizer_updates=0,
        stop="single checked forward/search or deterministic error; SL entry pre-stopped")
    save(output/"plan.json",plan)
    copied=output/"fixture-inference.pt";resume=output/"fixture-resume.pt"
    shutil.copyfile(checkpoint,copied);shutil.copyfile(resume_checkpoint,resume)
    torch.set_num_threads(1);torch.use_deterministic_algorithms(True)
    cases=[]
    def case(name,fn):
        with Monitor(output/name,config=plan) as m:
            m.bind(acceptance_case=name)
            fn(m)
        cases.append(dict(case=name,report=json.loads((output/name/"report.json").read_text(encoding="utf-8"))))

    def normal(m):
        evaluator=ModelPlayer(copied,monitor=m)
        with Environment(compact=WIN) as env:
            before=env.snapshot();evaluator.evaluate(env,env.mask())
            PUCTPlayer(evaluator,SearchConfig(2),monitor=m).select(env,env.mask())
            sample=dict(observation=list(env.observation()),mask=list(env.mask()),policy=dict(kind="action",action=109),
                policy_eligible=True,value=1.,value_eligible=True)
            with torch.inference_mode():p,v,metrics=losses(evaluator.model,[sample])
            numeric(m,dict(policy_loss=p,value_mse=v),epoch=0,step=0)
            loss_observation(m,dict(policy_loss=float(p),value_mse=float(v),samples=1,functional_only=True))
            assert env.snapshot()==before
    case("normal-model-numerics",normal)

    def broken(m,mode):
        player=ModelPlayer(copied,monitor=m);player.prepare()
        logits=torch.full((1,225),float("nan") if mode=="nan" else float("inf")) if mode in ("nan","inf") else torch.zeros(1,224)
        with Environment(compact=WIN) as env,patch.object(player.model,"forward",return_value=(logits,torch.zeros(1,1))):
            before=env.snapshot()
            try:player.evaluate(env,env.mask())
            except ValueError:pass
            else:raise AssertionError("bad model output accepted")
            assert env.snapshot()==before
        m.boundary("after_bad_output")
    case("nonfinite-output",lambda m:broken(m,"nan"))
    case("infinite-output",lambda m:broken(m,"inf"))
    case("wrong-shape",lambda m:broken(m,"shape"))
    def mutable(m):
        path=output/"mutable-inference.pt";shutil.copyfile(copied,path)
        player=ModelPlayer(path,monitor=m);player.prepare()
        path.write_bytes(b"injected change after fixed candidate loading")
        with Environment(compact=WIN) as env:
            try:player.evaluate(env,env.mask())
            except ValueError:pass
            else:raise AssertionError("changed model file accepted")
    case("mutable-loaded-model",mutable)

    def stopped_sl(m):
        m.request_stop("fixed pre-update acceptance stop")
        with patch("omok_learning.train._train",side_effect=AssertionError("new SL prohibited")):
            result=train("not-read-dataset",output/"must-not-train",monitor=m)
        assert result["status"]=="interrupted_before_training"
    case("sl-entry-stop",stopped_sl)

    def recovery_save(m):
        model,data=load(resume,kind="sl-resume",monitor=m)
        numeric(m,dict(optimizer=data["optimizer"]),epoch=data["epoch"],step=data["step"],collected=("optimizer",))
        m.request_stop("copy recovery state only")
        m.boundary("sl.last_saved_epoch",dict(epoch=data["epoch"],updates_performed=0))
        atomic_save(output/"copied-last-saved-epoch.pt",data,monitor=m)
        _,again=load(output/"copied-last-saved-epoch.pt",kind="sl-resume",monitor=m)
        assert again["epoch"]==data["epoch"]
    case("checkpoint-save-recovery",recovery_save)

    def invalid_checkpoint(m):
        path=output/"broken.pt";path.write_bytes(b"injected damaged checkpoint copy")
        try:load(path,kind="inference",monitor=m)
        except ValueError:pass
        else:raise AssertionError("broken file accepted")
    case("damaged-checkpoint",invalid_checkpoint)

    def failed_save(m):
        _,data=load(resume,kind="sl-resume",monitor=m)
        path=output/"existing-good-copy.pt";shutil.copyfile(resume,path);old=file_hash(path)
        m.request_stop("save-failure acceptance")
        m.boundary("sl.save_requested")
        with patch("torch.save",side_effect=OSError("injected checkpoint save error")):
            try:atomic_save(path,data,monitor=m)
            except OSError:pass
            else:raise AssertionError("save failure was hidden")
        assert file_hash(path)==old
    case("checkpoint-save-failure",failed_save)
    assert file_hash(checkpoint)==plan["checkpoint_sha256"] and file_hash(resume_checkpoint)==plan["resume_sha256"]
    result=dict(plan=plan,cases=cases,source_checkpoints_unchanged=True,optimizer_updates=0)
    save(output/"acceptance-summary.json",result)
    return result


if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("checkpoint");p.add_argument("resume_checkpoint");p.add_argument("output")
    a=p.parse_args()
    # Any accidental call to the existing optimizer is a test failure.
    with patch("torch.optim.AdamW.step",side_effect=AssertionError("optimizer updates prohibited")):
        result=execute(a.checkpoint,a.resume_checkpoint,a.output)
    print(json.dumps(dict(cases=len(result["cases"]),optimizer_updates=0,source_checkpoints_unchanged=result["source_checkpoints_unchanged"]),indent=2))
