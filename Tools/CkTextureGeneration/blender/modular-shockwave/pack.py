"""Portable native-frame packing and review; no historical comparison dependency."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from blender_validate import decode_native_png
from pack_helpers import identity, save_native, downsample, rgba8, record, monotonic

SOURCES = {'streaks': 'streaks', 'billows': 'billows/opacity',
           'billows_lit': 'billows/lit', 'soil': 'soil'}
TINTS = {'streaks': (196, 173, 135), 'billows_lit': (204, 181, 143), 'soil': (161, 130, 89)}


def preview_inputs(run):
    paths = [run / 'recipe.json']
    for layer in ('streaks', 'billows', 'soil'):
        folder = run / layer
        required = [folder / 'receipt.json', folder / 'source.blend']
        if layer == 'billows':
            required += [folder / 'source_lit.blend', folder / 'static_opacity_1024.png', folder / 'static_lit_1024.png']
        else:
            required += [folder / 'static_1024.png']
        if not all(p.is_file() for p in required):
            raise ValueError('Missing native source/receipt/static: ' + layer)
        receipt = json.loads((folder / 'receipt.json').read_text())
        if receipt.get('frames') != 32 or receipt.get('fps') != 16:
            raise ValueError('Unexpected native receipt timing: ' + layer)
        paths.extend(p for p in folder.rglob('*') if p.is_file())
    return {p.relative_to(run).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}


def validate_preview(run):
    manifest = json.loads((run / 'preview/manifest.json').read_text())
    if manifest.get('native_inputs') != preview_inputs(run):
        raise ValueError('Preview frames, recipe, source scenes or receipts changed; finalization refused')
    for name, value in manifest['preview_frames'].items():
        path = run / 'preview' / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != value:
            raise ValueError('Recorded preview changed: ' + name)
    if len(manifest['preview_frames']) != 32:
        raise ValueError('Expected 32 recorded preview frames')


def display(image):
    return Image.alpha_composite(Image.new('RGBA', image.size, (72, 68, 57, 255)), image).convert('RGB')


def tinted(image, rgb):
    a = np.array(image)
    a[..., :3] = np.rint(a[..., :3].astype(float) * np.array(rgb) / 255).astype('uint8')
    return Image.fromarray(a)


def verify_replays(run):
    receipt = json.loads((run / 'verification/receipt.json').read_text())
    checks = []
    for item in receipt:
        source = run / SOURCES[item['layer']] / ('frame_%02d.png' % item['index'])
        replay = Path(item['output'])
        if not np.array_equal(decode_native_png(source), decode_native_png(replay)):
            raise ValueError('Saved-source replay differs: ' + item['layer'])
        checks.append({'layer': item['layer'], 'index': item['index'], 'decoded_exact': True})
    if len(checks) != 8:
        raise ValueError('Expected eight source replay checks')
    normal_export = run / 'normals/export'
    alignment = []
    for name in ('streaks', 'billows', 'soil'):
        game_alpha = np.asarray(Image.open(run / 'review' / name / 'game_atlas_4x8.png').getchannel('A'))
        normal_alpha = np.asarray(Image.open(normal_export / name / 'matching_opacity_atlas.png'))
        if not np.array_equal(game_alpha, normal_alpha):
            raise ValueError('Normal atlas opacity differs from game atlas: ' + name)
        alignment.append({'layer': name, 'game_opacity_exact': True})
    (run / 'source-replay-validation.json').write_text(json.dumps(checks, indent=2))
    (run / 'normal-game-alignment.json').write_text(json.dumps(alignment, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', required=True, type=Path)
    parser.add_argument('--verify-replays', action='store_true')
    parser.add_argument('--preview-only', action='store_true', help='Motion review only; no atlases or normals')
    parser.add_argument('--validate-preview', action='store_true', help='Read-only immutable-input gate for finalization')
    args = parser.parse_args()
    run = args.run.resolve()
    if args.validate_preview:
        validate_preview(run)
        print('Recorded preview inputs unchanged')
        return
    if args.verify_replays:
        verify_replays(run)
        return
    # Resolve all sources before creating output. Individual empty terminal frames are legitimate.
    paths = {name: [run / folder / ('frame_%02d.png' % i) for i in range(32)] for name, folder in SOURCES.items()}
    for name, items in paths.items():
        if not all(path.is_file() for path in items) or set((run / SOURCES[name]).glob('frame_*.png')) != set(items):
            raise ValueError('Incomplete/unexpected native frame set: ' + name)
    snapshot = preview_inputs(run)
    out = run / ('preview' if args.preview_only else 'review')
    out.mkdir(exist_ok=False)
    report = {'frames': 32, 'fps': 16, 'duration_seconds': 2, 'layers': {},
              'atlas': '4 columns x 8 rows, top-left row-major, 256px tiles, no gutters',
              'provenance': 'Native streak cards and soil meshes; composed accepted CFD imagery. No new radial solve.',
              'preview': 'Review-only tint/background; native PNG and scalar precision retained',
              'acceptance': 'New output pending visual review; runtime and performance unverified'}
    frames = {}
    for name, items in paths.items():
        dest = out / name
        if not args.preview_only:
            dest.mkdir()
        records, smalls, frames[name] = [], [], []
        for i, path in enumerate(items):
            a, info = record(path, i, 512)
            if info['border_max'] != 0:
                raise ValueError('Nontransparent border: ' + str(path))
            records.append(info)
            if not args.preview_only:
                smalls.append(downsample(a))
            frames[name].append(rgba8(a))
        if args.preview_only:
            report['layers'][name] = {'frames': records, 'atlas': 'deferred until authorized'}
            continue
        atlas = np.zeros((2048, 1024, 4), dtype=smalls[0].dtype)
        game_atlas = Image.new('RGBA', (1024, 2048))
        for i, small in enumerate(smalls):
            x, y = i % 4 * 256, i // 4 * 256
            atlas[y:y+256, x:x+256] = small
            # Match inherited normal-export opacity support: high-byte RGBA conversion, then Lanczos.
            game_atlas.paste(frames[name][i].resize((256, 256), Image.Resampling.LANCZOS), (x, y))
        save_native(dest / 'native_precision_atlas_4x8.png', atlas)
        game_atlas.save(dest / 'game_atlas_4x8.png')
        decoded = decode_native_png(dest / 'native_precision_atlas_4x8.png')
        for i, small in enumerate(smalls):
            x, y = i % 4 * 256, i // 4 * 256
            if not np.array_equal(decoded[y:y+256, x:x+256], small):
                raise ValueError('Atlas tile mismatch')
        report['layers'][name] = {'frames': records, 'tile_parity': True,
                                'precision_bits': atlas.dtype.itemsize * 8}
    for name in ('streaks', 'soil', 'billows'):
        if np.asarray(frames[name][-1])[..., 3].any():
            raise ValueError('Expected transparent final frame: ' + name)
    for key in ('alpha_mean', 'brightness_mean'):
        result = monotonic(report['layers']['billows']['frames'], key)
        if not result['nonincreasing_after_peak']:
            raise ValueError('Billow rebound: ' + key)
        report['layers']['billows'][key + '_decay'] = result
    previews = []
    for i in range(32):
        layers = [tinted(frames[name][i], TINTS[name]) for name in ('streaks', 'billows_lit', 'soil')]
        combined = Image.new('RGBA', (512, 512))
        for layer in layers:
            combined = Image.alpha_composite(combined, layer)
        panel = Image.new('RGB', (1024, 282), (24, 27, 31))
        draw = ImageDraw.Draw(panel)
        for j, (label, image) in enumerate(zip(('Streaks', 'Billows', 'Soil', 'Combined'), [*layers, combined])):
            panel.paste(display(image).resize((256, 256)), (j*256, 26))
            draw.text((j*256+8, 6), label, fill='white')
        panel.save(out / ('layers_%02d.png' % i))
        previews.append(panel)
    # Browser timer is an authoring review at requested cadence, not gameplay/runtime evidence.
    normal_link = '' if args.preview_only else '<p><a href="../normals/export/verified-preview/normal-and-lighting.png">Exported normal lighting check</a></p>'
    html = '<!doctype html><meta charset="utf-8"><title>Modular shockwave review</title><style>body{background:#181b1f;color:white;font:16px sans-serif}img{max-width:100%}</style><h1>Layers and combined output</h1><p>32 frames, 16 fps, two-second one-shot. Review tint/background only, using Pillow encoded-color tint and alpha compositing; this is display media, not linear-light material data. Native PNG masters retained. Unreal runtime/performance unverified.</p><button id="play">Replay</button><p id="time"></p><img id="frame" src="layers_00.png">' + normal_link + '<script>let i=0,t;function play(){clearInterval(t);i=0;document.getElementById("frame").src="layers_00.png";t=setInterval(()=>{i++;if(i>=32){clearInterval(t);return;}document.getElementById("frame").src="layers_"+String(i).padStart(2,"0")+".png";document.getElementById("time").textContent=(i/16).toFixed(3)+" s";},1000/16);}document.getElementById("play").onclick=play;play();</script>'
    (out / 'index.html').write_text(html)
    (out / 'validation.json').write_text(json.dumps(report, indent=2))
    if args.preview_only:
        if snapshot != preview_inputs(run):
            raise ValueError('Native inputs changed while preparing preview')
        manifest = {'native_inputs': snapshot, 'preview_frames': {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('layers_*.png')}}
        (out / 'manifest.json').write_text(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
