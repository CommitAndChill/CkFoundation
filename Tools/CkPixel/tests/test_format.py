import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


TOOL_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_DIR))

from ckpixel_format import (Animation, FormatError, Frame, Sprite, evaluate_frame,
                            load_manifest, parse_manifest, parse_xpm)


FIXTURES = TOOL_DIR / "Fixtures"


def xpm(*lines):
    return "static char * image[] = {" + ",".join(json.dumps(line) for line in lines) + "};"


class XpmTests(unittest.TestCase):
    def test_conformance_fixtures(self):
        cases = json.loads((FIXTURES / "conformance.json").read_text(encoding="utf-8"))
        for case in cases["success"]:
            with self.subTest(case=case["file"]):
                self.assertEqual(parse_xpm((FIXTURES / case["file"]).read_bytes()).to_dict(), case["expected"])
        for case in cases["failure"]:
            with self.subTest(case=case["file"]):
                with self.assertRaises(FormatError) as caught:
                    parse_xpm((FIXTURES / case["file"]).read_bytes(), case["file"])
                self.assertEqual(caught.exception.code, case["code"])

    def test_bom_comments_and_case_sensitive_symbols(self):
        source = b"\xef\xbb\xbf/* caf\xc3\xa9 */\n" + xpm("2 1 2 1", "A c RED", "a c blue", "Aa").encode()
        self.assertEqual(parse_xpm(source).indices, (0, 1))
        with self.assertRaises(FormatError) as caught:
            parse_xpm("\ufeff\ufeff" + xpm("1 1 1 1", ". c red", "."))
        self.assertEqual(caught.exception.code, "syntax")

    def test_rejected_grammar_and_data(self):
        cases = [
            (xpm("1 1 1 1", ". c red", ".") + xpm("1 1 1 1", ". c red", "."), "syntax"),
            ('char *x[]={"1 1 1 1" "x c red","x"};', "syntax"),
            (xpm("2147483648 1 1 1", ". c red", "."), "header"),
            (xpm("00000000001 1 1 1", ". c red", "."), "header"),
            (xpm("0 1 1 1", ". c red", "."), "limit"),
            (xpm("1 1 1 1 1 0", ". c red", "."), "header"),
            (xpm("1 1 1 1", ". c red", ".."), "row_length"),
            (xpm("1 1 1 1", ". c red", "x"), "symbol"),
            (xpm("1 1 1 1", ". m red", "."), "color"),
            (xpm("1 1 1 1", ". c red c blue", "."), "palette"),
            (xpm("1 1 1 1", ". c", "."), "palette"),
            (xpm("1 1 1 1", ". c chartreuse", "."), "color"),
            (xpm("1 1 1 1", ". c #xz0", "."), "color"),
            ('char *x[]={"1 1 1 1","x c red","\\x100"};', "escape"),
            ('char *x[]={"1 1 1 1","x c red","\\n"};', "escape"),
            ('char *x[]={"1 1 1 1","x c red","x\ny"};', "character"),
        ]
        for source, code in cases:
            with self.subTest(code=code, source=source):
                with self.assertRaises(FormatError) as caught:
                    parse_xpm(source, "bad.xpm")
                self.assertEqual(caught.exception.code, code)
                self.assertEqual(caught.exception.path, "bad.xpm")
                self.assertGreaterEqual(caught.exception.line, 1)
                self.assertGreaterEqual(caught.exception.column, 1)

    def test_bounds_and_escape_rounding(self):
        self.assertEqual(parse_xpm(xpm("1 1 1 1", ". c #000800fff", ".")).palette[0].rgba,
                         (0, 128, 255, 255))
        self.assertEqual(parse_xpm(xpm("1 1 1 1", ". c Grey100", ".")).palette[0].rgba,
                         (255, 255, 255, 255))
        self.assertEqual(parse_xpm(xpm("1 1 1 1", ". c grey0000000100", ".")).palette[0].rgba,
                         (255, 255, 255, 255))
        with self.assertRaises(FormatError) as caught:
            parse_xpm(b"x" * (8 * 1024 * 1024 + 1))
        self.assertEqual(caught.exception.code, "source_limit")
        self.assertEqual(parse_xpm(xpm("1 1 1 1", ". c gray", ".")).palette[0].rgba,
                         (190, 190, 190, 255))
        with self.assertRaises(FormatError) as caught:
            parse_xpm(b"\xff")
        self.assertEqual(caught.exception.code, "utf8")


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=TOOL_DIR)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "a.xpm").write_text(xpm("2 1 1 1", ". c red", ".."), encoding="utf-8")

    def manifest(self, frame=None):
        if frame is None:
            frame = {"source": "a.xpm", "duration_ms": 100,
                     "anchors": {"origin": [1, 0]}, "events": [{"kind": "hit"}]}
        return {"version": 1, "canvas": {"width": 2, "height": 1},
                "animations": {"idle": {"playback": "loop", "frames": [frame]}}}

    def parse(self, value):
        return parse_manifest(json.dumps(value), self.root)

    def test_manifest_loads_sprite_and_preserves_extras(self):
        manifest = self.parse(self.manifest())
        frame = manifest.animations["idle"].frames[0]
        self.assertEqual(frame.sprite.indices, (0, 0))
        self.assertEqual(frame.anchors, {"origin": (1, 0)})
        self.assertEqual(frame.events, [{"kind": "hit"}])
        path = self.root / "manifest.json"
        path.write_text(json.dumps(self.manifest()), encoding="utf-8")
        self.assertEqual(load_manifest(path).canvas_width, 2)

    def test_rejects_duplicate_keys_and_unknown_fields(self):
        with self.assertRaises(FormatError) as caught:
            parse_manifest('{"version":1,"version":1}', self.root)
        self.assertEqual(caught.exception.code, "json")
        value = self.manifest()
        value["layers"] = []
        with self.assertRaises(FormatError) as caught:
            self.parse(value)
        self.assertEqual(caught.exception.code, "layers_unsupported")

    def test_nested_metadata_and_large_events_are_bounded(self):
        value = self.manifest()
        nested = "leaf"
        for _ in range(70):
            nested = [nested]
        value["metadata"] = {"nested": nested}
        with self.assertRaises(FormatError) as caught:
            self.parse(value)
        self.assertEqual(caught.exception.code, "limits")
        value = self.manifest()
        value["events"] = list(range(100001))
        with self.assertRaises(FormatError) as caught:
            self.parse(value)
        self.assertEqual(caught.exception.code, "limits")

    def test_numeric_overflow_inside_metadata_is_rejected(self):
        value = self.manifest()
        value["metadata"] = {"value": 0}
        source = json.dumps(value).replace('"value": 0', '"value": 1e999')
        with self.assertRaises(FormatError) as caught:
            parse_manifest(source, self.root)
        self.assertEqual(caught.exception.code, "json")

    def test_paths_cannot_escape_root(self):
        for source in ("../a.xpm", "/a.xpm", "C:/a.xpm", "a\\b.xpm", "./a.xpm", "a.png",
                       "NUL.xpm", "sub/COM1.xpm", "LPT9.backup.xpm", "con .xpm",
                       "folder./a.xpm", "folder /a.xpm", "a\x00.xpm", "a\x1f.xpm",
                       "a\x7f.xpm", "a\u0085.xpm", "bad*/a.xpm"):
            with self.subTest(source=source):
                with self.assertRaises(FormatError) as caught:
                    self.parse(self.manifest({"source": source, "duration_ms": 1}))
                self.assertEqual(caught.exception.code, "source_path")
        outside = self.root.parent / "outside.xpm"
        try:
            (self.root / "link.xpm").symlink_to(outside)
        except OSError:
            pass
        else:
            with self.assertRaises(FormatError) as caught:
                self.parse(self.manifest({"source": "link.xpm", "duration_ms": 1}))
            self.assertEqual(caught.exception.code, "source_path")
        with patch("ckpixel_format.Path.resolve", side_effect=OSError("resolve failed")):
            with self.assertRaises(FormatError) as caught:
                self.parse(self.manifest())
        self.assertEqual(caught.exception.code, "source_path")

    def test_duration_anchor_canvas_and_aggregate_limits(self):
        invalid_frames = [
            ({"source": "a.xpm", "duration_ms": 0}, "duration"),
            ({"source": "a.xpm", "duration_ms": 86400001}, "duration"),
            ({"source": "a.xpm", "duration_ms": True}, "duration"),
            ({"source": "a.xpm", "duration_ms": 1, "anchors": {"x": [2, 0]}}, "schema"),
        ]
        for frame, code in invalid_frames:
            with self.subTest(frame=frame):
                with self.assertRaises(FormatError) as caught:
                    self.parse(self.manifest(frame))
                self.assertEqual(caught.exception.code, code)
        value = self.manifest()
        value["canvas"]["width"] = 1
        with self.assertRaises(FormatError) as caught:
            self.parse(value)
        self.assertEqual(caught.exception.code, "canvas")
        value = self.manifest()
        value["animations"]["idle"]["frames"] *= 4097
        with self.assertRaises(FormatError) as caught:
            self.parse(value)
        self.assertEqual(caught.exception.code, "limits")


