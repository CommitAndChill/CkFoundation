# Online services progress

Updated 2026-10-04. This is the current evidence ledger; gate files define the
contracts. Keep verified automation separate from unobserved editor acceptance.

## Earlier local commit checkpoint, 2026-10-04

The user requested committing the completed work while remaining AFK. Foundation
`0f97cab77` stores the admitted host-address API. CkTests stores the shared panel
controls in `5be31772`, native lobby/gameplay binding and recovery in `145e40a5`,
and the guided online interface in `a48098fc`. Permanent gate contracts, evidence
and editor instructions are documented separately. The root checkpoint updates
only Foundation/CkTests pointers; unrelated dirty work and both separate
[DROP BEFORE MERGE] research commits remain preserved. No push or history rewrite
was requested. The evidence sections below retain their test-time status; their
earlier uncommitted notes do not describe this delivery checkpoint.

Gates 7/8/9 manual checks remain deferred by the user. Committing the green native
and script slices does not mark rendered recovery, colors, focus or remote
gameplay accepted. Joining is repeated in each guide: Host J, wait Hosting, P /
Client K locally; remote Client F2/O pastes the new host invite, Enter/F3 joins.

| Gate | Current evidence | Human acceptance |
|---|---|---|
| 1: persistent actorless host | All 15 existing focused tests pass again in OnlineDirectGates-08 | User tried gyms; individual checklist results unspecified |
| 2: actual gameplay IP | Compiles; host/join/proof/disconnect/rejoin completed in OnlineDirectGates-06, but the fixture fails on engine Iris ensures | Pending; use the separate-process Online Direct IP gym |
| 3: persistent Direct lobby | Focused native TCP lobby, ready, expiry, cleanup, admission and malformed-wire tests pass in OnlineCommitGate-01 | User reports the five-step Lobby/Chat checklist passed on 2026-10-03; optional checks remain unconfirmed |
| 4: Direct text/shareable joins | Production TCP feature passes ordered text, sender identity, invalid-input and peer-isolation checks in OnlineCommitGate-01 | Same reported pass; logs do not independently prove roster/readiness/message delivery |
| 5: editor evidence checkpoints | Build and seven focused Direct/script tests pass in OnlineEvidenceGate-01; existing real loopback test covers checkpoint IDs and redaction | Pending; press V in both Lobby windows and again in both Chat windows |
| 6: ready-lobby gameplay round trip | CkTests-only handoff implemented; build and seven focused Direct/script tests pass in OnlineLobbyGameplayGate-01 | User reports the four-step round trip works on 2026-10-04; separate from the known-red same-process Iris fixture |
| 7: easier editor testing | Incremental build, seven focused Direct tests and two panel-control tests pass in OnlineGuidedPanelGate-01 / OnlineGuidedPanelControls-01 | Deferred by user on 2026-10-04; panel/focus/round-trip recheck |
| 8: control-loss recovery | Incremental build and seven focused Direct/script tests pass in OnlineControlRecoveryGate-01 | Deferred by user on 2026-10-04; separate three-check recovery gate |
| 9: advertised gameplay endpoint | Incremental build and seven focused Direct/script tests pass in OnlineAdvertisedEndpointGate-01 | Deferred by user; local gameplay and two-machine checks remain unverified |
| 10: gameplay failure recovery | Incremental build and eight focused Direct/script tests pass in OnlineGameplayRecoveryGate-02, including real failed ClientTravel and retained ready lobby | Deferred while user is AFK; early Client G and Host X-first observations |
| 11: Direct invite renewal | Incremental build and eight focused Direct/script tests pass in OnlineInviteRenewalGate-02 | Deferred while user is AFK; rotate invite without changing the live lobby, reject old invite and join fresh |
| 12: public ready-lobby binding | Incremental build and eight focused Direct/script tests pass in OnlinePublicReadyLobbyGate-01 | Deferred while user is AFK; readiness barrier and gameplay round trip through Foundation validation |
| 13: configured listen target | Incremental build and eight focused Direct/script tests pass in OnlineConfiguredListenTargetGate-01 | Deferred while user is AFK; invalid-port preservation and actual gameplay on caller-selected port 7789 |
| 14: production gameplay operation owner | Contract proposed; source ownership and runner surfaces inspected; no implementation or runtime gate yet | Pending contract rulings A9-A11; existing gym behavior preserved |
| 15: gym failure-event ownership | Old behavior reproduced red; incremental build and eight focused Direct/script tests pass in OnlineFailureIsolationGate-01 | Deferred while user is AFK; numbered F8 rejection observation and genuine Host X-first recovery |

## Gym failure-event ownership phase, 2026-10-04

Entry revisions: root 64b54aa, Foundation d26ee05ac, CkTests 8b468864. Contract
written before implementation: Plan/Gate_15_GymFailureOwnership.md. Gate14 remains
a proposal pending A9-A11; this independent fix changes only the CkTests observer
and its diagnostic, not Foundation gameplay policy or engine configuration.

The gym previously accepted any network failure on its GI-owned world and could
mistake null pending-driver equality for ownership. Selected-engine source shows
the network-failure multicast fires before engine filtering. The observer now
validates thread, active phase, GI/context and exact current/pending driver before
changing state. Narrow host-creation and missing-pending-driver cases preserve
legitimate null callbacks. Those compatibility branches were source-reviewed;
they were not separately fault-injected in this run.

The existing real pending ClientTravel fixture now broadcasts malformed null and
foreign-driver ConnectionLost inputs through the subscribed observer. It checks
unchanged operational state/error, actual world/pending object/driver, queued
travel, ready binding, control revision and completion count, then retains the
genuine failed-travel return to a replacement Lobby world. No resource methods
are called on the constructor-created foreign driver. These negative inputs do
not represent an actual remote disconnect.

