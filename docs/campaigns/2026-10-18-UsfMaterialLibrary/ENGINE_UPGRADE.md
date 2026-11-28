# UE 5.8.4 engine integration

## Verified current state — 2026-10-21

Final native showcase update: the first texture-backed native Slab/Toon pair now has persistent
assets, gym stations and passing offscreen source-edit/recovery proofs. A stronger native
three-level diffuse profile is demonstrated alongside the retained factory baseline. Gallery and
full scope: NATIVE_BSDF.md and Saved/UsfMaterialGallery/Native-BSDF-Gallery.html. Both proofs
restore shader bytes, material overrides, rigs and renderer settings; the owned editor quit.
This supersedes pending showcase wording below; user visual approval remains separate.

The existing D:/Repos/UnrealEngineAngelscript checkout is upgraded in place to UE5.8.4,
based on Hazelight d811c292432fe125ed597ad704653750b2977e6f. Publication is independently
verified by `ls-remote`: origin/main-ck is now
`55c44c498c272fcc1f911d1c90e9885184b026f3`, replacing the approved previous tip
`7a4ad7f53ee2d472c778bc7748ca753f396a9108`. The exact approved lease retry reported that
the ref already matched the candidate; the independent remote query is the publication authority.
The original rebase checkpoint was66679d6b2d5c78413e56e08aeda8af95630ec227 with54 retained
local commits. That checkpoint and later publication-candidate receipts are historical states.

User approved the upgrade, narrow Iris stash exception, temporary serialized builds and exact
lease-protected publication. Build remains exclusive through tracked background writes;
QueryTargets sharing and UE5.8 scheduling remain. Native Toon GPU13 / CK raster14 are separate;
serialized CK enum13 remains. Protected unrelated edits and earlier material work are preserved.
Project association/storage profile are unchanged; no alternate checkout, cleanup, cook or package.

The focused C# gate passes83/83 in35s. Native BSDF incremental Build2 passes, and one offscreen
real-RHI CkUsf gate passes67/67 in3m13s. Temporary diagnostic config was restored byte-for-byte.
Native showcase asset preparation and rendered proof are still pending; these gates do not establish
native Slab/Toon showcase acceptance. The native gate budget announced up to3 discovery/gate boots
plus1 offscreen showcase boot; only1 native gate boot was actually used so far.

## Build and focused gates

- ShaderCompileWorker rebuilt successfully (286 actions).
- Editor build11 succeeded (365 final actions;431.28seconds). Earlier attempts progressively
  compiled the rest of the new engine/project; no outputs were cleared.
- Final Toolbox incremental check exited0, up to date with0 actions. Log
  Saved/Logs/Engine584-FinalBuild.log; direct exit receipt FinalBuild-process.json.
- Seven fresh focused Unreal gates passed103/103, zero failed/skipped/contaminated, no incomplete
  runs or AS_COMPILE_FAILED. Raw verdicts used --known-reds off. These are focused results,
  not repository-wide regression evidence.

| Gate | Passed | Evidence under Saved/Logs |
| --- | ---: | --- |
| CkUsf, real RHI | 61 | Engine584-CkUsf-Gate2.log |
| CkCore, including double limits and signal tests | 31 | Engine584-CkCore-Gate.log |
| AngelScript CppForm | 1 | Engine584-AS-CppForm-Gate.log |
| GitLink provider availability | 1 | Engine584-GitLink-Provider-Gate.log |
| PixelSprite manifest conformance | 1 | Engine584-PixelSprite-Manifest-Gate.log |
| PerfLab session codec | 7 | Engine584-PerfLab-Codec-Gate.log |
| Messaging cross-module delegate bind/broadcast/unbind | 1 | Engine584-Messaging-Unbind-Gate.log |

CkUsf covers generated usable masters, same-MID .ush reload, maintained CK Toon pipeline and
lighting reload, and legacy unversioned shading-model serialization. It does not establish
new upstream native Toon or explicit BSDF showcase acceptance. The later native slice has source
examples and focused coverage; persistent assets and rendered showcase proof remain pending.

