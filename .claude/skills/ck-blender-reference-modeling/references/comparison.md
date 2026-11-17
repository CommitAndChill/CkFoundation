# Fixed-camera reference comparisons

`scripts/compare_reference.py` runs outside Blender with Python 3 and Pillow. Use an environment
with Pillow and check its availability before running; Blender's bundled Python may not have it.
No add-on or global installation is needed by the helper.

Render images and masks from the SAME evaluated mesh, frame and camera. The helper never renders,
segments images, calibrates cameras, or proves freshness. Record the exact source revision/settings
in your asset record. A source hash identifies a file; it does not prove an image came from it.

## Inputs

- Reference crops, candidate renders, and their explicit masks must share exact pixel dimensions.
- Supply masks as single-channel `L` or binary `1` images: white foreground, black background;
  antialiasing is thresholded at 128. Check their overlays visually. Do not include floor shadows,
  labels or rulers. RGB masks and transparent renders are deliberately not guessed into masks.
- Rectangles and landmarks use crop pixel coordinates, origin top-left. Box right/bottom are
  exclusive. Regions should isolate important features, not just the whole body.
- Choose tolerances for this asset AFTER checking camera/pose alignment. The numbers in this
  example illustrate the schema; they are not recommended universal thresholds.

An asset-local JSON manifest (paths relative to that manifest):

```json
{
  "version": 1,
  "source_file": "../source/Creature.blend",
  "views": [
    {
      "name": "hero",
      "reference_image": "crops/hero.png",
      "candidate_image": "r03/hero.png",
      "reference_mask": "crops/hero_mask.png",
      "candidate_mask": "r03/hero_mask.png",
      "min_iou": 0.90,
      "regions": [
        {"name": "head", "box": [180, 100, 400, 300], "min_iou": 0.92}
      ],
      "landmarks": [
        {"name": "near_eye", "reference": [270, 160], "candidate": [274, 162], "max_error_px": 6}
      ]
    }
  ]
}
```

Omit a check's tolerance for a diagnostic-only measurement. Every declared check needs an explicit
tolerance before the metric verdict can pass. A failing check takes precedence over missing
tolerances. `source_file` is optional for diagnostics; supply it when reporting an asset revision.

## Run and inspect

From a shell, use the canonical skill path appropriate to the current checkout:

```powershell
python "<skill-directory>/scripts/compare_reference.py" --manifest "<asset>/review/compare.json" --out "<asset>/review/r03_compare"
```

Output must be new or empty; previous evidence is never overwritten. Each view produces
`<name>_compare.png`: reference | candidate | overlay. Red edges are reference; cyan edges are
candidate. `report.json` includes per-check values/tolerances, input hashes, manifest hash,
source hash when supplied, and the explicit verdict:

| Exit | Verdict | Meaning |
|---|---|---|
| 0 | `METRIC_PASS` | All configured measurements passed and every check had a tolerance |
| 1 | `METRIC_FAIL` | One or more measurements failed |
| 2 | `METRIC_UNCONFIGURED` | Measurements produced; at least one tolerance absent |
| 2 | Input/runtime error on stderr | Invalid inputs or comparison did not complete; no passing report |

`visual_acceptance` is always `PENDING_REVIEW`. Inspect the actual comparisons and playback where
applicable, then record your verdict separately with the source/report identity. Never promote the
helper's exit 0 into artistic approval. It does not score internal planes, appeal, motion, material
parity or unseen surfaces. Compare revisions at fixed calibration and keep a view/angle outside the
optimization set to expose overfitting.

For helper changes, run `python -m unittest discover -s "<skill-directory>/scripts" -p "test_*.py"`.