OnlineFailureIsolationRed-01.log: incremental build passed, the extended existing
test failed on the expected old state/error mutation; zero skipped/contaminated,
exit 1, tests 29s. OnlineFailureIsolationGate-01.log: final incremental build and
all eight compatible Direct/script tests passed, zero failed/skipped/contaminated,
exit 0, tests 1m 2s. Each invocation used two fresh boots (inline discovery and one
serial lane), four in total. No source edits occurred during either invocation.
Fresh final combined, discovery (CkPlugins.log) and external test (CkPlugins_2.log)
logs have zero Error/Fatal records, ensures, script errors or reload contamination.
Expected malformed-input warnings and existing nonfatal warnings remain. The
known-red same-process Iris fixture and renderer-only population stay excluded.

Final arguments: `--build --target=Editor --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --parallel=1 --no-live --skip-renderer-tests
--project=D:/Repos/CkPlugins --output=Saved/Logs/OnlineFailureIsolationGate-01.log`.
The red invocation used the same flags with the exact
GameplayEntryFailureReturnsToLobby pattern and OnlineFailureIsolationRed-01.log.

The gym appends F8 and a colored detail readback. A new OBSERVED number requires
unchanged established world/driver, session/proof, ready binding and travel state;
CHANGED reports an alteration without rolling it back. Diagnostic state is
separate from operational errors, resets on new entry and guards re-entry. The
new reflected surface and AS panel compile; running F8 in two windows and
Blueprint graph execution have not been observed.

FAILURE_ISOLATION_EDITOR_TESTING.md is a short, unchecked three-check card. It
repeats separate Standalone setup and joining: Host J, wait Hosting, P / Client K;
both R; Host G, wait IP Hosting / Client G, then Client P. Both F8 observations
must preserve the live session and permit newer proof. Host X first must still
cause automatic Client return with the retained ready control identity; manual
Client X is not a substitute. Earlier editor checks remain deferred separately.

Local delivery is split into observer/regression, gym diagnostic and deferred
instructions. CkTests e287c3ff holds the fix and b2e0b9e7 the gym API/panel. Only
Foundation/CkTests pointers are updated in the root checkpoint. Unrelated work
and both separate [DROP BEFORE MERGE] ancestors remain preserved. No push, history
rewrite, alternate checkout or artifact cleanup was performed.

## Production gameplay ownership proposal, 2026-10-04

Entry revisions: root 2a1ec73, Foundation 57592becc, CkTests 8b468864. Both
plugin checkouts are clean and match the root gitlinks; unrelated root/CkAuto
work remains untouched. Gate13 is locally committed and the eight-test baseline
remains valid for that code. Current contract: Plan/Gate_14_GameplayOperationOwner.md.

The next useful slice promotes the existing CkTests-only Host/Join/Return owner
into Foundation's persistent feature model. Connection ownership must survive
Direct profile stop. Caller-specific game readiness, maps/modes and RPC proof
stay in the consumer. Transport establishment is not account authentication.
Read-only ownership review identifies continuous control authority, failed-entry
return and established transport-loss recovery as real product choices; A9-A11
record the alternatives. Current gym behavior remains unchanged pending rulings.

The selected CkAuto binary exposes manifest-driven Gauntlet, but its implementation
source is not vendored here. No supported two-role runner/rendezvous route was
confirmed in CLI/help/docs and the CkTests AS bridge. This does not establish a
universal single-process limit. No raw editor launch, parallel lock bypass or
same-process Iris retry was attempted. Successful two-process gameplay remains
the separate editor observation; infrastructure automation needs its own contract.

Independent reviews reconciled operation identity, continued readiness deadlines,
separate connection-host ownership and phase/world/context/driver filtering of
engine events that carry no operation ID. A private exact-stock IP adapter and
explicit OnlineSubsystemUtils module/plugin dependencies are proposed against
selected-engine source. No dependency or driver configuration has been changed.
The native review also corrected readback order: validate an exact-stock client's
connection/ownership/open state before the driver's resource accessor, which
internally reads that connection's private resolver. Hosts require no server
connection. Constructor/cleanup source supports the stock resolver lifetime;
arbitrary subclasses or CDOs are outside the proposed adapter contract.

This phase has produced documentation and source evidence only. No new build,
editor boot, gameplay implementation or runtime success is claimed. The compact
AFK return card remains the user-facing check for the already committed work.
Steam/application setup, remote/packaging and Blueprint execution remain separate.

## Configured listen target phase

Gate12 is committed at root 01b4fbd, Foundation d3a2b3b06 and CkTests c55c07fc;
both plugin checkouts were clean at entry. Contract written before implementation:
Plan/Gate_13_ConfiguredListenTarget.md. The user authorized continued phases and
local commits while AFK. Unrelated root work and both removable research ancestors
remain intact. No checkout duplication, artifact deletion, push or history rewrite.

Foundation now exposes getter-only FCk_OnlineDirect_ListenTarget and reflected
generic/ready-host builders. Caller supplies the soft map, loaded GameMode class,
canonical IPv4 address and port. Functional validation precedes class operations;
default, malformed, abstract, deprecated or replaced classes cannot publish intent.
Map paths are bounded canonical mounted package/object paths with matching asset
names and no subobject. Exact local FURL map/port/game/listen options round-trip
before both target strings publish atomically. Failure resets both strings. The
helpers do not load assets, travel, bind drivers, authenticate or prove map cooking.
Class-flag rejection is source-reviewed; no flag-specific runtime assertion was run.

