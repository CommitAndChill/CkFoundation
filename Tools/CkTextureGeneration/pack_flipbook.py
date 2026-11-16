"""Pack an existing RGBA sequence without changing its decoded source samples."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

import numpy as np
from PIL import Image

from blender_validate import decode_native_png


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_png(path, samples):
    """Write native integer channels; Pillow truncates multichannel PNG16 on load."""
    height, width, channels = samples.shape
    depth = samples.dtype.itemsize * 8
    color = {1: 0, 2: 4, 3: 2, 4: 6}[channels]
    raw = samples.astype('>u2' if depth == 16 else 'u1').tobytes()
    stride = width * channels * (depth // 8)

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)

    rows = b''.join(b'\0' + raw[y * stride:(y + 1) * stride] for y in range(height))
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, depth, color, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
    Path(path).write_bytes(png)


def preview_image(samples, color_space):
    rgba = samples.astype(np.float64) / np.iinfo(samples.dtype).max
    rgb = rgba[:, :, :3]
    if color_space == 'srgb':
        rgb = np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)
    linear = rgb * rgba[:, :, 3:4] + .035 * (1 - rgba[:, :, 3:4])
    encoded = np.where(linear <= .0031308, linear * 12.92, 1.055 * linear ** (1 / 2.4) - .055)
    return Image.fromarray(np.uint8(np.clip(np.rint(encoded * 255), 0, 255)))


def pack(input_dir, output, fps, columns, rows, preview_only=False, color_space='srgb', pattern='*.png'):
    source = Path(input_dir).resolve(strict=True)
    destination = Path(output).resolve()
    plugin = Path(__file__).resolve().parents[2]
    if not source.is_dir() or destination.is_relative_to(plugin) or destination.is_relative_to(source):
        raise ValueError('Use an input directory and a new output outside source/plugin trees')
    if destination.exists():
        raise ValueError('Existing output preserved; choose a new directory')
    if not 0 < fps <= 100 or columns <= 0 or rows <= 0:
        raise ValueError('Positive dimensions and a playback rate <=100 fps are required')
    paths = sorted(source.glob(pattern))
    if not paths or len(paths) != columns * rows:
        raise ValueError('Frame count must exactly match rows * columns; filenames sort chronologically')
    if any(not path.is_file() or path.is_symlink() for path in paths):
        raise ValueError('Frames must be ordinary files')
    frames = [decode_native_png(path) for path in paths]
    first = frames[0]
    if first.ndim != 3 or first.shape[2] != 4:
        raise ValueError('This packer requires RGBA PNG8/16 frames; opacity is source alpha')
    if any(frame.shape != first.shape or frame.dtype != first.dtype for frame in frames):
        raise ValueError('Frame dimensions, channels and precision must match')
    height, width, _ = first.shape
    if width * columns > 16384 or height * rows > 16384:
        raise ValueError('Atlas dimensions exceed 16384')
    # All input validation precedes output creation. Errors retain only this new run's evidence.
    destination.mkdir(parents=True, exist_ok=False)
    manifest = {
        'schema': 1, 'frames': len(frames), 'fps': fps, 'seconds': len(frames) / fps,
        'layout': {'columns': columns, 'rows': rows, 'tile_size': [width, height], 'order': 'top-left row-major'},
        'source_bits': first.dtype.itemsize * 8, 'alpha': 'linear opacity; straight RGBA',
        'color_space': color_space, 'conversion': 'none for PNG exports; display conversion only for previews',
        'preview_only': preview_only,
        'inputs': [{'file': path.name, 'sha256': sha256(path)} for path in paths],
    }
    if not preview_only:
        atlas = np.zeros((height * rows, width * columns, 4), dtype=first.dtype)
        for index, frame in enumerate(frames):
            x, y = index % columns * width, index // columns * height
            atlas[y:y + height, x:x + width] = frame
        write_png(destination / 'atlas_rgba.png', atlas)
        write_png(destination / 'atlas_opacity.png', atlas[:, :, 3:4])
        decoded = decode_native_png(destination / 'atlas_rgba.png')
        assert all(np.array_equal(decoded[(i // columns) * height:(i // columns + 1) * height,
                                           (i % columns) * width:(i % columns + 1) * width], frame)
                   for i, frame in enumerate(frames)), 'Packed source mismatch'
        assert np.array_equal(decode_native_png(destination / 'atlas_opacity.png'), atlas[:, :, 3:4])
        manifest['verified_tiles'] = len(frames)
    preview = [preview_image(frame, color_space) for frame in frames]
    durations = [round((i + 1) * 100 / fps) * 10 - round(i * 100 / fps) * 10 for i in range(len(frames))]
    preview[0].save(destination / 'preview.gif', save_all=True, append_images=preview[1:], duration=durations, loop=0, disposal=2)
    contact = Image.new('RGB', (min(width, 256) * columns, min(height, 256) * rows))
    for i, image in enumerate(preview):
        image = image.resize((min(width, 256), min(height, 256)))
        contact.paste(image, ((i % columns) * image.width, (i // columns) * image.height))
    contact.save(destination / 'contact.png')
    manifest['preview_note'] = 'GIF repeats for review; this does not establish a seamless loop; timing quantized to centiseconds'
    manifest['output_hashes'] = {path.name: sha256(path) for path in destination.iterdir() if path.is_file()}
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fps', type=float, required=True)
    parser.add_argument('--columns', type=int, required=True)
    parser.add_argument('--rows', type=int, required=True)
    parser.add_argument('--pattern', default='*.png')
    parser.add_argument('--color-space', choices=['srgb', 'linear'], default='srgb')
    parser.add_argument('--preview-only', action='store_true')
    args = parser.parse_args()
    try:
        result = pack(args.input, args.output, args.fps, args.columns, args.rows, args.preview_only, args.color_space, args.pattern)
    except (ValueError, OSError) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
