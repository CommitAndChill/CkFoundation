# Renew Direct invites without restarting the lobby

Written 2026-10-04. Current evidence lives in ../PROGRESS.md. Retire when the
permanent consumer recipe fully replaces this gate. The user authorized continuing
the replacement phases while AFK; all rendered editor acceptance stays deferred.

Research read the Direct quartet, feature dispatch, GI facade, transport Host/Join/
Execute and admission paths, existing real TCP expiry/ready/text tests, shared gym
actions and AS smoke. The proven neighboring feature is the existing Direct Host
action: Utils admission, processor-owned monotonic clock, transport completion,
immutable snapshot capture and post-scheduler dispatch. No new module is needed.

Today Host issues one expiring descriptor. P only copies it; rehosting closes peers
and changes lobby identity. RenewInvite will rotate the private capability and
restart its configured lifetime while the current lobby remains intact. The action
is appended to the enum so existing serialized values retain their meaning. It
does not change framing, trust/admission policy, account authentication or gameplay
credentials. Joins accepted before rotation remain members; old capabilities
processed after rotation reject. Already-admitted peers continue using their socket
membership and epoch, independently of the invite secret.

1. Foundation: append RenewInvite, admit it through existing bounded Utils requests,
   and execute it locally only in a live Hosting state. Validate the immutable spec,
   advancing deadline, fresh nonzero/different capability and descriptor before
   committing any invite value or LastNow. Rejected renewal Execute leaves its
   clock cache unchanged; Host ticks and Poll still advance their clocks independently.
   A Host.Tick(max) test must not claim normal-clock recovery. Publish one revision
   and authoritative completion;
   preserve generation, epoch, address, peers, identities, readiness and transcript.
   -> Native TCP tests exercise renewed admission and revoked old capability,
   joined/offline rejection and unchanged state. No new wire message or secret log.
2. CkTests: extend existing InviteExpiry, LoopbackLobbyReadyTextLeaveRejoin and
   ExtremeClockRejectsBeforeTransport tests rather than create parallel mirrors.
   Verify real TCP Expired then renewed same-epoch success, old invite Unauthorized,
   connected two-ready-member/transcript/binding continuity, and safe deadline failure.
   -> Reuse the eight-test Gate10 population with focused siblings and fresh logs.
3. CkTests gym: add a host-owned Request_RenewInvite that never starts/replaces a
   profile, append details-only Y at fixed row 42 before dynamic transcript rows,
   and keep existing J/P/K/F2/F3 action indices. P copies the completed new invite.
   -> AS smoke exercises the actual reflected renewal action plus foreign-profile
   rejection. Compile/loading proves binding; rendered control remains manual.
4. Write a short self-contained deferred editor check. Setup is two separate
   Standalone PIE windows; join Host J, wait Hosting, P / Client K, both R. Keep an
   old invite, renew with Host F2/Y, compare identities/ready/chat, then prove old
   invite rejection and new invite admission after Client X. No five-minute wait
   is required for the basic check; native expiry coverage handles that deadline.
   -> [EDITOR-VERIFY, DEFERRED] Report observed results only. Blueprint-facing and
   two-window/clipboard behavior, remote reachability and packaging stay separate.

Entry: root 77687ce, Foundation 0006ad86a, CkTests 599d7cfc, CkAuto f1983a7e.
Baseline: OnlineGameplayRecoveryGate-02 incremental build/eight focused tests pass,
fresh logs have no Errors/ensures/script failures, nonfatal warnings remain.
No equivalent baseline rerun. Planned final gate: incremental build plus the same
eight tests, two fresh boots (discovery and one serial lane). The known-red
two-world Iris fixture remains excluded. Preserve unrelated work, current branches
and both separate removable research commits; no new checkout, push or cleanup.
