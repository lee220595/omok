"""Compare valid self-authored openings with pinned original Judge functions.

No external AI processes. Swap2 responses are deterministic test inputs; stop at
the first normal-move request. Invalid prepared data validation is a project
contract, since upstream __init__ trusts prepared positions without adjudication.
"""
import ast
import base64
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
COMMIT = "56cfd7dc1a0d1ec06d29f44a26931f375ad05991"
SOURCES = {
    "ai_match.py": ("client/ai_match.py", "93831daa81fcbcd40f4c6e87156589c6f49c5650063e5d6d7a6198e268bbe815"),
    "check_forbid.py": ("client/check_forbid.py", "784f5bf1c0d715078474adb89903cc62be443bc739376e1f2b371ed95f2d368b"),
    "utility.py": ("client/utility.py", "0013e5619189e54520a2d44f7f2e729fbd1826e68407d95db3af329794780785"),
    "server.py": ("server/server.py", "b377f6df4096d8b027a4fe60e414ac90dbef2d8dbcb5694d56b9de5443e6848c"),
}


def selected(path, names, namespace):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    body = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
    if len(body) != len(names):
        raise RuntimeError("Missing fixed-source functions")
    exec(compile(ast.Module(body=body, type_ignores=[]), str(path), "exec"), namespace)


def compare(cache, fetch, output):
    cache.mkdir(parents=True, exist_ok=True)
    for name, (upstream, digest) in SOURCES.items():
        path = cache / name
        if not path.exists() and fetch:
            path.write_bytes(urllib.request.urlopen(
                f"https://raw.githubusercontent.com/Gomocup/GomocupJudge/{COMMIT}/{upstream}", timeout=30).read())
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Missing or mismatched fixed source: {path}")
    spec = importlib.util.spec_from_file_location("opening_reference_forbid", cache / "check_forbid.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    namespace = {"sys": sys, "base64": base64, "check_forbid": module.check_forbid}
    selected(cache / "utility.py", {"str_to_pos", "pos_to_str"}, namespace)
    selected(cache / "server.py", {"opening2pos"}, namespace)
    tree = ast.parse((cache / "ai_match.py").read_text(encoding="utf-8"))
    original = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "ai_match")
    original.body = [n for n in original.body if isinstance(n, ast.FunctionDef)
                     and n.name in {"__init__", "make_move", "play"}]
    exec(compile(ast.Module(body=[original], type_ignores=[]), str(cache / "ai_match.py"), "exec"), namespace)
    referee = namespace["ai_match"]
    exe = ROOT / "build/native-release/bin/omok_opening_tests.exe"
    cpp = subprocess.run([str(exe), "--reference-snapshots"], check=True,
                         capture_output=True, text=True, timeout=30)
    snapshots = {f[0]: f[1:] for f in (line.split("|") for line in cpp.stdout.splitlines())}
    compact = (ROOT / "tests/fixtures/prepared-self-authored.txt").read_text(encoding="utf-8").strip()
    coordinates = namespace["str_to_pos"](compact)
    centered = "0,0,1,0,-1,1"
    converted = namespace["opening2pos"](centered, 15)
    failures = []
    if converted != compact or namespace["pos_to_str"](coordinates) != compact:
        failures.append("source coordinate conversion disagrees")

    def snapshot(match, black_owner, next_actor, actors):
        size = len(match.opening)
        board = "".join(str(match.board[c][r]) for r in range(15) for c in range(15))
        history = ";".join(f"{y},{x},{'B' if n % 2 == 0 else 'W'},{actors[n]}"
                           for n, (x, y) in enumerate(match.opening))
        return [black_owner, "B" if size % 2 == 0 else "W", next_actor, board, history]

    results = []
    fourth = (8, 8)  # protocol x,y; state row 8,column 8
    fifth = (8, 6)
    sixth = (5, 8)
    for count in (0, 3, 4):
        opening = [] if count == 0 else coordinates + ([fourth] if count == 4 else [])
        match = referee(15, opening.copy(), "A", "B", "", "", rule=4)
        # The project config assigns black to B. Judge __init__ has no participant ownership.
        next_actor = "B" if count % 2 == 0 else "A"
        actual = snapshot(match, "B", next_actor, ["-"] * count)
        key = f"prepared_{count}"
        results.append({"id": key, "input_xy": opening, "cpp": snapshots[key], "reference": actual})
        if snapshots[key] != actual:
            failures.append(key)

    class OpeningComplete(BaseException):
        pass

    for offer_two in (False, True):
        for take_white in (False, True):
            key = ("two_" if offer_two else "direct_") + ("white" if take_white else "black")
            responses = {"A": [coordinates.copy()], "B": []}
            responses["B"].append([fourth, fifth] if offer_two else [fourth] if take_white else [])
            if offer_two:
                responses["A"].append([sixth] if take_white else [])
            emissions = []

            class FixedInput:
                def __init__(self, name):
                    self.name, self.timeused = name, 0

                def swap2board(self):
                    points = responses[self.name].pop(0)
                    emissions.append((self.name, points.copy()))
                    return "", None, points

                def init_board(self, board):
                    pass

            notifications = []
            # Judge's UI notification callbacks; memory-only, no sockets or AI IO.
            settings = {"send": notifications.append, "recv": lambda count: None}
            match = referee(15, [], "A", "B", "", "", rule=0, special_rule="swap2", settings=settings)
            match.init_protocol = lambda name, *args: FixedInput(name)

            def next_move():
                raise OpeningComplete()

            match.next_move = next_move
            with contextlib.redirect_stdout(io.StringIO()):
                try:
                    match.play()
                except OpeningComplete:
                    pass
                else:
                    raise RuntimeError("Reference did not reach first main-game request")
            next_actor = match.engine_1.name
            black_owner = next_actor if len(match.opening) % 2 == 0 else ("B" if next_actor == "A" else "A")
            actors = [name for name, points in emissions for _ in points]
            actual = snapshot(match, black_owner, next_actor, actors)
            results.append({"id": key, "responses_xy": emissions, "manager_notifications": notifications,
                            "cpp": snapshots[key], "reference": actual})
            if snapshots[key] != actual:
                failures.append(key)
    report = {"commit": COMMIT, "sha256": {n: h for n, (_, h) in SOURCES.items()},
              "python": sys.version, "prepared_rule": 4, "swap2_reference_rule": 0,
              "coordinate_input_centered": centered, "coordinate_output_compact": converted,
              "results": results, "mismatches": failures,
              "limits": "Valid self-authored inputs only; original source selected methods; fixed Swap2 responses; no AI process/protocol IO; no Renju binding for Swap2"}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"3 prepared snapshots + 4 Swap2 paths + source coordinate conversion; {len(failures)} mismatches")
    return bool(failures)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=ROOT / "build/research/opening-referee")
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--output", type=Path, default=ROOT / "build/research/opening-comparison.json")
    args = parser.parse_args()
    sys.exit(compare(args.cache, args.fetch, args.output))