The actual IP consumer uses these outputs before arming travel. Its loaded gym
mode, map, ownership/live binding, driver/context checks, deadlines and recovery
remain consumer policy. The gym appends F4 in details after existing row42/Y and
uses one validated port for both G roles. Default is still 7787; the control TCP
invite stays on 7788 and does not advertise gameplay ports. Both windows must
choose the same port. Invalid decimal/range input preserves the previous value;
editing is limited to recognized offline gym worlds with no active driver, pending
connection or queued context/world travel. Text commit closes the modal first and
rechecks the same setter guards after focus callbacks.

Existing real ready TCP tests now cover generic/bound targets, exact URL options,
malformed map/mode/address/ports, default/client binding rejection, cleared output
and unchanged live control state/completions. The real failed-entry recovery test
edits/rejects/restores ports in its actual Offline Lobby, then rejects editing while
actual ClientTravel is pending without cancelling entry or changing the ready lobby.
Its FObjectKey identity and timing bounds remain. The AS smoke calls both reflected
builders using the consumer's typed map/mode and its actual ready host. Native/AS
calls are runtime evidence; Blueprint graph execution and serialized stale-class
branches are not established by this run.

`Saved/Logs/OnlineConfiguredListenTargetGate-01.log`: incremental editor build and
all eight focused tests passed, zero failed/skipped/contaminated, exit 0, tests 46s.
The detached run waited for another machine-local build without modifying that
checkout. Exactly two fresh boots covered discovery and one serial test lane.
Combined, discovery (CkPlugins-backup-2026.10.05-00.21.55.log) and external test
(CkPlugins.log) logs contain zero Error/Fatal records, ensures, script errors or
reload contamination. Existing nonfatal warnings remain. The known-red two-world
Iris fixture and renderer-only population remain excluded.

Arguments: `--build --target=Editor --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --discover-fresh --parallel=1 --no-live
--skip-renderer-tests --project=D:/Repos/CkPlugins
--output=Saved/Logs/OnlineConfiguredListenTargetGate-01.log`.

The short CONFIGURED_GAMEPLAY_PORT_EDITOR_TESTING.md stays unchecked. The 215-word
AFK_RETURN_EDITOR_TESTING.md combines the latest normal path into three checks,
repeats exact separate-Standalone setup and Host J > Hosting > P / Client K joining,
and requires one new completed renewal before reading Succeeded. Old-invite and
recovery-fault checks remain separately deferred. The same completion correction
was made in the earlier renewal guide. Neither card marks all outstanding gates
accepted. Actual two-process driver binding on 7789, proof, colors/focus, remote
reachability and packaging remain human/separate observations.

Local delivery: Foundation 70fd352bb holds the API/recipe; CkTests 43ce9194 holds
consumer/gym/coverage and 8b468864 holds deferred guides. Contract/evidence is a
separate Foundation documentation commit before the root's two-pointer checkpoint.

Next recommendation: consolidate the deferred editor observations before another
helper gate. More helpers would duplicate the proven preparation path. The next
substantive implementation needs either a defined production gameplay lifecycle/
travel owner and caller contract, or the Steam application's App ID and licensed
test-account setup. Those product/lifecycle decisions remain with the maintainer;
no broad migration or provider configuration has been started. Deferred human
acceptance does not make this completed code gate blocked.

## Public ready-lobby binding phase

Gate11 is locally committed at root cdb038f with Foundation 663a939f9 and CkTests
0efd1ea1; those checkouts are clean at entry. The last gate passed all eight
focused tests and fresh log review. Contract: Plan/Gate_12_PublicReadyLobbyBinding.md.
The active AFK objective continues. Unrelated work and both research ancestors
are intact; there is no blocking-condition recurrence at this checkpoint.

Source review found the reusable ready-lobby binding and endpoint validation still
inside CkTests. Promote those pure contracts into CkOnline, expose getter-only
reflected values/Utils, and make the existing gym/tests actual consumers. Caller
chooses expected roster count; the gym keeps exact two. Travel/recovery ownership
and private-network trust stay unchanged. Steam source/payload exists in the selected
engine, but no game App ID/account test setup was supplied. That platform gate stays
separate; no Steam capability or packaging success is inferred from this phase.

The public getter-only FCk_OnlineDirect_ReadyLobbyBinding and four reflected Utils
helpers now own capture, matching and endpoint parsing. Capture validates exact
caller-selected count 1..8, role, correlation IDs, admitted address and unique ready
local-containing roster before publication. Default/failed output is fully empty.
Matching uses the captured role/count and excludes chat, invite and revision.
Endpoint parsers normalize decimal input, reject oversized input before copying,
handle input/output aliasing and clear failure output. Binding endpoint parsing
also validates intrinsic correlation before address comparison. These pure helpers
do not authorize gameplay, prove service ownership or begin travel.

CkTests' bridge header is only a type alias. The IP consumer keeps ownership,
generation, world/driver, exact-two-member, travel and recovery checks while using
the public value/parser implementation. Its two existing parser methods forward
to Foundation; no duplicate parser remains. The existing real TCP test exercises
the reflected wrappers with actual ready/unready snapshots, invalid count/role/
identity/address/readiness, stale snapshots, unchanged live revisions/completions,
alias-safe normalization and rejection, and matching across chat/invite renewal.
The AS smoke calls all four reflected wrappers on its actual one-member host,
with explicit count 1, rejected count 2/default/not-ready output, selected port,
wrong address and renewal continuity. This covers native and AS API use, not a
constructed Blueprint graph or all serialized malformed-binding branches.

