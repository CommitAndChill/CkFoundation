"""Pack fresh native normal data; relight actual exported DX PNGs for review.

python normal_review.py --run PATH [--no-video]
No normal synthesis: only matched-white reconstruction, vector normalization,
alpha-weighted vector reduction, encoding, composition, and review lighting.
"""
import argparse
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from blender_validate import decode_native_png

LAYERS = ('streaks', 'billows', 'soil')
NEUTRAL = np.array([128, 128, 255], dtype=np.uint8)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def samples(path):
    values = decode_native_png(path)
    return values.astype(float) / np.iinfo(values.dtype).max


def unit(normal):
    length = np.linalg.norm(normal, axis=-1, keepdims=True)
    return np.divide(normal, length, out=np.broadcast_to([0., 0., 1.], normal.shape).copy(),
                     where=length > 1e-8)


def recover(native, white, mask):
    if not np.array_equal(native[..., 3], white[..., 3]):
        raise AssertionError('Matched normal/white alpha differs')
    if not (np.array_equal(white[..., 0], white[..., 1]) and
            np.array_equal(white[..., 1], white[..., 2])):
        raise AssertionError('White RGB reference is not achromatic')
    weighted = 2 * native[..., :3] - white[..., :3]
    visible = mask > 0
    length = np.linalg.norm(weighted, axis=-1)
    if not (np.all(white[..., 0][visible] > 0) and np.all(length[visible] > 0)):
        raise AssertionError('Missing native normal signal on fresh beauty support')
    result = unit(weighted)
    if not np.isfinite(result).all() or np.any(result[..., 2][visible] < -.001):
        raise AssertionError('Invalid or back-facing reconstructed normal')
    return result


def encode(normal, mask):
    gl = np.rint(np.clip((unit(normal) * .5 + .5) * 255, 0, 255)).astype('uint8')
    gl[mask == 0] = NEUTRAL
    dx = gl.copy()
    dx[..., 1] = 255 - gl[..., 1]
    dx[mask == 0] = NEUTRAL
    visible = mask > 0
    if not np.all(dx[..., 1][visible] == 255 - gl[..., 1][visible]):
        raise AssertionError('DX/GL green parity failed')
    for array in (gl, dx):
        if not np.all(array[~visible] == NEUTRAL):
            raise AssertionError('Non-neutral padding')
        error = np.abs(np.linalg.norm(array.astype(float) / 255 * 2 - 1, axis=-1) - 1)
        if visible.any() and error[visible].max() > math.sqrt(3) / 255 + 1e-9:
            raise AssertionError('Encoded vector length exceeds quantization tolerance')
    return gl, dx


def reduce_vectors(normal, alpha):
    weighted = normal * alpha[..., None]
    reduced = np.stack([np.asarray(Image.fromarray(weighted[..., i].astype('float32')).resize(
        (256, 256), Image.Resampling.BOX)) for i in range(3)], axis=-1)
    return unit(reduced)


def over(fields):
    weighted = np.zeros_like(fields[0][0])
    alpha = np.zeros_like(fields[0][1])
    for normal, opacity in fields:
        weighted = weighted * (1 - opacity[..., None]) + normal * opacity[..., None]
        alpha = alpha * (1 - opacity) + opacity
    return unit(weighted), alpha


def source_paths(run, name):
    if name == 'billows':
        return run / name / 'opacity', run / name / 'static_opacity_1024.png'
    return run / name, run / name / 'static_1024.png'


def reconstruct(normal_path, white_path, beauty_path, alpha_out):
    original = decode_native_png(beauty_path)
    normal_raw = decode_native_png(normal_path)
    white_raw = decode_native_png(white_path)
    if original.shape != normal_raw.shape or original.shape != white_raw.shape:
        raise AssertionError('Normal/reference/beauty dimensions differ')
    if normal_raw.dtype != np.uint16 or white_raw.dtype != np.uint16:
        raise AssertionError('Expected native 16-bit data render')
    precision = np.iinfo(original.dtype).max
    beauty = original.astype(float) / precision
    native = normal_raw.astype(float) / 65535
    white = white_raw.astype(float) / 65535
    mask = np.asarray(Image.open(beauty_path).getchannel('A'))
    normal = recover(native, white, mask)
    delta = int(np.abs(np.rint(native[..., 3] * precision).astype('int32') -
                       original[..., 3].astype('int32')).max())
    # RGBA8 beauty and RGBA16 normals use different native quantization. This is
    # a precision-aware tolerance, never an exact-bit cross-precision claim.
    if delta > 1:
        raise AssertionError(f'Native alpha exceeds one original sample: {beauty_path}: {delta}')
    Image.fromarray(normal_raw[..., 3]).save(alpha_out)
    if not np.array_equal(decode_native_png(alpha_out)[..., 0], normal_raw[..., 3]):
        raise AssertionError('Lossless native alpha extraction failed')
    metric = dict(beauty_sha256=sha(beauty_path), normal_sha256=sha(normal_path),
                  white_sha256=sha(white_path), beauty_precision_bits=original.dtype.itemsize * 8,
                  original_precision_alpha_max_sample_delta=delta,
                  matched_normal_white_alpha_exact=True,
                  exported_support_exact_beauty_alpha=True)
    return normal, beauty[..., 3], mask, metric


