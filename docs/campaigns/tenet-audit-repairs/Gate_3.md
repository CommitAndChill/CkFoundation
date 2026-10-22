# Gate 3: debugger lifecycle and final conformance

## Work and verification

- Profile replacement/unload -> preserve valid selection and invoke matching lifecycle cleanup;
  verify shorter/empty replacement profiles and activation/deactivation ordering.
- HangMonitor shutdown -> retain responsibility for cancellation before readiness;
  verify the owned worker cannot be abandoned by ticker removal.
- Inspect ensure candidates individually -> move each confirmed rejection into the macro body;
  verify no predicate side effects, invalid state access, or partial operation survives.
- Review combined changes against doctrine -> run appropriate final Toolbox coverage once, inspect fresh
  logs, and distinguish source checks, executed observations, and unresolved runtime verification.

## Exit

Every confirmed finding is fixed with evidence or explicitly deferred by the maintainer. No changes to
macro expansion/build flags, unrelated networking work, or existing user edits are introduced.
