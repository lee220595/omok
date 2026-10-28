"""Controlled evaluators verify PUCT independently of the weak learned model."""
import copy
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from omok_env import Environment,logical
from omok_env.runner import run
from omok_env.records import replay
from omok_learning.mcts import SearchConfig,Node,Edge,backup,search,terminal_value,SearchError,PUCTPlayer
from test_headless import draw_start


class ControlledEvaluator:
    descriptor={"kind":"controlled-test-only","sha256":None}

    def __init__(self,policy=None,value=0.0):
        self.policy=policy
        self.value=value
        self.calls=0

    def prepare(self): return 0.0

    def evaluate(self,env,mask):
        self.calls+=1
        if env.snapshot()["phase"]!="playing":raise AssertionError("terminal evaluated")
        priors=self.policy(env,mask) if self.policy else [float(x)/sum(mask) for x in mask]
        return dict(priors=priors,value=self.value)


def concentrated(action):
    return lambda env,mask:([float(i==action) for i in range(225)] if mask[action]
                           else [float(v)/sum(mask) for v in mask])


class MCTSTests(unittest.TestCase):
    def test_backup_current_actor_and_terminal_results(self):
        a,b,c=Node(0),Node(1),Node(0)
        ea,eb,ec=Edge(.5),Edge(.5),Edge(.5)
        backup([(a,ea),(b,eb),(c,ec)],.75,1)
        self.assertEqual((ea.q,eb.q,ec.q),(-.75,.75,-.75))
        self.assertEqual((a.visits,b.visits,c.visits),(1,1,1))
        for winner in (0,1):
            s=dict(terminated=True,result="won",winner=winner)
            for actor in (0,1):self.assertEqual(terminal_value(s,actor),1 if actor==winner else -1)
        self.assertEqual(terminal_value(dict(terminated=True,result="draw"),0),0)
        with self.assertRaises(ValueError):terminal_value(dict(terminated=False,result="aborted"),0)

    def test_real_clone_safety_budget_seed_full_candidates(self):
        with Environment(compact="g8a1h7c1i8e1h9g1") as env:
            before=env.snapshot();mask=env.mask();self.assertFalse(mask[112])
            evaluator=ControlledEvaluator(value=.4)
            result=search(env,mask,evaluator,SearchConfig(24,1.5,99,"seeded"))
            self.assertEqual(env.snapshot(),before)
            repeated=search(env,mask,ControlledEvaluator(value=.4),SearchConfig(24,1.5,99,"seeded"))
            for key in ("action","root_visits","root_edge_q","visit_distribution","root_priors","search"):
                self.assertEqual(result[key],repeated[key])
            self.assertTrue(mask[result["action"]]);self.assertEqual(result["root_visits"][112],0)
            self.assertEqual(result["root_legal_count"],sum(mask))
            self.assertEqual(sum(result["root_visits"]),24)
            self.assertAlmostEqual(sum(result["visit_distribution"]),1)
            self.assertAlmostEqual(sum(result["root_priors"]),1)
            stats=result["search"]
            self.assertEqual(stats["simulations"],24);self.assertEqual(stats["clones"],25)
            self.assertEqual(stats["network_evaluations"],evaluator.calls)
            self.assertEqual(stats["network_evaluations"],stats["expanded_nodes"])
            self.assertLessEqual(stats["network_evaluations"],25)
            self.assertEqual(stats["network_evaluations"]+stats["terminal_hits"],25)
            self.assertGreaterEqual(stats["transitions"],24)

    def test_leaf_sign_with_actual_turns(self):
        with Environment() as env:
            # Each simulation extends the same forced branch by one ply. v=.6 is
            # always the *new leaf actor's* value: root accumulates -.6,+.6,-.6.
            evaluator=ControlledEvaluator(policy=lambda e,m:[float(i==next(j for j,v in enumerate(m) if v)) for i in range(225)],value=.6)
            result=search(env,env.mask(),evaluator,SearchConfig(3))
            self.assertAlmostEqual(result["root_edge_q"][0],-.2)
            self.assertEqual(result["search"]["max_depth"],3)
            self.assertEqual(result["search"]["network_evaluations"],4)

    def test_immediate_win_overrides_network_terminal_not_evaluated(self):
        for owner in (0,1):
            with Environment(compact="a8a1b8c1c8e1d8g1",black_owner=owner) as env:
                before=env.snapshot();eval=ControlledEvaluator(concentrated(109),value=-1)
                result=search(env,env.mask(),eval,SearchConfig(8))
                self.assertEqual(result["action"],109)
                self.assertEqual(result["root_edge_q"][109],1)
                self.assertEqual(result["root_visits"][109],8)
                self.assertEqual(result["search"]["terminal_hits"],8)
                self.assertEqual(eval.calls,1)
                self.assertEqual(env.snapshot(),before)

    def test_forced_defense_opponent_win_sign(self):
        with Environment(compact="a1a8c1b8e1c8g1d8") as env:
            before=env.snapshot()
            def policy(e,mask):
                s=e.snapshot()
                if s["placements"]==8:
                    p=[0.0]*225;p[224]=.55;p[109]=.45;return p
                if s["next_color"]==1 and mask[109]:return [float(i==109) for i in range(225)]
                return [float(v)/sum(mask) for v in mask]
            result=search(env,env.mask(),ControlledEvaluator(policy),SearchConfig(32))
            self.assertEqual(result["action"],109)
            self.assertLess(result["root_edge_q"][224],0)
            self.assertGreater(result["root_visits"][109],result["root_visits"][224])
            self.assertEqual(env.snapshot(),before)

    def test_draw_limit_no_nn_terminal_and_terminal_root_rejection(self):
        with Environment(compact=draw_start()) as env:
            mask=env.mask();action=next(i for i,v in enumerate(mask) if v)
            eval=ControlledEvaluator(concentrated(action),value=.9)
            result=search(env,mask,eval,SearchConfig(4))
            self.assertEqual(result["root_edge_q"][action],0)
            self.assertEqual(eval.calls,1);self.assertEqual(result["search"]["terminal_hits"],4)
            env.step(action);before=env.snapshot()
            with self.assertRaises(SearchError):search(env,env.mask(),eval)
            self.assertEqual(env.snapshot(),before);self.assertEqual(eval.calls,1)

    def test_initial_prior_ties_and_rejected_errors_do_not_change_live_state(self):
        with Environment() as env:
            p=[0.0]*225;p[10]=.8;p[0]=.2
            result=search(env,env.mask(),ControlledEvaluator(lambda e,m:p if e.snapshot()["placements"]==0 else [float(v)/sum(m) for v in m]),SearchConfig(1))
            self.assertEqual(result["action"],10)
            self.assertEqual(result["root_visits"][10],1)
            uniform=search(env,env.mask(),ControlledEvaluator(),SearchConfig(1))
            self.assertEqual(uniform["action"],0)
            env.step(0);before=env.snapshot()
            invalid=ControlledEvaluator(lambda e,m:[1/225]*225)
            with self.assertRaises(SearchError):search(env,env.mask(),invalid)
            badvalue=ControlledEvaluator(value=float("nan"))
            with self.assertRaises(SearchError):search(env,env.mask(),badvalue)
            with self.assertRaises(SearchError):search(env,(False,)*225,ControlledEvaluator())
            def mutate(e,m):
                e.step(1)
                return [float(v)/sum(m) for v in m]
            with self.assertRaises(SearchError):search(env,env.mask(),ControlledEvaluator(mutate))
            self.assertEqual(env.snapshot(),before)
            with patch.object(Environment,"mask",return_value=(False,)*225):
                with self.assertRaises(SearchError):search(env,(False,)*225,ControlledEvaluator())
            self.assertEqual(env.snapshot(),before)
        for kwargs in ({"simulations":0},{"simulations":True},{"c_puct":float("nan")},{"seed":True}):
            with self.assertRaises(ValueError):SearchConfig(**kwargs)

    def test_runner_trace_replay_and_search_failure_diagnostics(self):
        folder=Path(__file__).resolve().parents[1]/"build/mcts-tests"
        folder.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=folder) as d:
            result=run(Path(d)/"games",games=2,compact="a8a1b8c1c8e1d8g1",
                players=(PUCTPlayer(ControlledEvaluator(concentrated(109)),SearchConfig(2)),"ordered"))
            self.assertEqual(result["counts"]["completed"],2)
            for path in (Path(d)/"games").glob("game-*.json"):replay(path)
            bad=PUCTPlayer(ControlledEvaluator(value=float("nan")),SearchConfig(2))
            result=run(Path(d)/"error",games=1,players=(bad,"random"))
            self.assertEqual(result["counts"],{"error":1})
            import json
            trace=json.loads((Path(d)/"error/game-000000.json").read_text(encoding="utf-8"))
            self.assertEqual(trace["diagnostic"]["details"]["search"]["simulations"],0)
            self.assertEqual(trace["diagnostic"]["details"]["search"]["evaluation_attempts"],1)
            replay(trace)


if __name__=="__main__":unittest.main()
