import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from omok_env import Environment, action_of, coordinate_of, logical
from omok_env.records import begin, apply, finish, save, replay
from omok_env.runner import run

TEST_TEMP = Path(__file__).resolve().parents[1]/"build"/"python-tests"
TEST_TEMP.mkdir(parents=True,exist_ok=True)


def draw_start():
    black, white = [], []
    for r in range(15):
        for c in range(15):
            (black if (c//2+r)%2 == 0 else white).append((r,c))
    return "".join(chr(97+c)+str(r+1) for r,c in
                   [(black if i%2 == 0 else white)[i//2] for i in range(199)])


class HeadlessTests(unittest.TestCase):
    def test_coordinates(self):
        for a in range(225):
            self.assertEqual(action_of(*coordinate_of(a)), a)
        for a in (-1,225,True,1.5,"1",2**80):
            with self.assertRaises(ValueError): coordinate_of(a)

    def test_observation_and_copies(self):
        with Environment(black_owner=1,compact="h8i8g9") as e:
            s=e.snapshot()
            self.assertEqual((s["prepared_count"],s["next_color"],s["next_actor"]),(3,1,0))
            self.assertTrue(all(m["actor"] is None for m in s["history"]))
            o=e.observation()
            self.assertEqual((o[112],o[113],o[225+112],o[450]),(0,1,1,0))
            self.assertAlmostEqual(o[675],197/200)
            s["board"][0]=99
            o[0]=99
            self.assertEqual(e.snapshot()["board"][0],0)
            before=e.snapshot()
            with e.clone() as clone:
                with self.assertRaises(ValueError): clone.step(0, expected=before["token"])
                clone.step(0)
                clone.reset()
            self.assertEqual(e.snapshot(),before)

    def test_rejections_and_tokens(self):
        with Environment() as e:
            before=e.snapshot()
            for kwargs in ({"action":-1},{"action":0,"actor":1},{"action":True}):
                with self.assertRaises(ValueError): e.step(**kwargs)
                self.assertEqual(e.snapshot(),before)
            for kwargs in ({"compact":"h8h8"},{"profile":"bogus"},{"black_owner":2}):
                with self.assertRaises(ValueError): e.reset(**kwargs)
                self.assertEqual(e.snapshot(),before)
            e.step(0)
            after=e.snapshot()
            with self.assertRaises(ValueError): e.step(0)
            self.assertEqual(e.snapshot(),after)
            e.reset()
            before_reset=e.snapshot()
            with self.assertRaises(ValueError): e.step(1,expected=before["token"])
            self.assertEqual(e.snapshot(),before_reset)

    def test_mask_matches_actual_decisions(self):
        for compact in ("", "h8i8g9", "g8a1h7c1i8e1h9g1", "a8a1b8c1c8e1d8g1"):
            with Environment(compact=compact) as e:
                before=e.snapshot()
                mask=e.mask()
                for action, legal in enumerate(mask):
                    with e.clone() as trial:
                        try:
                            decision=trial.step(action)
                            actual=decision["verdict"] in ("legal","win","draw")
                        except ValueError:
                            actual=False
                        self.assertEqual(legal,actual,(compact,action))
                self.assertEqual(e.snapshot(),before)

    def test_win_forbidden_limit_terminal(self):
        fixtures=[("a8a1b8c1c8e1d8g1",109,"five",9),
                  ("g8a1h7c1i8e1h9g1",112,"forbidden",8),
                  (draw_start(),None,"move_limit",200)]
        for compact,action,reason,n in fixtures:
            with Environment(compact=compact) as e:
                mask=e.mask()
                if action is None: action=next(i for i,x in enumerate(mask) if x)
                self.assertEqual(mask[action],reason!="forbidden")
                e.step(action)
                s=e.snapshot()
                self.assertEqual((s["end_reason"],s["placements"]),(reason,n))
                self.assertIsNone(s["next_actor"])
                self.assertIsNone(e.observation())
                self.assertFalse(any(e.mask()))
                with self.assertRaises(ValueError): e.step(224,actor=0)
                self.assertEqual(e.snapshot(),s)

    def test_lifecycle(self):
        for _ in range(100):
            with Environment() as e:
                e.step(0)
            with self.assertRaises(RuntimeError): e.snapshot()

    def test_signal_after_committed_step(self):
        with Environment() as e:
            start=dict(compact="",source_id="local-v1",black_owner=0,profile=e.snapshot()["profile"])
            record=begin(e,game_id="signal",start=start,config={},seed=0,code_version={})
            native_step=e.step
            def interrupt(action,**kwargs):
                native_step(action,**kwargs)
                raise KeyboardInterrupt()
            with patch.object(e,"step",side_effect=interrupt):
                with self.assertRaises(KeyboardInterrupt): apply(e,record,0)
            finish(e,record,"interrupted",dict(reason="signal_after_commit"))
            self.assertEqual(len(record["actions"]),1)
            self.assertEqual(replay(record),record["final"])

    def test_exact_five_priority_mask(self):
        # Self-authored sequential opening for existing five_over_double_three fixture.
        black=[(7,3),(7,4),(7,5),(7,6),(6,7),(8,7),(6,6),(8,8)]
        white=[(0,c) for c in (0,2,4,6,8,10,12,14)]
        compact="".join(chr(97+c)+str(r+1) for pair in zip(black,white) for r,c in pair)
        with Environment(compact=compact) as e:
            self.assertTrue(e.mask()[112])
            self.assertEqual(e.step(112)["verdict"],"win")

    def test_record_replay_corruption(self):
        fixture=Path(__file__).resolve().parents[1]/"tests/fixtures/headless-forbidden-v1.json"
        self.assertEqual(replay(fixture)["end_reason"],"forbidden")
        with Environment(compact="g8a1h7c1i8e1h9g1") as e:
            start=dict(compact="g8a1h7c1i8e1h9g1",source_id="local-v1",black_owner=0,profile=e.snapshot()["profile"])
            r=begin(e,game_id="test",start=start,config={},seed=1,code_version={})
            apply(e,r,112)
            finish(e,r,"completed")
            self.assertEqual(replay(r),logical(e.snapshot()))
            before=e.snapshot()
            for key in ("final","initial"):
                bad=copy.deepcopy(r)
                bad[key]["board"][0]=99
                with self.assertRaises(ValueError): replay(bad)
                self.assertEqual(e.snapshot(),before)
            bad=copy.deepcopy(r)
            bad["actions"][0]["color"]=1
            with self.assertRaises(ValueError): replay(bad)
            with tempfile.TemporaryDirectory(dir=TEST_TEMP) as d:
                path=Path(d)/"record.json"
                save(path,r)
                self.assertEqual(replay(path),r["final"])
                path.write_text('{broken',encoding="utf-8")
                with self.assertRaises(json.JSONDecodeError): replay(path)

    def test_runner_seed_resume_interrupt_error(self):
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as d:
            a,b=Path(d)/"a",Path(d)/"b"
            first=run(a,games=2,seed=17)
            second=run(b,games=2,seed=17)
            self.assertEqual(first["counts"]["completed"],2)
            self.assertEqual(first["new_moves"],second["new_moves"])
            for i in range(2):
                self.assertEqual(json.loads((a/f"game-{i:06d}.json").read_text()),
                                 json.loads((b/f"game-{i:06d}.json").read_text()))
            resumed=run(a,games=2,seed=17)
            self.assertEqual(resumed["new_moves"],0)
            self.assertEqual(resumed["counts"]["completed"],2)
            with self.assertRaises(ValueError): run(a,games=2,seed=18)
            capped=Path(d)/"cap"
            self.assertEqual(run(capped,max_actions=2)["counts"],{"interrupted":1})
            record=json.loads((capped/"game-000000.json").read_text())
            self.assertEqual(replay(record)["result"],"aborted")
            run(capped,max_actions=2)
            self.assertTrue((capped/"game-000000.attempt-1.json").exists())
            blocked=Path(d)/"blocked"
            with patch.object(Environment,"mask",return_value=(False,)*225):
                self.assertEqual(run(blocked,games=1)["counts"],{"blocked":1})
            self.assertEqual(replay(blocked/"game-000000.json")["result"],"aborted")
            for name,exc,status in (("cancel",KeyboardInterrupt(),"interrupted"),("failure",RuntimeError("injected"),"error")):
                folder=Path(d)/name
                with patch.object(Environment,"mask",side_effect=exc):
                    result=run(folder,games=1)
                self.assertEqual(result["counts"],{status:1})
                record=json.loads((folder/"game-000000.json").read_text())
                self.assertEqual(replay(record)["result"],"aborted")
            invalid=Path(d)/"invalid"
            self.assertEqual(run(invalid,games=1,compact="h8h8")["counts"],{"error":1})
            self.assertTrue((invalid/"game-000000.failure.json").exists())


if __name__ == "__main__":
    unittest.main()