`Saved/Logs/OnlinePublicReadyLobbyGate-01.log`: incremental build succeeded and
all eight selected Direct/script tests passed, zero failed/skipped/contaminated,
exit 0, tests 46 seconds. Two fresh boots covered discovery and one serial lane.
Combined, discovery (CkPlugins-backup-2026.10.04-23.42.30.log) and external test
(CkPlugins.log) logs contain no Error records, ensures, script errors or reload
contamination. Existing nonfatal warnings remain. The known-red two-world Iris
fixture and renderer-only tests remain excluded.

Arguments: `--build --target=Editor --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --discover-fresh --parallel=1 --no-live
--skip-renderer-tests --project=D:/Repos/CkPlugins
--output=Saved/Logs/OnlinePublicReadyLobbyGate-01.log`.
Source review corrected the short guide's unavailable IP Offline label to disabled
G/no gameplay travel in Lobby. PUBLIC_READY_LOBBY_EDITOR_TESTING.md remains unchecked;
it repeats Host J > Hosting > P / Client K, then readiness barrier, fresh gameplay
proof and same-control-identity return. Actual two-process gameplay/color/focus,
Blueprint graph, remote reachability and packaging acceptance remain deferred.

Local delivery separates Foundation 1bca988fa (public API/consumer recipe), CkTests
6965091d (actual consumer plus native/AS coverage), and c55c07fc (deferred guide).
Contract/evidence documentation is a separate Foundation commit before a root
checkpoint of only these two submodule pointers. No push or history rewrite.

## Direct invite renewal phase

The active objective is to continue the replacement phases until the user returns.
The previous goal turn made progress: Gate10 found and repaired actual pending
gameplay-driver survival, passed eight focused tests, and committed root 77687ce
with independent code/evidence slices. At Gate11 entry Foundation/CkTests matched
their root gitlinks and were clean; CkAuto remains its unrelated f1983a7e checkout.
Unrelated root dirt and both [DROP BEFORE MERGE] commits remain preserved.

Contract: Plan/Gate_11_DirectInviteRenewal.md. Source evidence: Host creates private
Capability/Expiry once; P copies the snapshot descriptor; Join checks capability
before expiry/capacity, while accepted peers subsequently use socket membership
and epoch. Renew can therefore be local and wire-compatible, preserving the live
lobby while revoking the old descriptor for future admission. Current Execute
writes LastNow before action validation; renewal validates its deadline before
that Execute cache write. Host ticks and transport Poll still independently advance
their monotonic clocks, so the extreme Host.Tick fixture proves unchanged invite/
snapshot and one failure, not subsequent normal-clock recovery.

Root owns the contract and integration. Bounded implementation ownership is
Foundation Direct enum/Utils/transport/consumer guide, the single existing native
Direct spec, and CkTests gym/controller/deferred guide; AS smoke stays with root.
The eight passing Gate10 tests are the entry baseline without a duplicate run.
Rendered/BP acceptance remains deferred. No Steam App ID, account/auth policy,
engine/wire change, EIK removal or packaging is inferred from this continuation.

RenewInvite is appended without changing existing serialized action values. The
host validates a fresh capability and advancing deadline before publishing one
changed snapshot and one completion. Existing members, ready state, transcript,
generation, epoch and address are retained. Old descriptors processed afterwards
return Unauthorized. Native tests use actual TCP admissions, including a real
Expired result followed by renewal and fresh same-epoch admission. The expiry
fixture alone controls monotonic service time while its socket pump and latent
timeout remain real. Extreme Host.Tick(max) rejects once without snapshot mutation;
it does not establish normal-clock recovery. The AS smoke runs actual reflected
Host, SetReady and RenewInvite requests and verifies matching identity/readiness.
The gym adds an owned host-only details Y action; P copies the completed invite.

OnlineInviteRenewalGate-01 built successfully and passed seven of eight tests.
Only the preceding recovery fixture failed: its raw initial-world pointer equaled
the newly allocated return world. Fresh logs prove old-world teardown, new PIE
copy, then Offline Lobby with no driver or pending game. The independent fixture
fix records FObjectKey identity instead, retaining the new-world requirement and
avoiding both stale dereference and keeping the old world alive. Two additional
fresh boots covered discovery and the same serial test lane after that correction.

`Saved/Logs/OnlineInviteRenewalGate-02.log`: incremental build succeeded, all eight
selected tests passed, zero failed/skipped/contaminated, exit 0, tests 57 seconds.
Combined, fresh discovery (CkPlugins-backup-2026.10.04-23.26.59.log) and external
test (CkPlugins.log) logs contain no Error records, ensures or script failures.
Existing nonfatal warnings remain. Source review corrected the action display
name and short guide: F2 is conditional because it toggles; an old valid invite
gets a new operation ID and terminal Unauthorized rather than local admission
rejection. Invite comparisons do not print capabilities even on test failure.

Arguments: `--build --target=Editor --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --discover-fresh --parallel=1 --no-live
--skip-renderer-tests --project=D:/Repos/CkPlugins
--output=Saved/Logs/OnlineInviteRenewalGate-02.log`.
The known-red two-world Iris fixture and renderer-only tests remain excluded.
Manual guide: CkTests/Script/CkOnline/INVITE_RENEWAL_EDITOR_TESTING.md, unchecked.
It repeats two separate Standalone windows, Host J > Hosting > P / Client K,
both R, then Y renewal and old/fresh invite comparison. No five-minute wait is
needed; rendered colors/focus/clipboard and Blueprint graphs remain unobserved.

Local delivery separates Foundation 8024159f9 (renewal API), CkTests 7a31995f
(world-identity fixture fix), and CkTests 36ee3c59 (gym plus native/script coverage).
The editor guide and contract/evidence are separate documentation commits, followed
by a root checkpoint updating only Foundation/CkTests. No push or history rewrite.

