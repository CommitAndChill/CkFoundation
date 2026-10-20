"""Accepted precision/packing helpers; no work at import."""
import hashlib, struct, zlib
import numpy as np
from PIL import Image
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from blender_validate import decode_native_png
N=32

def identity(path):
    return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'bytes': path.stat().st_size}

def save_native(path, a):
    """Lossless RGBA8/16 or grayscale8/16 PNG, preserving sample precision."""
    h, w = a.shape[:2]
    channels = 1 if a.ndim == 2 else a.shape[2]
    assert channels in (1, 4) and a.dtype in (np.uint8, np.uint16)
    bits = a.dtype.itemsize * 8
    raw = a.astype('>u2' if bits == 16 else 'u1').tobytes()
    stride = w * channels * (bits // 8)
    scan = b''.join(b'\0' + raw[y * stride:(y + 1) * stride] for y in range(h))
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
    header = struct.pack('>IIBBBBB', w, h, bits, 0 if channels == 1 else 6, 0, 0, 0)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(scan)) + chunk(b'IEND', b''))

def downsample(a):
    """Whole canvas LANCZOS, channelwise for PNG16; no aspect/crop changes."""
    if a.dtype == np.uint8:
        return np.asarray(Image.fromarray(a).resize((256, 256), Image.Resampling.LANCZOS))
    return np.stack([np.clip(np.rint(np.asarray(Image.fromarray(a[..., c].astype(np.float32))
                         .resize((256, 256), Image.Resampling.LANCZOS))), 0, 65535)
                     .astype(np.uint16) for c in range(4)], axis=-1)

def rgba8(a):
    return Image.fromarray((a >> 8).astype(np.uint8) if a.dtype == np.uint16 else a)

def record(path, index, size):
    a = decode_native_png(path)
    assert a.shape == (size, size, 4), (path, a.shape)
    maximum = np.iinfo(a.dtype).max
    alpha = a[..., 3].astype(float) / maximum
    border = np.concatenate((alpha[0], alpha[-1], alpha[:, 0], alpha[:, -1]))
    rec = dict(identity(path), frame=index, source_frame=int(path.stem.split('_')[-1]) if path.stem.startswith('frame_') else 12,
               sample_bits=a.dtype.itemsize * 8, alpha_mean=float(alpha.mean()), alpha_max=float(alpha.max()),
               border_max=float(border.max()), brightness_mean=float((a[..., :3].astype(float).mean(axis=2) / maximum * alpha).mean()))
    return a, rec

def monotonic(records, key):
    peak = max(range(N), key=lambda i: records[i][key])
    rebounds = [{'frame': i + 1, 'previous': records[i][key], 'current': records[i + 1][key]}
                for i in range(peak, N - 1) if records[i + 1][key] > records[i][key]]
    return {'peak_frame': peak, 'peak_value': records[peak][key],
            'nonincreasing_after_peak': not rebounds, 'rebounds': rebounds}
