# The non-negotiables — rationale and required evidence

Reference for `ck-change-control`: each non-negotiable with why it exists and exactly what evidence satisfies it. Find your row in the classification table in SKILL.md first.

## The non-negotiables (rationale + evidence)

The one-line rules are root `DEVELOPMENT.md` non-negotiables #1–#4. This section carries the *why* and
the evidence, so you can defend the rule instead of just obeying it.

### 1. Fire `CK_ENSURE_IF_NOT` — never stock `ensure`/`ensureMsgf`/`check` for recoverable validation

Compile modes, verified at `Source/CkCore/Public/CkCore/Ensure/CkEnsure.h:62-81` + the define
matrix in `Source/CkBuildConfig/CkBuildConfig.Build.cs`:

The full define matrix and expansion mechanics are owned by `ck-macros-and-codegen` §2.4. In the
current `CkBuildConfig.Build.cs`, `CK_DISABLE_ENSURE_CHECKS=0` in every standard configuration,
including Shipping. Test and Shipping also set `_DEBUGGING=1`, so the predicate and recovery run
silently. This is the current configuration, not the intended Shipping policy: Shipping should omit
the ensure branch unless a documented crash or deadline exception explicitly enables it.

Why this macro is the mandate:

- **It owns the failure branch.** The predicate and recovery belong to the same inverted `if`.
  Current Shipping configuration retains that branch; the intended default is to omit it unless
  a documented crash or deadline exception calls for a shipped guard.
- **It is loud with context.** fmt-style message with the failing values, plus C++ AND Blueprint
  AND AngelScript callstacks, frame number, PIE-ID, server/client attribution
  (`CkEnsure.cpp:92+`). `PLATFORM_BREAK()` is expanded textually at the call site so the debugger
  lands in *your* frame, not a wrapper (`CkEnsure.h:34-35`).
- **It cannot spam.** Fires are counted per file:line (`CkEnsure.cpp:108`) against an ignore list
  (`:110-111`); log-only display policies auto-ignore the site after the first fire, and the
  editor dialog offers per-site ignore.
- **It fails your test gate.** The AutoTest runner forces LogOnly display
  (comment `CkEnsure.cpp:217-219`), so a fired ensure logs at **Error** severity — which the
  automation framework reports as a test failure. Ensures are visible in CI; warnings are not.
- **Deliberate shipped tripwires can catch real bugs.** The packaged-build GC crash was diagnosed
  (`d77810096`), root-caused (`feb08ee94` — pre-GC rooting pass), and then fenced with a tripwire
  ensure that stays in the build (`a8a93baac`). Detection survived the fix.

The `{ ... }` body owns rejection and recovery: roll back partial state, complete a rejected
request, or perform another approved recovery before returning as the operation requires. Compute
a side-effect-safe predicate once. Never use an empty ensure body followed by an ordinary `if`
that repeats the predicate merely to keep recovery enabled. In the current Test and Shipping
configuration the body runs silently; with checks disabled it is omitted. Simple bail-out shape
(`Source/CkEcs/Public/CkEcs/EntityLifetime/CkEntityLifetime_Utils.cpp:114-117`):

```cpp
CK_ENSURE_IF_NOT(InHandle.Has<ck::FFragment_LifetimeOwner>(),
    TEXT("The Entity [{}] does NOT have a LifetimeOwner. Was this Entity created by Request_CreateEntity(RegistryType)?"),
    InHandle)
{ return {}; }
```

Variants for other shapes: `CK_TRIGGER_ENSURE` (unconditional), `CK_TRIGGER_ENSURE_IF`,
`CK_INVALID_ENUM` (unreachable switch defaults), `CK_ENSURE_VALID_IF_NOT(_MSG)`
(`CkEnsure.h:68-85`). Full macro mechanics: `ck-macros-and-codegen`.

Self-review grep before submitting (Git Bash, cwd `Plugins/CkFoundation`):