## Gameplay failure recovery phase

The user requested committing the completed work and continuing while AFK. The
root local checkpoint is b5c7b75; Foundation 09d46f0b5 and CkTests d29174bb are
clean at entry. CkAuto remains f1983a7e with its unrelated root pointer preserved.
Contract: Plan/Gate_10_GameplayFailureRecovery.md. Baseline is Gate 9's seven
green focused Direct/script tests and clean fresh logs, without an equivalent rerun.

This phase exercises the existing native failure/return path using one gameplay
client and a real native TCP lobby host. A held silent UDP endpoint cannot admit
gameplay, so the real ClientTravel waits for its existing bounded entry deadline
and deferred Lobby return. No second gameplay world or same-process Iris retry
is used. Owned invite joining gains a common clipboard-free API for reliable
automation, preserving validation and rollback. Recovery hints must direct prior
bridged sessions to Lobby G, because native R intentionally refuses that context.
All rendered/local-host-loss checks stay deferred in a short self-contained guide
that repeats Host J, wait Hosting, P / Client K joining. Authentication and protocol
policy remain separate architecture decisions; unrelated dirt is preserved.

The first incremental build passed. OnlineGameplayRecoveryGate-01 selected all
eight intended tests: seven passed, while the new real failed-entry fixture timed
out after its 35-second return bound (zero contamination). Fresh CkPlugins.log
shows local Entry reload at 22:17:31 UTC, but the pending gameplay driver survived
until test teardown at 22:17:52. Engine source confirms local OpenLevel/Browse
does not cancel that pending connection. The bounded fix uses engine CancelPending
on the validated owned world context before queuing return; both settlement and
Offline readback require no pending game. Disconnect reports a rejected return
explicitly. A sanitized fixture timeout snapshot preserves future failure evidence.
Two additional fresh boots covered discovery and one serial lane in the same
eight-test repair gate; the prior two-world Iris fixture stayed excluded.

`Saved/Logs/OnlineGameplayRecoveryGate-02.log`: incremental build succeeded and
all eight selected Direct/socket/AS tests passed, zero failed/skipped/contaminated,
exit 0, tests 46 seconds. The new GameplayEntryFailureReturnsToLobby test observed
real pending travel, then the helper's timeout and a new exact Standalone Lobby
world with no net driver or pending game, no proof and a captured error. It checks
the same GameInstance, owned Direct generation, epoch, local member, two ready
members and advertised address. Empty/malformed invites and wrong gameplay
address reject before profile start/travel; the foreign-service AS probe rejects
a syntactically valid invite without profile/draft/revision or adapter mutation.
The resident editor map is unchanged and owned cleanup completes.

Fresh combined, discovery (CkPlugins-backup-2026.10.04-22.24.26.log) and external
test (CkPlugins.log) logs have no Error records, ensures or script errors. Engine
late-module/console registration warnings remain visible; no engine/config change
suppresses them. Driver shutdown at 22:25:07.434 UTC precedes local Lobby browse
at 22:25:07.440. Independent source and evidence reviews cover the common invite
path, owned cancellation, failure control flow, fixture lifecycle and guide.
This proves failed entry recovery, not established two-process host loss, rendered
colors/focus, successful gameplay admission/retry or remote-machine reachability.
Those editor checks remain deferred in GAMEPLAY_RECOVERY_EDITOR_TESTING.md;
joining is Host J, wait Hosting, P / Client K, both R before Lobby G.

Arguments: `--build --target=Editor --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --discover-fresh --parallel=1 --no-live
--skip-renderer-tests --project=D:/Repos/CkPlugins
--output=Saved/Logs/OnlineGameplayRecoveryGate-02.log`.

Gate 10 is locally committed in CkTests as 05819d41 (common invite admission),
5e0d30d0 (pending-connection recovery plus fixture and hints), and 599d7cfc
(deferred editor guide). Evidence/contract documentation is committed separately
in Foundation, followed by a root update of only those two submodule pointers.
Manual checks remain unchecked; no push, cleanup or research-history rewrite.

## Advertised gameplay endpoint phase

The user explicitly deferred the latest gym checks while AFK and authorized the
next phase. CkTests/Script/CkOnline/DEFERRED_EDITOR_CHECKS.md is the short index;
it preserves Gate 6's earlier reported pass separately from Gates 7/8/9. No
reminder was scheduled. Contract: Plan/Gate_09_AdvertisedGameplayEndpoint.md.

Entry tips are root 2ccf30f, Foundation 2cb3c8f65, CkTests 0df75364 and CkAuto
f1983a7e. Gate 8's immediately preceding incremental build/seven-test clean-log
result is the baseline. Prior phase changes remain uncommitted. Independent
research confirmed the invite already carries the validated host IPv4 address;
the gameplay consumer alone hardcodes loopback. No wire field is necessary.

Publish an address-only snapshot value after Host binds or Join's authoritative
roster completes, keeping the pre-admission candidate private. Freeze that address
in the ready lobby binding, compare it during revalidation and reject a mismatched
gameplay target before travel. Shared Lobby/Chat G uses that address on gym port
7787; standalone IP and valid custom native port arguments keep their behavior.
No member credential/admission, engine, firewall or process orchestration change
is introduced. The two-machine editor result will remain deferred even if local
socket tests pass. The short new guide repeats setup and joining: Host J, wait
Hosting, P / Client K locally; across machines Host F2/L sets a reachable IPv4
before J/P, Client F2/O pastes the new invite, Enter/F3 joins it.