Machine-readable reconciliation: Saved/EngineUpgrade/20261020-UE584/Unreal-gates-final.json.
Focused-gates.jsonl records all six headless Toolbox process exit codes; the CkUsf log records
editor exit0 and complete61/61. All owned editor/Toolbox/shader-worker processes exited.

## Compatibility fixes and reviews

- Double numeric-limit fields: float -> double; finite exact extrema/reflection test added.
- GitLink5.8 provider interface: guarded old/new overrides, unchanged change predicate, unknown
  incoming status, full revert flag; version0.8.3/23 and offline provider test.
- Eleven canonical StructUtils includes replace deprecated wrappers; both paths exist in5.7.4.
  Removed-header audit checked602 public headers against7127 plugin sources with no remaining hits.
- Signal macros: remove previously ignored DLL attributes from six empty header-only wrappers.
  Moving attributes after class keys initially exposed44 imported template registrar link failures;
  Astra traced the ownership mismatch. Final correction preserves named types, signatures and
  registrations. Overlay DLL now links and native plus cross-module AS signal tests pass.
- Direct ensure/format headers in debugger style; direct formatter in inline crowd algorithm;
  direct MaterialShared definition in shadow test. No ensure policy/body or warning suppression.
- Explicit length-preserving JSON-key conversion at PixelSprite and PerfLab FString map boundaries.
- CkIskm VF .ush: implemented-hit-proxy marker suppresses new5.8 default-interface duplicate,
  preserving custom zero-hit-proxy behavior. Default material shader compilation now succeeds.

The first CkUsf invocation ran zero tests: renderer discovery idle-timed-out during active cold
shader compilation, then two resumed test boots failed on the VF shader duplicate. The later
successful gate is the evidence of record. Real shader-job diagnostics were temporarily enabled
for the retry and restored byte-for-byte; Config/DefaultEngine.ini SHA256 is
20D012DFF0397DA2198FB8AF0E7B19DADE2F8F204D10C54340F80C5EEB579438.
Engine shader fixtures have no remaining diff. ShaderDiagnostics-restored.json records recovery.
Toolbox rebuilt both discovery lists on retry; actual boots were4 in the failed invocation,
3 in the successful invocation, then6 focused headless boots (13 total). No further boot is active.

## Preserved work and remaining limits

All156 preserved dependency files match their recovery hashes; all18 diorama master/texture
binaries match the retained pre-upgrade receipt. These are file-preservation checks, not UE5.8
visual approval. The Iris diff remains exactly the original ensure deletion; exact old file,
patch, recovery ref and stash are retained. Unrelated credential/local engine data was not read.

The later user-approved narrow C# runner compiled and passed83/83 focused tests, including the52
lock/writer cases, in35s. This supersedes the earlier unexecuted-test checkpoint. Actual concurrent
build/generation scenarios remain outside that focused gate; no concurrency-performance claim.

Startup ValidatePlatforms cannot obtain exclusive admission while Toolbox holds its shared
editor lock. Astra verified its registration preamble can mutate SDK installations/manifests:
do not add SharedEngineLock or WaitMutex as a one-line fix. The editor continues; the103 tests
pass, but SDK-status validation remains unavailable in that situation. A read-only validation
path would need separate write-path review and scope approval.

Native BSDF/Toon source examples and focused tests are implemented; persistent showcase assets
and rendered acceptance remain pending. Material-gym visual acceptance on5.8, packaged behavior,
performance and full-suite coverage remain unverified. Preserve Blendable, the logo-free gym,
texture-backed .ush authoring and the existing material library. Engine publication was separately
approved and independently verified as recorded above.

## Recovery and target

- Original HEAD: e628343442a5724ede61a61177fd4a073386110d.
- Original branch: bugfix/selected-project-binaries.
- Recovery ref: refs/backup/ue584-pre-upgrade-20261020.
- Hazelight target: d811c292432fe125ed597ad704653750b2977e6f, verified Build.version 5.8.4.
- 57 local commits replayed from merge base 082479e2b6516f3bd702d1cee30b111ec8f9a501.
- Saved/EngineUpgrade/20261020-UE584 contains exact Iris file, binary patch, stash object ID,
  preflight JSON and current rebase todo/done receipts. Keep the stash until semantic restoration
  is verified; do not overwrite the 5.8 file with the old full-file copy.
