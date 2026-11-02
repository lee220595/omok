"""Bounded functional acceptance only: fixed fixtures, deterministic faults, no SL/RL."""
import argparse
from collections import Counter
import json
from pathlib import Path
from unittest.mock import patch
from omok_env import Environment
from omok_env.records import save,replay
from omok_env.runner import run
from omok_learning.mcts import PUCTPlayer,SearchConfig
from . import Monitor,MonitorViolation,RULES
from .checks import audited_step,reviewed_fixture,numeric,loss_observation,tactical,comparison,episode,fixed_artifact
from .events import file_hash

WIN="a8a1b8c1c8e1d8g1"
FORBIDDEN="g8a1h7c1i8e1h9g1"
PLAN=dict(schema="omok-monitor-acceptance-plan-v1",purpose="bounded event/response acceptance, not strength or learning",
    fixed_starts=dict(win=WIN,forbidden=FORBIDDEN),max_general_moves_per_game=1,max_simulations_per_search=2,
    player="controlled test double",sl_optimizer_updates=0,rl_updates=0,
    termination="fixed normal result, explicit cap/stop, or deterministic injected error; independent output per case")


class Evaluator:
    descriptor={"kind":"controlled-functional-fixture","sha256":None}
    def __init__(self,value=0.):self.value=value
    def prepare(self):return 0.
    def evaluate(self,env,mask):
        return dict(priors=[float(i==109) for i in range(225)] if mask[109] else [float(v)/sum(mask) for v in mask],value=self.value)


class Selector:
    descriptor={"kind":"controlled-functional-selector"}
    def __init__(self,action=109,stop=None):self.action=action;self.stop=stop
    def select(self,env,mask):
        if self.stop:self.stop.request_stop("fixed acceptance stop")
        return dict(action=self.action,token=env.snapshot()["token"])