`Saved/Logs/OnlineAdvertisedEndpointGate-01.log`: incremental C++ build and all
seven selected Direct/socket/AS tests passed, zero failed/skipped/contaminated,
exit 0, tests 39 seconds. Two fresh editor boots covered inline discovery and
one serial lane. Fresh combined, discovery and external test logs contain no
Error records, ensures or script errors. The new getter loaded in the shared
Lobby/Chat AngelScript panel. Renderer-only tests and the known-red same-process
DirectIp fixture were excluded; their verdicts are unchanged.

The existing real TCP loopback test now advertises 127.0.0.2 while the client
keeps its default 127.0.0.1. It verifies the actual invite/join/rejoin address,
unpublished Joining, rejected/expired/full admission and leave/host-loss clearing.
The same live ready snapshots seed binding capture/matching checks. Copied
malformed or changed snapshots reject atomically; the actual endpoint guard
rejects wrong address/DNS/options/invalid ports and clears output, while retaining
valid custom-port behavior. This is socket/binding/parser evidence, not rendered
gameplay travel, active foreign-profile rejection or remote-machine reachability.
Two bounded independent source reviews found no material Gate 9 issue.

Arguments: `--build --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --parallel=1 --no-live --skip-renderer-tests
--project=D:\Repos\CkPlugins --output=Saved/Logs/OnlineAdvertisedEndpointGate-01.log`.
Short human guide: CkTests/Script/CkOnline/ADVERTISED_ENDPOINT_EDITOR_TESTING.md.
It specifies one player per machine for the deferred remote check and repeats
how to join. Final branch tips and unrelated dirty paths are unchanged; all phase
work remains uncommitted, with both temporary research commits separate.

## Control-loss recovery phase

The user authorized the next phase while Gate 7's visual check is pending.
Entry revisions and unrelated dirt are unchanged. The immediately preceding
baseline is seven green Direct tests plus two green panel-control tests; no
equivalent rerun preceded new code. Gate 8's contract is
Plan/Gate_08_ControlLossRecovery.md.

Read the IP subsystem/GameState/proof component, FLobbyBinding, Direct
transport/feature/facade, real loopback test, AS smoke test and gym panels.
The proven neighboring path is Tick's observed binding loss, deferred return
and exact Lobby-world recognition. This phase adds explicit test controls over
that real path: F6 queues SetReady(false); F7 stops the same validated owned
Direct generation. Guards reject before mutation outside an established owned
bridge; one fault is accepted per gameplay entry. Both actions live in details,
preserving the guided first view and existing keys/row indices.

The AS smoke test exercises rejection while its actual fake service remains
Running, with no changed generation, operation counts, cancellation/shutdown,
IP state or bridge. That does not prove the active-bridge or foreign Direct
guard branches. Cleanup now runs at explicit finish and entity EndPlay, stopping
only the test's recorded generation. Rendered return remains a separate human
gate in CkTests/Script/CkOnline/DIRECT_RECOVERY_EDITOR_TESTING.md, which repeats
Host J, wait Hosting, P / Client K joining and three recovery observations.

Independent source research confirmed the remaining identity gap: public
member GUIDs and the replicated epoch are correlation, not credentials. A
private per-member proof plus gameplay admission policy needs its own contract;
no new ticket, PreLogin authentication or public-internet security claim is
introduced by this recovery gate.

`Saved/Logs/OnlineControlRecoveryGate-01.log`: incremental build and all seven
selected Direct/socket/AS tests passed, zero failed/skipped/contaminated, exit
0, tests 29 seconds. Two fresh editor boots covered inline discovery and one
serial lane. Fresh combined and external test logs contain no Error records,
ensures or script errors. The extended AS test executed both rejection calls
against its actual running fake service and verified no downstream mutation;
the active gameplay fault/return observations still require a human.

Arguments: `--build --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --parallel=1 --no-live --skip-renderer-tests
--project=D:\Repos\CkPlugins --output=Saved/Logs/OnlineControlRecoveryGate-01.log`.
Renderer-only tests and the known-red same-process DirectIp fixture were not
selected. Root/Foundation/Tests branch tips remain unchanged. CkAuto's current
HEAD was observed at `f1983a7e11fd1d9b83ed5c374cc92fb4c41a22f8`; compared with
the earlier `9457fe709`, only unrelated project-storage files changed. Its root
gitlink, that work, all other unrelated dirt and both temporary research commits
remain preserved. This phase remains uncommitted.

## Guided editor testing phase

The user accepted the prior four-step round trip and requested an easier test
flow and readable color feedback. Latest rendered logs show the host establishing
an IP listen session; they do not independently contain matching gameplay/control
checkpoint records or the client RPC proof. The reported visual pass remains
its own evidence, and does not turn the failing same-process Iris test green.

Read the shared row model, Slate renderer, Compact filter, AS row builders/HUD,
existing compact/native and numbered-shortcut/AS tests, Online panels and native
text-input helper. The neighboring implementation is the shared
`SCkGym_ControlPanel` using CkStyle's semantic palette. This phase stays within
CkTests test tooling; production online wire, admission and travel contracts
are unchanged. Contract: Plan/Gate_07_GuidedEditorTesting.md.

Online panels start with the next action and concise connection/readiness/chat/
proof feedback. F2 reveals technical details; existing H/Tab behavior remains.
Explicit row tones use the existing Info/Ok/Warn/Err palette with text labels.
Opt-in compact wrapping prevents instructions/errors being clipped. Legacy
gyms retain their row defaults. Host J/P and Client K join a copied invite;
B/Enter sends chat, Escape cancels. Manual O/Enter/F3 and saved-draft I remain
available. No automatic whole-test PASS is manufactured from local observations.
The F2 choice follows source review: D is reserved for pawn movement.

