"""Optional fixed-source referee comparison; no AI processes or product dependencies.

Downloads remain in ignored build/research. Upstream code is not bundled/translated.
Runs original check_forbid plus AST-selected original ai_match methods, with only
next_move replaced by deterministic input. This is NOT a full manager/engine test.
"""
import argparse
import ast
import base64
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import random
import subprocess
import sys
import urllib.request

COMMIT = "56cfd7dc1a0d1ec06d29f44a26931f375ad05991"
HASHES = {
    "check_forbid.py": "784f5bf1c0d715078474adb89903cc62be443bc739376e1f2b371ed95f2d368b",
    "ai_match.py": "93831daa81fcbcd40f4c6e87156589c6f49c5650063e5d6d7a6198e268bbe815",
}
ROOT = Path(__file__).resolve().parent.parent


def load_referee(cache, fetch):
    cache.mkdir(parents=True, exist_ok=True)
    for name, digest in HASHES.items():
        path = cache / name
        if not path.exists() and fetch:
            url = f"https://raw.githubusercontent.com/Gomocup/GomocupJudge/{COMMIT}/client/{name}"
            path.write_bytes(urllib.request.urlopen(url, timeout=30).read())
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Missing or mismatched fixed source: {path}; use --fetch for missing files")
    spec = importlib.util.spec_from_file_location("reference_check_forbid", cache / "check_forbid.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    tree = ast.parse((cache / "ai_match.py").read_text(encoding="utf-8"))
    original = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == "ai_match")
    # No copied method bodies in this repository. Compile these original nodes unchanged.
    original.body = [node for node in original.body if isinstance(node, ast.FunctionDef)
                     and node.name in {"__init__", "make_move", "play"}]
    namespace = {"check_forbid": module.check_forbid, "sys": sys, "base64": base64}
    exec(compile(ast.Module(body=[original], type_ignores=[]), str(cache / "ai_match.py"), "exec"), namespace)
    return namespace["ai_match"]


def coordinates(text):
    return [] if not text else [tuple(map(int, item.split(","))) for item in text.split(";")]


def board_of(fields):
    board = [[0] * 15 for _ in range(15)]
    for value, field in ((1, fields[6]), (2, fields[7])):
        for r, c in coordinates(field):
            board[r][c] = value
    return board


def format_points(points):
    return ";".join(f"{p // 15},{p % 15}" for p in points)


def transformed(fields, symmetry):
    def point(r, c):
        if symmetry >= 4:
            c = 14 - c
        for _ in range(symmetry % 4):
            r, c = c, 14 - r
        return r, c
    result = fields.copy()
    result[0] += f"_s{symmetry}"
    result[2:4] = map(str, point(int(fields[2]), int(fields[3])))
    for n in (6, 7):
        result[n] = ";".join(f"{r},{c}" for r, c in (point(*p) for p in coordinates(fields[n])))
    return result


