"""Single-writer Renju environment. All adjudication runs in the C++ core."""
import ctypes as C
import os
from pathlib import Path
import uuid
from array import array

PROFILE = "renju15-fixed200-v1"
END_REASONS = ("none", "five", "move_limit", "board_full", "forbidden", "resigned",
               "closed", "opening_result", "engine_start_failure", "engine_timeout",
               "engine_exited", "engine_bad_output", "engine_illegal_move")


def action_of(row, column):
    if type(row) is not int or type(column) is not int or not (0 <= row < 15 and 0 <= column < 15):
        raise ValueError("row/column must be integers in 0..14")
    return row * 15 + column


def coordinate_of(action):
    if type(action) is not int or not 0 <= action < 225:
        raise ValueError("action must be an integer in 0..224")
    return divmod(action, 15)


def _library(path):
    dll = C.CDLL(str(Path(path or os.environ.get("OMOK_HEADLESS_LIBRARY",
        Path(__file__).resolve().parents[2] / "build/native-release/bin/libomok_headless.dll")).resolve()))
    signatures = {
        "abi": ([], C.c_int), "create": ([], C.c_void_p),
        "destroy": ([C.c_void_p], None), "clone": ([C.c_void_p], C.c_void_p),
        "reset": ([C.c_void_p, C.c_char_p, C.c_char_p, C.c_int], C.c_int),
        "step": ([C.c_void_p, C.c_uint64, C.c_uint64, C.c_int, C.c_int, C.POINTER(C.c_int)], C.c_int),
        "close": ([C.c_void_p, C.c_uint64, C.c_uint64], C.c_int),
        "snapshot": ([C.c_void_p, C.POINTER(C.c_int), C.c_int, C.POINTER(C.c_uint64)], C.c_int),
        "mask": ([C.c_void_p, C.POINTER(C.c_ubyte), C.c_int], C.c_int),
    }
    for name, (args, result) in signatures.items():
        fn = getattr(dll, "omok_" + name)
        fn.argtypes, fn.restype = args, result
    if dll.omok_abi() != 1:
        raise RuntimeError("unsupported native ABI")
    return dll


class Environment:
    def __init__(self, library=None, *, black_owner=0, compact="", source_id="local-v1", profile=PROFILE):
        self._dll = _library(library)
        self._handle = self._dll.omok_create()
        if not self._handle:
            raise MemoryError("native environment allocation failed")
        self.game_id = uuid.uuid4().hex
        try:
            self.reset(black_owner=black_owner, compact=compact, source_id=source_id, profile=profile)
        except BaseException:
            self.dispose()
            raise

    def _check(self, code):
        if code:
            raise ValueError(f"native request rejected ({code})") if code != -2 else RuntimeError("native execution error")

    def _alive(self):
        if not self._handle:
            raise RuntimeError("environment disposed")

    def dispose(self):
        if getattr(self, "_handle", None):
            self._dll.omok_destroy(self._handle)
            self._handle = None

    def __del__(self):
        self.dispose()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.dispose()

    def reset(self, *, black_owner=0, compact="", source_id="local-v1", profile=PROFILE):
        self._alive()
        if profile != PROFILE or type(black_owner) is not int or black_owner not in (0, 1):
            raise ValueError("unsupported profile or black owner")
        if not isinstance(compact, str) or not isinstance(source_id, str) or "\0" in compact + source_id:
            raise ValueError("invalid opening text/id")
        self._check(self._dll.omok_reset(self._handle, source_id.encode(), compact.encode(), black_owner))
        self.source_id = source_id
        self.game_id = uuid.uuid4().hex

    def clone(self):
        self._alive()
        result = object.__new__(Environment)
        result._dll = self._dll
        result._handle = self._dll.omok_clone(self._handle)
        if not result._handle:
            raise MemoryError("native clone failed")
        result.game_id = uuid.uuid4().hex
        result.source_id = self.source_id
        return result

    def snapshot(self):
        self._alive()
        data, token = (C.c_int * 916)(), (C.c_uint64 * 2)()
        self._check(self._dll.omok_snapshot(self._handle, data, len(data), token))
        optional = lambda x: None if x == -1 else x
        return dict(profile=PROFILE, profile_version=1, source_id=self.source_id,
            phase=("not_started", "playing", "finished", "closed")[data[0]],
            placements=data[1], next_color=optional(data[2]), next_actor=optional(data[3]),
            black_owner=optional(data[4]), white_owner=1-data[4],
            result=("in_progress", "won", "draw", "aborted")[data[5]], winner=optional(data[6]),
            end_reason=END_REASONS[data[7]], rule_reason=data[8], move_limit=data[9],
            remaining_moves=data[9]-data[1], prepared_count=data[10],
            start_mode="prepared" if data[11] == 1 else "direct", event_count=data[12],
            board=list(data[16:241]),
            history=[dict(action=data[i], row=data[i]//15, column=data[i]%15,
                          color=data[i+1], actor=optional(data[i+2])) for i in range(241,241+3*data[1],3)],
            terminated=data[5] in (1,2), truncated=data[5] == 3,
            token=[self.game_id, int(token[0]), int(token[1])])

    def mask(self):
        self._alive()
        data = (C.c_ubyte * 225)()
        self._check(self._dll.omok_mask(self._handle, data, len(data)))
        return tuple(bool(x) for x in data)

    def observation(self):
        s = self.snapshot()
        if s["phase"] != "playing":
            return None
        own = s["next_color"] + 1
        return array("f", [float(x == own) for x in s["board"]]
                     + [float(x == 3-own) for x in s["board"]]
                     + [float(own == 1)] * 225 + [s["remaining_moves"]/s["move_limit"]] * 225)

    def _token(self, expected):
        current = self.snapshot()["token"]
        if expected is None:
            return current
        if expected != current:
            raise ValueError("stale or foreign game token")
        return current

    def step(self, action, *, actor=None, expected=None):
        coordinate_of(action)
        token = self._token(expected)
        if actor is None:
            actor = self.snapshot()["next_actor"]
        if type(actor) is not int or actor not in (0,1):
            raise ValueError("invalid participant")
        decision = (C.c_int * 2)()
        self._check(self._dll.omok_step(self._handle, token[1], token[2], actor, action, decision))
        return dict(verdict=("invalid", "forbidden", "legal", "win", "draw")[decision[0]], reason=decision[1])

    def abort(self, *, expected=None):
        token = self._token(expected)
        self._check(self._dll.omok_close(self._handle, token[1], token[2]))


def logical(snapshot):
    return {k: v for k, v in snapshot.items() if k != "token"}
