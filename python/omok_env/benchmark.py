"""Small sequential benchmark; no rule shortcuts or mask cache."""
import argparse
import json
import os
import platform
from pathlib import Path
import statistics
import subprocess
import time
from . import Environment
from .runner import run


def timed(fn, samples):
    values=[]
    for _ in range(samples):
        start=time.perf_counter_ns()
        fn()
        values.append((time.perf_counter_ns()-start)/1e6)
    return dict(samples=samples, mean_ms=statistics.mean(values), min_ms=min(values),max_ms=max(values))


def main():
    p=argparse.ArgumentParser()
    p.add_argument("output")
    p.add_argument("--samples",type=int,default=30)
    p.add_argument("--games",type=int,default=4)
    args=p.parse_args()
    if args.samples < 1 or args.games < 1: p.error("positive samples/games required")
    result=dict(os=platform.platform(),python=platform.python_version(),processor=platform.processor(),
                logical_cpus=os.cpu_count(), execution="single process, sequential; no affinity pinning",
                compiler=subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
                build="Release (build this configuration before measuring)")
    with Environment(compact="h8i8g9") as e:
        result["reset"]=timed(lambda:e.reset(compact="h8i8g9"),args.samples)
        result["snapshot"]=timed(e.snapshot,args.samples)
        result["observation"]=timed(e.observation,args.samples)
        result["mask_prepared3"]=timed(e.mask,args.samples)
        def clone():
            with e.clone(): pass
        result["clone_and_dispose"]=timed(clone,args.samples)
        copies=[e.clone() for _ in range(args.samples)]
        iterator=iter(copies)
        result["step"]=timed(lambda:next(iterator).step(0),args.samples)
        for item in copies: item.dispose()
        e.reset(compact="g8a1h7c1i8e1h9g1")
        result["mask_threat8"]=timed(e.mask,args.samples)
    start=time.perf_counter()
    games=run(Path(args.output)/"games",games=args.games,seed=20261028,players=("random","random"))
    elapsed=time.perf_counter()-start
    result["whole_run_with_save_and_replay"]=dict(**games,wall_seconds=elapsed,
            moves_per_second=games["new_moves"]/elapsed,
            games_per_second=games["counts"].get("completed",0)/elapsed)
    output=Path(args.output)/"benchmark.json"
    output.write_text(json.dumps(result,indent=2),encoding="utf-8")
    print(json.dumps(result,indent=2))


if __name__ == "__main__": main()
