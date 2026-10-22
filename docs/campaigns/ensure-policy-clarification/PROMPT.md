# Ensure policy clarification

Maintainer decision: 2026-10-22. Scope: active instructions, skill references, and macro documentation.

`CK_ENSURE_IF_NOT` intentionally omits predicate evaluation and its failure body in Shipping for
performance. An unfixed violation may crash. Retaining checks is an explicit release exception,
not the call-site contract. Failure completion, rollback, and return belong inside the macro body;
never add a duplicate ordinary guard. Validation-only work must also be omitted.

Done means the shared instructions are reviewed and pushed, active contradictory guidance is
corrected, and documentation checks pass. Runtime behavior, build flags, historical campaigns,
and the audit's unresolved GOAP/construction policy questions are outside this change.
