"""Bounded XPM3 and animation-manifest loading for pixel sprites."""

from __future__ import annotations

from array import array
from bisect import bisect_left
from dataclasses import dataclass, field
from decimal import Decimal, ROUND_FLOOR, localcontext
import json
import math
from pathlib import Path
import re
from typing import Any
import unicodedata


MAX_SOURCE_BYTES = 8 * 1024 * 1024
MAX_TOTAL_SOURCE_BYTES = 64 * 1024 * 1024
MAX_DIMENSION = 4096
MAX_PIXELS = 1048576
MAX_COLORS = 4096
MAX_CPP = 4
MAX_ANIMATIONS = 1024
MAX_FRAMES = 4096
MAX_FRAME_PIXELS = 16777216
MAX_DURATION_MS = 86400000
MAX_JSON_DEPTH = 64
MAX_JSON_NODES = 100000
_IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z_0-9]*")
_WINDOWS_DEVICE = re.compile(r"(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])", re.IGNORECASE)


class FormatError(ValueError):
    def __init__(self, path: str, line: int, column: int, code: str, detail: str):
        self.path, self.line, self.column, self.code = path, line, column, code
        super().__init__(f"{path}:{line}:{column}: {code}: {detail}")


@dataclass(frozen=True)
class PaletteEntry:
    symbol: str
    rgba: tuple[int, int, int, int]

    def to_dict(self) -> dict[str, Any]:
        return {"symbol": self.symbol, "rgba": list(self.rgba)}


@dataclass(frozen=True)
class Sprite:
    width: int
    height: int
    palette: tuple[PaletteEntry, ...]
    indices: tuple[int, ...]
    hotspot: tuple[int, int] | None = None

    def to_dict(self) -> dict[str, Any]:
        return {"width": self.width, "height": self.height,
                "palette": [p.to_dict() for p in self.palette],
                "indices": list(self.indices),
                "hotspot": list(self.hotspot) if self.hotspot is not None else None}


@dataclass(frozen=True)
class Frame:
    source: str
    duration_ms: float
    sprite: Sprite
    anchors: dict[str, tuple[int, int]] = field(default_factory=dict)
    events: Any = field(default_factory=list)
    metadata: Any = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {"source": self.source, "duration_ms": self.duration_ms,
                "anchors": {k: list(v) for k, v in self.anchors.items()},
                "events": self.events, "metadata": self.metadata,
                "sprite": self.sprite.to_dict()}


@dataclass(frozen=True)
class Animation:
    playback: str
    frames: tuple[Frame, ...]
    events: Any = field(default_factory=list)
    metadata: Any = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {"playback": self.playback, "frames": [f.to_dict() for f in self.frames],
                "events": self.events, "metadata": self.metadata}


@dataclass(frozen=True)
class Manifest:
    canvas_width: int
    canvas_height: int
    animations: dict[str, Animation]
    events: Any = field(default_factory=list)
    metadata: Any = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {"version": 1, "canvas": {"width": self.canvas_width,
                                           "height": self.canvas_height},
                "animations": {k: v.to_dict() for k, v in self.animations.items()},
                "events": self.events, "metadata": self.metadata}


@dataclass(frozen=True)
class FrameSelection:
    active_index: int
    selected_index: int
    frame: Frame
    elapsed_ms: float


def _decode(source: str | bytes, path: str, limit: int = MAX_SOURCE_BYTES) -> str:
    if isinstance(source, bytes):
        if len(source) > limit:
            raise FormatError(path, 1, 1, "source_limit", "source exceeds byte limit")
        try:
            return source.decode("utf-8-sig")
        except UnicodeDecodeError as error:
            raise FormatError(path, 1, 1, "utf8", "source is not UTF-8") from error
    if not isinstance(source, str):
        raise TypeError("source must be text or bytes")
    try:
        encoded = source.encode("utf-8")
    except UnicodeEncodeError as error:
        raise FormatError(path, 1, 1, "utf8", "source has invalid Unicode") from error
    if len(encoded) > limit:
        raise FormatError(path, 1, 1, "source_limit", "source exceeds byte limit")
    return source[1:] if source.startswith("\ufeff") else source


