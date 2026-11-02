"""Sequential diagnostic players, paired colors, per-game seeds and safe resume."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import random
import subprocess
import time
from . import Environment, PROFILE
from .records import begin, apply, finish, save, replay


def code_version():
    root = Path(__file__).resolve().parents[2]
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=root, text=True).strip())
    # Content fingerprint also distinguishes different uncommitted experimental builds.
    digest = hashlib.sha256()
    for folder in ("src", "python"):
        for path in sorted((root/folder).rglob("*")):
            if path.suffix in (".cpp", ".hpp", ".py", ".txt"):
                digest.update(path.relative_to(root).as_posix().encode())
                digest.update(path.read_bytes())
    return dict(commit=revision, dirty=dirty, source_sha256=digest.hexdigest())


def run(output, *, games=2, seed=0, players=("ordered", "random"), compact="",
        source_id="local-v1", black_owner=0, max_actions=None, library=None,monitor=None):
    bindings=[]
    if monitor:
        for player in players:
            for obj in (player,getattr(player,"evaluator",None)):
                if hasattr(obj,"monitor") and not any(item[0] is obj for item in bindings):
                    bindings.append((obj,obj.monitor));obj.monitor=monitor
        monitor.progress("runner.start",dict(games=games,seed=seed))
    try:
        return _run(output,games=games,seed=seed,players=players,compact=compact,source_id=source_id,
            black_owner=black_owner,max_actions=max_actions,library=library,monitor=monitor)
    finally:
        for obj,old in bindings:obj.monitor=old
        if monitor:monitor.progress("runner.returned")


def _run(output, *, games=2, seed=0, players=("ordered", "random"), compact="",
        source_id="local-v1", black_owner=0, max_actions=None, library=None,monitor=None):
    if type(games) is not int or games < 1 or type(seed) is not int:
        raise ValueError("invalid games/seed")
    if len(players) != 2 or any(not ((isinstance(p,str) and p in ("ordered","random"))
                                    or (hasattr(p,"select") and hasattr(p,"descriptor"))) for p in players):
        raise ValueError("unsupported players")
    if black_owner not in (0,1) or (max_actions is not None and (type(max_actions) is not int or max_actions < 0)):
        raise ValueError("invalid owner/action cap")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    config = dict(games=games, seed=seed, participants=[p if isinstance(p,str) else p.descriptor for p in players], compact=compact,
                  source_id=source_id, black_owner=black_owner, alternate_colors=True,
                  max_actions=max_actions, profile=PROFILE)
    version = code_version()
    with Environment(library) as probe:
        binary = Path(probe._dll._name)
        native_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
    manifest = dict(schema="omok-run-v1", config=config, code=version, native_sha256=native_hash)
    if monitor:monitor.bind(code=version,native_sha256=native_hash,config=config)
    manifest_path = output/"manifest.json"
    if manifest_path.exists():
        old = json.loads(manifest_path.read_text(encoding="utf-8"))
        if old != manifest:
            raise ValueError("resume configuration/code/native mismatch; use a new output directory")
    else:
        save(manifest_path, manifest)
    summary = Counter()
    started = time.perf_counter()
    played_moves = 0
    selection_ms=[]
    winners=Counter()
    for index in range(games):
        if monitor and monitor.boundary("game.begin",{"index":index}):break
        path = output/f"game-{index:06d}.json"
        start = dict(compact=compact, source_id=source_id, black_owner=(black_owner+index)%2, profile=PROFILE)
        if path.exists():
            prior = json.loads(path.read_text(encoding="utf-8"))
            replay(prior, library,**({"monitor":monitor} if monitor else {}))
            if (prior["config"] != config or prior["game_id"] != f"game-{index:06d}"
                or prior["seed"] != seed+index or prior["start"] != start or prior["code_version"] != version):
                raise ValueError("saved game identity/configuration mismatch")
            if prior["status"] == "completed":
                if monitor:monitor.completed_unit(str(path.resolve()),resumed=True)
                summary["completed"] += 1
                summary[prior["final"]["result"]] += 1
                if prior["final"]["winner"] is not None: winners[str(prior["final"]["winner"])]+=1
                continue
            attempt = 1
            while (output/f"game-{index:06d}.attempt-{attempt}.json").exists():
                attempt += 1
            save(output/f"game-{index:06d}.attempt-{attempt}.json", prior)
        record = None
        try:
            with Environment(library, **start) as env:
                if monitor:monitor.bind(game_id=f"game-{index:06d}",config=config,model_hashes=[getattr(p,"descriptor",{}) for p in players])
                record = begin(env, game_id=f"game-{index:06d}", start=start, config=config,
                               seed=seed+index, code_version=version)
                rng = random.Random(seed+index)
                pending = None
                try:
                    while env.snapshot()["phase"] == "playing":
                        if monitor and monitor.boundary("game.move",{"ply":env.snapshot()["placements"]}):
                            finish(env,record,"interrupted",dict(reason="monitor_stop"));break
                        if max_actions is not None and len(record["actions"]) >= max_actions:
                            finish(env, record, "interrupted", dict(reason="runner_action_cap"))
                            break
                        current_mask=env.mask()
                        legal = [i for i, valid in enumerate(current_mask) if valid]
                        if not legal:
                            finish(env, record, "blocked", dict(reason="no_legal_action"))
                            break
                        actor = env.snapshot()["next_actor"]
                        selection=dict(token=env.snapshot()["token"],mask=current_mask)
                        if monitor:monitor.bind(token=selection["token"],ply=env.snapshot()["placements"],profile=env.snapshot()["profile"])
                        player=players[actor]
                        tick=time.perf_counter()
                        inference=None
                        if isinstance(player,str):
                            action=legal[0] if player=="ordered" else rng.choice(legal)
                        else:
                            inference=player.select(env,current_mask)
                            action=inference["action"]
                            if monitor:
                                monitor.require("GR-01",type(action) is int and action in legal,"post_mask_player_action_legal",{"action":action})
                                if "token" in inference:
                                    monitor.require("GR-01",inference["token"]==selection["token"],"inference_selection_token_binding",{"actual":inference["token"],"expected":selection["token"]})
                            if action not in legal: raise ValueError("player returned an illegal action")
                        if monitor and monitor.boundary("game.before_apply",{"ply":env.snapshot()["placements"]}):
                            finish(env,record,"interrupted",dict(reason="monitor_stop_before_apply"));break
                        selection_ms.append((time.perf_counter()-tick)*1000)
                        pending = dict(action=action,actor=actor,token=env.snapshot()["token"])
                        apply(env, record, action,**({"monitor":monitor,"selection":selection} if monitor else {}))
                        if inference is not None:
                            record["actions"][-1]["inference"]={k:v for k,v in inference.items() if k!="token"}
                        pending = None
                        played_moves += 1
                    else:
                        finish(env, record, "completed")
                except KeyboardInterrupt:
                    finish(env, record, "interrupted", dict(reason="keyboard_interrupt",pending=pending))
                except Exception as exc:
                    from omok_monitoring import StopRequested
                    interrupted=isinstance(exc,StopRequested)
                    if monitor and not interrupted:monitor.emit("LP-09","failure","game_execution_error",dict(error=str(exc)))
                    finish(env, record, "interrupted" if interrupted else "error", dict(reason=type(exc).__name__, message=str(exc),pending=pending,
                        details=getattr(exc,"details",None)))
                if monitor:
                    from omok_monitoring.checks import episode
                    episode(monitor,record)
                    monitor.save_artifact("game:"+str(path),path,lambda:save(path,record))
                else:save(path, record)
                replay(record, library,**({"monitor":monitor} if monitor else {}))
                if monitor and record["status"]=="completed":monitor.completed_unit(str(path.resolve()))
        except (KeyboardInterrupt, Exception) as exc:
            if monitor:monitor.emit("LP-09","failure","game_or_storage_failed",dict(error=str(exc)))
            # Invalid starts and infrastructure failures have no fabricated board result.
            save(output/f"game-{index:06d}.failure.json", dict(schema="omok-run-failure-v1",
                 game_id=f"game-{index:06d}", start=start, config=config, seed=seed+index,
                 code_version=version, reason=type(exc).__name__, message=str(exc)))
            summary["error"] += 1
            break
        summary[record["status"]] += 1
        if record["status"] == "completed":
            summary[record["final"]["result"]] += 1
            if record["final"]["winner"] is not None: winners[str(record["final"]["winner"])]+=1
        if record["status"] == "interrupted":
            break
    result = dict(counts=dict(summary), winners=dict(winners),elapsed_seconds=time.perf_counter()-started, new_moves=played_moves,
                  selection_samples=len(selection_ms),mean_selection_ms=sum(selection_ms)/len(selection_ms) if selection_ms else None)
    if monitor:
        from omok_monitoring.checks import comparison
        comparison(monitor,result)
        if monitor.stop_requested:monitor.boundary("runner.summary",{"completed":summary["completed"]})
        monitor.save_artifact("game_summary",output/"summary.json",lambda:save(output/"summary.json",result))
    else:save(output/"summary.json", result)
    return result


def main():
    p = argparse.ArgumentParser()
    p.add_argument("output")
    p.add_argument("--games", type=int, default=2)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--players", nargs=2, choices=("ordered", "random"), default=("ordered", "random"))
    p.add_argument("--compact", default="")
    p.add_argument("--source-id", default="local-v1")
    p.add_argument("--black-owner", type=int, choices=(0,1), default=0)
    p.add_argument("--max-actions", type=int)
    p.add_argument("--monitor-output")
    args = vars(p.parse_args())
    args["players"] = tuple(args["players"])
    monitoring=args.pop("monitor_output")
    if monitoring:
        from omok_monitoring import Monitor
        with Monitor(monitoring,config=dict(kind="sequential-games",**args)) as monitor:
            result=run(**args,monitor=monitor)
    else:result=run(**args)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