- Iris opening SHA256: 69813456749ab1e6ace335a81231761f589552adbf5bdeb00949d7f2fe990c92.
- Untracked engine credential files, BuildGraph and Saved_ParityEditor directories are untouched.

## Reviewed conflict ledger

| Original commit | Resolution |
| --- | --- |
| 4a052b1232ad | Keep upstream stack-walk re-entry guard and CK profiling scope. |
| 53d697f3ace6 | Native 5.8 deferred-tooltip implementation and both call sites already supply the backport behavior; retain upstream, commit becomes empty. |
| d79df8f228a5 | Keep CK platform allow-list with upstream UE_LOGF call. |
| d063dd8c10ec | Upstream scalar/vector null-context guards already supply the fix; retain upstream, commit becomes empty. |
| 3c427a2b7cdd | Keep upstream randomized temp retry and dependency backup; keep CK unchanged-DLL timestamp restoration. Sol independently reviewed. |
| 78bd170230d6 | Merge ownerless registration with new CreationMethod, bAutoRegister and pool flags. Followup fixes remove the new actor-only early return and register an existing root with the component world; these followups are included in the published port. |
| 280eb65e45df | Keep CK path-colored badges, upstream tooltip/context menu/clipboard wrapper; Sol reviewed. Original path tooltip omitted, visible path retained. |
| a4f69c610bb4 / 66bfe15c220c / 0277e892f916 | Preserve upstream final-property size formula through the historical fix/revert/revert sequence; final CK type-named assertions retained. |
| a625e1140304 | Keep new project cook-version support, assign fresh combined schema GUID; no cook executed. |
| 0b36f2621b91 | Upstream TMap already guards native/generic nesting together; preserve it. Retain CK iterator safety helper, upstream global bindings and CK tests. |
| effb70976adf | Stable project-name identity with upstream ordinal comparison and VFS lookup list. |
| dd2d99974cd7 | Stable UHT IDs in both binary and memory settings writers and binary reader; keep upstream stale-settings exception handling and new scheduling. |
| 5f05b0cfbbf8 | Approved whole-Build exclusive protection, new IDisposable counted leases, atomic rules publication and background-writer drain on success/failure. Astra reviewed; build and focused runtime gates passed; later focused C# execution passes83/83. |
| 86895dfdd83d | Preserve protocol-2 primitives, atomic metadata and query admission; do not transplant old phased Build scheduling or transient UHT scopes into 5.8. Pure partition helpers retained for existing unit coverage. |
| 63a865e25c4d | Defer project-only UHT lock optimization and its specific test under approved serialized-build policy; patch becomes empty. |
| 8a664fe314ec | Preserve QueryTargets turnstile bypass with new static mode options; adapt rules mutex use and initial-versus-upgrade wait policy. |
| dde876d318b3 | Preserve both fc null-stdin fixes and upstream up-to-date message. |
| c68d75b0cec2 | Separate native GPU Toon 13 / CK GPU 14 from serialized CK enum 13; map legacy/MIR emissions, shared topology guard, cache version 2 and conservative ray/path fallback. Astra reviewed; CK Toon reload/serialization gates pass; upstream native Toon remains unverified. |

Other replayed commits applied without text conflict. Range-diff is saved and reconciled against
this ledger. Source review is not compile or runtime verification. Tooltip/MPC fixes are upstream
equivalents; the UHT optimization is explicitly deferred, not claimed upstream-equivalent.

## Decisions and required followups

Astra found process-wide transient engine-lock downgrade unsafe with overlapping 5.8 writers.
Recommended bounded compatibility route: Build-mode sticky exclusive protection before scheduling,
held through asynchronous writer completion and failure cleanup. Preserve QueryTargets sharing;
serializing every mode can deadlock editor queries. User approved temporary serialized builds.
The integrated port keeps Build exclusive before scheduling, tracks background writer creation,
drains before successful return and on failure before releasing protection. Source review is
complete; editor build and focused runtime gates passed. The later83/83 C# gate includes the52
lock/writer tests. Cross-project build concurrency is intentionally
restricted until a dedicated concurrent-lock port is implemented.

