# Direct text and shareable join gate

Written 2026-10-03. Current evidence and moving status live in ../PROGRESS.md.
This contract is superseded when the permanent consumer guide replaces it.

After this gate, admitted lobby members exchange typed text in both directions,
see the same host-assigned sequence/sender, and use a copied join descriptor to
join explicitly. This builds on Gate 3's persistent TCP membership, not gameplay
replication, Steam social services, voice or durable direct messages.

Keep text within the same feature/provider lifecycle. Use bounded framed values,
host-derived sender identity, per-member request limits, a bounded transcript and
private wire correlation tickets distinct from public operation IDs. A local
send request is Pending until the host acknowledges it. Cancelled/timed-out/stale
responses cannot complete later work or leave runnable partial membership.

Acceptance:

- Host and guest send different typed strings; both see each once in sequence,
  attributed to the actual admitted member. Ready and chat cannot spoof sender.
- Empty/oversize/malformed/unauthorized text and rate floods fail explicitly;
  transcript/queue memory stays bounded and the provider keeps healthy peers.
- Travel does not reset the transcript, service generation or membership.
- Expired/previous-host-generation descriptors cannot admit into a new lobby.
- Online Chat gym offers focused text editing and visible acknowledged transcript,
  plus copy/paste join details. Focused input suspends gym key captures and restores
  viewport focus on commit/cancel. It includes instructions for comparing two
  windows and for testing retention through travel and host loss.
- Native tests use actual sockets and verify negative cases and shutdown. The
  two-process rendered input/clipboard path remains an explicit human checklist.

No automatic Steam fallback, public discovery, account service, SDK installation,
asset cleanup, Shipping build or published-history rewrite belongs to this gate.
