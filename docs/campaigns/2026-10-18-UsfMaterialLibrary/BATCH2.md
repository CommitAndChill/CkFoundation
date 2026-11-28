# Mixed library batch 2

2026-10-19. User accepted the linear aluminum refinement and authorized the next mixed texture-backed batch, entirely offscreen. Moving completion state belongs in PROGRESS.md; evidence for this batch is recorded below as it is verified.

## Scope and material contracts

| Look | Intended identity | Existing response and controls |
| --- | --- | --- |
| LibraryCanvas | Cotton plain weave, from genuine poplin source; not claimed to be heavy canvas | DefaultLit, BC/N/R, weave contrast, shallow normal, dry broad highlights |
| LibrarySuede | Fine short leather nap, distinct from pebbled leather | Cloth, BC/N/R, nap amount/color and source-roughness-driven modulation; fuzz is not volumetric SSS |
| LibraryTerracotta | Warm terracotta floor tiles with porous ceramic detail | DefaultLit, BC/N/R, roughness, normal, firing-color saturation |
| LibraryMarble | Coherent branching mineral veins beneath a smooth polish | DefaultLit, BC/N/R, very shallow normal, vein contrast and polish; no arbitrary separate slab or resin-coat claim |
| LibraryRubber | Dense dark rubber with restrained fine grain | DefaultLit, BC/N/R, broad dielectric reflections and subtle normal; rubber-floor source identity retained, no certified molding-process claim |
| LibraryGlass | Clear near-neutral thin-surface transmission | ThinTranslucent, N/R, white specular, subtle source-backed surface variation, transmission/coverage and modest screen-space IOR; no thickness trace |

The originally proposed heavy-canvas slot uses cotton poplin after the publisher's exact BlueCotton01 download returned HTTP403. The stable LibraryCanvas look ID remains, but the visible label states cotton plain weave. Clear glass does not need a diffuse base-color map: tint/transmission are authored independently while sourced N/R supply surface detail. Its authored opacity source serves only as roughness variation, not an opacity/transmission mask or calibrated measured roughness. Clean defaults keep these variations exceptionally quiet; stronger smearing is optional.

## Pattern and boundaries

The implementation follows batch one's LibrarySurface sampling, ordered AS LookDefinition parameters, per-look .ush entry points, persistent generated masters, runtime gym MIDs and JSON/Python offscreen queue. Native Cloth and ThinTranslucent already exist in the shading-model contract. Cloth SubsurfaceColor is fuzz color; ClothMask is fuzz amount. ThinTranslucent Opacity=0 leaves the surface transmissive, ThinSurfaceCoverage=1 retains its specular interface. The material's IOR pin offsets screen color, not actual ray entry/exit geometry.

No general output, graph schema, C++, engine, Substrate mode, native Toon source, approved shader ball, CoreBase material or studio-lighting change is required. Each material owns its samples, UV transform, normal strength, roughness/color math and specialized response in .ush; defaults and parameter bindings remain AS assets. Source files and licensed maps remain editable. Existing materials and their evidence are preserved.

## Verification plan

1. Source audit: retain exact originals, official metadata/license, channel meanings, dimensions, DirectX normal conversion and hashes in CkTests Batch2Textures. Verify every required input before any import.
2. Import/individual surfaces: all six masters and texture settings; close/macro/gameplay real PIE captures under the original lighting, native AA separately from temporary TAA. Tune from actual appearance.
3. Live workflow: six existing master/MID objects, source edit/recovery for each .ush and shared inspection dependency, exact source restoration, scalar/vector/texture snapshots; normal/roughness/UV/specialized-control response and restoration.
4. Fresh final gate: real-RHI GeneratesUsableMasters through detached Toolbox, offscreen. This checks all look generation/compilation and runtime master/MID resolution; no global native-Toon lighting mutation. It is a focused test, not repository-wide coverage.

Boot budget: two fresh editors, one offscreen import/iteration studio and one fresh focused gate. No C++ build is needed for these AS/.ush/assets-only additions. No desktop input, cook/package, new checkout, cache deletion, commit or push.

## Source and import observations

All source maps independently match the retained manifest's SHA256 values. Manifest SHA256 `f942fb0b9e670acbf424cf29758beddc35bb8ac7ff3d47bd05d13e02f526d512`; cached recipe regeneration was byte-identical. Source records: Poly Haven stretch_poplin and brown_floor_tiles; ambientCG Leather039, Marble012, Rubber004 and Smear005. Official CC0 license snapshots, authors, source/download metadata and normal conversions are retained with originals. Selected source channels total 61.31 MB; all retained candidates/provenance total 202.41 MB. The source contact/detail sheets are pixel inspection, not Unreal acceptance.

Seventeen runtime textures and six persistent masters imported/generated successfully in offscreen PID36504. Data maps are linear; BC is sRGB; DirectX normals use TC_NORMALMAP with no green flip; wrap and mips are enabled. Cotton's original 1024x988 maps are retained and explicitly resized only in the runtime build to 1024x1024, a roughly 3.6% vertical resampling for streaming/mips. All other runtime maps are 1024 square. Batch2Preparation.json records source/runtime sizes and settings verified after save; it does not claim an unload/reload disk round trip.