def run(exe, cache, fetch, output, summary):
    referee = load_referee(cache, fetch)
    rows = []
    for line in (ROOT / "tests/fixtures/renju.tsv").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        f = line.split("|")
        # Judge make_move does not validate negative coordinates; compare valid inputs only.
        if f[4] != "Invalid":
            rows.extend(transformed(f, s) for s in range(8))
    rng = random.Random(20261023)
    for n in range(1000):
        # Arbitrary local boards, NOT a claim of reachable/legal match histories.
        cells = rng.sample(range(225), rng.randrange(4, 85))
        split_at = rng.randrange(len(cells) + 1)
        candidate = rng.choice([i for i in range(225) if i not in cells])
        rows.append([f"random_{n}", "B" if n % 2 == 0 else "W", str(candidate // 15),
                     str(candidate % 15), "Legal", "None", format_points(cells[:split_at]),
                     format_points(cells[split_at:])])
    for n in range(1000):
        cells = [i for i in range(225) if i != 112 and abs(i // 15 - 7) <= 4 and abs(i % 15 - 7) <= 4]
        rng.shuffle(cells)
        occupied = rng.randrange(8, 45)
        black_count = rng.randrange(4, occupied + 1)
        rows.append([f"local_{n}", "B", "7", "7", "Legal", "None",
                     format_points(cells[:black_count]), format_points(cells[black_count:occupied])])
    run_cpp = subprocess.run([str(exe), "--probe"], input="\n".join("|".join(f) for f in rows) + "\n",
                             text=True, capture_output=True, check=True, timeout=60)
    cpp = {parts[0]: parts[1:] for parts in (line.split("|") for line in run_cpp.stdout.splitlines())}
    results = []
    mismatches = []
    for f in rows:
        match = referee(15, [], "", "", "", "", rule=4)
        match.board = board_of(f)
        actual = match.make_move(int(f[2]), int(f[3]), 1 if f[1] == "B" else 2)
        expected = {"Forbidden": -2, "Win": 1, "Legal": 0, "Draw": 0}[cpp[f[0]][0]]
        entry = {"id": f[0], "input": "|".join(f), "cpp": cpp[f[0]], "judge": actual}
        results.append(entry)
        if actual != expected:
            mismatches.append(entry)
    # Execute the original play loop with deterministic next_move; no protocols/engines.
    boundaries = []
    for opening_count in (198, 199, 200):
        for winning in (False, True):
            opening = [(i // 15, i % 15) for i in range(opening_count)]
            match = referee(15, opening, "", "", "", "", rule=4)
            # Synthetic no-five background; opening length is the policy input under test.
            match.board = [[1 if (c // 2 + r) % 2 == 0 else 2 for c in range(15)] for r in range(15)]
            match.board[14][13] = match.board[14][14] = 0
            if winning:
                match.board[14][9:13] = [2] * 4
            initial_board = ["".join(map(str, row)) for row in match.board]
            moves = [(14, 13), (14, 14)] if opening_count == 198 else [(14, 13)]
            called = []
            def next_move():
                r, c = moves[len(called)]
                called.append((r, c))
                return "", r, c, 0
            match.next_move = next_move
            with contextlib.redirect_stdout(io.StringIO()):
                _, _, result, endby = match.play()
            # 199 opening + white 200th exact five wins; 200 opening requests no move.
            expected_result = 2 if winning and opening_count == 199 else 0
            entry = {"opening_count": opening_count, "winning_setup": winning,
                     "opening_sequence": "row-major board indices [0, opening_count)",
                     "board_before": initial_board,
                     "requests": called, "result": result, "endby": endby}
            boundaries.append(entry)
            if result != expected_result or endby != 0 or len(called) != 200 - opening_count:
                mismatches.append({"boundary": entry, "expected_result": expected_result})
    report = {"commit": COMMIT, "sha256": HASHES, "python": sys.version,
              "settings": {"board_size": 15, "rule": 4, "special_rule": ""},
              "seed": 20261023, "fixture_symmetries": len(rows) - 2000, "random_cases": 1000,
              "local_threat_cases": 1000,
              "case_results": results, "boundary_results": boundaries, "mismatches": mismatches,
              "scope": "original check_forbid + original make_move/play methods; deterministic input; no AI processes"}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if summary:
        compact = {key: value for key, value in report.items() if key != "case_results"}
        compact["fixture_sha256"] = hashlib.sha256((ROOT / "tests/fixtures/renju.tsv").read_bytes()).hexdigest()
        compact["full_report_sha256"] = hashlib.sha256(output.read_bytes()).hexdigest()
        compact["fixed_fixture_outputs"] = [entry for entry in results if entry["id"].endswith("_s0")]
        summary.parent.mkdir(parents=True, exist_ok=True)
        summary.write_text(json.dumps(compact, indent=2) + "\n", encoding="utf-8")
    print(f"{len(results)} board comparisons; {len(boundaries)} play-loop boundaries; {len(mismatches)} mismatches")
    print(f"Report: {output}")
    return 1 if mismatches else 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "build/native-release/bin/omok_rules_tests.exe")
    parser.add_argument("--cache", type=Path, default=ROOT / "build/research")
    parser.add_argument("--output", type=Path, default=ROOT / "build/research/rules-comparison.json")
    parser.add_argument("--fetch", action="store_true", help="Download missing fixed sources; never replace mismatched files")
    parser.add_argument("--summary", type=Path, help="Also save a compact reviewable report (full log remains in build)")
    args = parser.parse_args()
    sys.exit(run(args.exe.resolve(), args.cache.resolve(), args.fetch, args.output.resolve(), args.summary))
