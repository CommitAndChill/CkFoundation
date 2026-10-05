# Code-authored layer lifetime

`UCk_UI_LayerMembership_UE` is a reflected observer for one realized widget's
admission to a player's CkUI layer. It supplies the lifetime distinction between
CommonUI covering a retained parent and removing it. It has no dependency on
CkWidgets and does not own pause, focus, input or the widget.

Prepare the widget before pushing it. After the push succeeds, create and retain
a membership object, then call `TryObserve(Owner, LayerTag, Errors)`. Admission
requires the game thread and initialized Slate, the owner's current local
controller/player and consistent live world, its current CkUI layout and exact
layer, and cached owner/stack Slate roots. It does not construct missing Slate or
register a player. Rejected observation publishes no handles/context and can be
retried before any successful admission or explicit Stop.

```angelscript
// Retain this member so deferred retirement can reach the consumer.
UPROPERTY()
private UCk_UI_LayerMembership_UE _Membership;

// After the exact prepared Owner has been successfully pushed:
auto Candidate = Cast<UCk_UI_LayerMembership_UE>(NewObject(this, UCk_UI_LayerMembership_UE));
if (!ck::IsValid(Candidate)) { return false; }
auto Errors = TArray<FString>();
if (!Candidate.TryObserve(Owner, LayerTag, Errors))
{
    Candidate.Request_Stop();
    // Roll back only the candidate entry through the screen's own policy.
    return false;
}
_Membership = Candidate;
_Membership.OnRetired.AddUFunction(this, n"DoOnRetired");
```

Use `Get_IsCurrent()` before input/action work. The query is synchronous and
callback-free. It checks the exact weak controller/player/world/subsystem/layout/
layer identities, cached roots and stack admission generation. Activation is
independent: a covered parent remains current and may retain its session and
pause ownership while suspending interaction.

The stack publishes a nonzero generation before Slate construction/activation
can reenter. Cover/uncover preserves it; removal/re-push, including same-frame
reuse of a pooled UObject, changes it. Resource teardown revokes records before
callbacks and retains the monotonic counter. Generation zero represents no
accepted current admission, including exhausted generation allocation.

Pop/clear/displayed/layout events and CoreTicker reconcile invalid contexts.
`OnRetired(InMembership)` runs at most once, from CoreTicker after observation
has been revoked and native subscriptions removed. It can execute while the
world is paused. Ticker-detected invalidation queues delivery for a later
invocation. This is deferred notification, not immediate committed-removal
notification; queries must gate work during the interval.

Before terminal consumer cleanup, compare `InMembership` with the consumer's
current membership object. A callback may create a newer entry. Retire only the
old admission's resources; do not let its later cleanup target the newer
session, focus or pause owner. The observer itself never removes either widget.

`Request_Stop()` silently cancels observation and pending notification. It is
terminal and idempotent. A successful observation also consumes that object's
single lifetime; reopening requires a fresh membership object. The observer's
native delegates/ticker and captured context are weak, so they retain neither
the observer nor its owner. The caller must retain the observer to receive
retirement. A callback-local strong reference protects it if a callback drops
the last external root and runs GC.

The CkTests [layer membership proof](../../../CkTests/Documentation/UI/LayerMembershipProof/README.md)
passed a focused 13-test real-renderer Editor gate. Its actual AngelScript
UObject probe exercises reflected admission/queries/stop and delegate delivery;
native cases cover CommonUI cover/uncover, paused clear, inherited remove/re-push,
class-pool reuse, layout/resource teardown and callback GC/reentry. This does not
establish a migrated Pause menu, nonzero transitions, Widget Blueprint VM or
packaged execution. See the proof for its complete limits.
