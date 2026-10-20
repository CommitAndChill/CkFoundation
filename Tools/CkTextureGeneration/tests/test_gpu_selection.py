import importlib.util
from pathlib import Path
import unittest

module_path = Path(__file__).resolve().parents[1] / 'blender/fresh-smoke/select_gpu.py'
spec = importlib.util.spec_from_file_location('texture_gpu_selection', module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class GpuSelection(unittest.TestCase):
    def setUp(self):
        self.devices = [{'platform_index': p, 'device_index': d, 'device_name': name}
                        for p in (0, 1) for d, name in enumerate(('gfx1036', 'gfx1201'))]

    def test_explicit_platform_preserves_full_list_slot(self):
        self.assertEqual(module.select_gpu_slot(self.devices, 'gfx1201', 0), 1)
        self.assertEqual(module.select_gpu_slot(self.devices, 'gfx1201', 1), 3)

    def test_ambiguous_missing_and_malformed_devices_rejected(self):
        for devices, token, platform in ((self.devices, 'gfx1201', None), (self.devices, 'RX 9070 XT', 0),
                                         (self.devices, 'gfx1201', 4), (self.devices, 'gfx1201', True),
                                         (self.devices, 'gfx1201', -1), ([], 'gfx1201', 0),
                                         ([{'device_name': 'gfx1201'}], 'gfx1201', 0)):
            with self.subTest(devices=devices, token=token, platform=platform), self.assertRaises(ValueError):
                module.select_gpu_slot(devices, token, platform)


if __name__ == '__main__':
    unittest.main()