def lit_image(normal, alpha, theta, soil):
    light = np.array([.72 * math.cos(theta), .72 * math.sin(theta), .694])
    light /= np.linalg.norm(light)
    intensity = .22 + .78 * np.maximum(0, (normal * light).sum(-1))
    base = np.broadcast_to(np.array([.35, .25, .13]), normal.shape).copy()
    base = base * (1 - soil[..., None]) + np.array([.12, .075, .035]) * soil[..., None]
    foreground = base * intensity[..., None]
    background = (np.array([72, 68, 57], float) / 255) ** 2.2
    rgb = np.clip(foreground * alpha[..., None] + background * (1 - alpha[..., None]), 0, 1) ** (1 / 2.2)
    return Image.fromarray(np.rint(rgb * 255).astype('uint8'))


def video(folder, target, fps):
    subprocess.run(['ffmpeg', '-n', '-loglevel', 'error', '-framerate', str(fps), '-i',
                    str(folder / 'frame_%02d.png'), '-c:v', 'libx264', '-crf', '17',
                    '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(target)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--no-video', action='store_true')
    args = parser.parse_args()
    run = args.run.resolve()
    native_root = run / 'normals'
    receipt = json.loads((native_root / 'receipt.json').read_text())
    selected = receipt['selected_frame']
    if receipt['frames'] != 32:
        raise ValueError('Expected 32 native frames')
    # Preflight every prerequisite before creating any processed output.
    for name in LAYERS:
        beauty_folder, static = source_paths(run, name)
        expected = [static, native_root / name / 'normal_1024.png',
                    native_root / name / 'white_1024.png']
        for i in range(32):
            expected.extend((beauty_folder / f'frame_{i:02d}.png',
                             native_root / name / 'normal' / f'frame_{i:02d}.png',
                             native_root / name / 'white' / f'frame_{i:02d}.png'))
        if not all(path.is_file() for path in expected):
            raise FileNotFoundError('Incomplete native/beauty input for ' + name)
    out = native_root / 'export'
    out.mkdir()
    data, statics, masks256, metrics = {}, {}, {}, {}
    for name in LAYERS:
        beauty_folder, static = source_paths(run, name)
        (out / name / 'alpha16').mkdir(parents=True)
        for space in ('dx', 'gl'):
            (out / name / space / 'frames').mkdir(parents=True)
        data[name], masks256[name], metrics[name] = [], [], []
        for i in range(32):
            normal, alpha, mask, metric = reconstruct(
                native_root / name / 'normal' / f'frame_{i:02d}.png',
                native_root / name / 'white' / f'frame_{i:02d}.png',
                beauty_folder / f'frame_{i:02d}.png',
                out / name / 'alpha16' / f'frame_{i:02d}.png')
            for space, array in zip(('gl', 'dx'), encode(normal, mask)):
                Image.fromarray(array).save(out / name / space / 'frames' / f'frame_{i:02d}.png')
            # Match the beauty packer's exact RGBA Lanczos resize, including
            # Pillow alpha handling, then extract the downsampled support.
            with Image.open(beauty_folder / f'frame_{i:02d}.png') as image:
                masks256[name].append(np.array(image.convert('RGBA').resize(
                    (256, 256), Image.Resampling.LANCZOS).getchannel('A')))
            data[name].append((normal, alpha, mask))
            metrics[name].append(dict(frame=i, **metric))
        normal, alpha, mask, metric = reconstruct(
            native_root / name / 'normal_1024.png', native_root / name / 'white_1024.png',
            static, out / name / 'alpha16' / 'static_1024.png')
        if normal.shape != (1024, 1024, 3):
            raise AssertionError('Expected 1024px native static')
        statics[name] = (normal, alpha, mask)
        metrics[name].append(dict(frame=selected, static=True, **metric))
    data['combined'], masks256['combined'] = [], []
    for i in range(32):
        normal, alpha = over([(data[name][i][0], data[name][i][1]) for name in LAYERS])
        data['combined'].append((normal, alpha, np.rint(alpha * 255).astype('uint8')))
        alpha256 = np.zeros((256, 256))
        for name in LAYERS:
            layer_alpha = masks256[name][i].astype(float) / 255
            alpha256 = alpha256 * (1 - layer_alpha) + layer_alpha
        masks256['combined'].append(np.rint(alpha256 * 255).astype('uint8'))
    normal, alpha = over([(statics[name][0], statics[name][1]) for name in LAYERS])
    statics['combined'] = (normal, alpha, np.rint(alpha * 255).astype('uint8'))
    for name, frames in data.items():
        if name == 'combined':
            for space in ('dx', 'gl'):
                (out / name / space / 'frames').mkdir(parents=True)
        atlas = {space: Image.new('RGB', (1024, 2048), (128, 128, 255)) for space in ('gl', 'dx')}
        opacity_atlas = Image.new('L', (1024, 2048))
        for i, (normal, alpha, mask) in enumerate(frames):
            if name == 'combined':
                for space, array in zip(('gl', 'dx'), encode(normal, mask)):
                    Image.fromarray(array).save(out / name / space / 'frames' / f'frame_{i:02d}.png')
            mask256 = masks256[name][i]
            for space, array in zip(('gl', 'dx'), encode(reduce_vectors(normal, alpha), mask256)):
                tile = Image.fromarray(array)
                tile.save(out / name / space / f'tile_{i:02d}_256.png')
                atlas[space].paste(tile, (i % 4 * 256, i // 4 * 256))
            opacity_atlas.paste(Image.fromarray(mask256), (i % 4 * 256, i // 4 * 256))
        for space, image in atlas.items():
            image.save(out / name / space / 'atlas_4x8.png')
            for i in range(32):
                tile = Image.open(out / name / space / f'tile_{i:02d}_256.png')
                x, y = i % 4 * 256, i // 4 * 256
                if not np.array_equal(np.array(image.crop((x, y, x + 256, y + 256))), np.array(tile)):
                    raise AssertionError('Atlas tile placement mismatch')
        opacity_atlas.save(out / name / 'matching_opacity_atlas.png')
        normal, alpha, mask = statics[name]
        for space, array in zip(('gl', 'dx'), encode(normal, mask)):
            Image.fromarray(array).save(out / name / space / f'normal_1024_frame{selected:02d}.png')
        Image.fromarray(np.rint(alpha * 65535).astype('uint16')).save(out / name / 'opacity_1024.png')
    preview = out / 'verified-preview'
    for folder in ('animated', 'rotating'):
        (preview / folder).mkdir(parents=True)
    # Read final quantized DX files back from disk, correcting green for the
    # +Y-up Lambert shader. Never relight the unquantized working normals.
    actual_fields = []
    for i in range(32):
        normal = np.array(Image.open(out / 'combined' / 'dx' / 'frames' / f'frame_{i:02d}.png')).astype(float) / 255 * 2 - 1
        normal[..., 1] *= -1
        normal = unit(normal)
        alpha, soil = data['combined'][i][1], data['soil'][i][1]
        actual_fields.append((normal, alpha, soil))
        lit_image(normal, alpha, math.radians(40), soil).save(preview / 'animated' / f'frame_{i:02d}.png')
    normal, alpha, soil = actual_fields[selected]
    for i in range(48):
        lit_image(normal, alpha, 2 * math.pi * i / 48, soil).save(preview / 'rotating' / f'frame_{i:02d}.png')
    panel = Image.new('RGB', (1280, 350), (24, 27, 31))
    draw = ImageDraw.Draw(panel)
    panels = [('Exported DirectX normal', Image.open(out / 'combined' / 'dx' / 'frames' / f'frame_{selected:02d}.png')),
              ('Light from left', lit_image(normal, alpha, math.pi, soil)),
              ('Light from right', lit_image(normal, alpha, 0, soil)),
              ('Light from above', lit_image(normal, alpha, math.pi / 2, soil))]
    for i, (label, image) in enumerate(panels):
        panel.paste(image.resize((320, 320), Image.Resampling.LANCZOS), (i * 320, 28))
        draw.text((i * 320 + 10, 8), label, fill='white')
    panel.save(preview / 'normal-and-lighting.png')
    if not args.no_video:
        video(preview / 'animated', preview / 'animated-relight.mp4', 16)
        video(preview / 'rotating', preview / 'rotating-light.mp4', 24)
    result = dict(frames=32, fps=16, selected_frame=selected,
                  atlas='4 columns x 8 rows; 256px tiles; 1024x2048; row-major from top left',
                  primary='DirectX RGB normals; OpenGL references supplied; sRGB off',
                  basis='+X image right,+Y image up,+Z toward viewer before DX green inversion',
                  native_method='Blender density-height bump relief for dust; actual mesh facets for soil',
                  reconstruction='normalize(2*Cnormal-Cwhite); exact native matched alpha',
                  alpha_policy='Native alpha requantized to beauty precision differs by at most one sample. Exact beauty support controls RGB padding.',
                  alpha16_reference='Lossless extraction of native normal render alpha; white alpha exact',
                  layer_metrics=metrics, neutral_padding=[128, 128, 255],
                  dx_gl_green_complement_on_visible=True, atlas_tile_parity=True,
                  unit_length_tolerance=math.sqrt(3) / 255,
                  preview_source='Actual exported 8-bit DX PNG frames, green-corrected; CPU Lambert',
                  limits=['Density relief approximation, not physical fluid surface.',
                          'No Unreal/Niagara runtime validation.',
                          'Native sign calibration inherited from accepted normals-r04 algorithm; no fresh calibration claim.'])
    (out / 'validation.json').write_text(json.dumps(result, indent=2))
    print(json.dumps({key: value for key, value in result.items() if key != 'layer_metrics'}, indent=2))


if __name__ == '__main__':
    main()