class TimingTests(unittest.TestCase):
    def setUp(self):
        sprite = Sprite(1, 1, (), ())
        self.frames = tuple(Frame("a.xpm", duration, sprite) for duration in (100, 200, 300))

    def test_loop_once_pingpong_half_open_and_negative(self):
        loop = Animation("loop", self.frames)
        self.assertEqual([evaluate_frame(loop, t).active_index for t in (-1, 0, 99, 100, 299, 300, 599, 600)],
                         [2, 0, 0, 1, 1, 2, 2, 0])
        once = Animation("once", self.frames)
        self.assertEqual(evaluate_frame(once, -5).active_index, 0)
        self.assertEqual(evaluate_frame(once, 600).active_index, 2)
        pingpong = Animation("ping-pong", self.frames)
        self.assertEqual([evaluate_frame(pingpong, t).active_index for t in (0, 100, 300, 600, 800)],
                         [0, 1, 2, 1, 0])

    def test_sampling_decimation_and_empty(self):
        loop = Animation("loop", self.frames)
        self.assertEqual(evaluate_frame(loop, 110, sample_ms=100).active_index, 1)
        self.assertEqual(evaluate_frame(loop, 305, sample_ms=100).active_index, 2)
        self.assertEqual(evaluate_frame(loop, 305, decimation=2).selected_index, 2)
        self.assertEqual(evaluate_frame(loop, 150, decimation=2).selected_index, 0)
        self.assertIsNone(evaluate_frame(Animation("loop", ()), 0))
        for t in (float("nan"), float("inf")):
            with self.assertRaises(ValueError):
                evaluate_frame(loop, t)
        with self.assertRaises(ValueError):
            evaluate_frame(loop, 10 ** 10000)
        self.assertIsNotNone(evaluate_frame(loop, 1e308, sample_ms=1e-308))
        with self.assertRaises(ValueError):
            evaluate_frame(loop, -1.7e308, sample_ms=1e308)
        with self.assertRaises(ValueError):
            evaluate_frame(Animation("loop", self.frames * 1366), 0)
        oversized = Frame("a.xpm", 86400001, self.frames[0].sprite)
        with self.assertRaises(ValueError):
            evaluate_frame(Animation("loop", (oversized,)), 0)

    def test_decimal_boundaries_remain_half_open(self):
        sprite = self.frames[0].sprite
        frames = tuple(Frame("a.xpm", duration, sprite) for duration in (0.1, 0.2, 0.3))
        loop = Animation("loop", frames)
        self.assertEqual(evaluate_frame(loop, 0.1).active_index, 1)
        self.assertEqual(evaluate_frame(loop, 0.3).active_index, 2)
        self.assertEqual(evaluate_frame(loop, 0.6).active_index, 0)
        negative_near_zero = evaluate_frame(loop, -1e-17)
        self.assertEqual(negative_near_zero.active_index, 2)
        self.assertLess(negative_near_zero.elapsed_ms, frames[2].duration_ms)
        once = Animation("once", frames)
        clamped = evaluate_frame(once, 100)
        self.assertEqual(clamped.active_index, 2)
        self.assertLess(clamped.elapsed_ms, frames[2].duration_ms)


if __name__ == "__main__":
    unittest.main()