Entry branch tips remain root `2ccf30f`, Foundation `2cb3c8f65`, Tests `0df75364`.
Prior Gate 6 changes are still uncommitted and preserved. Baseline Online coverage
is seven green tests from OnlineLobbyGameplayGate-01; the legacy CompactRows
assertions remain in the extended native test. New rendered behavior is pending.

Final focused verification: `Saved/Logs/OnlineGuidedPanelGate-01.log` rebuilt
incrementally and passed seven Direct/socket/script tests in 30 seconds;
`Saved/Logs/OnlineGuidedPanelControls-01.log` reused those binaries and passed
native CompactRows and AS NumberedShiftShortcuts in 28 seconds. Both exited 0,
with zero failed, skipped or contaminated selected tests, and no ensures or
script errors. Three fresh editor boots were used: discovery plus one Direct
lane, then the cached panel-control lane. Toolbox's single AND-token pattern
cannot select both families together. Renderer-only tests were waived; the
known-red same-process DirectIp fixture was not selected.

The Direct run has one startup LogAssetRegistry error opening
`Intermediate/CachedAssetRegistry_0.bin.tmp`. Its concurrent discovery process
wrote that cache successfully 28 ms later. Cache-write contention is plausible,
but the OS reason is unlogged; do not describe this startup log as error-free.
The panel-control run contains no Error, ensure or script-error records.

Review corrected the movement-key conflict (F2 replaces D), preserved manual
invite drafts on rejected clipboard join, rolled back only a profile newly
started by that failed join, and preserved the previous draft on empty chat
Enter. Lobby/Chat G is disabled until exactly two members are ready and IP is
Offline. User verification is now three checks in
`CkTests/Script/CkOnline/ONLINE_PANEL_EDITOR_TESTING.md`; it repeats setup and
Host J, wait Hosting, P / Client K joining. Changes remain uncommitted, with
the removable research commits and unrelated checkout work preserved.

## Ready-lobby gameplay round-trip phase

Entry revisions: root `2ccf30f`, Foundation `2cb3c8f65`, CkTests `0df75364`.
The selected CkAuto remains `9457fe709` with unrelated storage-tooling work;
the root's older CkAuto pointer is preserved. Foundation/Tests were clean at
entry, and the six unrelated root changes remain outside this work.

Read the IP fixture's subsystem, GameState/proof component, existing AS panels
and TCP loopback test. The reusable neighbor is the existing IP helper's normal
listen/client travel and deferred return handling. Its original entry/return
paths require the IP gym, so this phase adds explicit ready-lobby entry paths
and a fixed Online Lobby return mode. No provider/wire/authentication expansion
is included. Two independent source reads confirmed that a matching lobby epoch
correlates the session but cannot authenticate a Direct member as a PlayerState.

The frozen binding rejects invalid, unready or changed identity/roster before
travel and throughout the active handoff. TCP remains GameInstance-owned. Native
coverage extends the existing real loopback test; the actual rendered connection
and return are human checks. Contract: Plan/Gate_06_LobbyGameplayRoundTrip.md.
Instructions: CkTests/Script/CkOnline/LOBBY_GAMEPLAY_EDITOR_TESTING.md.

Lobby/Chat **G** now captures the owned ready two-member binding before starting
the existing IP fixture. Gameplay replicates the control lobby epoch separately
from its session marker; client **P** uses the owned controller's RPC proof.
Bridged **X** returns to Online Lobby without stopping TCP. The return readback
reports preservation only when the frozen binding still matches. Rejected
Host/Join calls leave an active bridge intact. Control/evidence loss starts a
fresh bounded return deadline, including after a long-running session.

`Saved/Logs/OnlineLobbyGameplayGate-01.log`: incremental C++ build and all
**seven selected tests passed**, zero failed/skipped/contaminated, process exit
0. The fresh combined log has no ensures or script errors. Two fresh editor
boots ran: inline discovery and one serial lane; tests took 36 seconds.
The extended real TCP loopback test covers unready/default/malformed bindings,
atomic output rejection, retained per-process identity and matching roster,
chat-independent binding validity, and rejection of changed identity/readiness.
This verifies socket/binding behavior and script compilation, not rendered
two-process gameplay travel, proof, or return. Exact runtime GameMode return
recognition and failure recovery remain editor observations.

Arguments: `--build --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --parallel=1 --no-live --skip-renderer-tests
--project=D:\Repos\CkPlugins --output=Saved/Logs/OnlineLobbyGameplayGate-01.log`.
The known-red DirectIp fixture and renderer-only tests were not selected; their
verdicts are unchanged. No authentication, two-machine or orchestration claim
is made. This phase remains uncommitted; the prior checkpoint commits and both
temporary research commits remain separate and unchanged. The final checkout
check found only the expected phase paths and pre-existing unrelated dirt.

## Evidence checkpoint phase, 2026-10-04

The Lobby and Chat gyms now expose **V** to record one immutable snapshot from
their owned active profile. A stale generation is rejected before the snapshot
read. Records include process/generation/epoch/local IDs, a sorted bounded ready
roster and ordered retained message sequence/sender IDs. Invite descriptors,
capabilities, endpoints, names, message text and error details are excluded.
This is test-gym instrumentation, with no change to the production wire protocol
or automatic pass verdict.

`Saved/Logs/OnlineEvidenceGate-01.log`: incremental build and **seven selected
tests passed**, zero failed/skipped/contaminated, process exit 0, no ensures or
script errors. Two fresh boots were used: inline discovery and one serial lane.
The existing real TCP loopback test now asserts identity/readiness/message
metadata and absence of names, text and invite capabilities in the formatted
records. It does not automate the rendered V action or prove gym travel.
The short human gate is `CkTests/Script/CkOnline/DIRECT_EVIDENCE_EDITOR_TESTING.md`.