```bash
rg --no-ignore -n '\b(ensure|ensureMsgf|ensureAlways|check|checkf)\s*\(' <your changed files>
```

Any hit you introduced in runtime Ck code is a review rejection unless engine-forced.

### 2. Never silently handle an error

Maintainer's direct statement (2026-07-02, recorded in `.claude/reports/DECISIONS.md` §22): the
mandate is to fire an ensure rather than silently ignore the error behind a Warning/Error log,
because **logs get ignored, ensures don't**. A log-and-continue where validation failed is a
review rejection — and so is any fallback that hides a problem (root `DEVELOPMENT.md` #3).

Decision table when a check fails:

| Situation | Do |
|---|---|
| Recoverable precondition violation (bad handle, missing fragment, invalid params) | `CK_ENSURE_IF_NOT` with rollback, completion, or approved recovery in its body, then terminate the rejected operation |
| Unreachable branch (switch default on an enum) | `CK_INVALID_ENUM` / `CK_TRIGGER_ENSURE` |
| Legitimate, expected absence | `TryGet_*` contract: return the invalid handle quietly, **no ensure** — absence is not an error. (`Get_*` must not fail; `TryGet_*` may. Root `DEVELOPMENT.md` naming rules.) |
| "Log a warning and use a default" | Never, unless the maintainer explicitly requested a fallback |

### 3. Research first — the reading ritual

Maintainer's direct statement (recorded in `DECISIONS.md` §23): incoming sessions "don't research
the codebase enough". The test from root `DEVELOPMENT.md` #1: **if your change doesn't resemble the
code around it, you have not researched enough.**

For an ECS change, read IN THIS ORDER before writing anything:

1. Root `Plugins/CkFoundation/DEVELOPMENT.md` — re-read the style block and non-negotiables.
2. `Source/ARCHITECTURE.md` — locate your module via the "I need to…" decision tree; confirm dependency
   tier discipline (deps only point to same-or-lower tiers).
3. `Source/<TargetModule>/Claude.md` — purpose, key API, anti-patterns. Some are stale
   (`DECISIONS.md` §15): trust code over doc on conflict and note the drift.
4. The nearest sibling feature's **quartet** — `CkTimer` is the canonical small exemplar
   (`Source/CkTimer/Public/CkTimer/`), in this order:
   - `CkTimer_Fragment_Data.h` — reflected surface: the Spec struct, requests, typed handle, delegates
   - `CkTimer_Fragment.h` — runtime fragments, tags, signals, record-of-children
   - `CkTimer_Processor.h` + `.cpp` — phases, `CK_REGISTER_PROCESSOR`, request draining
   - `CkTimer_Utils.h` + `.cpp` — the only public API surface
   (When scaffolding a whole module, also mimic the module-root `CkTimer_Log.h/.cpp` and
   `CkTimer_Module.cpp/.h`.)
5. Macro mechanics you're about to use → `ck-macros-and-codegen`. Architecture invariants you're
   about to lean on → `ckecs-architecture-contract`.

Then mimic. In your plan, cite what you read (file:line) — mimicry of adjacent code beats
invention, and reviewers check.

### 4. Three environments: C++, Blueprint, AngelScript

Root `DEVELOPMENT.md` #4: every public API must work — and be **verified** — in all three. "Works in
C++" is one third of done. What each environment's check actually is:

| Env | "Verified" means | How |
|---|---|---|
| C++ | Compiles AND is exercised — a test calls it | Host editor build (`ck-build-and-env`) + tests (`ck-tests-authoring-and-running`) |
| Blueprint | The reflected surface renders and behaves in the BP editor | `[EDITOR-VERIFY]` checklist below — agents cannot launch the editor; a human runs it |
| AngelScript | The binding registers at editor boot and the call form works at runtime | Headless boot + log grep (shape below); full recipe + the silent-break catalog: `ck-angelscript-interop` |

One API in all three (Timer exemplar):

```cpp
// C++ — CkTimer_Utils.h:53-56
auto TimerHandle = UCk_Utils_Timer_UE::Add(InHandle, TimerParams);
```

- Blueprint: node **"[Ck][Timer] Add New Timer"** (DisplayName; Category `Ck|Utils|<Feature>` for
  the public surface, `Ck|BLUEPRINT_INTERNAL_USE_ONLY` for plumbing — both real, `CkTimer_Utils.h:50-68`).

```angelscript
// AngelScript — generated wrapper, Script/Generated/utils_timer.as:265-270
auto TimerHandle = utils_timer::Add(Handle, TimerParams);
```

#### `[EDITOR-VERIFY]` Blueprint checklist (exact steps — human-only)

1. Build the host editor and launch it (worked example BusterBlock; commands: `ck-build-and-env`).
2. Toolbar → Blueprints dropdown → Open Level Blueprint (any BP graph works).
3. Right-click the graph → type `[Ck][<Feature>]` → your new node appears under its DisplayName
   (untick "Context Sensitive" if hunting). Title must render as `[Ck][<Feature>] <Action>` — a
   missing prefix is a style bug.
4. Place the node. Every UENUM parameter renders as a dropdown listing all enumerators.
5. For functions with `meta = (ExpandEnumAsExecs = "OutResult")` (104 header call sites as of
   2026-07-02): the node shows one exec **output** pin per enum entry — e.g. `[Ck][Timer] Cast`
   shows `Succeeded` + `Failed` pins (`CkTimer_Utils.h:99-106`).
6. Request struct visibility: right-click → search `Make FCk_Request_<Feature>_<Action>` → the
   Make node exists and lists the private `_`-prefixed UPROPERTYs (BP-visible via
   `AllowPrivateAccess`). Alternatively add a BP variable of the struct type and inspect Details.
7. EditCondition behavior: open a Details panel showing a struct whose fields carry
   `EditCondition` (exemplar: `FCk_FloatAttribute_Magnitude`,
   `CkAttribute/Public/CkAttribute/FloatAttribute/CkFloatAttribute_Fragment_Data.h:85-96` — this
   codebase usually pairs it with `EditConditionHides`). Flip the driving field
   (`_CalculationMode`): dependent fields must appear/disappear (or enable/disable without
   `Hides`).
8. If you added a typed handle: the `<As<Feature>>` autocast node appears, and a generic
   `FCk_Handle` wire auto-converts into the typed pin (`BlueprintAutocast` on the CastChecked
   surface — §2.7 machinery in `ck-macros-and-codegen`).
9. If you added a signal: the generated `BindTo_On<X>` node appears and accepts your delegate type.

Report the checklist as per-line pass/fail in your change summary.

#### AngelScript verification — the one-line shape

Boot the host editor headless so AS compiles, quit, then grep the **fresh** log (PowerShell, host
project root — BusterBlock worked example):

```powershell
& "Binaries\Win64\BusterBlockEditor-Cmd.exe" "$PWD\BusterBlock.uproject" -skipcompile -nullrhi -unattended -nosplash -ExecCmds="Quit"
rg -n "Angelscript: Error" Saved\Logs\BusterBlock.log
```

`-skipcompile` suppresses only the boot-time UBT/C++ compile (AS still compiles in-process, so the
gate is unaffected); without it, a canceled boot can orphan UBT and delete module DLLs in
multi-session environments — `ck-build-and-env` §4 owns the boot shape (trap T3).

Exact flags, the multi-instance generator lock, and why a clean boot is NOT enough (e.g. a raw
handle in an AS f-string throws only at PIE/exec time — `Script/ARCHITECTURE.md` §22 item 1): all owned
by `ck-angelscript-interop`. If the change is callable from AS, exercise it at runtime (AutoTest or
PIE), not just at boot.

