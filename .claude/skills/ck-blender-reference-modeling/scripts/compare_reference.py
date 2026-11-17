#!/usr/bin/env python3
"""Measure aligned reference/render pairs; metric success is never visual acceptance."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter


def file_identity(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return {"path": str(path), "sha256": digest.hexdigest()}


def number(value, label, minimum=0, maximum=None):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError(f"{label} must be a finite number")
    if value < minimum or (maximum is not None and value > maximum):
        raise ValueError(f"{label} is outside its allowed range")
    return value


def point(value, size, label):
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError(f"{label} must be [x, y]")
    x = number(value[0], label)
    y = number(value[1], label)
    if x >= size[0] or y >= size[1]:
        raise ValueError(f"{label} must lie inside the image")
    return x, y


def load_image(path, mask=False):
    with Image.open(path) as image:
        if mask and image.mode not in ("1", "L"):
            raise ValueError(f"{path}: mask must be single-channel L or binary 1")
        if mask:
            return image.convert("L").point(lambda value: 255 if value >= 128 else 0)
        return image.convert("RGB")


def overlap(reference, candidate):
    reference_pixels = reference.histogram()[255]
    if not reference_pixels:
        raise ValueError("reference mask/region has no foreground")
    intersection = ImageChops.multiply(reference, candidate).histogram()[255]
    union = ImageChops.lighter(reference, candidate).histogram()[255]
    return intersection / union


def check(name, metric, value, tolerance, greater):
    if tolerance is None:
        status = "UNCONFIGURED"
    else:
        number(tolerance, name, maximum=1 if metric == "iou" else None)
        passed = value >= tolerance if greater else value <= tolerance
        status = "PASS" if passed else "FAIL"
    return {"name": name, "metric": metric, "value": value,
            "tolerance": tolerance, "status": status}


def compare_view(view, base):
    if not isinstance(view, dict):
        raise ValueError("each view must be an object")
    name = view.get("name", "")
    if not isinstance(name, str) or not re.fullmatch(r"[a-z][a-z0-9_-]{0,63}", name):
        raise ValueError("view name must be a short lowercase identifier")
    keys = ("reference_image", "candidate_image", "reference_mask", "candidate_mask")
    paths = {}
    for key in keys:
        if not isinstance(view.get(key), str) or not view[key]:
            raise ValueError(f"{name}: missing {key} path")
        paths[key] = (base / view[key]).resolve()
    images = {key: load_image(path, mask=key.endswith("mask")) for key, path in paths.items()}
    size = images["reference_image"].size
    if any(image.size != size for image in images.values()):
        raise ValueError(f"{name}: images and masks must have identical dimensions")
    reference = images["reference_mask"]
    candidate = images["candidate_mask"]
    checks = [check("whole", "iou", overlap(reference, candidate), view.get("min_iou"), True)]
    for region in view.get("regions", []):
        box = region["box"]
        if not isinstance(box, list) or len(box) != 4 or any(type(v) is not int for v in box):
            raise ValueError(f"{name}: region box must contain four integers")
        left, top, right, bottom = box
        if not (0 <= left < right <= size[0] and 0 <= top < bottom <= size[1]):
            raise ValueError(f"{name}: region box must lie inside the image")
        checks.append(check(region["name"], "iou", overlap(reference.crop(box), candidate.crop(box)),
                            region.get("min_iou"), True))
    for landmark in view.get("landmarks", []):
        ref = point(landmark["reference"], size, "reference landmark")
        cand = point(landmark["candidate"], size, "candidate landmark")
        checks.append(check(landmark["name"], "error_px", math.dist(ref, cand),
                            landmark.get("max_error_px"), False))
    overlay = Image.blend(images["reference_image"], images["candidate_image"], 0.5)
    for mask, color in ((reference, "red"), (candidate, "cyan")):
        edge = ImageChops.subtract(mask, mask.filter(ImageFilter.MinFilter(3)))
        overlay.paste(color, (0, 0), edge)
    width, height = size
    sheet = Image.new("RGB", (width * 3, height + 24), "#303030")
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate((("Reference", images["reference_image"]),
                                          ("Candidate", images["candidate_image"]),
                                          ("Overlay: red reference / cyan candidate", overlay))):
        sheet.paste(image, (index * width, 24))
        draw.text((index * width + 4, 4), label, fill="white")
    return {"name": name, "size": list(size), "checks": checks,
            "inputs": {key: file_identity(path) for key, path in paths.items()}}, sheet


def run(manifest_path, out_dir):
    manifest_path = manifest_path.resolve()
    out_dir = out_dir.resolve()
    if out_dir.exists() and (not out_dir.is_dir() or any(out_dir.iterdir())):
        raise ValueError("output directory must be new or empty; previous evidence is preserved")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    if not isinstance(manifest, dict) or type(manifest.get("version")) is not int or manifest["version"] != 1:
        raise ValueError("manifest must be an object with version 1")
    views = manifest.get("views")
    if not isinstance(views, list) or not views:
        raise ValueError("manifest must contain at least one view")
    prepared = [compare_view(view, manifest_path.parent) for view in views]
    names = [view["name"] for view, _ in prepared]
    if len(set(names)) != len(names):
        raise ValueError("view names must be unique")
    source = manifest.get("source_file")
    if source is not None and (not isinstance(source, str) or not source):
        raise ValueError("source_file must be a nonempty path when supplied")
    source_identity = file_identity((manifest_path.parent / source).resolve()) if source else None
    statuses = {item["status"] for view, _ in prepared for item in view["checks"]}
    if "FAIL" in statuses:
        verdict, code = "METRIC_FAIL", 1
    elif "UNCONFIGURED" in statuses:
        verdict, code = "METRIC_UNCONFIGURED", 2
    else:
        verdict, code = "METRIC_PASS", 0
    report = {"version": 1, "verdict": verdict, "visual_acceptance": "PENDING_REVIEW",
              "manifest": file_identity(manifest_path), "source": source_identity,
              "views": [view for view, _ in prepared]}
    out_dir.mkdir(parents=True, exist_ok=True)
    for view, sheet in prepared:
        sheet.save(out_dir / f"{view['name']}_compare.png")
    (out_dir / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"{verdict}; visual acceptance PENDING_REVIEW; {out_dir / 'report.json'}")
    return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    try:
        return run(args.manifest, args.out)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"COMPARISON_ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