class _Lexer:
    def __init__(self, text: str, path: str):
        self.text, self.path, self.pos = text, path, 0
        self._newlines = array("I", (index for index, char in enumerate(text) if char == "\n"))

    def location(self, pos: int | None = None) -> tuple[int, int]:
        pos = self.pos if pos is None else pos
        preceding = bisect_left(self._newlines, pos)
        start = self._newlines[preceding - 1] if preceding else -1
        return preceding + 1, pos - start

    def fail(self, code: str, detail: str, pos: int | None = None) -> None:
        line, col = self.location(pos)
        raise FormatError(self.path, line, col, code, detail)

    def skip(self) -> None:
        text = self.text
        while self.pos < len(text):
            if text[self.pos].isspace():
                self.pos += 1
            elif text.startswith("/*", self.pos):
                end = text.find("*/", self.pos + 2)
                if end < 0:
                    self.fail("syntax", "unclosed comment")
                self.pos = end + 2
            elif text.startswith("//", self.pos):
                end = text.find("\n", self.pos + 2)
                self.pos = len(text) if end < 0 else end
            else:
                break

    def word(self) -> str:
        self.skip()
        match = _IDENTIFIER.match(self.text, self.pos)
        if not match:
            self.fail("syntax", "expected identifier")
        self.pos += len(match.group())
        return match.group()

    def punct(self, value: str) -> None:
        self.skip()
        if not self.text.startswith(value, self.pos):
            self.fail("syntax", f"expected {value!r}")
        self.pos += len(value)

    def literal(self) -> tuple[str, int]:
        self.skip()
        at = self.pos
        self.punct('"')
        out = bytearray()
        while self.pos < len(self.text):
            ch = self.text[self.pos]
            self.pos += 1
            if ch == '"':
                return out.decode("ascii"), at
            if ch in "\r\n":
                self.fail("character", "multiline string literal", self.pos - 1)
            if ch == "\\":
                if self.pos >= len(self.text):
                    self.fail("literal", "unclosed escape")
                esc = self.text[self.pos]
                self.pos += 1
                if esc in '\\"\'?':
                    value = ord(esc)
                elif esc in "01234567":
                    digits = esc
                    while len(digits) < 3 and self.pos < len(self.text) and self.text[self.pos] in "01234567":
                        digits += self.text[self.pos]
                        self.pos += 1
                    value = int(digits, 8)
                elif esc == "x":
                    start = self.pos
                    while self.pos < len(self.text) and self.text[self.pos] in "0123456789abcdefABCDEF":
                        self.pos += 1
                    if self.pos == start:
                        self.fail("escape", "hex escape requires digits", start)
                    digits = self.text[start:self.pos]
                    if len(digits) > 2 and any(ch != "0" for ch in digits[:-2]):
                        self.fail("escape", "hex escape exceeds one byte", start)
                    value = int(digits[-2:], 16)
                else:
                    self.fail("escape", "unsupported escape", self.pos - 1)
            else:
                value = ord(ch)
            if value < 32 or value > 126:
                self.fail("character", "data must contain printable ASCII bytes", self.pos - 1)
            out.append(value)
        self.fail("literal", "unclosed string literal", at)


def _xpm_lines(source: str | bytes, path: str) -> list[tuple[str, int]]:
    lex = _Lexer(_decode(source, path), path)
    word = lex.word()
    if word == "static":
        word = lex.word()
    if word == "const":
        word = lex.word()
    if word != "char":
        lex.fail("syntax", "expected char array declaration")
    lex.punct("*")
    identifier = lex.word()
    if identifier == "const":
        identifier = lex.word()
    if identifier in ("static", "const", "char"):
        lex.fail("syntax", "expected array identifier")
    lex.punct("[")
    lex.punct("]")
    lex.punct("=")
    lex.punct("{")
    lines: list[tuple[str, int]] = []
    while True:
        lex.skip()
        if lex.text.startswith("}", lex.pos):
            if not lines:
                lex.fail("syntax", "empty XPM array")
            lex.pos += 1
            break
        value, at = lex.literal()
        lines.append((value, at))
        if len(lines) > MAX_DIMENSION + MAX_COLORS + 1:
            lex.fail("limit", "too many XPM strings", at)
        lex.skip()
        if lex.text.startswith('"', lex.pos):
            lex.fail("syntax", "adjacent string literals are unsupported")
        if lex.text.startswith(",", lex.pos):
            lex.pos += 1
            continue
        if lex.text.startswith("}", lex.pos):
            lex.pos += 1
            break
        lex.fail("syntax", "expected comma or closing brace")
    lex.punct(";")
    lex.skip()
    if lex.pos != len(lex.text):
        lex.fail("syntax", "only one XPM array is allowed")
    located = []
    line = 1
    previous = 0
    for value, at in lines:
        line += lex.text.count("\n", previous, at)
        located.append((value, line))
        previous = at
    return located


