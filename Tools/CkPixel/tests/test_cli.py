"""Exercise the published commands against actual source and exported PNG files."""

from contextlib import redirect_stderr
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib


ROOT = Path(__file__).resolve().parents[1]
CLI = ROOT / "ckpixel.py"
ROBOT = ROOT / "Examples" / "Robot.xpm"
CHARACTER = ROOT / "Examples" / "Character" / "character.json"


def call(*args):
    result = subprocess.run([sys.executable, str(CLI), *map(str, args)], text=True, capture_output=True)
    stream = result.stdout if result.returncode == 0 else result.stderr
    return result.returncode, json.loads(stream.strip().splitlines()[-1])


def decode_png(path):
    data = path.read_bytes()
    assert data.startswith(b"\x89PNG\r\n\x1a\n")
    cursor, packed, width, height = 8, bytearray(), None, None
    while cursor < len(data):
        count = struct.unpack_from(">I", data, cursor)[0]
        name = data[cursor + 4:cursor + 8]
        payload = data[cursor + 8:cursor + 8 + count]
        assert struct.unpack_from(">I", data, cursor + 8 + count)[0] == zlib.crc32(name + payload) & 0xFFFFFFFF
        cursor += 12 + count
        if name == b"IHDR":
            width, height, depth, color, _, _, _ = struct.unpack(">IIBBBBB", payload)
            assert (depth, color) == (8, 6)
        elif name == b"IDAT":
            packed.extend(payload)
        elif name == b"IEND":
            break
    raw = zlib.decompress(packed)
    rows = []
    for y in range(height):
        start = y * (width * 4 + 1)
        assert raw[start] == 0
        rows.append(raw[start + 1:start + 1 + width * 4])
    return width, height, b"".join(rows)


def tiny(palette, row):
    return '/* XPM */\nstatic char * sprite[] = {\n"2 1 2 1",\n' + ",\n".join(f'"{line}"' for line in [*palette, row]) + "\n};\n"


