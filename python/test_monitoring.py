import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from omok_monitoring import Monitor,MonitoringStorageError,RULES
from omok_monitoring.acceptance import execute
from omok_monitoring.events import file_hash

TEMP=Path(__file__).resolve().parents[1]/"build/monitor-tests"
TEMP.mkdir(parents=True,exist_ok=True)


class MonitoringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(dir=TEMP)
        cls.root=Path(cls.temp.name)
        cls.summary=execute(cls.root/"acceptance")
        cls.cases={c["case"]:c["report"] for c in cls.summary["cases"]}

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def events(self,name):
        return [json.loads(line) for line in (self.root/"acceptance"/name/"events.jsonl").read_text(encoding="utf-8").splitlines()]

    def test_schema_order_coverage_and_no_unexecuted_pass(self):
        self.assertEqual(len(RULES),13)
        self.assertEqual(len({r["run_id"] for r in self.cases.values()}),len(self.cases))
        self.assertEqual(set(self.summary["states"]),{"normal","warning","failure","deferred"})
        for name,report in self.cases.items():
            events=self.events(name)
            self.assertEqual([e["sequence"] for e in events],list(range(1,len(events)+1)))
            self.assertEqual(len(set((e["run_id"],e["attempt_id"],e["sequence"],e["rule_version"]) for e in events)),len(events))
            self.assertEqual(file_hash(self.root/"acceptance"/name/"events.jsonl"),report["events_sha256"])
            self.assertEqual(report["coverage"]["LP-01"]["status"],"deferred")
            self.assertEqual(report["coverage"]["LP-01"]["reason"],"not_executed")
            for e in events:
                self.assertEqual(e["rule_version"],1)
                self.assertTrue(e["utc"] and e["implementation_sha256"] and e["config_sha256"])
                self.assertGreaterEqual(e["elapsed_seconds"],0)

    def test_intended_and_operational_rejections_forbidden(self):
        ev=self.events("rejections")
        self.assertEqual(sum(e["rule_id"]=="GR-02" and e["state"]=="normal" for e in ev),4)
        self.assertEqual(sum(e["rule_id"]=="GR-02" and e["state"]=="warning" for e in ev),1)
        ev=self.events("forbidden")
        self.assertTrue(any(e["reason"]=="forbidden_adjudication_contract" and e["state"]=="normal" for e in ev))
        self.assertFalse(any(e["state"]=="failure" for e in ev))
        self.assertTrue(any(e["reason"]=="rejected_request_mutated_state" and e["state"]=="failure" for e in self.events("rejection-state-mutation")))

    def test_post_mask_error_and_partial_search_not_failed_equations(self):
        self.assertTrue(any(e["rule_id"]=="GR-01" and e["state"]=="failure" for e in self.events("post-mask-illegal")))
        ev=self.events("partial-puct")
        self.assertTrue(any(e["reason"]=="partial_search_not_success_checked" and e["state"]=="deferred" for e in ev))
        self.assertFalse(any(e["reason"]=="completed_search_metadata_invariants" for e in ev))
        cancelled=self.events("partial-puct-cancel")
        partial=[e for e in cancelled if e["reason"]=="partial_search_not_success_checked"]
        self.assertEqual(partial[0]["evidence"]["partial"]["search"]["simulations"],1)
        self.assertFalse(any(e["reason"]=="completed_search_metadata_invariants" for e in cancelled))
        self.assertTrue(any(e["rule_id"]=="GR-04" and e["state"]=="deferred" for e in self.events("blocked")))

    def test_request_accept_save_end_order_and_failure_distinction(self):
        ev=self.events("stop-save-end")
        seq=lambda reason:next(e["sequence"] for e in ev if e["reason"]==reason)
        self.assertLess(seq("stop_requested"),seq("stop_accepted"))
        self.assertLess(seq("stop_accepted"),seq("artifact_save_confirmed"))
        self.assertLess(seq("artifact_save_confirmed"),seq("execution_scope_ended"))
        stop=self.cases["stop-save-end"]["stop"]
        self.assertTrue(stop["requested"] and stop["accepted"] and stop["execution_scope_ended"] and stop["safe_stop_complete"])
        fail=self.cases["save-failure"]["stop"]
        self.assertTrue(fail["storage_failed"] and fail["execution_scope_ended"])
        self.assertFalse(fail["safe_stop_complete"])

    def test_rule_ground_truth_labels_artifact_corruption_and_criteria(self):
        self.assertTrue(any(e["rule_id"]=="GR-03" and e["state"]=="failure" for e in self.events("wrong-expectation-and-label")))
        self.assertTrue(any(e["rule_id"]=="GR-04" and e["state"]=="failure" for e in self.events("wrong-expectation-and-label")))
        self.assertTrue(any(e["rule_id"]=="LP-07" and e["state"]=="failure" for e in self.events("search-metadata-corruption")))
        self.assertTrue(any(e["rule_id"]=="LP-07" and e["state"]=="failure" for e in self.events("logical-record-corruption")))
        self.assertTrue(any(e["reason"]=="duplicate_result_inclusion" and e["state"]=="failure" for e in self.events("artifact-and-duplicate")))
        self.assertTrue(any(e["rule_id"]=="LP-02" and e["state"]=="failure" for e in self.events("numeric-and-unestablished-strength")))
        self.assertTrue(any(e["rule_id"]=="LP-04" and e["state"]=="failure" for e in self.events("numeric-and-unestablished-strength")))
        for e in self.events("heartbeat-clock"):
            self.assertNotIn(e["state"],("warning","failure"))
        self.assertEqual(self.cases["normal"]["coverage"]["LP-06"]["status"],"deferred")

    def test_event_store_and_report_save_failures_are_not_success(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as folder:
            m=Monitor(Path(folder)/"event-store")
            original=Path.open
            def failing(path,*a,**kw):
                if path==m.path:raise OSError("injected event-store failure")
                return original(path,*a,**kw)
            with patch.object(Path,"open",failing):
                with self.assertRaises(MonitoringStorageError):m.progress("injected")
            self.assertTrue(m.storage_failed and not m.report_saved)
            m.finish(outcome="event_persistence_failed")
            report=json.loads((m.output/"report.json").read_text(encoding="utf-8"))
            self.assertTrue(report["unpersisted_events"])
            self.assertFalse(report["stop"]["safe_stop_complete"])
            n=Monitor(Path(folder)/"report-store")
            with patch("omok_env.records.save",side_effect=OSError("injected report failure")):
                with self.assertRaises(MonitoringStorageError):n.finish()
            self.assertFalse(n.report_saved)
            self.assertFalse((n.output/"report.json").exists())
            self.assertTrue(any(e["reason"]=="monitor_storage_failure" for e in n.events))


if __name__=="__main__":unittest.main()
