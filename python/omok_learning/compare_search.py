"""Small sequential development comparison; fixed simulations, never a strength claim."""
import argparse
from collections import Counter
from dataclasses import asdict
import json
from pathlib import Path
import platform
import statistics
import time
import torch
from omok_env import Environment
from omok_env.records import replay,save
from omok_env.runner import run,code_version
from .data import file_hash
from .mcts import PUCTPlayer,SearchConfig
from .player import ModelPlayer


def mean(values):
    return statistics.mean(values) if values else None


def compare(model_path,output,*,simulations=(1,8,32),games=2,seed=23,c_puct=1.5,
            starts=("","h8i8g9"),samples=3):
    if type(games) is not int or games<2 or games%2 or type(samples) is not int or samples<1:
        raise ValueError("positive samples and even games >=2 required for color pairs")
    if not simulations or len(set(simulations))!=len(simulations) or not starts or len(set(starts))!=len(starts):
        raise ValueError("nonempty unique budgets/starts required")
    configs=[SearchConfig(n,c_puct,seed) for n in simulations]
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    torch.set_num_threads(1);torch.use_deterministic_algorithms(True)
    evaluator=ModelPlayer(model_path)
    frozen=dict(schema="omok-puct-comparison-v1",code=code_version(),model=evaluator.descriptor,
        configs=[asdict(c) for c in configs],games_per_start=games,seed=seed,starts=list(starts),samples=samples,
        os=platform.platform(),python=platform.python_version(),torch=str(torch.__version__),
        threads=1,device="cpu",root_noise=False,tree_reuse=False,
        role="development-functional-comparison-not-strength",time_budget_comparison=False)
    with Environment() as probe:frozen["native_sha256"]=file_hash(probe._dll._name)
    manifest=output/"comparison-manifest.json"
    if manifest.exists() and json.loads(manifest.read_text(encoding="utf-8"))!=frozen:
        raise ValueError("comparison configuration/code/model changed; use a new output")
    save(manifest,frozen)
    tick=time.perf_counter();load_ms=evaluator.prepare()
    warmup=[]
    with Environment(compact="h8i8g9") as env:
        for _ in range(3):warmup.append(evaluator.select(env,env.mask())["milliseconds"])
    preparation=dict(model_load_ms=load_ms,model_load_and_warmup_ms=(time.perf_counter()-tick)*1000,
        warmup_calls=3,warmup_ms=warmup,process_import_excluded=True)
    positions=[];results=[]
    for opening_index,compact in enumerate(starts):
        start=dict(compact=compact,source_id=f"puct-dev-start-{opening_index}-v1")
        with Environment(**start) as env:
            mask=env.mask();before=env.snapshot()
            policy=[evaluator.select(env,mask) for _ in range(samples)]
            positions.append(dict(start=start,kind="policy",samples=samples,
                mean_ms=mean([x["milliseconds"] for x in policy]),network_evaluations_per_move=1,
                actions=[x["action"] for x in policy]))
            for config in configs:
                player=PUCTPlayer(evaluator,config)
                selected=[player.select(env,mask) for _ in range(samples)]
                positions.append(dict(start=start,kind="puct",config=asdict(config),samples=samples,
                    mean_ms=mean([x["milliseconds"] for x in selected]),
                    network_evaluations_per_move=mean([x["search"]["network_evaluations"] for x in selected]),
                    simulations_per_move=mean([x["search"]["simulations"] for x in selected]),
                    mean_transitions=mean([x["search"]["transitions"] for x in selected]),
                    actions=[x["action"] for x in selected],
                    reproducible_visits=all(x["root_visits"]==selected[0]["root_visits"] for x in selected)))
            if env.snapshot()!=before:raise RuntimeError("benchmark mutated its live environment")
        for config in configs:
            folder=output/f"start-{opening_index}-sim-{config.simulations}"
            # A is always PUCT, black ownership alternates. Both share this exact
            # loaded checkpoint evaluator, and each game runs sequentially.
            summary=run(folder,games=games,seed=seed,players=(PUCTPlayer(evaluator,config),evaluator),**start)
            outcomes=Counter();failures=[];moves=[];policy_moves=[]
            for i in range(games):
                path=folder/f"game-{i:06d}.json"
                if not path.exists():
                    failures.append(dict(game=i,reason="no trace; inspect failure file"));continue
                trace=json.loads(path.read_text(encoding="utf-8"));replay(trace)
                if trace["status"]!="completed":
                    failures.append(dict(game=i,status=trace["status"],diagnostic=trace["diagnostic"]));continue
                final=trace["final"]
                outcomes["draw" if final["result"]=="draw" else "win" if final["winner"]==0 else "loss"]+=1
                for event in trace["actions"]:
                    info=event.get("inference",{})
                    (moves if event["actor"]==0 else policy_moves).append(info)
                    if event["actor"]==0:
                        if sum(info["root_visits"])!=config.simulations or info["search"]["simulations"]!=config.simulations:
                            raise ValueError("trace search budget mismatch")
            entry=dict(start=start,config=asdict(config),summary=summary,puct_outcomes=dict(outcomes),
                failures=failures,pairs_complete=len(failures)==0,trace_replay_verified=True,
                puct_moves=len(moves),policy_moves=len(policy_moves),
                mean_puct_ms=mean([m["milliseconds"] for m in moves]),
                mean_policy_ms=mean([m["milliseconds"] for m in policy_moves]),
                mean_network_evaluations=mean([m["search"]["network_evaluations"] for m in moves]),
                total_network_evaluations=sum(m["search"]["network_evaluations"] for m in moves),
                total_simulations=sum(m["search"]["simulations"] for m in moves),
                mean_transitions=mean([m["search"]["transitions"] for m in moves]))
            results.append(entry)
            save(output/"progress.json",dict(manifest=frozen,preparation=preparation,positions=positions,results=results))
            print(json.dumps(dict(start=opening_index,simulations=config.simulations,outcomes=dict(outcomes),
                failures=len(failures),mean_puct_ms=entry["mean_puct_ms"])),flush=True)
    report=dict(manifest=frozen,preparation=preparation,positions=positions,results=results)
    save(output/"report.json",report)
    return report


if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("model");p.add_argument("output")
    p.add_argument("--simulations",nargs="+",type=int,default=(1,8,32));p.add_argument("--games",type=int,default=2)
    p.add_argument("--seed",type=int,default=23);p.add_argument("--c-puct",type=float,default=1.5)
    p.add_argument("--starts",nargs="+",default=("","h8i8g9"));p.add_argument("--samples",type=int,default=3)
    a=p.parse_args();compare(a.model,a.output,simulations=tuple(a.simulations),games=a.games,
        seed=a.seed,c_puct=a.c_puct,starts=tuple(a.starts),samples=a.samples)
