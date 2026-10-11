"""Command-line inspection and lossless PNG export for CkPixel source files."""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys
import tempfile
import zlib
from pathlib import Path

from ckpixel_format import FormatError, load_manifest, load_xpm

MAX_PIXELS = 16_777_216
MIN_PYTHON = (3, 12)


class JsonArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise ValueError(message)


def emit(value: dict, *, error: bool = False) -> None:
    print(json.dumps(value, ensure_ascii=False, separators=(",", ":")), file=sys.stderr if error else sys.stdout)


def sprite_data(path: Path) -> dict:
    return load_xpm(path).to_dict()


def rgba_pixels(sprite: dict) -> bytes:
    colors = [entry["rgba"] for entry in sprite["palette"]]
    return bytes(channel for index in sprite["indices"] for channel in colors[index])


def dimensions(sprite: dict) -> tuple[int, int]:
    return sprite["width"], sprite["height"]


def scaled_rgba(sprite: dict, scale: int) -> tuple[int, int, bytes]:
    width, height = dimensions(sprite)
    if not 1 <= scale <= 32:
        raise ValueError("scale must be an integer from 1 to 32")
    output_width, output_height = width * scale, height * scale
    if output_width * output_height > MAX_PIXELS:
        raise ValueError(f"output exceeds {MAX_PIXELS} pixels")
    source = rgba_pixels(sprite)
    rows = []
    for y in range(height):
        row = b"".join(source[(y * width + x) * 4:(y * width + x + 1) * 4] * scale for x in range(width))
        rows.extend([row] * scale)
    return output_width, output_height, b"".join(rows)


def png_chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def png_bytes(width: int, height: int, pixels: bytes) -> bytes:
    stride = width * 4
    raw = b"".join(b"\0" + pixels[y * stride:(y + 1) * stride] for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b""))


def manifest_sources(path: Path, manifest) -> list[Path]:
    return [path, *(path.parent / frame.source for animation in manifest.animations.values()
                    for frame in animation.frames)]


def preflight_output(path: Path, protected: list[Path]) -> None:
    current = path
    while True:
        if current.is_symlink() or getattr(current, "is_junction", lambda: False)():
            raise ValueError(f"linked output path is refused: {current}")
        if current == current.parent:
            break
        current = current.parent
    resolved = path.resolve()
    if any(resolved == source.resolve() for source in protected):
        raise ValueError(f"output would overwrite source: {path}")
    if path.exists():
        if not path.is_file():
            raise ValueError(f"output is not a regular file: {path}")
        if path.stat().st_nlink > 1:
            raise ValueError(f"hard-linked output is refused: {path}")
        if any(path.samefile(source) for source in protected):
            raise ValueError(f"output would overwrite source: {path}")


def sibling_temp(path: Path) -> Path:
    descriptor, name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    os.close(descriptor)
    return Path(name)


def publish_outputs(outputs: dict[Path, bytes], protected: list[Path]) -> None:
    if sys.version_info < MIN_PYTHON:
        raise ValueError("CkPixel CLI requires Python 3.12 or newer")
    targets = list(outputs)
    if any(path.suffix.lower() != suffix for path, suffix in zip(targets, (".png", ".json"))):
        raise ValueError("export paths must end in .png and optional .json")
    for target in targets:
        preflight_output(target, protected)
    for target in targets:
        target.parent.mkdir(parents=True, exist_ok=True)
        preflight_output(target, protected)
    staged: dict[Path, Path] = {}
    backups: dict[Path, Path] = {}
    published: set[Path] = set()
    committed = False
    try:
        for target, data in outputs.items():
            staged[target] = sibling_temp(target)
            staged[target].write_bytes(data)
        for target in targets:
            preflight_output(target, protected)
            if target.exists():
                backup = sibling_temp(target)
                try:
                    os.replace(target, backup)
                except OSError:
                    backup.unlink(missing_ok=True)
                    raise
                backups[target] = backup
        for target in targets:
            os.replace(staged[target], target)
            published.add(target)
        committed = True
    except Exception as error:
        rollback_errors = []
        for target in reversed(targets):
            try:
                if target in backups:
                    os.replace(backups[target], target)
                elif target in published:
                    target.unlink()
            except OSError as rollback_error:
                rollback_errors.append(str(rollback_error))
        if rollback_errors:
            raise OSError(f"export failed; rollback incomplete: {'; '.join(rollback_errors)}") from error
        raise
    finally:
        for temporary in staged.values():
            temporary.unlink(missing_ok=True)
        if committed:
            for temporary in backups.values():
                temporary.unlink(missing_ok=True)


