import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from omok_monitoring.neural_acceptance import execute

ROOT=Path(__file__).resolve().parents[1]
TEMP=ROOT/"build/monitor-neural-tests"
TEMP.mkdir(parents=True,exist_ok=True)


class MonitoringNeuralTests(unittest.TestCase):
    def test_copied_checkpoints_no_update_outputs_and_recovery(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as folder,patch("torch.optim.AdamW.step",side_effect=AssertionError("no updates authorized")):
            result=execute(ROOT/"runs/sl-release-trained/best.pt",ROOT/"runs/sl-release-trained/latest.pt",Path(folder)/"audit")
            cases={c["case"]:c["report"] for c in result["cases"]}
            self.assertTrue(result["source_checkpoints_unchanged"])
            self.assertEqual(result["optimizer_updates"],0)
            for name in ("nonfinite-output","infinite-output","wrong-shape","damaged-checkpoint"):
                self.assertEqual(cases[name]["coverage"]["LP-01"]["status"],"failure")
            self.assertEqual(cases["mutable-loaded-model"]["coverage"]["LP-08"]["status"],"failure")
            ev=[json.loads(x) for x in (Path(folder)/"audit/normal-model-numerics/events.jsonl").read_text(encoding="utf-8").splitlines()]
            masses=[e for e in ev if e["reason"]=="pre_mask_mass_observed_no_threshold"]
            self.assertTrue(masses)
            self.assertTrue(all(e["state"]=="deferred" and not e["evidence"]["actual_illegal_placement"] for e in masses))
            self.assertTrue(any(e["evidence"]["raw_illegal_mass"]>0 for e in masses))
            self.assertTrue(cases["sl-entry-stop"]["stop"]["accepted"])
            self.assertFalse(cases["sl-entry-stop"]["stop"]["safe_stop_complete"])
            self.assertTrue(cases["checkpoint-save-recovery"]["stop"]["safe_stop_complete"])
            self.assertFalse(cases["checkpoint-save-failure"]["stop"]["safe_stop_complete"])


if __name__=="__main__":unittest.main()
