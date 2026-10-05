# Gate 17: Steam lobby profile foundation

Prepared 2026-10-04 on `feature/online-services-research` after the persistent
Foundation gameplay connection owner. This gate starts the Steam/social
delivery phase. It does not claim Steam runtime support until its editor check
is observed with the project's App ID and two licensed accounts.

## Engine compatibility evidence

The selected engine checkout is `D:/Repos/UnrealEngineAngelscript` and contains
`Engine/Plugins/Online/OnlineSubsystemSteam` with the Steamworks third-party
dependency. The project enables `OnlineSubsystem` and
`OnlineSubsystemUtils`, but does not enable `OnlineSubsystemSteam`; Steam config,
App ID and account credentials are absent from this checkout.

The checked-in engine Steam OSS source establishes these constraints:

- `FOnlineFriendsSteam::SendInvite` and `AcceptInvite` return unsupported. Do
  not build the feature around `IOnlineFriends` invite calls.
- `FOnlineSessionSteam::SendSessionInviteToFriend(s)` invites through a real
  Steam lobby/session or advertised game session. The Steam overlay's
  `ShowInviteUI` also requires a named session with valid session info; lobby
  invitations require an actual lobby session.
- Invite acceptance reaches OSS through session invite callbacks and uses
  different payload paths for lobby and advertised-game invitations. Normalize
  both into one correlated join intent before any Ck membership or gameplay
  action.
- Steam OSS is disabled by default in its engine plugin descriptor. Keep Steam
  optional and do not enable it globally or invent an App ID in the host project.

These are source findings for the local engine revision, not live Steam proof.

## Smallest implementation gate

Add one optional Steam-backed lobby vertical slice behind Ck-owned lobby
contracts. Establish Steam provider readiness and one Steam lobby membership
before adding broad friends/presence or voice features. The lobby/session owner
must be singular: Ck state mirrors the selected OSS session authority and must
not create a second native Steam lobby for the same operation.

The initial slice should cover:

1. Explicit Steam profile initialization with typed unavailable, logged-out,
   ready and failed observations. No silent Direct fallback.
2. Create, find/join and leave one bounded Steam lobby through OSS session
   operations, with operation IDs, deadlines, generation checks and terminal
   completion before notifications.
3. Show the Steam invite overlay only while the owned lobby session is valid;
   normalize warm in-game and cold-launch lobby invite intents into the same
   join admission path. Preserve early intents until the Steam profile is ready.
4. A test seam for fake session callbacks. Cover synchronous callback,
   duplicate/stale callback, cancellation, timeout, account-generation change,
   lobby-full/rejected join and shutdown re-entry without Steam or credentials.
5. A CkTests Steam Lobby gym with explicit setup diagnostics and no automatic
   pass. Keep display name and provider account ID separate; never use names as
   identity. Do not expose SDK IDs or invite payloads in diagnostic logs.

Defer a persistent cross-provider friend graph, offline inbox, chat, voice,
EOS/EGS, public matchmaking, dedicated servers and Direct-to-Steam fallback.
Keep the existing Direct profile usable independently. Do not claim gameplay
admission from lobby membership; use the existing Foundation connection owner
and require its consumer readiness confirmation.

## Verification and human check

The code gate needs an incremental UnrealToolbox Editor build and focused
protocol tests for fake callback ordering, operation correlation, stale
generation rejection, atomic failure and optional-provider absence. Reuse the
current compatible Online tests; do not boot Steam or require an App ID in
automation. Inspect fresh discovery and test logs for errors, ensures and
script failures. Preserve the actual editor boot count in the evidence ledger.

Steam runtime acceptance is a separate, unchecked editor gate. It requires a
project-owned Steam App ID, Steam running for both local users, and two licensed
Steam accounts. Open `Plugins/CkTests/Content/TestGyms/TestGyms_CkTests_Level.umap`.
In Play > Advanced Settings choose **Number of Players 2**, **Play Net Mode
Play Standalone**, **Launch Separate Server Off**, and **Run Under One Process
Off**. Launch **New Editor Window (PIE)** so each process can use a different
Steam account. In each process open **Tab > CkOnline > Steam Lobby**.

1. Host selects **Create Lobby** and waits for the gym to report OSS session
   success plus a Steam lobby ID. Client selects **Find/Join** and selects the
   host result. Both panels must show the same lobby ID and one member each.
2. Host selects **Invite Overlay**, invites the other account, then leaves and
   repeats from a fresh lobby. Accept one invitation while the client is
   already running and the other from a cold game launch. Each must enter the
   same Ck join-intent pipeline and finish as one lobby membership; repeated
   callbacks must not produce a second join.
3. Each client selects **Leave**. Confirm both Ck views become unjoined and a
   later create starts a new lobby ID. Check a deliberate Steam-unavailable
   launch reports unavailable and offers explicit Direct selection without
   starting Direct automatically.

The test card must repeat this exact two-process join setup and key sequence.
If App ID/two-account setup is not available, only the fake-backed code gate
can be reported; the Steam runtime gate remains pending. No packaging, cross-
machine Tailscale or installed-build claim follows from this editor check.

## Current blocker and preserved work

The project App ID, two Steam accounts and first consuming game are not present
in this checkout. Code can proceed through the fake-backed gate, but live invite
acceptance cannot be claimed or self-tested here. Do not add secrets or App IDs
to source/config or overwrite project provider configuration to make the gym
appear green.

Keep this gate's implementation/evidence in permanent commits, independently
from the two research-only `[DROP BEFORE MERGE]` commits. Preserve unrelated
root/submodule work and use only the selected existing checkout.
