"""Self-authored short legal traces for plumbing tests, never expert games."""
import argparse
import random
from pathlib import Path
from omok_env import Environment,PROFILE
from omok_env.records import begin,apply,finish,save
from .data import file_hash


def generate(output, games=10):
    output=Path(output)
    if (output/"sources.json").exists():
        raise ValueError("fixtures already exist")
    sources=[]
    for i in range(games):
        # Alternate scripted black/white wins with deterministic random distraction
        # stones in the opposite half. This is deliberately an easy, biased fixture.
        winner=i%2
        line_row=2+i//2
        line=[line_row*15+c for c in range(3,8)]
        rng=random.Random(8000+i)
        candidates=[r*15+c for r in range(10,15) for c in range(15)]
        rng.shuffle(candidates)
        start=dict(compact="",source_id=f"authored-learning-fixture-{i}-v1",black_owner=(i//2)%2,profile=PROFILE)
        with Environment(**start) as env:
            record=begin(env,game_id=f"authored-{i}",start=start,config=dict(purpose="functional-verification-only"),
                         seed=8000+i,code_version=dict(generator="authored-short-lines-v1"))
            j=0
            while env.snapshot()["phase"] == "playing":
                color=env.snapshot()["next_color"]
                if color == winner:
                    action=line[j]; j+=1
                else:
                    mask=env.mask()
                    action=next(a for a in candidates if mask[a])
                    candidates.remove(action)
                apply(env,record,action)
            if env.snapshot()["end_reason"] != "five":
                raise RuntimeError("fixture did not finish with a five")
            finish(env,record,"completed")
            path=output/f"authored-{i:02d}.json"
            save(path,record)
            sources.append(dict(path=path.name,sha256=file_hash(path),role="functional-fixture",
                rights=dict(status="self-authored-local-verification",evidence="omok_learning.fixtures authored-short-lines-v1; no external source",usage="functional-verification-only")))
    save(output/"sources.json",dict(schema="omok-sources-v1",sources=sources))
    return output/"sources.json"


if __name__ == "__main__":
    p=argparse.ArgumentParser(); p.add_argument("output"); p.add_argument("--games",type=int,default=10)
    a=p.parse_args(); print(generate(a.output,a.games))
