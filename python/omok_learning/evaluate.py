"""Development-only model games, or a separate explicit final loss evaluation."""
import argparse
import json
from pathlib import Path
import torch
from omok_env.runner import run
from omok_env.records import save
from .player import ModelPlayer
from .checkpoint import load
from .data import load_dataset,file_hash
from .train import metrics,TrainConfig
from .mcts import PUCTPlayer,SearchConfig


def games(model_path,output,*,opponent="random",count=2,seed=23,player="policy",simulations=16,c_puct=1.5,
          search_seed=None,tie_break="index",compact="",source_id="local-v1"):
    torch.set_num_threads(1);torch.use_deterministic_algorithms(True)
    evaluator=ModelPlayer(model_path)
    if player not in ("policy","puct"):raise ValueError("unknown player")
    config=SearchConfig(simulations,c_puct,seed if search_seed is None else search_seed,tie_break)
    own=evaluator if player=="policy" else PUCTPlayer(evaluator,config)
    if opponent not in ("self","policy","random","ordered"): raise ValueError("unknown opponent")
    other=own if opponent=="self" else evaluator if opponent=="policy" else opponent
    return run(output,games=count,seed=seed,players=(own,other),compact=compact,source_id=source_id)


def final_metrics(model_path,dataset_path,output):
    # Invoked only after model/config selection is frozen; never called by train.
    torch.set_num_threads(1); torch.use_deterministic_algorithms(True)
    model,artifact=load(model_path,kind="inference")
    dataset=load_dataset(dataset_path)
    if artifact["metadata"]["dataset_id"]!=dataset["id"]: raise ValueError("evaluation split mismatch")
    result=dict(role="final-held-out-fixture-check-not-strength",model_sha256=file_hash(model_path),
        dataset_id=dataset["id"],metrics=metrics(model,dataset["samples"]["final"],TrainConfig()))
    save(output,result); return result


if __name__=="__main__":
    p=argparse.ArgumentParser();sub=p.add_subparsers(dest="command",required=True)
    g=sub.add_parser("games");g.add_argument("model");g.add_argument("output")
    g.add_argument("--opponent",choices=("ordered","random","self","policy"),default="random")
    g.add_argument("--player",choices=("policy","puct"),default="policy")
    g.add_argument("--simulations",type=int,default=16);g.add_argument("--c-puct",type=float,default=1.5)
    g.add_argument("--search-seed",type=int);g.add_argument("--tie-break",choices=("index","seeded"),default="index")
    g.add_argument("--compact",default="");g.add_argument("--source-id",default="local-v1")
    g.add_argument("--games",type=int,default=2);g.add_argument("--seed",type=int,default=23)
    f=sub.add_parser("final");f.add_argument("model");f.add_argument("dataset");f.add_argument("output")
    a=p.parse_args()
    result=games(a.model,a.output,opponent=a.opponent,count=a.games,seed=a.seed,player=a.player,
        simulations=a.simulations,c_puct=a.c_puct,search_seed=a.search_seed,tie_break=a.tie_break,
        compact=a.compact,source_id=a.source_id) if a.command=="games" else final_metrics(a.model,a.dataset,a.output)
    print(json.dumps(result,indent=2))
