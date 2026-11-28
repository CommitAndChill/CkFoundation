# Opal and coated display — rendered review

2026-10-22. The user accepted the polished amber and requested the next two materials
from the concept: opal and coated glass over a display. This pair is additive and uses
the existing approved offscreen Adaptive profile. No C++, engine or renderer-default
change was made. No Git publication was requested or performed.

## Delivered

- Two-Slab native opal: polished neutral upper layer, quiet milky lower body, original
  texture-backed colored domains, independent normal and roughness responses. Static
  lit body color supplements native colored specular flashes. Neither Slab emits.
- Native transmitting glass over four separate original display parts: a solid beveled
  cover, recessed illuminated panel, housing and trim. Thin-film RGB reflectance is a
  bounded approximation in `.ush`; only the underlying panel emits.
- Twelve source maps, four original display meshes, five generated masters/definitions,
  immutable original opal artwork, hash receipts and reproducible offscreen scripts.
- Actual 1600x1200 final frames and two three-second H.264 clips with a circling key light.
  The opal reuses the existing closed amber body without modifying it.

## Evidence

`Saved/UsfMaterialGallery/Optical-Pair-Gallery.html` is the user-facing review.
`OpticalPair-Receipt.json` links the underlying records.

Two fresh Toolbox boots were used: warm look development and a fresh saved-content check.
All five masters compile; opal retains two native closures, glass and each opaque part
retain one. Fresh defaults were checked before any transient parameter overrides.
Known initial recipe values were verified before migrating only the three tuned owned
definitions. Ten fresh controls verify plain/clear endpoints, changed angles, lights off,
and restored settings. The fresh log contains no matched Error, ensure, traceback,
fatal or material-compile failure. Both owned editors and Toolbox processes closed.

Live `.ush` changes were rendered and restored on the same master/MID identities:
opal texture UV sampling and glass roughness. Mean RGB changes on a 0–255 scale were
4.093 and 8.203; restored differences were 0.633 and 0.453. Independent review checked
the identities, shader hashes, compiled records and final image hashes.

With all lights and sky disabled, opal contains zero nonzero RGB pixels. The display
retains its readable graphic (25,379 nonzero RGB pixels). Final control deltas were
2.671 for plain opal and 0.314 for clear glass; recovered deltas were 0.141 and 0.125.
The coating difference is intentionally subtle. Each encoded clip decodes successfully
and contains 36 frames at 12 fps. This is not a performance benchmark.

The normal project configuration, accepted AmberPolish final image and AmberSpecimen
shader retain their pre-change hashes. No existing showcase source was intentionally
edited. No broad automation-suite or repository-wide regression claim is made for
this content/shader-only delivery.

## Visual and physical limits

The original opal source is generated artwork (1254x1254), not a measured scan. Its
derived data maps are artistic estimates. Native lighting drives the specular flashes;
static colored diffuse approximates unresolved internal scattering. This does not
simulate spectral diffraction or volumetric opal transport.

The display's seven 2048-square maps are original code-authored graphics and finish
data. Its glass uses neutral Pixel Normal Offset and visible geometric thickness,
not traced entry/exit refraction or caustics. The accepted amber and normal gym remain
separate. Final artistic acceptance of this new pair belongs to the user.

## Continue

Use `Tools/UsfMaterials/Launch-OpticalPair.ps1`, then the serial
`optical_pair_command.py` prepare/scene/capture commands. Normal preparation now uses
the selected saved recipe. Never prepare/regenerate definitions inside an active scene;
save-triggered generation can replace an initially bound master on a later editor tick.
Use warm `.ush` edits for iteration and preserve material instances in place.
