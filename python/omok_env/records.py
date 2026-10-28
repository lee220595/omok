"""Versioned, atomic per-game JSON traces; replay always uses a separate environment."""
import json
import os
from pathlib import Path
import tempfile
from . import Environment, PROFILE, logical

SCHEMA = "omok-trace-v1"


def save(path, record):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(record, stream, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def begin(env, *, game_id, start, config, seed, code_version):
    return dict(schema=SCHEMA, profile=PROFILE, profile_version=1, game_id=game_id,
                config=config, seed=seed, code_version=code_version, start=start,
                initial=logical(env.snapshot()), actions=[], status="incomplete", diagnostic=None)


def apply(env, record, action):
    s = env.snapshot()
    count = len(record["actions"])
    def append(decision, after):
        record["actions"].append(dict(action=action, row=action//15, column=action%15,
            color=s["next_color"], actor=s["next_actor"], decision=decision, after=logical(after)))
    try:
        decision = env.step(action, actor=s["next_actor"], expected=s["token"])
        append(decision, env.snapshot())
    except BaseException:
        # A signal can arrive just after the native transaction commits. Preserve
        # that committed action before recording an interruption of the runner.
        after = env.snapshot()
        if after["token"] != s["token"] and len(record["actions"]) == count:
            verdict = {"five":"win", "forbidden":"forbidden", "move_limit":"draw", "board_full":"draw"}.get(after["end_reason"],"legal")
            append(dict(verdict=verdict,reason=after["rule_reason"]),after)
        raise


def finish(env, record, status, diagnostic=None):
    if status == "interrupted" and env.snapshot()["terminated"]:
        status, diagnostic = "completed", None
    if status != "completed" and env.snapshot()["phase"] == "playing":
        env.abort()
    record.update(status=status, diagnostic=diagnostic, final=logical(env.snapshot()))
    return record


def replay(record, library=None):
    if isinstance(record, (str, Path)):
        with open(record, encoding="utf-8") as stream:
            record = json.load(stream)
    if record["schema"] != SCHEMA or record["profile"] != PROFILE or record["profile_version"] != 1:
        raise ValueError("unsupported record/profile version")
    if record["status"] not in ("completed", "interrupted", "error", "blocked"):
        raise ValueError("incomplete/unknown record status")
    if not isinstance(record["game_id"], str) or not record["game_id"]:
        raise ValueError("invalid game id")
    start = record["start"]
    if set(start) != {"compact", "source_id", "black_owner", "profile"}:
        raise ValueError("invalid start specification")
    with Environment(library, **start) as env:
        if logical(env.snapshot()) != record["initial"]:
            raise ValueError("initial state mismatch")
        for event in record["actions"]:
            s = env.snapshot()
            if (event["actor"], event["color"]) != (s["next_actor"], s["next_color"]):
                raise ValueError("actor/color mismatch")
            if (event["row"], event["column"]) != divmod(event["action"],15):
                raise ValueError("coordinate mismatch")
            if env.step(event["action"], actor=event["actor"]) != event["decision"]:
                raise ValueError("decision mismatch")
            if logical(env.snapshot()) != event["after"]:
                raise ValueError("intermediate state mismatch")
        if record["status"] == "completed":
            if not env.snapshot()["terminated"] or record["diagnostic"] is not None:
                raise ValueError("completion without rule termination")
        else:
            if env.snapshot()["terminated"] or not isinstance(record["diagnostic"], dict):
                raise ValueError("invalid interruption/error")
            env.abort()
        final = logical(env.snapshot())
        if final != record["final"]:
            raise ValueError("final state mismatch")
        return final
