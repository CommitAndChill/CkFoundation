"""CLI checks for false-green, alignment, region and evidence-preservation behavior."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image, ImageDraw


SCRIPT = Path(__file__).with_name("compare_reference.py")


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name)
        for name in ("reference", "candidate"):
            Image.new("RGB", (320, 240), "#808080").save(self.root / f"{name}.png")
            self.mask(name, (40, 40, 160, 160))
        self.view = {"name": "hero", "reference_image": "reference.png", "candidate_image": "candidate.png",
                     "reference_mask": "reference_mask.png", "candidate_mask": "candidate_mask.png", "min_iou": 0.95}
        self.manifest = {"version": 1, "views": [self.view]}

    def mask(self, name, box):
        image = Image.new("L", (320, 240), 0)
        if box:
            ImageDraw.Draw(image).rectangle(box, fill=255)
        image.save(self.root / f"{name}_mask.png")

    def invoke(self, output="out"):
        manifest = self.root / "manifest.json"
        manifest.write_text(json.dumps(self.manifest), encoding="utf-8")
        result = subprocess.run([sys.executable, str(SCRIPT), "--manifest", str(manifest),
                                 "--out", str(self.root / output)], capture_output=True, text=True)
        report_path = self.root / output / "report.json"
        report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.exists() else None
        return result, report

    def test_identical_passes_metrics_but_never_visual_review(self):
        (self.root / "source.blend").write_bytes(b"source identity fixture")
        self.manifest["source_file"] = "source.blend"
        result, report = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(report["verdict"], "METRIC_PASS")
        self.assertEqual(report["visual_acceptance"], "PENDING_REVIEW")
        self.assertEqual(report["views"][0]["checks"][0]["value"], 1)
        self.assertEqual(len(report["source"]["sha256"]), 64)
        with Image.open(self.root / "out/hero_compare.png") as image:
            self.assertEqual(image.size, (960, 264))

    def test_shifted_candidate_fails(self):
        self.mask("candidate", (80, 40, 200, 160))
        result, report = self.invoke()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(report["verdict"], "METRIC_FAIL")

    def test_missing_tolerance_cannot_pass(self):
        del self.view["min_iou"]
        result, report = self.invoke()
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(report["verdict"], "METRIC_UNCONFIGURED")

    def test_region_failure_is_not_hidden_by_whole_body(self):
        self.mask("candidate", (40, 40, 150, 160))
        self.view["min_iou"] = 0.8
        self.view["regions"] = [{"name": "edge_feature", "box": [140, 40, 170, 161], "min_iou": 0.9}]
        result, report = self.invoke()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual([c["status"] for c in report["views"][0]["checks"]], ["PASS", "FAIL"])

    def test_landmark_failure_and_failure_precedes_unconfigured(self):
        del self.view["min_iou"]
        self.view["landmarks"] = [{"name": "eye", "reference": [80, 80], "candidate": [90, 80], "max_error_px": 3}]
        result, report = self.invoke()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(report["views"][0]["checks"][1]["value"], 10)

    def test_empty_candidate_scores_zero_but_empty_reference_is_invalid(self):
        self.mask("candidate", None)
        result, report = self.invoke()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(report["views"][0]["checks"][0]["value"], 0)
        self.mask("reference", None)
        result, report = self.invoke("empty_reference")
        self.assertEqual(result.returncode, 2)
        self.assertIsNone(report)
        self.assertIn("no foreground", result.stderr)

    def test_mismatched_dimensions_and_rgb_masks_are_rejected(self):
        Image.new("L", (160, 120)).save(self.root / "candidate_mask.png")
        result, report = self.invoke()
        self.assertEqual(result.returncode, 2)
        self.assertIsNone(report)
        Image.new("RGB", (320, 240)).save(self.root / "candidate_mask.png")
        result, report = self.invoke("rgb")
        self.assertEqual(result.returncode, 2)
        self.assertIn("single-channel", result.stderr)

    def test_invalid_tolerances_regions_and_duplicate_views_are_rejected(self):
        for tolerance in (True, float("nan"), -1, 1.1):
            with self.subTest(tolerance=tolerance):
                self.view["min_iou"] = tolerance
                result, report = self.invoke()
                self.assertEqual(result.returncode, 2)
                self.assertIsNone(report)
        self.view["min_iou"] = 0.95
        self.view["regions"] = [{"name": "bad", "box": [-1, 0, 20, 20], "min_iou": 0.9}]
        result, report = self.invoke()
        self.assertEqual(result.returncode, 2)
        self.assertIsNone(report)
        del self.view["regions"]
        self.manifest["views"].append(dict(self.view))
        result, report = self.invoke()
        self.assertEqual(result.returncode, 2)
        self.assertIn("unique", result.stderr)

    def test_existing_evidence_is_preserved(self):
        first, _ = self.invoke()
        self.assertEqual(first.returncode, 0, first.stderr)
        evidence = (self.root / "out/report.json").read_bytes()
        self.mask("candidate", None)
        result, _ = self.invoke()
        self.assertEqual(result.returncode, 2)
        self.assertEqual((self.root / "out/report.json").read_bytes(), evidence)


if __name__ == "__main__":
    unittest.main()