def is_manifest(path: Path) -> bool:
    return path.suffix.lower() == ".json"


def command_validate(args: argparse.Namespace) -> dict:
    source = Path(args.source)
    if is_manifest(source):
        manifest = load_manifest(source)
        return {"ok": True, "type": "manifest", "source": str(source), "canvas": [manifest.canvas_width, manifest.canvas_height],
                "animations": {name: len(animation.frames) for name, animation in manifest.animations.items()}}
    sprite = sprite_data(source)
    return {"ok": True, "type": "sprite", "source": str(source), "canvas": list(dimensions(sprite)),
            "colors": len(sprite["palette"])}


def command_inspect(args: argparse.Namespace) -> dict:
    source = Path(args.source)
    data = load_manifest(source).to_dict() if is_manifest(source) else sprite_data(source)
    return {"ok": True, "source": str(source), "type": "manifest" if is_manifest(source) else "sprite", "data": data}


def command_grid(args: argparse.Namespace) -> dict:
    sprite = sprite_data(Path(args.source))
    width, height = dimensions(sprite)
    symbols = [color["symbol"] for color in sprite["palette"]]
    rows = [[symbols[index] for index in sprite["indices"][y * width:(y + 1) * width]] for y in range(height)]
    return {"ok": True, "source": args.source, "canvas": [width, height], "rows": rows}


def command_render(args: argparse.Namespace) -> dict:
    source = Path(args.source)
    if is_manifest(source):
        if args.animation is None:
            raise ValueError("--animation is required when rendering a manifest")
        manifest = load_manifest(source)
        animation = manifest.animations.get(args.animation)
        if animation is None:
            raise ValueError(f"unknown animation: {args.animation}")
        if args.frame < 0 or args.frame >= len(animation.frames):
            raise ValueError(f"frame index out of range: {args.frame}")
        sprite = animation.frames[args.frame].sprite.to_dict()
        protected = manifest_sources(source, manifest)
    else:
        if args.animation is not None or args.frame != 0:
            raise ValueError("--animation and --frame require a manifest")
        sprite = sprite_data(source)
        protected = [source]
    width, height, pixels = scaled_rgba(sprite, args.scale)
    publish_outputs({Path(args.output): png_bytes(width, height, pixels)}, protected)
    return {"ok": True, "source": str(source), "output": args.output, "canvas": [width, height]}