Native Substrate Toon GPU ID 13 collides with CK raster ID 13. Preserve native 13, serialized
MSM_CkUsf 13, and assign separate CK raster ID 14 with NUM 15. Explicit CPU-to-GPU mapping is
needed in legacy and MIR translators, plus cache-version bump. Preserve upstream path-tracing
namespace and CK conservative fallback. This is implemented and source-reviewed. Compilation, legacy serialization and CK Toon live-reload
gates pass. New upstream native Toon rendering has not been exercised.

Iris semantic deletion is restored separately and the recovery stash is retained. Its diff is
exactly the original ensure deletion; the surrounding 5.8 file remains intact. Lock tests are
migrated to the actual 5.8 UnrealBuildTool.Tests project, retaining 48 methods, with four new
background-writer tests. These are included in the published port and the later83/83 C# pass.

Dependency sync uses GitDependencies directly with modified-file overwrites refused, cache-days
365000 to preserve existing cache entries, and non-Windows/templates/sample folders excluded.
The original full dry-run and scoped sync logs are saved in the recovery directory. Sync reported
modified WorldGridMaterial and Flutter files and preserves them; it completed with exit 0.
Orphan MSBuild node workers were identified; no active engine
editor/build was found and no process was stopped.

Inspected setup:
Setup.bat includes hook replacement, redistributable installation and registration in addition to
GitDependencies, so do not invoke it blindly. Build/test through host UnrealToolbox only, with
fresh process preflight, incremental outputs, offscreen validation and explicit boot-count plan.

## Publication and native first-slice followup (2026-10-21 UTC)

Independent `ls-remote` verification establishes origin/main-ck at
`55c44c498c272fcc1f911d1c90e9885184b026f3`. The previous remote tip was
`7a4ad7f53ee2d472c778bc7748ca753f396a9108`. The user-approved exact lease retry reported
that the ref already matched the candidate; successful remote verification, not that retry alone,
is the delivery evidence. `Publication-candidate.json` and `Metadata-reword-receipt.json` retain
earlier candidate/recovery states and must not be read as current publication status.

The reconciled remote additions preserve binary-user protections, shared game-project generation
and warm-server descendant coverage. Build remains exclusive under the approved temporary policy.
The binary probe fails before outdated-output deletion; generation retains lock ordering, atomic
engine metadata and the UE5.8 engine-plugin props writer. Actual concurrent generation and
shared-parent stale-rules admission remain separate limits; no concurrency-performance claim.
Nine inherited attribution trailers were removed by metadata-only replay with source-tree equality
checked. Protected Iris and Flutter edits were not included in publication.

The approved focused C# runner completed83/83, zero failed/skipped, in35s:
`Saved/Logs/Engine584-CSharp-Focused2.log` and
`Saved/EngineUpgrade/20261020-UE584/CSharpResults/Engine584-FocusedUBT.trx`.
Coverage comprises48 lock,4 background-writer,24 binary-user and7 project-files methods.

Native BSDF Build2 exited0 with zero editor boots:
`Saved/Logs/NativeBsdf-Build2.log` and `NativeBsdf-Build2-process.json` in the recovery directory.
The fresh offscreen real-RHI focused CkUsf gate passes67/67, zero failed/skipped/contaminated,
actual exit0,3m13s: `Saved/Logs/NativeBsdf-CkUsf-Gate1.log` and `NativeBsdf-Gate1-process.json`.
It includes the six native BSDF contract/topology/reload tests and compatible existing CkUsf coverage.
Temporary diagnostic config is restored byte-for-byte. The announced allowance was up to3
fresh discovery/gate boots plus1 offscreen showcase; only1 native gate boot was consumed.

Native Slab/Toon source examples and initial-only preparation tooling are authored. Persistent
native profile/LookDefinition/master assets, matched captures and moving-light visual proof remain
pending. Numeric .ush values feed engine-owned BSDF expressions; Toon ramps/hatching remain
profile asset data. Existing legacy selection and approved logo-free presentation are preserved.