The earlier implementation checkpoint and this subsequent evidence phase remain
separate local commit groups. The user supplied the commit author on 2026-10-04;
no commit was created by the earlier failed attempt. No push or history rewrite
occurred.

## Full focused online checkpoint before evidence instrumentation

`Saved/Logs/OnlineCommitGate-01.log`: incremental C++ build passed, AngelScript
loaded, Toolbox exited 0, and **22 selected tests passed**, with zero failed,
skipped or contaminated tests. Fresh output contains no ensures or script errors.
This includes the 15 host/Utils tests, atomic feature composition, and six Direct
service/wire tests. It excludes the separately failing gameplay IP fixture and
renderer-only tests. Native loopback coverage does not establish rendered UI,
GameInstance map-travel persistence or cross-machine behavior.

The final run used two fresh editor boots: inline discovery and one serial test
lane. OnlineDirectGates-08 previously needed three boots because the clock
regression was newly discovered. Serial limits concurrency, not the total
discovery/new-test boot count.
No extra rebuild or equivalent test rerun is required for documentation edits.

Use explicit native prefixes: the project has no AutomationGate.json, and the
default project prefix selected only the script AutoTest in OnlineDirectGates-05.
That run was not a native gate. The final arguments were:

```text
--build --test --test-pattern=Online
--project-prefix=Ck.Online.Direct --project-prefix=Ck.Online.Host
--project-prefix=Ck.Online.Utils --project-prefix=Ck.Online.Feature
--parallel=1 --no-live --skip-renderer-tests
--project=D:\Repos\CkPlugins --output=Saved/Logs/OnlineCommitGate-01.log
```

Launch through the documented detached UnrealToolbox workflow. DirectIp has a
different prefix and must retain its own verdict when selected.

## Failures and limits

OnlineDirectGates-06 selected 22 tests: 18 passed and four failed. Three Direct
failures exposed an idle-socket defect. Engine source confirms a nonblocking
Recv may succeed with zero bytes on would-block. The transport now peeks only
when the socket reports read readiness and treats a failed peek as disconnect;
an idle zero-byte successful read leaves the peer connected. All affected Direct
tests pass in the final run.

A final read-only review found that adding a short positive lifetime/timeout to
an extreme finite clock can round back to the same timestamp. Direct now rejects
those deadlines before opening a socket or queuing remote ready/text. The generic
host likewise rejects a non-advancing deadline before invoking its adapter.
The new ExtremeClockRejectsBeforeTransport test checks real feature/host rejection,
same-port availability, and no remote ready/text mutation after a real loopback
join; the existing generic deadline test also covers this rounding case.

Gate 2 first failed because late Listen created Iris before its PIE ID was set.
Host now uses normal local listen travel and waits for the replacement world's
bound IP driver before publishing Hosting. OnlineDirectGates-06 then completed
host/join, a replicated client RPC proof, disconnect, rejoin and a second proof.
Its verdict still fails: Iris reports both a DataStream first-packet ensure and
a PIE_1 PlayerState referenced by the PIE_0 NetDriver. Functional observations
cannot override those errors. No ensure suppression, Iris disablement, engine
patch or global networking configuration change is used.

After two failed approaches, do not repeat speculative same-process attempts.
The separate-process editor gym is ready for human verification. A future clean
automated gameplay gate needs two-role process orchestration with a combined
verdict and cleanup. No supported two-role orchestration route has been confirmed
in the inspected Toolbox surface. That orchestration has not been implemented or run.

Work stays in D:/Repos/CkPlugins. Root feature/online-services-research and both
[DROP BEFORE MERGE] docs commits remain unchanged. Foundation/CkTests actual
starting baselines were 6cf825dd/66d8cbed; unrelated root/submodule dirt remains preserved.
Implementation and permanent documentation use local checkpoint commits
independently reviewable from the removable research commits. No push,
cleanup or published-history rewrite is authorized by this checkpoint.

## Reported editor acceptance, 2026-10-03

The user reports all checklist steps passed. The latest presented checklist was
the five-step Lobby/Chat guide; gameplay IP and the optional checks are not marked
accepted without a separate report. Archived logs are
`Saved/Logs/CkPlugins-backup-2026.10.04-06.20.47.log` and
`Saved/Logs/CkPlugins_2-backup-2026.10.04-06.20.29.log`. Both processes used this
checkout and entered Direct Lobby; the editor also entered Direct Chat. No
join/roster/ready/message outcome records exist in these logs, so the visual
report remains the evidence for those outcomes. Stable IDs, retained transcript
and generation across travel will gain explicit gym checkpoints in the next
bounded phase.

The rendered travel logs exposed a legitimate unset loading-screen widget class
being passed to CreateWidget. A small independent guard now selects the existing
built-in throbber without that invalid call. OnlineLoadingScreenGuard-01 passed
the existing LoadingScreenHolderSemantics test; its headless run does not verify
the rendered fallback. The final build and 22-test online gate passed after this
guard and the test plugin's missing OnlineSubsystemUtils dependency metadata
were added. Existing startup logger/FText automation errors are unrelated and
are not described as clean logs.

User authorized proceeding through the next few playable gates while AFK.
The bounded sequence is gameplay IP, persistent Direct lobby/ready, then text
and shareable join details. Lobby and chat use a real actorless TCP control
channel, not the fake Gate 1 adapter or UE gameplay replication. Entering gameplay
still depends on truthful Gate 2 evidence.
Steam/account/App ID setup, two-machine/Tailscale evidence, packaging, Blueprint
graph execution and rendered human acceptance remain separate observations.
