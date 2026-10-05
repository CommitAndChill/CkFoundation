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
- OSS lobby-invite acceptance is ignored while `CurrentSessionSearch` is valid.
  An invite accepted during an already-running search is dropped inside engine
  code before Ck can observe it. Serialize Ck-started searches and invite joins,
  but document that this cannot recover an invite accepted mid-search. Gate17's
  invite acceptance check must run when no search is active; lossless concurrent
  invite/search support needs a separate engine fix or API decision.
- `FindSessionById` currently calls its completion with an empty failed result,
  so it cannot resolve an invite. Use the session search result delivered by
  Steam's invite callback instead.
- The invite overlay API returns success after activating the overlay, not after
  the user sends an invite. Report that as UI-opened only; do not complete a
  friend-invite operation as sent without an authoritative Steam result.
- Steam OSS is disabled by default in its engine plugin descriptor. Keep Steam
  optional and do not enable it globally or invent an App ID in the host project.

These are source findings for the local engine revision, not live Steam proof.
The relevant implementation is in `OnlineFriendsInterfaceSteam.cpp:119-128`,
`OnlineExternalUIInterfaceSteam.cpp:67-93`,
`OnlineSessionInterfaceSteam.cpp:716-789, 767-772, 1152-1229`, and
`OnlineSessionAsyncLobbySteam.cpp:1205-1220` under the selected engine's
`Engine/Plugins/Online/OnlineSubsystemSteam/Source/Private` directory.

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
   completion before notifications. Require matching `bUsesPresence` and
   `bUseLobbiesIfAvailable` values, and use the `SEARCH_LOBBIES` query flag.
3. Show the Steam invite overlay only while the owned lobby session is valid.
   Bind warm invite acceptance before any search; copy accepted lobby results
   into one bounded correlated join intent and feed them through the same
   admission path. Serialize Ck-started searches and invite joins, while
   documenting that the engine can drop an invite accepted during an already
   active search. Do not claim lossless invite delivery in that overlap or fall
   back to the unsupported FindSessionById.
4. A test seam for fake session callbacks. Cover synchronous callback,
   duplicate/stale callback, cancellation, timeout, account-generation change,
   lobby-full/rejected join and shutdown re-entry without Steam or credentials.
5. A CkTests Steam Lobby gym with explicit setup diagnostics and no automatic
   pass. Keep display name and provider account ID separate; never use names as
   identity. Do not expose SDK IDs or invite payloads in diagnostic logs.

Defer a persistent cross-provider friend graph, offline inbox, chat, voice,
EOS/EGS, public matchmaking, dedicated servers and Direct-to-Steam fallback.
Defer cold-launch invites to a separate installed-build gate because Steam must
launch the registered game executable with its lobby command-line intent; an
editor PIE session launched directly from Unreal Editor does not prove that
launch path.
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
project-owned Steam App ID, two computers that can reach each other, Steam
running on each computer, and one licensed account per computer. One Steam
client cannot represent two accounts just because two PIE processes are open.
On each computer, open
`Plugins/CkTests/Content/TestGyms/TestGyms_CkTests_Level.umap`. In Play > Advanced
Settings choose **Number of Players 1** and **Play Net Mode Play Standalone**;
keep **Launch Separate Server Off** and **Run Under One Process Off**. Launch
**New Editor Window (PIE)** on both computers. In each process open
**Tab > CkOnline > Steam Lobby**. Join the same lobby by using the explicit
**Find/Join** action shown by the gym; on the joining computer, select the host's
listed lobby and wait for **Joined** before checking either roster.

1. Host selects **Create Lobby** and waits for OSS session success plus a Steam
   lobby ID. Joining computer selects **Find/Join**, selects that host listing,
   and waits for **Joined**. Both panels must show the same lobby ID and the
   expected two-member roster.
2. Host selects **Invite Overlay**, invites the other account, and the joining
   computer accepts while no search is active. Confirm its visible invitation
   enters the same correlated join pipeline and reaches exactly one membership.
   Then both leave; host creates a fresh lobby and repeats while the joining
   computer is already running to check the warm in-game invite. Do not run a
   separate lobby search during either invite acceptance. An invite accepted
   during an already-running search is a known engine limitation and is not a
   pass/fail check for this gate.
3. Each computer selects **Leave**. Confirm both Ck views become unjoined and a
   later create starts a new lobby ID. Check a deliberate Steam-unavailable
   launch reports unavailable and offers explicit Direct selection without
   starting Direct automatically. Cold-launch acceptance is not an editor pass;
   it needs a later installed-build check through the registered Steam launch
   path.

For every join above, the host is the computer that pressed **Create Lobby**;
the other computer joins by selecting the host's listed lobby with **Find/Join**
or by accepting the host's Steam overlay invite. Wait for **Joined** on the
joining gym before treating lobby membership as successful.

The test card must repeat this exact two-computer, one-process-per-computer join
setup and key sequence.
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
