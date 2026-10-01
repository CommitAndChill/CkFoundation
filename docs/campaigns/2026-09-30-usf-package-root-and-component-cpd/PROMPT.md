# PROMPT — per-look generated package root (F1) + component custom-primitive-data request (F2)

> As of 2026-09-30, CkFoundation `dev` @ `e90e4fdbf`, CkTests `dev` @ `feddf883`, host project Mars (`D:\Repo\Mars`).
> This doc dies when both phases are merged; the durable record is each module's `Claude.md`.

## Why

Mars is adding a game-owned CkUsf look (a character "eye plate") driven per character through custom
primitive data on an entity-owned mesh component. Two framework gaps block doing that inside the contract:

1. **F1.** A look's generated master is always written under `/CkFoundation/CkUsf/GeneratedLooks`
   (`Source/CkUsf/Public/CkUsf/LookDefinition/CkUsf_LookDefinition_Naming.h:8-21`; runtime lookup
   `Source/CkUsf/Public/CkUsf/Apply/CkUsf_Utils.cpp:31`). A game look therefore commits a `.uasset` into the
   framework repo. Five such orphans are already there (`CameoReveal`, `CharacterMaster`,
   `CharacterMasterBatched`, `IskmBatchedDebugHue`, `LoreGlow` have no look definition in this repo).
2. **F2.** `CkUnrealComponent` has no request to write custom primitive data on the component it hosts
   (`CkUnrealComponent_Utils.h`), although `FCk_CustomPrimitiveData` exists in `CkGraphics_Common.h:121-144`
   and both `CkIsmRenderer` and `CkIskmRenderer` have their own request for it. A feature must resolve the
   component itself and tolerate it being null until setup has run.

Both are **class 2 (additive API)** under `ck-change-control`: no existing behaviour may change.

## Success criteria (observations)

- Every look that exists today resolves and regenerates to exactly the path it had before.
- A look definition with `_GeneratedPackageRoot = "/Game/X/Y"` generates to and resolves from
  `/Game/X/Y/M_CkUsf_Look_<Name>`.
- `utils_unreal_component::Request_SetCustomPrimitiveData` issued in the same step as `Add` lands on the
  component once it exists; invalid input is rejected loudly with no write.
- Named tests in each phase doc are green under the toolbox; baseline failing names are unchanged.

## Locked decisions (do not relitigate; a question goes to the planner via PROGRESS.md Blockers)

| # | Decision |
|---|---|
| 1 | F1 is a field on `UCkUsf_LookDefinition`, not a project setting and not a second naming scheme. Empty means the framework root. |
| 2 | The tests-only `InPackageRootOverride` keeps winning over the definition's root, so parallel test lanes stay isolated. |
| 3 | Name-only callers of `ck::usf::Get_GeneratedMaster*Path(FName)` (stylize subsystems, CkVat, CkPixelArt, CkParticles' mirrored convention) are NOT changed. They address framework looks, which keep the framework root. |
| 4 | F2 lives in `CkUnrealComponent` and reuses `FCk_CustomPrimitiveData` from `CkGraphics`. No new value type. No material/MID API in this change. |
| 5 | F2 is a deferred request with the standard completion contract (root `CLAUDE.md` § Request completion; canonical reference `CkTimer`). A request made before the component is set up is held and applied after setup, never dropped and never failed. |
| 6 | One request struct carries one `FCk_CustomPrimitiveData` (the `FCk_Request_IsmProxy_SetCustomPrimitiveData` shape). No array form, no scalar convenience overload. |
| 7 | Tests live in CkTests. C++ tests use the `CkTests.UnitTests.*` pretty-name family. |

## Non-goals

Moving the five orphan masters; a generic texture baker; a material-parameter request; changing `Ck_Usf_GenerateLooks`' arguments; any `CkIsmRenderer`/`CkIskmRenderer` change.

## Rejected approaches

| Approach | Why it was killed |
|---|---|
| Keep generating game looks into the framework root (current practice) | Puts game content in the framework repo; every host inherits other games' masters |
| Project-settings root for all looks | A host uses framework looks and its own at once; one global root cannot express both |
| Write CPD from game code on the resolved component | Each feature re-implements the "component not created yet" wait; the request lane is where two renderers already put this |
| Put the CPD writer in `CkGraphics` as a plain utility | It would take a raw component and bypass the entity; the gap is specifically entity-owned components |

## Reading list (before writing anything)

1. `Plugins/CkFoundation/CLAUDE.md` (style, ensure rules, request completion) — binding.
2. `Source/CLAUDE.md` § "Request_* takes the request STRUCT", module-authoring rules.
3. F1: `Source/CkUsf/Claude.md` (first 240 lines), `CkUsf_LookDefinition.h`, `CkUsf_LookDefinition_Naming.h`,
   `CkUsfEditor/.../Generator/CkUsf_Generator.cpp` (~420-460, ~800-890), `CkUsf_LookValidator.cpp` (~360-480),
   `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkUsf/Test_Usf_GeneratesUsableMasters.cpp`, `CkUsf_TestLookMasters.h`.
4. F2: the whole `Source/CkUnrealComponent/Public/CkUnrealComponent/` folder; the CkTimer quartet
   (`Source/CkTimer/Public/CkTimer/`) for the request/completion shape;
   `CkIsmRenderer/.../Proxy/CkIsmProxy_Fragment_Data.h:118-139` and its handler in `CkIsmProxy_Processor.cpp` (~797-812);
   `CkGraphics_Common.h:1-145`.
5. Skills: `ck-macros-and-codegen` before adding the request/fragment/processor; `ck-change-control` for the gate.

## Things ruled out

| Trap | Why |
|---|---|
| Do not invoke `Build.bat`, UBT or `UnrealEditor-Cmd` | The `/build-test` skill owns builds. In this campaign the executor does NOT build at all; the planner runs the gate. |
| Do not commit, push, stage or stash | The planner hands the user a staging list. |
| Do not use Grep/Glob under `Script/`, `docs/`, `Content/` | A repo `.ignore` hides them; use `rg --no-ignore` in Bash. |
| Do not use stock `ensure`/`check`, `!`, `std::move`, anonymous namespaces, file-local statics | Root `CLAUDE.md`. |
| Do not add comments that name this campaign, a phase or "F1/F2" | Root `CLAUDE.md` § Comments. |

## Glossary

**Look** = one `.ush` entry point + one `UCkUsf_LookDefinition`. **Master** = the generated `UMaterial`
`M_CkUsf_Look_<Name>`. **CPD** = custom primitive data (36 floats per primitive, read by material parameter
nodes with `bUseCustomPrimitiveData`). **Planner** = the session orchestrating this work; executors report to it.