class CliTests(unittest.TestCase):
    def test_requires_python_312_before_source_access(self):
        sys.path.insert(0, str(ROOT))
        import ckpixel

        diagnostic = io.StringIO()
        with mock.patch.object(ckpixel.sys, "version_info", (3, 11, 0)), redirect_stderr(diagnostic):
            code = ckpixel.main(["validate", str(ROBOT)])
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(diagnostic.getvalue())["code"], "python_version")

    def test_examples_validate_and_inspect(self):
        for path in (ROBOT, CHARACTER):
            code, result = call("validate", path)
            self.assertEqual(code, 0, result)
            self.assertEqual(result["canvas"], [16, 16])
        code, result = call("inspect", ROBOT)
        self.assertEqual(code, 0, result)
        self.assertEqual(len(result["data"]["indices"]), 256)
        code, grid = call("grid", ROBOT)
        self.assertEqual(code, 0, grid)
        self.assertEqual(len(grid["rows"]), 16)
        self.assertEqual(len(grid["rows"][0]), 16)

    def test_render_rgba_scale_and_no_source_mutation(self):
        before = hashlib.sha256(ROBOT.read_bytes()).digest()
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            output = Path(folder) / "robot.png"
            code, result = call("render", ROBOT, output, "--scale", "2")
            self.assertEqual(code, 0, result)
            width, height, pixels = decode_png(output)
            self.assertEqual((width, height), (32, 32))
            self.assertEqual(pixels[0:4], bytes((0, 0, 0, 0)))
            antenna = pixels[14 * 4:15 * 4]
            self.assertEqual(antenna, bytes((255, 227, 122, 255)))
            self.assertEqual(pixels[15 * 4:16 * 4], antenna)
            self.assertEqual(pixels[(32 + 14) * 4:(32 + 15) * 4], antenna)
        self.assertEqual(hashlib.sha256(ROBOT.read_bytes()).digest(), before)

    def test_sheet_order_timing_anchors_and_rgba(self):
        sources = [CHARACTER, *sorted(CHARACTER.parent.glob("*.xpm"))]
        before = {path: hashlib.sha256(path.read_bytes()).digest() for path in sources}
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            output = Path(folder) / "walk.png"
            code, result = call("sheet", CHARACTER, output, "--animation", "walk", "--scale", "2")
            self.assertEqual(code, 0, result)
            width, height, pixels = decode_png(output)
            self.assertEqual((width, height), (128, 32))
            self.assertEqual(len(pixels), width * height * 4)
            sheet = json.loads(output.with_suffix(".json").read_text(encoding="utf-8"))
            self.assertEqual([entry["source"] for entry in sheet["frames"]],
                             [f"walk_{index}.xpm" for index in range(1, 5)])
            self.assertEqual([entry["duration_ms"] for entry in sheet["frames"]], [110, 95, 110, 95])
            self.assertEqual(sheet["frames"][1]["rect"], [32, 0, 32, 32])
            self.assertEqual(sheet["frames"][1]["source_rect"], [0, 0, 16, 16])
            self.assertEqual(sheet["frames"][1]["anchors"], {"origin": [8, 15]})
        self.assertEqual({path: hashlib.sha256(path.read_bytes()).digest() for path in sources}, before)

    def test_diff_compares_colors_and_reports_exclusive_bounds(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            left = Path(folder) / "left.xpm"
            right = Path(folder) / "right.xpm"
            left.write_text(tiny(["a c #f00", "b c None"], "ab"), encoding="utf-8")
            right.write_text(tiny(["b c None", "a c #f00"], "ab"), encoding="utf-8")
            code, result = call("diff", left, right)
            self.assertEqual(code, 0, result)
            self.assertEqual(result["changed_pixels"], 0)
            self.assertIsNone(result["bbox"])
            right.write_text(tiny(["b c None", "a c #0f0"], "ab"), encoding="utf-8")
            code, result = call("diff", left, right)
            self.assertEqual(code, 0, result)
            self.assertEqual(result["changed_pixels"], 1)
            self.assertEqual(result["bbox"], [0, 0, 1, 1])
            right.write_text('/* XPM */\nstatic char * sprite[] = {\n"1 1 1 1",\n"a c #f00",\n"a"\n};\n', encoding="utf-8")
            code, result = call("diff", left, right)
            self.assertNotEqual(code, 0)
            self.assertIn("canvas mismatch", result["error"])

    def test_failures_are_json_and_nonzero(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            output = Path(folder) / "bad.png"
            code, result = call("render", ROBOT, output, "--scale", "33")
            self.assertNotEqual(code, 0)
            self.assertFalse(result["ok"])
            self.assertFalse(output.exists())
            code, result = call("sheet", CHARACTER, output, "--animation", "missing")
            self.assertNotEqual(code, 0)
            self.assertIn("unknown animation", result["error"])
            code, result = call("sheet", CHARACTER, CHARACTER.with_suffix(".png"), "--animation", "walk")
            self.assertNotEqual(code, 0)
            self.assertIn("overwrite", result["error"])
            code, result = call("render", ROBOT, output, "--scale", "bad")
            self.assertNotEqual(code, 0)
            self.assertFalse(result["ok"])
            bad = Path(folder) / "bad.xpm"
            bad.write_text("not an XPM", encoding="utf-8")
            code, result = call("validate", bad)
            self.assertNotEqual(code, 0)
            self.assertIn("line", result)
            self.assertIn("column", result)
            layered = Path(folder) / "layers.json"
            layered.write_text(json.dumps({"version": 1, "canvas": {"width": 1, "height": 1},
                                           "animations": {}, "layers": []}), encoding="utf-8")
            code, result = call("validate", layered)
            self.assertNotEqual(code, 0)
            self.assertEqual(result["code"], "layers_unsupported")

    def test_render_rejects_source_named_png_and_linked_destinations(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            directory = Path(folder)
            source = directory / "source.png"
            source.write_bytes(ROBOT.read_bytes())
            original = source.read_bytes()
            code, result = call("render", source, source)
            self.assertNotEqual(code, 0)
            self.assertIn("overwrite source", result["error"])
            self.assertEqual(source.read_bytes(), original)
            linked = directory / "linked.png"
            try:
                os.link(source, linked)
            except OSError:
                pass
            else:
                code, result = call("render", source, linked)
                self.assertNotEqual(code, 0)
                self.assertIn("linked output", result["error"])
                self.assertEqual(source.read_bytes(), original)
            symbolic = directory / "symbolic.png"
            try:
                os.symlink(source, symbolic)
            except OSError:
                pass
            else:
                code, result = call("render", source, symbolic)
                self.assertNotEqual(code, 0)
                self.assertIn("linked output", result["error"])
                self.assertEqual(source.read_bytes(), original)

    def test_sheet_refuses_linked_frame_sidecar_and_preflights_both_outputs(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            directory = Path(folder)
            source = directory / "frame.xpm"
            source.write_text(tiny(["a c #f00", "b c None"], "ab"), encoding="utf-8")
            manifest = directory / "clip.json"
            manifest.write_text(json.dumps({"version": 1, "canvas": {"width": 2, "height": 1},
                                            "animations": {"idle": {"playback": "loop", "frames": [
                                                {"source": "frame.xpm", "duration_ms": 100}]}}}), encoding="utf-8")
            output = directory / "sheet.png"
            sidecar = output.with_suffix(".json")
            original = source.read_bytes()
            try:
                os.link(source, sidecar)
            except OSError:
                pass
            else:
                code, result = call("sheet", manifest, output, "--animation", "idle")
                self.assertNotEqual(code, 0)
                self.assertIn("linked output", result["error"])
                self.assertFalse(output.exists())
                self.assertEqual(source.read_bytes(), original)
                sidecar.unlink()
            output.write_bytes(b"previous PNG")
            sidecar.mkdir()
            code, result = call("sheet", manifest, output, "--animation", "idle")
            self.assertNotEqual(code, 0)
            self.assertEqual(output.read_bytes(), b"previous PNG")
            self.assertTrue(sidecar.is_dir())

    def test_sheet_restores_existing_pair_if_sidecar_publish_fails(self):
        sys.path.insert(0, str(ROOT))
        import ckpixel

        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            directory = Path(folder)
            output = directory / "sheet.png"
            sidecar = directory / "sheet.json"
            output.write_bytes(b"previous PNG")
            sidecar.write_bytes(b"previous JSON")
            real_replace = os.replace
            failed = False

            def fail_sidecar(source, destination):
                nonlocal failed
                if Path(destination) == sidecar and not failed:
                    failed = True
                    raise PermissionError("sidecar publication blocked")
                return real_replace(source, destination)

            with mock.patch.object(ckpixel.os, "replace", side_effect=fail_sidecar):
                with self.assertRaises(PermissionError):
                    ckpixel.publish_outputs({output: b"new PNG", sidecar: b"new JSON"}, [ROBOT])
            self.assertEqual(output.read_bytes(), b"previous PNG")
            self.assertEqual(sidecar.read_bytes(), b"previous JSON")
            self.assertEqual(list(directory.glob(".*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
