"""Fixture-only staged workflow test. Never invokes Blender or accepted sources.

Run: python -m unittest discover -s blender/modular-shockwave/tests -v
Optional CK_TEXTURE_TEST_TEMP selects a writable parent for the temporary fixture.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np
from PIL import Image

RECIPE = Path(__file__).resolve().parents[1]


class StageContractTest(unittest.TestCase):
    def invoke(self, script, *arguments, succeeds=True):
        environment = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
        result = subprocess.run([sys.executable, str(RECIPE / script), *map(str, arguments)],
                                capture_output=True, text=True, env=environment)
        self.assertEqual(result.returncode == 0, succeeds, result.stdout + result.stderr)
        return result

    def create_fixture(self, run):
        (run / 'recipe.json').write_bytes((RECIPE / 'variation.json').read_bytes())
        canvas = np.zeros((512, 512, 4), dtype='uint8')
        canvas[250:255, 250:255, :3] = 200
        for layer in ('streaks', 'billows', 'soil'):
            folder = run / layer
            folder.mkdir()
            # Deliberately invalid scene fixtures: this test must never launch a renderer.
            (folder / 'source.blend').write_bytes(b'FIXTURE ONLY; not a Blender scene')
            (folder / 'receipt.json').write_text(json.dumps({'frames': 32, 'fps': 16}))
            if layer == 'billows':
                (folder / 'source_lit.blend').write_bytes(b'FIXTURE ONLY; not a Blender scene')
                targets = [folder / 'opacity', folder / 'lit']
                for target in targets:
                    target.mkdir()
                statics = ['static_opacity_1024.png', 'static_lit_1024.png']
            else:
                targets = [folder]
                statics = ['static_1024.png']
            for index in range(32):
                canvas[250:255, 250:255, 3] = round(180 * (31 - index) / 31)
                for target in targets:
                    Image.fromarray(canvas).save(target / f'frame_{index:02d}.png')
            for name in statics:
                Image.new('RGBA', (1024, 1024)).save(folder / name)

    def test_preview_immutable_inputs_and_existing_frame_pack(self):
        parent = os.environ.get('CK_TEXTURE_TEST_TEMP')
        with tempfile.TemporaryDirectory(prefix='shockwave-stage-fixture-', dir=parent) as directory:
            run = Path(directory)
            self.create_fixture(run)
            self.invoke('pack.py', '--run', run, '--preview-only')
            self.assertTrue((run / 'preview/index.html').is_file())
            self.assertFalse((run / 'review').exists())
            self.assertFalse((run / 'normals').exists())
            self.assertFalse(list((run / 'preview').rglob('*atlas*')))
            self.assertNotIn('href="../normals', (run / 'preview/index.html').read_text())
            manifest = json.loads((run / 'preview/manifest.json').read_text())
            self.invoke('pack.py', '--run', run, '--validate-preview')

            # Python is only an existing executable stand-in for file preflight. No
            # successful full --finalize call is made, so no renderer can be invoked.
            launch = ['--python', sys.executable, '--blender', sys.executable,
                      '--output', run, '--finalize']
            self.invoke('run.py', *launch, '--preflight')
            self.assertFalse((run / 'review').exists())
            self.assertFalse((run / 'normals').exists())
            for relative, replacement in (
                ('soil/frame_00.png', b'altered frame'),
                ('recipe.json', json.dumps({**json.loads((run / 'recipe.json').read_text()), 'seed': 80424}).encode()),
            ):
                path = run / relative
                original = path.read_bytes()
                try:
                    path.write_bytes(replacement)
                    self.invoke('pack.py', '--run', run, '--validate-preview', succeeds=False)
                    result = self.invoke('run.py', *launch, succeeds=False)
                    self.assertIn('changed', result.stdout + result.stderr)
                    self.assertFalse((run / 'review').exists())
                    self.assertFalse((run / 'normals').exists())
                    self.assertFalse((run / 'finalization.json').exists())
                finally:
                    path.write_bytes(original)
            missing = run / 'soil/frame_00.png'
            original = missing.read_bytes()
            try:
                missing.unlink()
                self.invoke('run.py', *launch, succeeds=False)
                self.assertFalse((run / 'review').exists())
                self.assertFalse((run / 'normals').exists())
            finally:
                missing.write_bytes(original)
            self.invoke('run.py', *launch, '--preflight')

            # Pack only the already-recorded native fixtures. No render command runs.
            self.invoke('pack.py', '--run', run)
            self.assertTrue((run / 'review/soil/game_atlas_4x8.png').is_file())
            for relative, digest in manifest['native_inputs'].items():
                self.assertEqual(hashlib.sha256((run / relative).read_bytes()).hexdigest(), digest)
            self.invoke('run.py', *launch, '--preflight', succeeds=False)
            self.invoke('pack.py', '--run', run, succeeds=False)
            self.assertFalse((run / 'normals').exists())


if __name__ == '__main__':
    unittest.main()
