# Investigation evidence and reuse boundaries

This skill consolidates three investigations reviewed on 2026-10-19. Chat references locate their artifacts; resolve current local locations from the originating chat rather than assuming another machine has the same drive layout. Paths below are relative to the named investigation's deliverable root and are discovery hints, historical artifact labels; portable counterparts are indexed in ../README.md.

## Procedural tools and reusable recipes

Source: [Evaluate procedural texture tools](codex://threads/01a14825-8804-73e1-afe4-f6d3a2b6cdd6).

Root: `TextureBackendEvaluation`.

- Latest `PROGRESS.md` records explicit user acceptance of reusable smoke and shockwave recipes. It supersedes pending statements in older appended notes and portions of their READMEs.
- `blender/fresh-smoke-20261018`: `README.md`, `ACCEPTANCE.json`, `reproduce.ps1`, `variation.json`, `recipe.py`, `audit.json`, `repeat-comparison.json`. Accepted fresh seeded Continuum GPU smoke solve with fixed recipe/camera and lit/unlit exports; selected 16-frame window at 8 fps is neither a complete birth/death sequence nor a proven seamless loop.
- `blender/reusable-shockwave-20261018`: `README.md`, `ACCEPTANCE.json`, `reproduce.ps1`, `variation.json`, `runs/seed80423/review/index.html`, `runs/seed80423/audit.json` and `runs/seed80423/normals/export`. Accepted procedural composition of new streaks/soil plus reused simulated smoke imagery. No new radial fluid solve.
- `blender/shockwave/README-r02.md` documents dither/speckle, decay rebound and coarse debris corrections. `blender/shockwave/normals-r04/normal-textures/README.md` documents calibrated billboard basis and density-relief versus mesh-face normal provenance.
- `unreal/niagara-20261018/STOPPED.md`: attempted runtime test stopped; user explicitly skipped it. No pass follows from the user's confidence that it would work. Do not resume this stopped test without new direction.

Historical recipes referenced experiment-local helpers and machine paths. The portable smoke, modular shockwave and procedural sources now live in this tool package; README.md identifies entry points, required external runtimes, preserved source art and migration verification limits. Historical review media, full caches and all experimental backends are not bundled.

## Procedural/data, generated-art and Niagara evaluation

Source: [Build texture generation pipeline](codex://threads/01a14886-82fa-7362-bfa1-f6926f8b0240).

Root: `CkTextureGeneration`.

- `DECISION.md`, `EVALUATION.md`, `PROGRESS.md`: native Blender masks/flow/correlated PBR, actual local SDXL GPU inference and optional hybrid art were executed. Final records supersede earlier incomplete setup notes. Hybrid art did not consistently outperform procedural smoke/shockwave. No universal artistic ranking or stable throughput guarantee follows.
- `generators/procedural/BLENDER.md` and `assets/blender/data_export_verification.json`: native nodes, Non-Color data, explicit artistic opacity curve, denoising correction and calibrated vector export. Static procedural volumes are distinct from the later accepted fluid recipes.
- `exports/smoke-reference-flipbook/README.md`, `manifest.json`, `editor/live-verification.json`, `editor/audition-verification.json`: 64 original simulation frames packed and captured in actual Niagara. Source frame matching and saved settings were checked; repeated review relied on an editor-only restart script. The inherited system cycle, uncompressed/no-mip texture and capture cadence were audition limitations, not gameplay or production-performance proof.

The investigated generated-art backend was local SDXL. The ChatGPT imagegen route in this skill is a tool-selection policy for future requests, not a claim that its quality, temporal coherence, hardware execution or speed was benchmarked in these investigations.

## Simulation and staged visual approval

Source: [Create explosion VFX flipbook](codex://threads/01a147f9-d98b-7261-b579-b624437b8d0c).

Root: the originating art project's `experiments` directory.

- `explosion_01_continuum_v2/full/README.md`, `technical_audit.json`: GPU gas simulation retained earlier directional lobe opening trajectories and a declared procedural ignition core. User accepted the full animation later in chat; the README's older pending line is stale. Sharp cooling transitions and broad folds remain recorded limitations.
- `smoke_02/ACCEPTANCE.json`: accepted as good enough for the test despite recorded internal rolling/visual failures.
- `fire_03/loop_v2/ACCEPTANCE.json`: accepted for the test with internal gate still false; simulated head/tail blend was a loop treatment, not proof that all temporal concerns disappeared.
- `dust_04_v2/ACCEPTANCE.json`, `technical_audit.json`, `output/settling_report.json`, `final/README.md`, `final/packing_receipt.json`: user accepted the motion and authorized exact existing-frame packaging. Gas/concentration dust approximation, not physical grain settling. All 64 approved frames were packed without a visual revision; opacity and grayscale meanings were explicit.

Free software, explicit GPU execution and preview-before-atlas were concrete constraints in those experiments. Preserve them when continuing the same authorized experiment. Reusing a recipe in a new task does not automatically inherit them; derive that task's constraints and approval boundaries from its current brief.

## Policy derived from the evidence

The core sequence and gates synthesize these lessons for future work; they are not claims that every route is already production validated. In particular, CkUsf integration and performance require inspecting the selected project's current implementation and measuring the requested effect. Neither offline acceptance nor the single Niagara audition establishes all-platform support or game performance.