Initial visual review selected cotton UVScale .5 with unchanged .55 normal, terracotta tint(1.4,.72,.45), and rubber tint2/roughness .5*map+.15. Remaining defaults retained. Cotton is fine poplin, not heavy canvas; detail is intentionally quiet. Marble veins are color/mineral structure, not raised grooves. Glass's opaque CoreBase remains visible through the thin interface; the ball does not demonstrate solid optical glass.

## Iteration and retained failures

The initial flat-pane tool attempted a Python GameplayStatics spawning API that is not exposed. It was replaced with an owned AngelScript inspection actor and Exec command; no engine change. A transient AS compile error occurred between coupled hot-reload edits outside PIE; the final reload succeeded before reopening. Plane orientation follows the existing ImageShowcase's +90 pitch. The comparison was moved ahead of the specimen after observing CoreBase occluding the background. These were diagnostic-layout corrections, not changes to the accepted ball or lights.

One capture/proof host-job overlap invalidated the tail of a capture sequence. All seven source files were restored and independently hash-checked; the capture sequence was rerun serially. The first proof also incorrectly rejected the intentionally darkened rubber frame through its baseline-validity check; that check now applies only to baseline. Neither failed attempt establishes a production shader failure.

A subsequent complete HighResShot proof passed all six entrypoint edit/recoveries and five shared-helper recoveries, but shared-helper glass recovery lost the translucent outer shell, leaving CoreBase visible. The report `Batch2-LiveReloadProof-InitialFailure.json` retains numeric/state evidence and 22 original 1280x720 frames under `InitialFailure/`, including the missing-shell image; other overwritten image references are explicitly marked. All source bytes were restored. Reload reported 12 masters/0 failures, but that path checks a game-thread shader map rather than render-thread draw readiness.

Bounded Astra consultation distinguished missing geometry from merely low emissive. Native game-viewport Shot color/normal/color captures responded correctly with no source edit. A separate native shared-helper edit/recovery then passed all six in the same existing objects (`Batch2-SharedNativeProof.json`). Final proof uses this ordinary viewport route. This is validated native capture behavior, not a fixed HighResShot or shader-readiness bug; its exact cause remains unproven. No arbitrary readiness delay, cache deletion or renderer patch was added.

## Final verified evidence

All evidence paths below are under host Saved/UsfMaterialGallery unless specified.

- `Batch2-LitCaptures.json`: 36 matched real PIE images, six looks at close/macro/gameplay, native AA separately from temporary TAA/100%. Final material tunings are the saved AS defaults. Gallery/contact sheet are `Batch2-Gallery.html` and `Batch2-ContactSheet-TAA.png`.
- `Batch2-LiveReloadProof.json`: final complete native Shot proof passed, 30 images, PID36504. All six .ush entrypoints and the shared inspection dependency changed/recovered visibly on the same twelve library masters/MIDs, preserving every recorded scalar/vector/texture binding. All seven source files restored byte-exactly. The original HighResShot failure remains separately retained.
- `Batch2-ControlProof.json`: passed for all six, 42 diagnostic frames. UV rotation/scale, normal-strength, roughness and a material-specific property produced visible changes. Normal inspection temporarily uses strength2 to expose very subtle source maps; this is not the clean default. Every override restored. Tint tuning was also rendered before saving its defaults.
- `Batch2-GlassPaneCaptures.json` / `Batch2-GlassPaneSetup.json`: same glass MID over the existing tile MID, one side uncovered. IOR1 isolates transmission; diagnostic transmission .2 darkens only the glass-covered region. Both overrides restored and the owned temporary actor removed. The ordinary shader ball/CoreBase presentation remains unchanged.
- `Batch2-Costs.json`: material API statistics, pixel/vertex instructions: cotton336/296, suede350/296, terracotta337/296, marble339/296, rubber332/296, glass1879/492. Six samplers for opaque looks, eight for glass; two UV scalars. Authored fetches are three per opaque look and two for glass. Forward translucent glass is materially more expensive in these shader statistics; no GPU timing, overdraw or performance acceptance is claimed.
- `Batch2-Gate.exit.json` / `Saved/Logs/UsfLibrary-Batch2.log`: fresh offscreen real-RHI GeneratesUsableMasters passed 1/1, no failed/skipped/contaminated tests, actual exit0, 48s. This generates/compiles every declared look in a lane-owned test root and resolves shipped masters/runtime MIDs. No final AS/shader compile failure or ensure was found; pre-existing missing Scan texture warnings and a Chromium USB log are not suppressed. No C++ rebuild or global Toon reload.
- `Batch2-Verification.json`: independent reconciliation passed, 23 new assets (17 textures/six masters), source/input hashes, 30 final source-proof images, all control restores and exact 184 protected baseline asset/shader hashes. Native Toon sources, approved shader-ball assets and first-batch materials are unchanged. The unrelated engine Iris file retains SHA256 `69813456749AB1E6ACE335A81231761F589552ADBF5BDEB00949D7F2FE990C92`.

Two fresh editor boots were used: one offscreen studio with bounded PIE restarts for outside-PIE AS edits, then one focused gate. Final studio state restored AA0/screen percentage87, diagnostic point lighting off and every DebugView0. PIE ended, explicit Python/UE GC succeeded, and the studio quit cleanly. No mouse/keyboard/clipboard/focus automation, engine/cache/config change, cook/package, checkout creation, commit or push. This is implementation and technical/rendered evidence ready for user review; it does not claim user artistic approval or resolve the earlier native Python/DDC failures.
