# Gate 2: request and persistence contracts

## Work and verification

- GOAP missing required keys -> ensure and reject the requested goal; test intentionally empty goals separately.
- GOAP tuning requests -> implement the documented operation or resolve its intended API explicitly;
  test changed runtime behavior and completion, never a warning plus false success.
- Construction batches -> prevalidate all entries before any script runs; test zero mutations on rejection.
- EntityTag hydration -> reject malformed payloads atomically and reconcile both tag representations;
  test malformed and valid round trips through the production persistence handler.
- External settings setters and confirmation windows -> reject before state/events; test all typed families
  and nonfinite durations while preserving accepted/idempotent behavior.
- Test request parser and dirty-world policy -> reject malformed batches and preserve user work;
  test actual processing/admission boundaries.

## Exit

Focused regressions and compatible siblings pass on the final code; module docs reflect confirmed contracts.
Record API choices and all remaining limitations in PROGRESS.md before advancing.
