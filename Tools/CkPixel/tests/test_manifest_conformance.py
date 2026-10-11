"""Shared on-disk manifest conformance cases for Python and native loaders."""

import json
from pathlib import Path
import sys
import unittest


TOOL_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_DIR))

from ckpixel_format import FormatError, evaluate_frame, load_manifest


FIXTURES = TOOL_DIR / "Fixtures" / "Manifest"


class ManifestConformanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = json.loads((FIXTURES / "conformance.json").read_text(encoding="utf-8"))

    def test_fixture_index_covers_manifests(self):
        self.assertEqual(self.cases["format"], "ckpixel-manifest-conformance-v1")
        listed = [case["file"] for kind in ("valid", "invalid") for case in self.cases[kind]]
        self.assertEqual(len(listed), len(set(listed)))
        self.assertEqual(set(listed),
                         {path.name for path in FIXTURES.glob("*.json")} - {"conformance.json"})

    def test_valid_manifests(self):
        for case in self.cases["valid"]:
            with self.subTest(file=case["file"]):
                manifest = load_manifest(FIXTURES / case["file"])
                self.assertEqual([manifest.canvas_width, manifest.canvas_height], case["canvas"])
                self.assertEqual(list(manifest.animations), case["animation_order"])
                self.assertEqual(manifest.events, case.get("events", []))
                metadata = dict(case.get("metadata", {}))
                if "number" in metadata:
                    metadata["number"] = int(metadata["number"])
                self.assertEqual(manifest.metadata, metadata)

                unique_sprites = {}
                for expected in case["animations"]:
                    animation = manifest.animations[expected["name"]]
                    self.assertEqual(animation.playback, expected["playback"])
                    self.assertEqual(animation.events, expected.get("events", []))
                    self.assertEqual(animation.metadata, expected.get("metadata", {}))
                    self.assertEqual([frame.source for frame in animation.frames],
                                     expected["frame_sources"])
                    self.assertEqual([frame.duration_ms for frame in animation.frames],
                                     expected["durations_ms"])
                    self.assertEqual([{name: list(point) for name, point in frame.anchors.items()}
                                      for frame in animation.frames],
                                     expected.get("anchors", [{} for _ in animation.frames]))
                    self.assertEqual([frame.events for frame in animation.frames],
                                     expected.get("frame_events", [[] for _ in animation.frames]))
                    self.assertEqual([frame.metadata for frame in animation.frames],
                                     expected.get("frame_metadata", [{} for _ in animation.frames]))
                    indices = []
                    for frame in animation.frames:
                        identity = id(frame.sprite)
                        if identity not in unique_sprites:
                            unique_sprites[identity] = len(unique_sprites)
                        indices.append(unique_sprites[identity])
                    self.assertEqual(indices, expected["source_indices"])
                    for time_ms, active_index in expected["samples"]:
                        with self.subTest(file=case["file"], animation=expected["name"],
                                          time_ms=time_ms):
                            self.assertEqual(evaluate_frame(animation, time_ms).active_index,
                                             active_index)
                self.assertEqual(len(unique_sprites), case["source_count"])

    def test_invalid_manifests(self):
        for case in self.cases["invalid"]:
            with self.subTest(file=case["file"]):
                with self.assertRaises(FormatError) as caught:
                    load_manifest(FIXTURES / case["file"])
                self.assertEqual(caught.exception.code, case["code"])
                self.assertGreaterEqual(caught.exception.line, 1)
                self.assertGreaterEqual(caught.exception.column, 1)

    def test_escaped_extras_keep_distinct_decoded_keys_and_value(self):
        source = (FIXTURES / "valid-escaped-extras.json").read_text(encoding="utf-8")
        self.assertIn(r"a\u0000b", source)
        self.assertIn(r"before\u0000after", source)
        manifest = load_manifest(FIXTURES / "valid-escaped-extras.json")
        self.assertEqual(manifest.metadata, {"ab": "plain", "a\0b": "nul-key"})
        self.assertEqual(manifest.animations["idle"].frames[0].metadata,
                         {"text": "before\0after"})

    def test_negative_zero_anchor_lexeme_is_integer_zero(self):
        source = (FIXTURES / "valid-anchor-negative-zero.json").read_text(encoding="utf-8")
        self.assertIn('"origin": [-0, 1]', source)
        manifest = load_manifest(FIXTURES / "valid-anchor-negative-zero.json")
        self.assertEqual(manifest.animations["idle"].frames[0].anchors["origin"], (0, 1))

    def test_escaped_duplicate_and_source_path_regressions(self):
        for name, code in (
            ("invalid-duplicate-escaped-nul-key.json", "json"),
            ("invalid-source-nul.json", "source_path"),
            ("invalid-path-device-spaced-con.json", "source_path"),
            ("invalid-path-device-spaced-com1.json", "source_path"),
            ("invalid-path-device-spaced-nul-child.json", "source_path"),
        ):
            with self.subTest(file=name):
                with self.assertRaises(FormatError) as caught:
                    load_manifest(FIXTURES / name)
                self.assertEqual(caught.exception.code, code)


if __name__ == "__main__":
    unittest.main()
