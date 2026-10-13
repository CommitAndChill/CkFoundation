# CkFoundation instructions

These rules apply throughout CkFoundation and to CK framework changes in its consumers.
Read [DEVELOPMENT.md](DEVELOPMENT.md) before changing framework code. It owns style, naming,
ECS patterns, requests, signals, and collaboration rules. Read
[Source/ARCHITECTURE.md](Source/ARCHITECTURE.md) for C++ topology and the target module's
`Claude.md`; read [Script/ARCHITECTURE.md](Script/ARCHITECTURE.md) for AngelScript.

## CK_ENSURE_IF_NOT

- The macro is an inverted `if`: put the failure return, rollback, completion notification,
  or approved recovery directly in its body. Evaluate a side-effect-safe condition once.
- Never write an empty ensure body followed by a separate ordinary `if` repeating the
  same condition. Older history, memory, or skills recommending that shape are superseded.
- Checks belong in all non-Shipping builds. The intended Shipping policy skips both the
  predicate and body (`CK_DISABLE_ENSURE_CHECKS=1` expands to `if constexpr(false)`). Shipping
  may explicitly keep checks enabled when needed to avoid crashes under delivery constraints.
- Current build flags retain checks in Shipping with diagnostics disabled. Verify the flags
  in `Source/CkBuildConfig/CkBuildConfig.Build.cs`; change Shipping policy only on an explicit
  maintainer decision. Do not add unconditional recovery or alter the macro to defeat omission.
- Validate prerequisites one at a time. Every evaluated predicate must be safe for malformed,
  default, stale, and partially initialized input; an ensure does not make its expression safe.

```cpp
const auto IsVelocityFinite = NOT Motion.Get_Velocity().ContainsNaN();
CK_ENSURE_IF_NOT(IsVelocityFinite, TEXT("Sampled a non-finite velocity"))
{ Motion = gait::Get_RestMotion(); }
```

## Skill discovery

The skill bodies and their references are maintained in `.claude/skills/`. They use the shared
`SKILL.md` format. Codex uses the thin `.agents/skills/` wrappers in the consuming project root;
CkPlugins supplies wrappers for all 27 Foundation skills. Read the repository-local target in
full and resolve references from that target's directory. Prefer it over global shortcuts or
copies. If working directly in this submodule without host wrappers, consult the skill index
in DEVELOPMENT.md and read the relevant `.claude/skills/<name>/SKILL.md` explicitly.

## Code Review Rules

- Reject an empty `CK_ENSURE_IF_NOT` body followed by an ordinary `if` repeating its condition.
- Confirm rejection/recovery and request failure completion share the macro's body.
- Confirm evaluated prerequisites are safe before dereferencing or accessing deeper state.
- Distinguish the intended Shipping policy from the current flags; no build-policy changes
  are implied by a validation fix.