_BASE_COLORS = {"black": (0, 0, 0), "white": (255, 255, 255),
                "red": (255, 0, 0), "green": (0, 255, 0),
                "blue": (0, 0, 255), "yellow": (255, 255, 0),
                "cyan": (0, 255, 255), "magenta": (255, 0, 255),
                "grey": (190, 190, 190), "gray": (190, 190, 190)}


def _color(value: str, path: str, line: int) -> tuple[int, int, int, int]:
    value = value.strip()
    lower = value.lower()
    if lower == "none":
        return 0, 0, 0, 0
    if value.startswith("#"):
        digits = value[1:]
        if len(digits) not in (3, 6, 9, 12) or not re.fullmatch(r"[0-9a-fA-F]+", digits):
            raise FormatError(path, line, 1, "color", "invalid hexadecimal color")
        width = len(digits) // 3
        maximum = (1 << (4 * width)) - 1
        channels = tuple((int(digits[i:i + width], 16) * 255 * 2 + maximum) // (2 * maximum)
                         for i in range(0, len(digits), width))
        return (*channels, 255)
    if lower in _BASE_COLORS:
        return (*_BASE_COLORS[lower], 255)
    gray = re.fullmatch(r"gr[ae]y([0-9]{1,10})", lower)
    if gray and int(gray.group(1)) <= 100:
        component = (int(gray.group(1)) * 255 * 2 + 100) // 200
        return component, component, component, 255
    raise FormatError(path, line, 1, "color", f"unsupported color {value!r}")


def parse_xpm(source: str | bytes, path: str = "<memory>") -> Sprite:
    lines = _xpm_lines(source, path)
    header = lines[0][0].split()
    if len(header) not in (4, 6) or any(not re.fullmatch(r"[0-9]+", x) for x in header):
        raise FormatError(path, lines[0][1], 1, "header", "expected four or six decimal integers")
    if any(len(x) > 10 or int(x) > 2147483647 for x in header):
        raise FormatError(path, lines[0][1], 1, "header", "header number exceeds supported range")
    width, height, color_count, cpp = map(int, header[:4])
    if not (1 <= width <= MAX_DIMENSION and 1 <= height <= MAX_DIMENSION and
            width * height <= MAX_PIXELS and 1 <= color_count <= MAX_COLORS and 1 <= cpp <= MAX_CPP):
        raise FormatError(path, lines[0][1], 1, "limit", "XPM dimensions, colors, or cpp exceed limits")
    hotspot = None
    if len(header) == 6:
        x, y = map(int, header[4:])
        if x >= width or y >= height:
            raise FormatError(path, lines[0][1], 1, "header", "hotspot outside image")
        hotspot = (x, y)
    if len(lines) != 1 + color_count + height:
        raise FormatError(path, lines[-1][1], 1, "rows", "palette or pixel row count differs from header; XPMEXT is unsupported")
    palette: list[PaletteEntry] = []
    lookup: dict[str, int] = {}
    for value, line in lines[1:1 + color_count]:
        symbol = value[:cpp]
        if len(symbol) != cpp or len(value) <= cpp or not value[cpp].isspace() or symbol in lookup:
            raise FormatError(path, line, 1, "palette", "missing or duplicate palette symbol")
        tokens = value[cpp:].split()
        selectors: dict[str, str] = {}
        current = None
        for token in tokens:
            if token in ("c", "m", "g", "g4", "s"):
                if current is not None and not selectors[current]:
                    raise FormatError(path, line, 1, "palette", "selector has no value")
                if token in selectors:
                    raise FormatError(path, line, 1, "palette", "repeated selector")
                current = token
                selectors[token] = ""
            elif current is None:
                raise FormatError(path, line, 1, "palette", "expected color selector")
            else:
                selectors[current] += (" " if selectors[current] else "") + token
        if current is not None and not selectors[current]:
            raise FormatError(path, line, 1, "palette", "selector has no value")
        if "c" not in selectors:
            raise FormatError(path, line, 1, "color", "c selector required")
        lookup[symbol] = len(palette)
        palette.append(PaletteEntry(symbol, _color(selectors["c"], path, line)))
    indices = []
    for value, line in lines[1 + color_count:]:
        if len(value) != width * cpp:
            raise FormatError(path, line, 1, "row_length", "pixel row has wrong length")
        for offset in range(0, len(value), cpp):
            symbol = value[offset:offset + cpp]
            if symbol not in lookup:
                raise FormatError(path, line, offset + 1, "symbol", f"unknown palette symbol {symbol!r}")
            indices.append(lookup[symbol])
    return Sprite(width, height, tuple(palette), tuple(indices), hotspot)


