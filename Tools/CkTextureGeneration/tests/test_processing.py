import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from pack_flipbook import pack, sha256, write_png
from blender_validate import decode_native_png


class Processing(unittest.TestCase):
    def make_frames(self, root, dtype):
        frames_dir = root / 'frames'
        frames_dir.mkdir()
        rng = np.random.default_rng(7924)
        frames = []
        for index in range(4):
            frame = rng.integers(0, np.iinfo(dtype).max + 1, size=(3, 5, 4), dtype=dtype)
            frame[0, :, 3] = 0
            frames.append(frame)
            write_png(frames_dir / f'{index:03}.png', frame)
        return frames_dir, frames

    def test_native_precision_tile_order_and_opacity(self):
        for dtype in [np.uint8, np.uint16]:
            with self.subTest(dtype=dtype), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                source, frames = self.make_frames(root, dtype)
                original = {p.name: sha256(p) for p in source.iterdir()}
                output = root / 'packed'
                receipt = pack(source, output, 24, 2, 2)
                atlas = decode_native_png(output / 'atlas_rgba.png')
                opacity = decode_native_png(output / 'atlas_opacity.png')
                self.assertEqual(atlas.dtype, dtype)
                for i, frame in enumerate(frames):
                    y, x = i // 2 * 3, i % 2 * 5
                    np.testing.assert_array_equal(atlas[y:y + 3, x:x + 5], frame)
                    np.testing.assert_array_equal(opacity[y:y + 3, x:x + 5, 0], frame[:, :, 3])
                self.assertEqual(receipt['verified_tiles'], 4)
                self.assertEqual(original, {p.name: sha256(p) for p in source.iterdir()})

    def test_preview_omits_atlas(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, _ = self.make_frames(root, np.uint16)
            pack(source, root / 'preview', 8, 2, 2, preview_only=True)
            self.assertTrue((root / 'preview/preview.gif').is_file())
            self.assertFalse((root / 'preview/atlas_rgba.png').exists())

    def test_existing_destination_and_invalid_grid_reject_before_write(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, _ = self.make_frames(root, np.uint8)
            with self.assertRaises(ValueError):
                pack(source, root / 'invalid', 24, 3, 2)
            self.assertFalse((root / 'invalid').exists())
            existing = root / 'existing'
            existing.mkdir()
            sentinel = existing / 'keep.txt'
            sentinel.write_text('accepted')
            with self.assertRaises(ValueError):
                pack(source, existing, 24, 2, 2)
            self.assertEqual(sentinel.read_text(), 'accepted')

    def test_mismatched_frames_and_corrupt_input_do_not_create_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, _ = self.make_frames(root, np.uint8)
            write_png(source / '003.png', np.zeros((2, 2, 4), dtype=np.uint8))
            with self.assertRaises(ValueError):
                pack(source, root / 'mismatch', 8, 2, 2)
            self.assertFalse((root / 'mismatch').exists())
            (source / '003.png').write_bytes(b'not a PNG')
            with self.assertRaises(ValueError):
                pack(source, root / 'corrupt', 8, 2, 2)
            self.assertFalse((root / 'corrupt').exists())


if __name__ == '__main__':
    unittest.main()
