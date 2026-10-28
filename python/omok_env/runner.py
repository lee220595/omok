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
        source_id="local-v1", black_owner=0, max_actions=None, library=None):
    if type(games) is not int or games < 1 or type(seed) is not int:
        raise ValueError("invalid games/seed")
    if len(players) != 2 or any(p not in ("ordered", "random") for p in players):
        raise ValueError("unsupported players")
    if black_owner not in (0,1) or (max_actions is not None and (type(max_actions) is not int or max_actions < 0)):
        raise ValueError("invalid owner/action cap")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    config = dict(games=games, seed=seed, participants=list(players), compact=compact,
                  source_id=source_id, black_owner=black_owner, alternate_colors=True,
                  max_actions=max_actions, profile=PROFILE)
    version = code_version()
    with Environment(library) as probe:
        binary = Path(probe._dll._name)
        native_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
    manifest = dict(schema="omok-run-v1", config=config, code=version, native_sha256=native_hash)
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
    for index in range(games):
        path = output/f"game-{index:06d}.json"
        start = dict(compact=compact, source_id=source_id, black_owner=(black_owner+index)%2, profile=PROFILE)
        if path.exists():
            prior = json.loads(path.read_text(encoding="utf-8"))
            replay(prior, library)
            if (prior["config"] != config or prior["game_id"] != f"game-{index:06d}"
                or prior["seed"] != seed+index or prior["start"] != start or prior["code_version"] != version):
                raise ValueError("saved game identity/configuration mismatch")
            if prior["status"] == "completed":
                summary["completed"] += 1
                summary[prior["final"]["result"]] += 1
                continue
            attempt = 1
            while (output/f"game-{index:06d}.attempt-{attempt}.json").exists():
                attempt += 1
            save(output/f"game-{index:06d}.attempt-{attempt}.json", prior)
        record = None
        try:
            with Environment(library, **start) as env:
                record = begin(env, game_id=f"game-{index:06d}", start=start, config=config,
                               seed=seed+index, code_version=version)
                rng = random.Random(seed+index)
                pending = None
                try:
                    while env.snapshot()["phase"] == "playing":
                        if max_actions is not None and len(record["actions"]) >= max_actions:
                            finish(env, record, "interrupted", dict(reason="runner_action_cap"))
                            break
                        legal = [i for i, valid in enumerate(env.mask()) if valid]
                        if not legal:
                            finish(env, record, "blocked", dict(reason="no_legal_action"))
                            break
                        actor = env.snapshot()["next_actor"]
                        action = legal[0] if players[actor] == "ordered" else rng.choice(legal)
                        pending = dict(action=action,actor=actor,token=env.snapshot()["token"])
                        apply(env, record, action)
                        pending = None
                        played_moves += 1
                    else:
                        finish(env, record, "completed")
                except KeyboardInterrupt:
                    finish(env, record, "interrupted", dict(reason="keyboard_interrupt",pending=pending))
                except Exception as exc:
                    finish(env, record, "error", dict(reason=type(exc).__name__, message=str(exc),pending=pending))
                save(path, record)
                replay(record, library)
        except (KeyboardInterrupt, Exception) as exc:
            # Invalid starts and infrastructure failures have no fabricated board result.
            save(output/f"game-{index:06d}.failure.json", dict(schema="omok-run-failure-v1",
                 game_id=f"game-{index:06d}", start=start, config=config, seed=seed+index,
                 code_version=version, reason=type(exc).__name__, message=str(exc)))
            summary["error"] += 1
            break
        summary[record["status"]] += 1
        if record["status"] == "completed":
            summary[record["final"]["result"]] += 1
        if record["status"] == "interrupted":
            break
    result = dict(counts=dict(summary), elapsed_seconds=time.perf_counter()-started, new_moves=played_moves)
    save(output/"summary.json", result)
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
    args = vars(p.parse_args())
    args["players"] = tuple(args["players"])
    print(json.dumps(run(**args), indent=2))


if __name__ == "__main__":
    main()
