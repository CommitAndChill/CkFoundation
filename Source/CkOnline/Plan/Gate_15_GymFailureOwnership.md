# Isolate the gym's owned gameplay failure

Prepared 2026-10-04 at root 64b54aa, Foundation d26ee05ac, CkTests 8b468864.
Gate14 remains a production-owner proposal pending A9-A11. This independent
localized observer fix preserves all existing control/recovery behavior.
Baseline at entry: Gate13 built and passed eight focused tests; source was unchanged.

CkTests' IP OnNetworkFailure accepts any failure on its GI-owned world, without
checking the supplied driver. Engine BroadcastNetworkFailure publishes its
observer event before applying its own driver filtering. A different driver's
failure must not fail this consumer. The current pending-driver comparison also
allows accidental null equality. These are source findings; malformed-input
injection below is a defensive reproduction, not an actual remote disconnect.

The observer must validate thread, active phase, live GI and owned context before
mutation. Accept a valid driver's failure only when it is the current context
world's actual driver or the exact owned pending driver during Joining. Preserve
null-world pending errors. Preserve null-driver NetDriverCreateFailure during
StartingHost on the exact current world with no driver; preserve null-driver
PendingConnectionFailure only for Joining, null event world, a valid owned
pending object with null driver and nonempty ConnectionError. No generic
null-pointer equality proves ownership. Rejected events leave operational
state/error/binding/deadline/travel unchanged; accepted events retain deferred
return. No engine configuration, replication or production policy change.

Extend the existing real recovery fixture's actual pending ClientTravel step:
broadcast null/foreign-driver ConnectionLost values through the subscribed
engine observer and assert unchanged real state/context/pending driver/queued
URLs/control binding/completions immediately. Then retain the genuine pending
timeout/replacement-world return. A constructor-created foreign stock driver
is negative input only; do not call resource virtuals or fake ready engine state.
Run this regression before the filter fix, then the final compatible eight-test
gate after it. Incremental builds only. Account for discovery and serial-lane
boots explicitly; inspect fresh complete logs after the final build.

Expose a gym-only F8 diagnostic during established bridged Host/Client gameplay.
Publish OBSERVED with an increasing sample number only after unchanged world,
driver, session/proof, ready binding and travel readback. Keep diagnostics
separate from operational errors; reset evidence on new entry; guard re-entry.
Append Direct IP rows27/28, retaining rows0-26/F2/F6/F7. No overall PASS.

The short unchecked guide repeats exact separate-Standalone setup and joining:
Host J, wait Hosting, P / Client K; both R, Host G, wait IP Hosting / Client G,
then Client P. F8 in both windows must preserve live state and permit newer proof.
Host X first must still produce automatic client return with retained ready
control identity. Manual Client X cannot substitute for that observation.

Root owns IP header/cpp, plan/evidence and integration; bounded workers own the
single native recovery spec and AS panel/deferred guides. Freeze code throughout
verification. Stage exact owned paths, preserve unrelated work and both separate
[DROP BEFORE MERGE] commits. No new checkout, cleanup, push or history rewrite.

Evidence, 2026-10-04: the extended real pending-travel regression reproduced the
old state/error mutation in OnlineFailureIsolationRed-01.log (1 failed, exit 1).
After the ownership filter, incremental build and all eight compatible tests
passed in OnlineFailureIsolationGate-01.log (0 failed/skipped/contaminated, exit 0).
Both invocations used inline discovery and one serial lane, four fresh boots
total. Fresh final combined/discovery/external logs have no errors, ensures,
script failures or reload contamination. The narrow legitimate null-event
branches were source-reviewed, not separately fault-injected. Rendered F8 and
actual two-window Host X-first recovery remain unchecked in the short guide.
Production Gate14 policy and its A9-A11 rulings remain pending.
