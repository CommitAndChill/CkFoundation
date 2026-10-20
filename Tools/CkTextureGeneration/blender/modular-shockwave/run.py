"""Run the accepted layered composition into a new absolute external directory.

Preflight performs no Blender execution, render, bake or output-directory creation.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def configuration(path):
    config = json.loads(path.read_text(encoding='utf-8-sig'))
    bounds = {'seed': (1, 2147483639), 'radius_scale': (.8, 1.1),
              'streak_spread_degrees': (3, 15), 'streak_width_multiplier': (1.2, 2.5),
              'billow_weight': (.5, 1.5), 'debris_count': (5, 35),
              'motion_speed': (.8, 1.25)}
    for name, (low, high) in bounds.items():
        value = config.get(name)
        if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError('Invalid configuration value: ' + name)
    for name in ('seed', 'debris_count', 'hero_debris_count'):
        if type(config.get(name)) is not int:
            raise ValueError('Integer required: ' + name)
    if not 0 <= config['hero_debris_count'] <= config['debris_count']:
        raise ValueError('Invalid hero_debris_count')
    return config


def main():
    # All generated content belongs in --output, including import caches.
    os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--blender', required=True, type=Path, help='Explicit Blender executable')
    parser.add_argument('--python', required=True, type=Path, help='Explicit Python with numpy and Pillow')
    parser.add_argument('--output', required=True, type=Path, help='New absolute output outside plugin source')
    parser.add_argument('--config', type=Path)
    parser.add_argument('--preflight', action='store_true', help='Validate only; no Blender launch or output writes')
    stages = parser.add_mutually_exclusive_group()
    stages.add_argument('--pack', action='store_true', help='Explicit authorization to generate and pack full output')
    stages.add_argument('--finalize', action='store_true', help='Finalize exact existing preview inputs; never regenerate layers')
    args = parser.parse_args()
    output = args.output.resolve()
    if not args.output.is_absolute() or output.is_relative_to(ROOT.parents[1]):
        raise ValueError('Output must be an absolute directory outside plugin source')
    if args.finalize:
        if not output.is_dir():
            raise ValueError('Finalization requires an existing preview run')
        if any((output / name).exists() for name in ('review', 'normals', 'verification')):
            raise ValueError('Occupied final output preserved; finalization refused')
    elif output.exists():
        raise ValueError('Existing output preserved; choose a new external directory')
    for name, path in (('Blender', args.blender), ('Python', args.python)):
        if not path.is_absolute() or not path.is_file():
            raise ValueError('Supply an existing absolute ' + name + ' executable')
    config = configuration(output / 'recipe.json' if args.finalize else (args.config or HERE / 'variation.json'))
    if args.finalize and args.config and configuration(args.config) != config:
        raise ValueError('Finalization configuration differs from the recorded preview')
    inputs = json.loads((HERE / 'inputs/manifest.json').read_text())
    for item in inputs:
        path = HERE / 'inputs' / item['staged']
        if not path.is_file() or digest(path) != item['sha256']:
            raise ValueError('Missing/changed accepted source art: ' + item['staged'])
    scripts = ('streaks.py', 'soil.py', 'billows.py', 'normals.py', 'normal_review.py',
               'verify_sources.py', 'pack.py', 'node_helpers.py', 'pack_helpers.py')
    for name in scripts:
        compile((HERE / name).read_text(), str(HERE / name), 'exec')
    if not (ROOT / 'blender_validate.py').is_file():
        raise FileNotFoundError('Package-root blender_validate.py is required')
    subprocess.run([str(args.python), '-c', 'import numpy; from PIL import Image'], check=True)
    # Images are bundled accepted source art; verify decode/layout with the chosen host Python.
    subprocess.run([str(args.python), '-c',
                    'from PIL import Image; import sys; '
                    'images=[Image.open(p) for p in sys.argv[1:]]; '
                    'assert all(i.size==(1024,1024) and i.mode=="RGBA" for i in images)',
                    *(str(HERE / 'inputs' / item['staged']) for item in inputs)], check=True)
    if args.finalize:
        subprocess.run([str(args.python), str(HERE / 'pack.py'), '--run', str(output), '--validate-preview'], check=True)
    if args.preflight:
        print(json.dumps({'preflight': 'passed', 'seed': config['seed'], 'output_created': False,
                          'render_executed': False, 'input_hashes_verified': len(inputs)}, indent=2))
        return
    if not args.finalize:
        output.mkdir(parents=True, exist_ok=False)
        (output / 'recipe.json').write_text(json.dumps(config, indent=2) + '\n')
    before = {item['staged']: item['sha256'] for item in inputs}
    receipt = {'schema': 1, 'stages': [], 'accepted_inputs': before, 'runtime_validation': 'not performed',
               'mode': 'finalize' if args.finalize else ('pack' if args.pack else 'preview')}
    commands = []
    for layer in (() if args.finalize else ('streaks', 'soil', 'billows')):
        commands.append((layer, [str(args.blender), '--background', '--factory-startup', '--threads', '8',
            '--python-exit-code', '1', '--python', str(HERE / (layer + '.py')), '--',
            '--config', str(output / 'recipe.json'), '--out', str(output / layer)]))
    commands.append(('pack' if args.pack or args.finalize else 'preview',
                     [str(args.python), str(HERE / 'pack.py'), '--run', str(output)] +
                     ([] if args.pack or args.finalize else ['--preview-only'])))
    for stage, script in ((('normals', 'normals.py'), ('source-replay', 'verify_sources.py')) if args.pack or args.finalize else ()):
        commands.append((stage, [str(args.blender), '--background', '--factory-startup', '--threads', '8',
            '--python-exit-code', '1', '--python', str(HERE / script), '--', '--run', str(output)]))
    if args.pack or args.finalize:
        commands.append(('normal-export', [str(args.python), str(HERE / 'normal_review.py'), '--run', str(output), '--no-video']))
        commands.append(('verify', [str(args.python), str(HERE / 'pack.py'), '--run', str(output), '--verify-replays']))
    execution_path = output / ('finalization.json' if args.finalize else 'execution.json')
    for stage, command in commands:
        with (output / (stage + '.log')).open('w', encoding='utf-8') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        receipt['stages'].append({'stage': stage, 'exit_code': result.returncode})
        execution_path.write_text(json.dumps(receipt, indent=2))
        if result.returncode:
            raise RuntimeError(stage + ' failed; partial output/logs preserved')
    if any(digest(HERE / 'inputs' / name) != value for name, value in before.items()):
        raise RuntimeError('Bundled accepted source art changed during execution')
    receipt['source_art_unchanged'] = True
    receipt['source_scripts'] = {name: digest(HERE / name) for name in scripts}
    execution_path.write_text(json.dumps(receipt, indent=2))
    print(str(output / ('review' if args.pack or args.finalize else 'preview') / 'index.html'))


if __name__ == '__main__':
    main()