def load_xpm(path: str | Path, root: str | Path | None = None) -> Sprite:
    file = Path(path)
    if root is not None:
        base = Path(root).resolve()
        file = _source_path(str(path), base, str(path))
    try:
        return parse_xpm(_read_bounded(file, MAX_SOURCE_BYTES), str(file))
    except OSError as error:
        raise FormatError(str(file), 1, 1, "io", str(error)) from error


def _json_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _json_constant(value: str) -> None:
    raise ValueError(f"non-finite JSON number: {value}")


def _validate_json_tree(value: Any, path: str) -> None:
    stack = [(value, 1)]
    nodes = 0
    while stack:
        current, depth = stack.pop()
        nodes += 1
        if nodes > MAX_JSON_NODES or depth > MAX_JSON_DEPTH:
            raise FormatError(path, 1, 1, "limits", "JSON nesting or node count exceeds limit")
        if isinstance(current, float) and not math.isfinite(current):
            raise FormatError(path, 1, 1, "json", "JSON numbers must be finite")
        if isinstance(current, dict):
            nodes += len(current)
            if nodes > MAX_JSON_NODES:
                raise FormatError(path, 1, 1, "limits", "JSON node count exceeds limit")
            stack.extend((child, depth + 1) for child in current.values())
        elif isinstance(current, list):
            stack.extend((child, depth + 1) for child in current)


