"""Filesystem and recipe validation; imports no Blender or GPU runtime."""
import argparse
import json
import math
from pathlib import Path


def validate_config(path):
    data = json.loads(Path(path).read_text(encoding='utf-8-sig'))
    if not isinstance(data, dict):
        raise ValueError('Config must be an object')
    controls = {'seed': (1, 2147483646), 'cell_size': (.04, .15),
                'turbulence_amplitude': (0, 40), 'source_width': (.5, 1.5),
                'upward_speed': (.5, 1.5)}
    unknown = set(data) - set(controls) - {'schema', 'description'}
    if unknown:
        raise ValueError(f'Unknown controls: {sorted(unknown)}')
    if type(data.get('schema')) is not int or data['schema'] != 1:
        raise ValueError('Config schema must be 1')
    for name, (low, high) in controls.items():
        value = data.get(name)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError(f'{name} must be finite in [{low}, {high}]')
    if not isinstance(data['seed'], int):
        raise ValueError('Seed must be an integer')
    return data


def validate_destination(path):
    original = Path(path)
    if not original.is_absolute():
        raise ValueError('Output path must be absolute')
    destination = original.resolve()
    if destination.exists():
        raise FileExistsError(f'Existing destination preserved: {destination}')
    source = Path(__file__).resolve().parent
    protected = [source.parents[3]]
    if any(destination.is_relative_to(root) for root in protected):
        raise ValueError('Output must be outside the plugin source tree')
    for parent in destination.parents:
        if parent.exists():
            if not parent.is_dir():
                raise ValueError(f'Output ancestor is not a directory: {parent}')
            break
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gpu', required=True)
    parser.add_argument('--platform-index', type=int)
    args = parser.parse_args()
    if not args.gpu.strip():
        parser.error('--gpu must be a nonempty device-name substring')
    if args.platform_index is not None and args.platform_index < 0:
        parser.error('--platform-index must be nonnegative')
    validate_config(args.config)
    validate_destination(args.output)
    print('SMOKE_PATH_CONFIG_PREFLIGHT_PASS (GPU/add-on availability requires Blender execution)')
