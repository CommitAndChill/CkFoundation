# Advertised gameplay endpoint

Written 2026-10-04. Moving evidence lives in ../PROGRESS.md. The user deferred
Gate 7/8 editor checks while AFK and authorized advancing to this phase.

The smallest next slice replaces the ready-lobby consumer's hardcoded gameplay
loopback address with the host address already carried by the validated Direct
invite. Default local behavior stays 127.0.0.1. This is a private-network gym
handoff, without a new wire version, credentials, member authentication, provider,
engine, firewall or orchestration change. Existing native gameplay port arguments
remain supported; this gym uses gameplay 7787 and control TCP 7788.

Research read the Direct snapshot/spec/transport, lobby binding, IP travel helper,
shared Lobby/Chat panel and existing real TCP service tests. Proven neighbors are
the strict production IPv4 parser, authoritative joined-roster completion and
the frozen ready-binding validation before travel and during gameplay.

1. Foundation owns a reflected address-only HostAddress snapshot value and a
   reusable strict validator. Publish after successful Host or authoritative
   Joined admission; keep the joining candidate private and clear it on failure,
   close and loss. Reuse existing Changed dispatch and wire fields.
   -> Extend existing real TCP tests for publication, rejection, lifecycle clear
   and a client spec whose local address differs from the invite target.
2. CkTests freezes and validates HostAddress in FLobbyBinding, compares it during
   all binding revalidation, displays it on bridged Host and rejects a Client
   endpoint with a different address before any bridge/travel mutation. Keep
   standalone IP behavior and valid custom-port API behavior.
   -> Extend existing binding/parser assertions, including atomic output clearing
   and malformed or changed addresses. Build/AS getter loading is the integration
   gate; the known-red same-process Iris fixture keeps its verdict.
3. Shared Lobby/Chat G composes the admitted host address with gym port 7787 and
   exposes the target under F2 without moving existing action indices. Add a short
   self-contained editor guide and link it from the deferred-check index.
   -> [EDITOR-VERIFY, DEFERRED] Local: Host J, wait Hosting, P / Client K. Both R,
   Host G, then Client G/P, X. Two machines: Host F2/L selects its reachable IPv4
   before J/P; Client F2/O pastes that new invite, Enter/F3 joins; then R/G/P/X.
   Compare gameplay endpoint/proof and preserved lobby. No two-machine pass is
   inferred from a native loopback test.

Ownership is disjoint: Foundation snapshot/transport; CkTests native binding/IP
helper; native service spec; AS panel/editor guide. Root owns scope, integration
and final evidence. Entry revisions are root 2ccf30f, Foundation 2cb3c8f65,
CkTests 0df75364, CkAuto f1983a7e. Gate 8's seven focused tests and incremental
build pass with clean fresh logs. Gate 6/7/8 changes are uncommitted; preserve
all unrelated dirty work and both separate [DROP BEFORE MERGE] root commits.

Verification, 2026-10-04: OnlineAdvertisedEndpointGate-01 passed the incremental
build and all seven selected Direct/script tests, zero failed/skipped/contaminated,
exit 0, 39 seconds of tests, two fresh editor boots. Combined/discovery/test logs
contain no Error records, ensures or script errors. The existing real TCP test
uses host 127.0.0.2 and client local spec 127.0.0.1, covering admission, rejoin,
address clearing and frozen binding/endpoint rejection. Independent source reviews
found no material scoped issue. Rendered/local gameplay and two-machine checks
are deferred by the user; renderer-only and known-red same-process IP tests were
excluded. No commit, push or history rewrite occurred.
