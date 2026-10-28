"""Rights-gated trace conversion, game/D4 grouping and fixed split manifest."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import random
from omok_env import Environment
from omok_env.records import replay, save
from omok_env.runner import code_version
from .contracts import CONTRACT, SAMPLE_SCHEMA, PREPROCESS, transform_action, spatial


def digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(",",":"),allow_nan=False).encode()).hexdigest()


def file_hash(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def value_target(final, actor):
    if final["end_reason"] not in ("five","move_limit","board_full"):
        return None
    if final["result"] == "draw":
        return 0.0
    if final["result"] == "won":
        return 1.0 if final["winner"] == actor else -1.0
    return None


def canonical_game(record):
    # Ignore participant names/ownership; preserve colors and ordered prepared/normal events.
    sequence=[(m["action"],m["color"],"prepared") for m in record["initial"]["history"]]
    sequence += [(m["action"],m["color"],m["decision"]["verdict"]) for m in record["actions"]]
    return min(digest([[transform_action(a,k),color,kind] for a,color,kind in sequence]) for k in range(8))


def position_key(sample):
    return min(digest(spatial(sample["observation"],k)) for k in range(8))


def extract(record, parent_hash):
    replay(record)
    if record["status"] != "completed":
        return [],Counter({"execution_"+record["status"]:1})
    result=[]
    excluded=Counter()
    with Environment(**record["start"]) as env:
        for i,event in enumerate(record["actions"]):
            s=env.snapshot()
            mask=env.mask()
            if event["decision"]["verdict"] == "forbidden":
                excluded["forbidden_attempt"]+=1
            elif mask[event["action"]]:
                z=value_target(record["final"],s["next_actor"])
                result.append(dict(id=f"{parent_hash}:{i}",parent_hash=parent_hash,game_id=record["game_id"],
                    ply=s["placements"],actor=s["next_actor"],color=s["next_color"],
                    observation=list(env.observation()),mask=list(mask),
                    policy=dict(kind="action",action=event["action"]),policy_eligible=True,
                    value=z,value_eligible=z is not None,
                    value_reason="board-outcome-v1" if z is not None else "non_board_outcome_held"))
            else:
                raise ValueError("replayed policy target is illegal")
            env.step(event["action"],actor=event["actor"])
    return result,excluded


def convert(source_manifest, output, *, seed=20261028):
    source_manifest=Path(source_manifest)
    sources=json.loads(source_manifest.read_text(encoding="utf-8"))
    if sources["schema"] != "omok-sources-v1":
        raise ValueError("unsupported sources schema")
    output=Path(output)
    if (output/"dataset.json").exists():
        raise ValueError("dataset exists; use a new output directory")
    games=[]; rejected=[]; exclusions=Counter(); rules=Counter(); seen=set()
    for source in sources["sources"]:
        try:
            rights=source["rights"]
            if rights["status"] not in ("self-authored-local-verification","permission-confirmed") or not rights["evidence"]:
                raise ValueError("rights_unconfirmed")
            if source["role"] not in ("functional-fixture","supervised-authorized"):
                raise ValueError("evaluation_or_unknown_role_excluded")
            path=(source_manifest.parent/source["path"]).resolve()
            h=file_hash(path)
            if h != source["sha256"]:
                raise ValueError("source_hash_mismatch")
            record=json.loads(path.read_text(encoding="utf-8"))
            samples,reason=extract(record,h)
            exclusions.update(reason)
            rules[record["profile"]]+=1
            if not samples:
                rejected.append(dict(source=source,reason="no_eligible_samples")); continue
            key=canonical_game(record)
            if key in seen:
                rejected.append(dict(source=source,reason="duplicate_or_d4_game")); continue
            seen.add(key)
            games.append(dict(group=key,hash=h,source=source,samples=samples))
            save(output/"normalized"/(h+".json"),record)
        except (ValueError,KeyError,TypeError,IndexError,OSError) as exc:
            rejected.append(dict(source=source,reason=f"{type(exc).__name__}: {exc}"))
    save(output/"conversion-report.json",dict(accepted_games=len(games),rejected=rejected,exclusions=dict(exclusions),rules=dict(rules)))
    if len(games)<3:
        raise ValueError(f"at least 3 nonduplicate eligible games required; rejected={rejected}")
    games.sort(key=lambda g:g["group"])
    random.Random(seed).shuffle(games)
    n=len(games); nv=max(1,n//5); nt=max(1,n//5)
    assignment={g["hash"]:("validation" if i<nv else "final" if i<nv+nt else "train") for i,g in enumerate(games)}
    samples={name:[] for name in ("train","validation","final")}
    split_games={name:[] for name in samples}
    keys={name:set() for name in samples}
    for game in games:
        split=assignment[game["hash"]]
        samples[split].extend(game["samples"])
        split_games[split].append(dict(hash=game["hash"],group=game["group"],source=game["source"]))
        keys[split].update(position_key(s) for s in game["samples"])
    overlap={a+"/"+b:len(keys[a]&keys[b]) for a,b in (("train","validation"),("train","final"),("validation","final"))}
    # Explicit diagnostic, not a claim of independent strength data. Similar opening
    # groups are exact first-four ordered actions up to D4, including prepared stones.
    prefixes={name:set() for name in samples}
    for game in games:
        record=json.loads((output/"normalized"/(game["hash"]+".json")).read_text(encoding="utf-8"))
        seq=[m["action"] for m in record["initial"]["history"]]+[m["action"] for m in record["actions"]]
        prefixes[assignment[game["hash"]]].add(min(tuple(transform_action(a,k) for a in seq[:4]) for k in range(8)))
    prefix_overlap={a+"/"+b:len(prefixes[a]&prefixes[b]) for a,b in (("train","validation"),("train","final"),("validation","final"))}
    with Environment() as probe:
        native_hash=file_hash(probe._dll._name)
    manifest=dict(schema=SAMPLE_SCHEMA,contract=CONTRACT,preprocess=PREPROCESS,seed=seed,converter_code=code_version(),native_sha256=native_hash,
        source_manifest_sha256=file_hash(source_manifest),games=split_games,rejected=rejected,
        exclusions=dict(exclusions),rules=dict(rules),counts={name:len(s) for name,s in samples.items()},
        value_counts={name:sum(s["value_eligible"] for s in data) for name,data in samples.items()},
        exact_d4_position_overlap=overlap,first4_d4_prefix_overlap=prefix_overlap,
        leakage_policy="full-game D4 groups isolated; shared positions/prefixes reported, not removed; diagnostic fixtures only",
        augmentation="D4 after split, train only",target_schema="action index or normalized distribution225; independent policy/value eligibility")
    dataset=dict(manifest=manifest,samples=samples)
    dataset["id"]=digest(dataset)
    save(output/"dataset.json",dataset)
    save(output/"manifest.json",dict(id=dataset["id"],**manifest))
    return dict(id=dataset["id"],**manifest)


def load_dataset(path):
    data=json.loads(Path(path).read_text(encoding="utf-8"))
    if data["manifest"]["schema"] != SAMPLE_SCHEMA or data["manifest"]["contract"] != CONTRACT or data["manifest"]["preprocess"] != PREPROCESS:
        raise ValueError("dataset contract mismatch")
    if data["id"] != digest({k:v for k,v in data.items() if k!="id"}):
        raise ValueError("dataset content hash mismatch")
    return data


if __name__ == "__main__":
    p=argparse.ArgumentParser()
    p.add_argument("sources"); p.add_argument("output"); p.add_argument("--seed",type=int,default=20261028)
    args=p.parse_args()
    print(json.dumps(convert(args.sources,args.output,seed=args.seed),indent=2))