def execute(output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):raise ValueError("acceptance output must be empty")
    # Publish fixed inputs/bounds BEFORE creating environments or running scenarios.
    save(output/"plan.json",PLAN)
    reports=[]
    def case(name,fn):
        with Monitor(output/name,config=PLAN) as monitor:
            monitor.bind(acceptance_case=name)
            fn(monitor)
        report=json.loads((output/name/"report.json").read_text(encoding="utf-8"))
        reports.append(dict(case=name,report=report))

    def normal(m):
        result=run(output/"normal-games",games=1,compact=WIN,max_actions=1,
            players=(PUCTPlayer(Evaluator(),SearchConfig(2),monitor=m),"ordered"),monitor=m)
        assert result["counts"]=={"completed":1,"won":1}
        trace=json.loads((output/"normal-games/game-000000.json").read_text(encoding="utf-8"))
        reviewed_fixture(m,fixture_id="prepared-black-five-at-e8-v1",actual=trace["final"]["end_reason"],expected="five",
            source="src/match/development_positions.cpp:Win; docs/rules-spec.md; existing match/rules regressions",reviewed=True)
        actual_action=trace["actions"][0]["action"]
        tactical(m,actual_action,expected=[109],reviewed=True,spec=dict(fixture="controlled-immediate-win-v1",simulations=2))
        tactical(m,actual_action)
        numeric(m,dict(loss=1.,policy_loss=.5,value_mse=.5),epoch=0,step=0)
        loss_observation(m,dict(train=1.,validation=2.,source="synthetic observation, no training"))
        m.completed_unit(str((output/"normal-games/game-000000.json").resolve()),resumed=True)
    case("normal",normal)

    def rejected(m):
        with Environment() as env:
            before=env.snapshot()
            for kwargs in (dict(action=-1),dict(action=0,actor=1),dict(action=0,expected=["wrong",1,0])):
                try:audited_step(m,env,**kwargs,intentional=True)
                except ValueError:pass
                assert env.snapshot()==before
            try:audited_step(m,env,0,expected=["wrong",1,0])
            except ValueError:pass
        with Environment(compact=WIN) as env:
            audited_step(m,env,109)
            before=env.snapshot()
            try:audited_step(m,env,110,actor=0,intentional=True)
            except ValueError:pass
            assert env.snapshot()==before
    case("rejections",rejected)
    def mutated_rejection(m):
        with Environment() as env:
            original=env.step
            def bad_step(*a,**kw):
                original(0)
                raise ValueError("injected state change followed by rejection")
            with patch.object(env,"step",side_effect=bad_step):
                try:audited_step(m,env,0,intentional=True)
                except ValueError:pass
    case("rejection-state-mutation",mutated_rejection)

    def forbidden(m):
        with Environment(compact=FORBIDDEN) as env:
            before=env.snapshot();actual=audited_step(m,env,112,allow_forbidden=True,intentional=True)
            assert env.snapshot()["board"]==before["board"] and env.snapshot()["winner"]==1
            from omok_learning.data import value_target
            assert value_target(env.snapshot(),0) is None
            assert value_target(dict(result="won",winner=0,end_reason="resigned"),0) is None
            reviewed_fixture(m,fixture_id="prepared-double-three-h8-v1",actual=actual,
                expected=dict(verdict="forbidden",reason=12),reviewed=True,
                source="src/match/development_positions.cpp:Forbidden; docs/renju-cases.md; rules.double_three")
    case("forbidden",forbidden)

    case("post-mask-illegal",lambda m:run(output/"illegal-games",games=1,compact=WIN,max_actions=1,
        players=(Selector(105),"ordered"),monitor=m))
    case("partial-puct",lambda m:run(output/"partial-games",games=1,compact=WIN,max_actions=1,
        players=(PUCTPlayer(Evaluator(float("nan")),SearchConfig(2),monitor=m),"ordered"),monitor=m))
    case("stop-save-end",lambda m:run(output/"stop-games",games=1,compact=WIN,max_actions=1,
        players=(Selector(stop=m),"ordered"),monitor=m))
    def interrupted_search(m):
        class InterruptedEvaluator(Evaluator):
            calls=0
            def evaluate(self,env,mask):
                self.calls+=1
                action=next(i for i,legal in enumerate(mask) if legal)
                if self.calls==2:m.request_stop("fixed stop after one leaf evaluation")
                return dict(priors=[float(i==action) for i in range(225)],value=0.)
        result=run(output/"interrupted-search-games",games=1,compact=WIN,max_actions=1,
            players=(PUCTPlayer(InterruptedEvaluator(),SearchConfig(2),monitor=m),"ordered"),monitor=m)
        assert result["counts"]=={"interrupted":1}
    case("partial-puct-cancel",interrupted_search)
    def blocked(m):
        with patch.object(Environment,"mask",return_value=(False,)*225):
            result=run(output/"blocked-games",games=1,compact=WIN,max_actions=1,players=(Selector(),"ordered"),monitor=m)
        assert result["counts"]=={"blocked":1}
    case("blocked",blocked)

    def save_failure(m):
        original=save
        def fail(path,record):
            if Path(path).name=="game-000000.json":raise OSError("injected game save failure")
            return original(path,record)
        with patch("omok_env.runner.save",side_effect=fail):
            result=run(output/"save-failed-games",games=1,compact=WIN,max_actions=1,players=(Selector(),"ordered"),monitor=m)
        assert result["counts"]=={"error":1}
    case("save-failure",save_failure)

    def corrupted(m):
        trace=json.loads((output/"normal-games/game-000000.json").read_text(encoding="utf-8"))
        trace["actions"][0]["inference"]["root_visits"][109]+=1
        try:replay(trace,monitor=m)
        except ValueError:pass
        else:raise AssertionError("corrupted search metadata accepted")
    case("search-metadata-corruption",corrupted)
    def bad_board(m):
        trace=json.loads((output/"normal-games/game-000000.json").read_text(encoding="utf-8"))
        trace["final"]["board"][0]=99
        try:replay(trace,monitor=m)
        except ValueError:pass
        else:raise AssertionError("corrupted final board accepted")
    case("logical-record-corruption",bad_board)

    def false_expectation(m):
        reviewed_fixture(m,fixture_id="injected-wrong-expectation",actual="five",expected="draw",reviewed=True,source="deliberately wrong acceptance expectation")
        reviewed_fixture(m,fixture_id="unreviewed",actual="five")
        episode(m,dict(status="completed",final=dict(terminated=False,result="aborted",end_reason="closed")))
    case("wrong-expectation-and-label",false_expectation)

    def artifact(m):
        path=output/"mutable-fixture.bin";path.write_bytes(b"original")
        expected=file_hash(path);path.write_bytes(b"changed copy")
        try:fixed_artifact(m,path,expected)
        except MonitorViolation:pass
        m.completed_unit("fixture-result")
        m.completed_unit("fixture-result")
    case("artifact-and-duplicate",artifact)
    def numeric_fault(m):
        try:numeric(m,dict(loss=float("inf"),gradient=[float("nan")]),epoch=0,step=0,collected=("loss","gradient"))
        except MonitorViolation:pass
        tactical(m,0,expected=[109],reviewed=True,required=False,spec=dict(simulations=2,source="injected nonrequired miss"))
        tactical(m,0,expected=[109],reviewed=True,required=True,spec=dict(simulations=2,source="injected required miss"))
        comparison(m,dict(samples=0))
    case("numeric-and-unestablished-strength",numeric_fault)

    # A fake clock advances without assigning any made-up timeout threshold.
    clock=[0.]
    with Monitor(output/"heartbeat-clock",config=PLAN,clock=lambda:clock[0]) as m:
        m.bind(acceptance_case="heartbeat-clock")
        m.progress("evaluation",{"step":0});clock[0]=600.;m.progress("evaluation",{"step":0})
    reports.append(dict(case="heartbeat-clock",report=json.loads((output/"heartbeat-clock/report.json").read_text(encoding="utf-8"))))
    states=Counter();coverage={rule:Counter() for rule in RULES}
    for item in reports:
        states.update(item["report"]["states"])
        for rule,value in item["report"]["coverage"].items():coverage[rule].update(value["states"])
    summary=dict(schema="omok-monitor-acceptance-v1",plan=PLAN,cases=reports,states=dict(states),
        rule_states={k:dict(v) for k,v in coverage.items()},neural_checks="not executed here; separate no-update optional tests")
    save(output/"acceptance-summary.json",summary)
    return summary


if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("output");args=p.parse_args()
    result=execute(args.output);print(json.dumps(dict(cases=len(result["cases"]),states=result["states"],rule_states=result["rule_states"]),indent=2))
