# Tenet audit repairs

Written 2026-10-22. Current evidence and decisions live in PROGRESS.md. This document is retired
when the repairs are delivered and the permanent module contracts describe the resulting behavior.

## Goal

Fix the confirmed CkFoundation, CkTests, and CkGameplayDebugger audit findings, starting with
ownership integrity, test-result integrity, and settings persistence. Rejected operations must
not publish partial success or mutate unrelated state. Failures must reach their consumers.

## Constraints

- Follow Foundation CLAUDE.md, Source/CLAUDE.md, module guides, and RESILIENCE_TENETS.md.
- CK_ENSURE_IF_NOT owns validation and its failure body. Validation-only work disappears with
  the macro in intended Shipping. No duplicate guards, predicate side effects, or flag changes.
- Work only in the selected checkout; preserve unrelated changes and the prior instruction edits.
- Builds/tests run only through this checkout's UnrealToolbox. No clean rebuild or artifact deletion.
- Material API, compatibility, persistence, and lifetime decisions require maintainer agreement.
- No project commit or push is requested. Shared settings publication was completed separately.

## Confirmed scope

- Ownership transfer admission, cycles, same-registry reverse-link integrity, safe traversal.
- Test bridge false success, lost result publication, malformed request subsets, unsafe dirty-map exemption.
- Settings unreadable-file preservation, truthful outcomes, missing external setters, invalid confirm windows.
- GOAP unresolved required keys and unimplemented runtime tuning requests.
- Construction-batch all-entry validation before execution.
- EntityTag malformed hydration and inconsistent restored gameplay-tag state.
- Debug profile selection/lifecycle and HangMonitor cancellation during shutdown.
- Confirmed duplicate ensure guards; search candidates require individual verification before editing.

## Maintainer clarification

The shared invalid-fragment fallback in reference-returning helpers is intentional: the ensure fires,
and a developer may choose to continue with the invalid fragment. Withdraw the sentinel audit finding;
do not migrate these primitives to pointer/result APIs as part of this repair.

## Success criteria

1. Focused production-path regressions prove rejection, no downstream mutation/callback, and no partial state.
2. Valid sibling behavior, pending-destruction ownership, and cross-registry forward ownership remain supported.
3. Fresh Toolbox evidence covers the changed modules; failures and unavailable checks are explicitly recorded.
4. No remaining confirmed finding is silently omitted or described as resolved without evidence.

## References

Existing ownership setup and CkRegistry_TransientIdentity.spec.cpp supply lifecycle fixtures;
CkGameSettings_Store.spec.cpp supplies provider tests; the bridge's request processor and subsystem
own its protocol/lifetime. See PLAN.md for gates.