def _object(value: Any, allowed: set[str], required: set[str], path: str, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise FormatError(path, 1, 1, "schema", f"{label} must be an object")
    if "layers" in value:
        raise FormatError(path, 1, 1, "layers_unsupported", "layer composition is not supported in version 1")
    extra = value.keys() - allowed
    missing = required - value.keys()
    if extra or missing:
        raise FormatError(path, 1, 1, "schema", f"{label} unknown fields {sorted(extra)} or missing fields {sorted(missing)}")
    return value


def _int(value: Any, low: int, high: int, path: str, label: str) -> int:
    if type(value) is not int or not low <= value <= high:
        raise FormatError(path, 1, 1, "schema", f"{label} must be integer in [{low}, {high}]")
    return value


def _finite_number(value: Any) -> bool:
    if type(value) not in (int, float):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False


def _extras(value: dict[str, Any], path: str, label: str) -> None:
    if "metadata" in value and not isinstance(value["metadata"], dict):
        raise FormatError(path, 1, 1, "schema", f"{label} metadata must be an object")
    if "events" in value and not isinstance(value["events"], list):
        raise FormatError(path, 1, 1, "schema", f"{label} events must be a list")


def _source_path(value: Any, root: Path, manifest_path: str) -> Path:
    if not isinstance(value, str) or not value or len(value) > 1024 or \
            "\\" in value or ":" in value or any(unicodedata.category(char) == "Cc" for char in value):
        raise FormatError(manifest_path, 1, 1, "source_path", "source must be a short forward-slash relative path")
    parts = value.split("/")
    if any(part in ("", ".", "..") or part.endswith((".", " ")) or
           any(char in '<>"|?*' for char in part) or
           _WINDOWS_DEVICE.fullmatch(part.split(".", 1)[0].rstrip(" ")) for part in parts) or \
            not value.lower().endswith(".xpm"):
        raise FormatError(manifest_path, 1, 1, "source_path", "source must be a relative .xpm path inside root")
    try:
        candidate = (root / value).resolve()
    except (OSError, RuntimeError, ValueError) as error:
        raise FormatError(manifest_path, 1, 1, "source_path", "cannot resolve source path") from error
    if not candidate.is_relative_to(root):
        raise FormatError(manifest_path, 1, 1, "source_path", "source escapes manifest root")
    return candidate


def _read_bounded(path: Path, limit: int) -> bytes:
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise FormatError(str(path), 1, 1, "source_limit", "source exceeds byte limit")
    return data


def parse_manifest(source: str | bytes, root: str | Path, path: str = "<manifest>") -> Manifest:
    text = _decode(source, path)
    try:
        data = json.loads(text, object_pairs_hook=_json_pairs, parse_constant=_json_constant)
    except (ValueError, RecursionError) as error:
        raise FormatError(path, getattr(error, "lineno", 1), getattr(error, "colno", 1), "json", str(error)) from error
    _validate_json_tree(data, path)
    obj = _object(data, {"version", "canvas", "animations", "events", "metadata"},
                  {"version", "canvas", "animations"}, path, "manifest")
    _extras(obj, path, "manifest")
    if type(obj["version"]) is not int or obj["version"] != 1:
        raise FormatError(path, 1, 1, "version", "only manifest version 1 is supported")
    canvas = _object(obj["canvas"], {"width", "height"}, {"width", "height"}, path, "canvas")
    width = _int(canvas["width"], 1, MAX_DIMENSION, path, "canvas width")
    height = _int(canvas["height"], 1, MAX_DIMENSION, path, "canvas height")
    if width * height > MAX_PIXELS:
        raise FormatError(path, 1, 1, "limits", "canvas exceeds pixel limit")
    definitions = obj["animations"]
    if not isinstance(definitions, dict) or not 1 <= len(definitions) <= MAX_ANIMATIONS:
        raise FormatError(path, 1, 1, "limits", "animation count must be 1..1024")
    try:
        base = Path(root).resolve()
    except (OSError, RuntimeError, ValueError) as error:
        raise FormatError(path, 1, 1, "source_path", "cannot resolve source root") from error
    animations = {}
    total_frames = total_pixels = total_bytes = 0
    sprite_cache: dict[Path, Sprite] = {}
    for name, definition in definitions.items():
        if not name or len(name) > 128:
            raise FormatError(path, 1, 1, "schema", "animation name must have 1..128 characters")
        anim = _object(definition, {"playback", "frames", "events", "metadata"},
                       {"playback", "frames"}, path, f"animation {name}")
        _extras(anim, path, f"animation {name}")
        if anim["playback"] not in ("loop", "once", "ping-pong"):
            raise FormatError(path, 1, 1, "playback", "unsupported playback mode")
        definitions_frames = anim["frames"]
        if not isinstance(definitions_frames, list) or not definitions_frames:
            raise FormatError(path, 1, 1, "schema", "animation frames must be a nonempty list")
        frames = []
        for frame_value in definitions_frames:
            total_frames += 1
            total_pixels += width * height
            if total_frames > MAX_FRAMES or total_pixels > MAX_FRAME_PIXELS:
                raise FormatError(path, 1, 1, "limits", "aggregate frame limit exceeded")
            frame = _object(frame_value, {"source", "duration_ms", "anchors", "events", "metadata"},
                            {"source", "duration_ms"}, path, "frame")
            _extras(frame, path, "frame")
            duration = frame["duration_ms"]
            if not _finite_number(duration) or not 0 < duration <= MAX_DURATION_MS:
                raise FormatError(path, 1, 1, "duration", "duration_ms must be finite and positive")
            source_path = _source_path(frame["source"], base, path)
            if source_path not in sprite_cache:
                try:
                    raw = _read_bounded(source_path, MAX_SOURCE_BYTES)
                except OSError as error:
                    raise FormatError(str(source_path), 1, 1, "io", str(error)) from error
                total_bytes += len(raw)
                if total_bytes > MAX_TOTAL_SOURCE_BYTES:
                    raise FormatError(path, 1, 1, "source_limit", "aggregate XPM sources exceed limit")
                sprite_cache[source_path] = parse_xpm(raw, str(source_path))
            sprite = sprite_cache[source_path]
            if sprite.width != width or sprite.height != height:
                raise FormatError(path, 1, 1, "canvas", "sprite dimensions differ from canvas")
            raw_anchors = frame.get("anchors", {})
            if not isinstance(raw_anchors, dict) or len(raw_anchors) > 256:
                raise FormatError(path, 1, 1, "anchors", "anchors must be an object with at most 256 entries")
            anchors = {}
            for anchor_name, point in raw_anchors.items():
                if not anchor_name or len(anchor_name) > 128 or not isinstance(point, list) or len(point) != 2:
                    raise FormatError(path, 1, 1, "anchors", "anchor must name a two-element point")
                anchors[anchor_name] = (_int(point[0], 0, width - 1, path, "anchor x"),
                                        _int(point[1], 0, height - 1, path, "anchor y"))
            frames.append(Frame(frame["source"], float(duration), sprite, anchors,
                                frame.get("events", []), frame.get("metadata", {})))
        animations[name] = Animation(anim["playback"], tuple(frames),
                                     anim.get("events", []), anim.get("metadata", {}))
    return Manifest(width, height, animations, obj.get("events", []), obj.get("metadata", {}))


def load_manifest(path: str | Path, root: str | Path | None = None) -> Manifest:
    manifest_path = Path(path).resolve()
    try:
        source = _read_bounded(manifest_path, MAX_SOURCE_BYTES)
    except OSError as error:
        raise FormatError(str(manifest_path), 1, 1, "io", str(error)) from error
    return parse_manifest(source, Path(root) if root is not None else manifest_path.parent, str(manifest_path))


def evaluate_frame(animation: Animation, time_ms: float, sample_ms: float | None = None,
                   decimation: int = 1) -> FrameSelection | None:
    if not _finite_number(time_ms):
        raise ValueError("time_ms must be finite")
    if sample_ms is not None and (not _finite_number(sample_ms) or sample_ms <= 0):
        raise ValueError("sample_ms must be finite and positive")
    if type(decimation) is not int or decimation < 1:
        raise ValueError("decimation must be a positive integer")
    frames = animation.frames
    if len(frames) > MAX_FRAMES:
        raise ValueError("animation exceeds frame limit")
    if not frames:
        return None
    if animation.playback not in ("loop", "once", "ping-pong"):
        raise ValueError("invalid playback mode")
    durations = [f.duration_ms for f in frames]
    if any(not _finite_number(d) or not 0 < d <= MAX_DURATION_MS for d in durations):
        raise ValueError("frame durations must be finite and within limits")
    sequence = list(range(len(frames)))
    if animation.playback == "ping-pong" and len(frames) > 1:
        sequence += list(range(len(frames) - 2, 0, -1))
    with localcontext() as context:
        context.prec = 1024
        time = Decimal(str(time_ms))
        if sample_ms is not None:
            sample = Decimal(str(sample_ms))
            time = (time / sample).to_integral_value(rounding=ROUND_FLOOR) * sample
            if not _finite_number(float(time)):
                raise ValueError("sampled time exceeds finite range")
        decimal_durations = [Decimal(str(d)) for d in durations]
        total = sum((decimal_durations[index] for index in sequence), Decimal(0))
        if animation.playback == "once":
            local = max(time, Decimal(0))
            if local >= total:
                local = total.next_minus()
        else:
            local = time % total
            if local < 0:
                local += total
            if local >= total:
                local = total.next_minus()
        elapsed = Decimal(0)
        for active in sequence:
            duration = decimal_durations[active]
            if local < elapsed + duration:
                selected = active - active % decimation
                frame_local = float(local - elapsed)
                if frame_local >= durations[active]:
                    frame_local = math.nextafter(durations[active], 0)
                return FrameSelection(active, selected, frames[selected], frame_local)
            elapsed += duration
    raise ValueError("time did not select a frame")