def command_diff(args: argparse.Namespace) -> dict:
    first = sprite_data(Path(args.first))
    second = sprite_data(Path(args.second))
    if dimensions(first) != dimensions(second):
        raise ValueError(f"canvas mismatch: {dimensions(first)} versus {dimensions(second)}")
    width, height = dimensions(first)
    a, b = rgba_pixels(first), rgba_pixels(second)
    changed = [i for i in range(width * height) if a[i * 4:i * 4 + 4] != b[i * 4:i * 4 + 4]]
    bbox = None
    if changed:
        xs, ys = [i % width for i in changed], [i // width for i in changed]
        bbox = [min(xs), min(ys), max(xs) + 1, max(ys) + 1]
    return {"ok": True, "first": args.first, "second": args.second, "changed_pixels": len(changed), "bbox": bbox}


def command_sheet(args: argparse.Namespace) -> dict:
    source = Path(args.source)
    manifest = load_manifest(source)
    animation = manifest.animations.get(args.animation)
    if animation is None:
        raise ValueError(f"unknown animation: {args.animation}")
    if not animation.frames:
        raise ValueError("animation has no frames")
    width, height = manifest.canvas_width, manifest.canvas_height
    count = len(animation.frames)
    if not 1 <= args.scale <= 32:
        raise ValueError("scale must be an integer from 1 to 32")
    output_width, output_height = width * args.scale * count, height * args.scale
    if output_width * output_height > MAX_PIXELS:
        raise ValueError(f"output exceeds {MAX_PIXELS} pixels")
    frame_pixels = []
    entries = []
    for i, frame in enumerate(animation.frames):
        sprite = frame.sprite.to_dict()
        if dimensions(sprite) != (width, height):
            raise ValueError(f"frame {i} canvas mismatch")
        _, _, pixels = scaled_rgba(sprite, args.scale)
        frame_pixels.append(pixels)
        entries.append({"index": i, "source": str(frame.source), "duration_ms": frame.duration_ms,
                        "rect": [i * width * args.scale, 0, width * args.scale, height * args.scale],
                        "source_rect": [0, 0, width, height],
                        "anchors": {key: list(value) for key, value in frame.anchors.items()}})
    stride = width * args.scale * 4
    sheet_pixels = b"".join(b"".join(pixels[y * stride:(y + 1) * stride] for pixels in frame_pixels)
                            for y in range(output_height))
    output = Path(args.output)
    sidecar = output.with_suffix(".json")
    sheet = {"version": 1, "animation": args.animation, "playback": animation.playback, "scale": args.scale,
             "canvas": [output_width, output_height], "frames": entries}
    publish_outputs({output: png_bytes(output_width, output_height, sheet_pixels),
                     sidecar: (json.dumps(sheet, indent=2) + "\n").encode("utf-8")},
                    manifest_sources(source, manifest))
    return {"ok": True, "source": args.source, "output": str(output), "sidecar": str(sidecar), "frames": count,
            "canvas": [output_width, output_height]}


def make_parser() -> argparse.ArgumentParser:
    parser = JsonArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True, parser_class=JsonArgumentParser)
    for name, action in (("validate", command_validate), ("inspect", command_inspect), ("grid", command_grid)):
        command = sub.add_parser(name)
        command.add_argument("source")
        command.set_defaults(action=action)
    render = sub.add_parser("render")
    render.add_argument("source")
    render.add_argument("output")
    render.add_argument("--animation")
    render.add_argument("--frame", type=int, default=0)
    render.add_argument("--scale", type=int, default=1)
    render.set_defaults(action=command_render)
    diff = sub.add_parser("diff")
    diff.add_argument("first")
    diff.add_argument("second")
    diff.set_defaults(action=command_diff)
    sheet = sub.add_parser("sheet")
    sheet.add_argument("source")
    sheet.add_argument("output")
    sheet.add_argument("--animation", required=True)
    sheet.add_argument("--scale", type=int, default=1)
    sheet.set_defaults(action=command_sheet)
    return parser


def main(argv: list[str] | None = None) -> int:
    if sys.version_info < MIN_PYTHON:
        emit({"ok": False, "code": "python_version", "error": "CkPixel CLI requires Python 3.12 or newer"}, error=True)
        return 1
    parser = make_parser()
    try:
        args = parser.parse_args(argv)
        emit(args.action(args))
        return 0
    except (FormatError, OSError, ValueError, KeyError, IndexError) as error:
        diagnostic = {"ok": False, "error": str(error), "code": getattr(error, "code", type(error).__name__)}
        for key in ("path", "line", "column"):
            value = getattr(error, key, None)
            if value is not None:
                diagnostic[key] = str(value) if key == "path" else value
        emit(diagnostic, error=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
