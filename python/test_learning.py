import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import torch
from omok_env import Environment,logical
from omok_env.records import replay,save
from omok_env.runner import run
from omok_learning.contracts import CONTRACT,augment,spatial,transform_action
from omok_learning.data import convert,load_dataset,value_target,canonical_game,file_hash
from omok_learning.fixtures import generate
from omok_learning.model import PolicyValueNet,ModelConfig,checked_outputs,probabilities
from omok_learning.train import TrainConfig,train,losses
from omok_learning.checkpoint import atomic_save,payload,load,initialize_for_rl,content_hash
from omok_learning.player import ModelPlayer
from omok_learning.mcts import PUCTPlayer,SearchConfig

ROOT=Path(__file__).resolve().parents[1]
TEMP=ROOT/"build/learning-tests"
TEMP.mkdir(parents=True,exist_ok=True)


class LearningTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(1);torch.use_deterministic_algorithms(True)
        cls.temp=tempfile.TemporaryDirectory(dir=TEMP)
        cls.root=Path(cls.temp.name)
        cls.sources=generate(cls.root/"raw",games=6)
        cls.manifest=convert(cls.sources,cls.root/"data")
        cls.path=cls.root/"data/dataset.json"
        cls.data=load_dataset(cls.path)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_data_values_splits_and_rights(self):
        self.assertEqual(value_target(dict(end_reason="five",result="won",winner=1),0),-1)
        self.assertEqual(value_target(dict(end_reason="five",result="won",winner=1),1),1)
        self.assertEqual(value_target(dict(end_reason="move_limit",result="draw"),0),0)
        self.assertIsNone(value_target(dict(end_reason="resigned",result="won",winner=0),0))
        groups=[set(g["group"] for g in self.manifest["games"][s]) for s in ("train","validation","final")]
        self.assertFalse(groups[0]&groups[1] or groups[0]&groups[2] or groups[1]&groups[2])
        raw=json.loads(self.sources.read_text())
        raw["sources"].append(copy.deepcopy(raw["sources"][0]))
        denied=copy.deepcopy(raw["sources"][1]);denied["rights"]["status"]="unknown"
        raw["sources"].append(denied)
        source=self.root/"raw/filtered-sources.json";save(source,raw)
        m=convert(source,self.root/"filtered")
        reasons=str(m["rejected"])
        self.assertIn("duplicate_or_d4_game",reasons);self.assertIn("rights_unconfirmed",reasons)
        for split,samples in self.data["samples"].items():
            for sample in samples:
                self.assertTrue(sample["mask"][sample["policy"]["action"]])
                self.assertIn(sample["value"],(-1,1))
        corrupted=json.loads(self.path.read_text());corrupted["samples"]["train"][0]["value"]=99
        save(self.root/"corrupted-data.json",corrupted)
        with self.assertRaises(ValueError):load_dataset(self.root/"corrupted-data.json")

    def test_d4_observation_action_mask_and_cpp(self):
        sample=self.data["samples"]["train"][-1]
        record=json.loads((self.root/"data/normalized"/(sample["parent_hash"]+".json")).read_text())
        key=canonical_game(record)
        for k in range(8):
            self.assertEqual(len(set(transform_action(a,k) for a in range(225))),225)
            transformed=augment(sample,k)
            self.assertTrue(transformed["mask"][transformed["policy"]["action"]])
            altered=copy.deepcopy(record)
            for m in altered["actions"]:m["action"]=transform_action(m["action"],k)
            self.assertEqual(canonical_game(altered),key)
            with Environment(black_owner=record["start"]["black_owner"]) as e:
                for m in record["actions"][:sample["ply"]]:e.step(transform_action(m["action"],k))
                self.assertEqual(list(e.observation()),transformed["observation"])
                self.assertEqual(list(e.mask()),transformed["mask"])

    def test_outputs_mask_losses_and_overfit(self):
        torch.manual_seed(3)
        model=PolicyValueNet()
        sample=copy.deepcopy(self.data["samples"]["train"][-1])
        p,v,_=losses(model,[sample]);initial=float(p.detach()+v.detach())
        before=copy.deepcopy(model.state_dict())
        optimizer=torch.optim.Adam(model.parameters(),lr=.03)
        for _ in range(100):
            p,v,_=losses(model,[sample]);optimizer.zero_grad();(p+v).backward();optimizer.step()
        p,v,m=losses(model,[sample])
        self.assertLess(float(p.detach()+v.detach()),initial*.25)
        self.assertEqual(m["top1"],1)
        self.assertTrue(any(not torch.equal(before[k],x) for k,x in model.state_dict().items()))
        sample["value_eligible"]=False;sample["value"]=None
        _,v,m=losses(model,[sample]);self.assertEqual(float(v.detach()),0);self.assertEqual(m["value_n"],0)
        invalid=copy.deepcopy(sample);invalid["mask"][invalid["policy"]["action"]]=False
        with self.assertRaises(ValueError):losses(model,[invalid])
        logits=torch.zeros(1,225);mask=torch.ones(1,225,dtype=torch.bool);mask[0,0]=False
        probs=probabilities(logits,mask)
        self.assertEqual(float(probs[0,0]),0);self.assertAlmostEqual(float(probs.sum()),1,places=5)
        with self.assertRaises(ValueError):probabilities(logits,torch.zeros_like(mask))
        with self.assertRaises(ValueError):checked_outputs(torch.full((1,225),float("nan")),torch.zeros(1,1),1)
        with self.assertRaises(ValueError):checked_outputs(torch.zeros(1,224),torch.zeros(1,1),1)
        with self.assertRaises(ValueError):checked_outputs(torch.zeros(1,225),torch.full((1,1),2.),1)
        distribution=copy.deepcopy(sample)
        distribution["policy"]=dict(kind="distribution",probabilities=[float(i==sample["policy"]["action"]) for i in range(225)])
        losses(model,[distribution])

    def test_exact_resume_checkpoint_and_rl_initialization(self):
        config=TrainConfig(batch_size=16)
        full=train(self.path,self.root/"full",epochs=2,config=config)
        train(self.path,self.root/"part",epochs=1,config=config)
        resumed=train(self.path,self.root/"resumed",epochs=2,config=config,resume=self.root/"part/latest.pt")
        model,a=load(self.root/"full/latest.pt",kind="sl-resume")
        other,b=load(self.root/"resumed/latest.pt",kind="sl-resume")
        self.assertEqual(content_hash(a["weights"]),content_hash(b["weights"]))
        self.assertEqual(content_hash(a["optimizer"]),content_hash(b["optimizer"]))
        self.assertEqual(content_hash(a["rng"]),content_hash(b["rng"]))
        self.assertEqual(full["history"],resumed["history"])
        inference,_=load(self.root/"full/best.pt",kind="inference")
        initialized,optimizer,meta=initialize_for_rl(self.root/"full/best.pt")
        x=torch.zeros(1,4,15,15)
        self.assertTrue(torch.equal(inference(x)[0],initialized(x)[0]));self.assertIsNone(optimizer)
        _,optimizer,meta=initialize_for_rl(self.root/"full/latest.pt",optimizer_mode="preserve")
        self.assertTrue(optimizer.state);self.assertEqual(meta["rl_step"],0)
        with self.assertRaises(ValueError):load(self.root/"full/best.pt",kind="sl-resume")
        with self.assertRaises(ValueError):train(self.path,self.root/"bad-resume",epochs=2,config=TrainConfig(lr=.001),resume=self.root/"part/latest.pt")
        artifact=self.root/"full/best.pt"
        original=artifact.read_bytes()
        with patch("torch.save",side_effect=OSError("injected")):
            with self.assertRaises(OSError):atomic_save(artifact,payload(model,"inference",{}))
        self.assertEqual(original,artifact.read_bytes())
        bad=dict(a);bad["contract"]=dict(CONTRACT,action="wrong")
        atomic_save(self.root/"incompatible.pt",bad)
        with self.assertRaises(ValueError):load(self.root/"incompatible.pt")
        bad=copy.deepcopy(a);bad["weights"]["policy.weight"]=torch.zeros(2,8,1,1)
        atomic_save(self.root/"bad-shape.pt",bad)
        with self.assertRaises(ValueError):load(self.root/"bad-shape.pt")
        (self.root/"broken.pt").write_bytes(b"broken")
        with self.assertRaises(ValueError):load(self.root/"broken.pt")

    def test_model_games_and_failures(self):
        path=self.root/"untrained.pt"
        torch.manual_seed(5)
        atomic_save(path,payload(PolicyValueNet(),"inference",dict(purpose="untrained-test")))
        for opponent in ("ordered",ModelPlayer(path)):
            folder=self.root/("games-self" if not isinstance(opponent,str) else "games-ordered")
            result=run(folder,games=2,players=(ModelPlayer(path),opponent))
            self.assertEqual(result["counts"].get("completed"),2)
            for f in folder.glob("game-*.json"):replay(f)
        with Environment() as e:
            before=e.snapshot();player=ModelPlayer(path)
            with self.assertRaises(ValueError):player.select(e,(False,)*225)
            self.assertEqual(e.snapshot(),before)
        result=run(self.root/"load-error",games=1,players=(ModelPlayer(self.root/"missing.pt"),"random"))
        self.assertEqual(result["counts"],{"error":1})
        replay(self.root/"load-error/game-000000.json")
        with patch.object(PolicyValueNet,"forward",return_value=(torch.full((1,225),float("nan")),torch.zeros(1,1))):
            result=run(self.root/"nonfinite-error",games=1,players=(ModelPlayer(path),"ordered"))
        self.assertEqual(result["counts"],{"error":1})
        replay(self.root/"nonfinite-error/game-000000.json")
        with Environment() as e:
            e.step(0)
            with patch.object(PolicyValueNet,"forward",return_value=(torch.zeros(1,225),torch.zeros(1,1))):
                self.assertEqual(ModelPlayer(path).select(e,e.mask())["action"],1)

    def test_puct_real_model_load_once_eval_mode_and_reproducibility(self):
        path=self.root/"puct-model.pt"
        torch.manual_seed(41)
        atomic_save(path,payload(PolicyValueNet(),"inference",dict(purpose="PUCT interface test")))
        evaluator=ModelPlayer(path)
        with patch("omok_learning.player.load",wraps=load) as loader:
            evaluator.prepare()
            forward=evaluator.model.forward
            modes=[]
            def checked_forward(x):
                modes.append((evaluator.model.training,torch.is_grad_enabled()))
                return forward(x)
            with Environment(compact="h8i8g9") as env,patch.object(evaluator.model,"forward",side_effect=checked_forward):
                before=env.snapshot();mask=env.mask()
                priors=evaluator.evaluate(env,mask)["priors"]
                self.assertAlmostEqual(sum(priors),1,places=5)
                self.assertTrue(all(p==0 for p,legal in zip(priors,mask) if not legal))
                player=PUCTPlayer(evaluator,SearchConfig(3))
                first=player.select(env,mask);second=player.select(env,mask)
                self.assertEqual(first["root_visits"],second["root_visits"])
                self.assertEqual(first["root_edge_q"],second["root_edge_q"])
                self.assertEqual(first["search"]["network_evaluations"],4)
                self.assertEqual(first["model_load_ms"],0)
                self.assertEqual(env.snapshot(),before)
            self.assertEqual(loader.call_count,1)
            self.assertTrue(modes and all(mode==(False,False) for mode in modes))


if __name__=="__main__":unittest.main()
