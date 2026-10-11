"""Prepare non-destructive, local fixtures for a future native manifest gate.

Run with Python 3.12+ on Windows. This script never removes or overwrites
unknown files. A repeated run ID verifies ordinary files and reuses valid links.
The index records unavailable link cases instead of treating them as passes.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[4]
EXPECTED_ROOT = PROJECT_ROOT / "Saved" / "PixelSpriteNativeFixtures"
SOURCES = Path(__file__).resolve().parent / "Fixtures" / "Manifest"
OWNER = "ckpixel-native-gate-fixtures-v1"
RUN_ID = "native-gate-v1"


def ordinary_directory(path: Path) -> bool:
    return path.is_dir() and not path.is_symlink() and not path.is_junction()


def reject_existing_link(path: Path) -> None:
    if path.is_symlink() or path.is_junction():
        raise RuntimeError(f"fixture directory is linked: {path}")


def write_owned_bytes(path: Path, data: bytes) -> None:
    if path.exists() or path.is_symlink() or path.is_junction():
        if not path.is_file() or path.is_symlink() or path.is_junction() or path.read_bytes() != data:
            raise RuntimeError(f"refusing to overwrite unknown or changed fixture: {path}")
        return
    path.write_bytes(data)


def write_owned_json(path: Path, value: dict) -> None:
    write_owned_bytes(path, (json.dumps(value, indent=2, ensure_ascii=True) + "\n").encode("utf-8"))


def encoded_json(value: dict) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=True) + "\n").encode("utf-8")


def manifest(*sources: str) -> dict:
    return {
        "version": 1,
        "canvas": {"width": 2, "height": 2},
        "animations": {"idle": {"playback": "loop", "frames": [
            {"source": source, "duration_ms": 100} for source in sources
        ]}},
    }


def prepare_link(link: Path, target: Path, *, kind: str) -> tuple[bool, str]:
    if link.exists() or link.is_symlink() or link.is_junction():
        try:
            linked = link.is_junction() if kind == "junction" else link.is_symlink()
            if linked and os.path.samefile(link, target):
                return True, ""
        except OSError:
            pass
        return False, "existing path does not match the owned link target"
    try:
        if kind == "junction":
            if os.name != "nt":
                return False, "Windows junctions are unavailable on this platform"
            result = subprocess.run(
                ["cmd.exe", "/d", "/c", "mklink", "/J", str(link), str(target)],
                capture_output=True, text=True, check=False,
            )
            if result.returncode != 0:
                return False, f"junction creation failed (exit {result.returncode})"
        else:
            os.symlink(target, link, target_is_directory=(kind == "directory"))
    except OSError as error:
        return False, f"{kind} creation unavailable: {error.strerror or type(error).__name__}"
    try:
        linked = link.is_junction() if kind == "junction" else link.is_symlink()
        if linked and os.path.samefile(link, target):
            return True, ""
    except OSError:
        pass
    return False, "created link could not be verified"


def case(name: str, path: Path, valid: bool, code: str | None = None,
         supported: bool = True, reason: str = "") -> dict:
    return {
        "name": name,
        "manifest": str(path),
        "expected_valid": valid,
        "expected_code": code,
        "supported": supported,
        "skip_reason": reason,
    }


def prepare(run_id: str) -> Path:
    if sys.version_info < (3, 12):
        raise RuntimeError("Python 3.12+ is required for junction inspection")
    if os.name != "nt":
        raise RuntimeError("this named Windows fixture root requires Windows")
    host_marker = PROJECT_ROOT / "CkPlugins.uproject"
    if not ordinary_directory(PROJECT_ROOT) or not host_marker.is_file() or \
            host_marker.is_symlink() or host_marker.is_junction():
        raise RuntimeError(f"script is outside an ordinary CkPlugins host project: {PROJECT_ROOT}")
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", run_id):
        raise RuntimeError("run ID must use 1..64 ASCII letters, digits, hyphens or underscores")
    saved = PROJECT_ROOT / "Saved"
    if not ordinary_directory(saved):
        raise RuntimeError(f"expected an existing ordinary Saved directory: {saved}")
    root = saved / "PixelSpriteNativeFixtures"
    if root != EXPECTED_ROOT:
        raise RuntimeError(f"fixture root mismatch: {root}")
    if root.exists() or root.is_symlink() or root.is_junction():
        if not ordinary_directory(root):
            raise RuntimeError(f"fixture root is not an ordinary directory: {root}")
    else:
        root.mkdir()
    reject_existing_link(root)

    run = root / run_id
    marker = run / "owner.json"
    ownership = {"generator": OWNER, "run_id": run_id}
    if run.exists() or run.is_symlink() or run.is_junction():
        if not ordinary_directory(run) or not marker.is_file() or marker.is_symlink() or marker.is_junction():
            raise RuntimeError(f"refusing existing unowned run directory: {run}")
        if json.loads(marker.read_text(encoding="utf-8")) != ownership:
            raise RuntimeError(f"refusing existing run with a different owner marker: {run}")
    else:
        run.mkdir()
        write_owned_json(marker, ownership)

    ordinary = run / "ordinary"
    if ordinary.exists() or ordinary.is_symlink() or ordinary.is_junction():
        if not ordinary_directory(ordinary):
            raise RuntimeError(f"refusing nonordinary fixture directory: {ordinary}")
    else:
        ordinary.mkdir()
    targets = ordinary / "targets"
    if targets.exists() or targets.is_symlink() or targets.is_junction():
        if not ordinary_directory(targets):
            raise RuntimeError(f"refusing nonordinary link-target directory: {targets}")
    else:
        targets.mkdir()
    for name in ("red.xpm", "blue.xpm", "mismatch.xpm"):
        write_owned_bytes(targets / name, (SOURCES / name).read_bytes())

    valid = targets / "ordinary-valid.json"
    missing = targets / "late-missing-second-source.json"
    mismatch = targets / "dimension-mismatch.json"
    bom_valid = targets / "utf8-bom-valid.json"
    malformed_manifest = targets / "malformed-utf8-manifest.json"
    raw_nul_manifest = targets / "raw-nul-manifest.json"
    malformed_xpm = targets / "malformed-utf8.xpm"
    malformed_xpm_manifest = targets / "malformed-utf8-xpm.json"
    write_owned_json(valid, manifest("red.xpm", "blue.xpm"))
    write_owned_json(missing, manifest("red.xpm", "missing-second.xpm"))
    write_owned_json(mismatch, manifest("red.xpm", "mismatch.xpm"))
    write_owned_bytes(bom_valid, b"\xef\xbb\xbf" + encoded_json(manifest("red.xpm", "blue.xpm")))
    write_owned_bytes(malformed_manifest, encoded_json(manifest("red.xpm")) + b"\xc0\xaf")
    write_owned_bytes(raw_nul_manifest, encoded_json(manifest("red.xpm")).replace(
        b'"canvas":', b'\x00"canvas":', 1))
    write_owned_bytes(malformed_xpm, (SOURCES / "red.xpm").read_bytes() + b"\xed\xa0\x80")
    write_owned_json(malformed_xpm_manifest, manifest("red.xpm", malformed_xpm.name))
    expected_targets = {"red.xpm", "blue.xpm", "mismatch.xpm", valid.name, missing.name, mismatch.name,
                        bom_valid.name, malformed_manifest.name, raw_nul_manifest.name,
                        malformed_xpm.name, malformed_xpm_manifest.name}
    if {entry.name for entry in targets.iterdir()} != expected_targets or any(
            not entry.is_file() or entry.is_symlink() or entry.is_junction()
            for entry in targets.iterdir()):
        raise RuntimeError(f"link-target directory contains an unknown or linked entry: {targets}")

    cases = [
        case("ordinary_valid", valid, True),
        case("late_missing_second_source", missing, False, "io"),
        case("dimension_mismatch", mismatch, False, "canvas"),
        case("utf8_bom_manifest", bom_valid, True),
        case("malformed_utf8_manifest", malformed_manifest, False, "utf8"),
        case("raw_nul_json", raw_nul_manifest, False, "json"),
        case("malformed_utf8_xpm", malformed_xpm_manifest, False, "utf8"),
    ]

    directory_link = ordinary / "directory_link"
    file_link = ordinary / "file_link.xpm"
    linked_root = run / "linked_root"
    inside_junction = ordinary / "junction_inside"
    outside_junction = root / f"{run_id}-junction-outside"
    links = [
        ("directory_link", directory_link, targets, "directory", ordinary / "directory-link.json",
         manifest("directory_link/red.xpm")),
        ("file_link", file_link, targets / "red.xpm", "file", ordinary / "file-link.json",
         manifest("file_link.xpm")),
        ("linked_root", linked_root, targets, "directory", linked_root / valid.name, None),
        ("junction_inside", inside_junction, targets, "junction", ordinary / "junction-inside.json",
         manifest("junction_inside/red.xpm")),
        ("junction_outside", outside_junction, targets, "junction", outside_junction / valid.name, None),
    ]
    for name, link, target, kind, test_manifest, contents in links:
        if contents is not None:
            # The manifest is ordinary even if link creation is unsupported.
            write_owned_json(test_manifest, contents)
        supported, reason = prepare_link(link, target, kind=kind)
        cases.append(case(name, test_manifest, False, "io", supported, reason))

    index = {"generator": OWNER, "run_id": run_id, "root": str(root), "cases": cases}
    index_path = run / "index.json"
    if index_path.exists() or index_path.is_symlink() or index_path.is_junction():
        if not index_path.is_file() or index_path.is_symlink() or index_path.is_junction():
            raise RuntimeError(f"refusing nonordinary index: {index_path}")
        previous = json.loads(index_path.read_text(encoding="utf-8"))
        if previous.get("generator") != OWNER or previous.get("run_id") != run_id:
            raise RuntimeError(f"refusing unowned index: {index_path}")
        if previous == index:
            return index_path
    temporary = run / "index.pending.json"
    if temporary.exists() or temporary.is_symlink() or temporary.is_junction():
        raise RuntimeError(f"refusing existing temporary index: {temporary}")
    write_owned_json(temporary, index)
    os.replace(temporary, index_path)
    return index_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-id", default=RUN_ID)
    arguments = parser.parse_args()
    try:
        print(prepare(arguments.run_id))
    except (OSError, RuntimeError, ValueError) as error:
        print(f"fixture preparation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
