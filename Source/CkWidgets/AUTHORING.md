# Authored UMG from scripts

Before authoring a screen, apply the [shared UI authoring rules](../CkSlateLayout/UI_AUTHORING_RULES.md). They cover fonts, measured padding/alignment, reusable widgets, visual effects and rendered acceptance. CkTests links below are optional examples and historical evidence; the rules and API guidance live in CkFoundation.

`UCk_UI_AuthoredSession_UE` adapts scalar and flat collection models for `FCkUiView`. A script can construct a normal UMG widget tree, own the session, declare values/actions and mount an accepted document in `UCk_UI_AuthoredSurface_UE`. The underlying HTML/CSS grammar and native controls remain [CkSlateLayout](../CkSlateLayout/AUTHORING.md). The adapter does not introduce a second parser or layout engine.

The first implementation is exercised by CkTests' [scripted card](../../../CkTests/Documentation/UI/ScriptingCardProof/README.md). Its [focused Editor gate](../../../CkTests/Documentation/UI/ScriptingCardProof/verification.md) passed 11 selected tests with five inspected captures. This establishes the scalar scripting path in that Development Editor fixture.

1. Create the session with a reflected UObject owner and retain it through a script member. In C++, use a UPROPERTY or another explicit GC root. Native view/widget references do not retain the session.
2. Build `FCk_UI_AuthoredSession_Config` with arrays of `FCk_UI_AuthoredText`, `FCk_UI_AuthoredNumber` and `FCk_UI_AuthoredBool`, plus declared actions and regions. `Regions` defaults to `main`; use `Set_Regions` for other names. The document must declare exactly this complete region set on every reload. Entries have Name/Value constructors and CK getters/setters. Call `TryInitialize`; handle a false result and its errors before loading or mounting anything.
3. Bind the dynamic action/proposed-edit events using `AddUFunction`. Match the delegate signature, including `const FString&` names and `float32` numeric proposals in AngelScript. Native events propose changes; handlers publish accepted state explicitly through typed setters.
4. Call `TryReload` with complete markup/CSS strings or `TryReloadFiles` with both paths. A failure preserves the committed document/revision; diagnostics change. An accepted reload retains model values.
5. Construct the authored surface inside the UUserWidget tree and call `TryMount` with an accepted region. Compose scaling/safe areas in the ordinary UMG shell. The adapter only owns its view and model.
6. Call `Request_Release` on teardown before detaching the shell. Destruction also retires the native owner. Release is terminal and idempotent; reopening creates a new session.

The production [AngelScript card](../../../CkTests/Script/CkUiAuthoring/CkScriptingCard_Gym.as) shows the complete construction/event/teardown path without a Widget Blueprint or C++ screen host.

Declarations freeze after initialization succeeds. A malformed declaration set publishes no partial model/view and can be retried. Scalar, action, collection and item-action names follow the parser's identifier grammar, are case-sensitive and are unique across those declaration kinds. Regions have a separate nonempty, unique name set; initialization prepares all retained mounts before publishing the view. Numbers must be finite. Wrong-type/unknown names and nonfinite setters reject without mutation; failed getters preserve their output values.

## Shared style tokens

Before initialization, use `Config.Set_StyleTokens` to provide named CSS values:

```angelscript
TMap<FString, FString> StyleTokens;
StyleTokens.Add("--space-s", "12px");
StyleTokens.Add("--panel-inset", "16px 20px");
StyleTokens.Add("--ink-body", "#CBC2AE");
Config.Set_StyleTokens(StyleTokens);
```

Use them in supported declarations, for example `padding: var(--panel-inset);`
or `color: var(--ink-body);`. Token names start with `--`; each name and value
is limited to 1,048,576 characters. The accepted map is copied into the
native view and frozen for the session. Mutating the caller's config does not
change subsequent reloads. Referenced unresolved tokens or values invalid for
their target property reject reload and preserve the accepted document.
This exposes the native token resolver; it does not add CSS inheritance or
browser custom-property declarations.

## Required source contracts

Optional `Config.Set_RequiredButtons` declares source contracts with
`FCk_UI_AuthoredButtonRequirement(Id, Action, EnabledBool)`. `Region` defaults to
`main`; `Set_Region` selects another declared region. Every field must be an
identifier, requirement IDs must be unique, and the action and enabled bool
must name declarations of those exact types. Empty/default requirements,
unknown declarations and unknown regions reject initialization atomically.
The requirement list freezes with the accepted configuration; changing the
caller's array does not change the session's contract.

Every string or file reload checks those contracts against the production
parser's document before staging or committing widgets. Each required ID must
identify an ordinary concrete button in its exact region, with the exact
`action` and `enabled-bind`. Repeat templates, custom slots and table/tree cell
views cannot satisfy a root-view requirement; nested scoped views do not inherit
the root requirement list. A mismatch preserves the accepted view, revision,
controls, model and focus, without dispatching actions or proposals. An empty
requirement list preserves the existing source admission behavior.

One session may mount on one surface during its lifetime. Invalid mount candidates leave the surface unchanged. Replacing or releasing the surface's native view makes the old session unusable. Releasing a stale session never clears the newer surface owner. Initialization, reload, mounting and scalar access require the game thread and initialized Slate; retirement still runs when Slate is unavailable. Recursive construction/reload/mount operations reject, and events are suppressed during native operations or after retirement.

Text maps to text/search bindings, booleans to visibility/enabled/checkbox values, and numbers to numeric bindings. The session internally registers the existing slider and checkbox. A slider's required `interaction` refers to its number declaration and emits a proposal on commit; optional `changed` also emits live proposals. Use both only when the consumer intentionally handles both. Actions refer to the declared action list.

Optional `Config.Set_RegisterHudControls(true)` registers `hud-meter`, `hud-graphic`, `hud-orb`, `hud-ability` and `key-hint`;
`Set_RegisterRadialControls(true)` registers `radial-selector`.
`Set_RegisterItemControls(true)` registers `item-slot`, `atlas-image` and
`inventory-grid`. The [spatial inventory contract](../CkSlateLayout/AUTHORING.md#spatial-inventory-grids)
documents multi-cell records, rotation, cross-grid proposals and their ownership.
`Set_RegisterMapControls(true)` registers `world-map`; its
[native grammar and marker schema](../CkSlateLayout/AUTHORING.md#world-maps)
are documented alongside the other custom controls. All four flags default to false. Registration is completed
before view publication, and a failed registration rejects the full session.
The native [HUD and radial grammar](../CkSlateLayout/AUTHORING.md) remains the
source of control properties and collection schema requirements.

`Config.Set_Images` declares named texture resources. In AngelScript:

```angelscript
FCk_UI_AuthoredImage Frame("hud-frame");
Frame.Set_Texture(FrameTexture); // UTexture2D loaded/owned by the host
TArray<FCk_UI_AuthoredImage> Images;
Images.Add(Frame);
Config.Set_Images(Images);
Config.Set_RegisterHudControls(true);
Config.Set_RegisterRadialControls(true);
```

The Blueprint struct exposes Name and Texture; C++ uses the same Name
constructor and `Set_Texture(UTexture2D*)` accessor. Textures must be valid
nonnull objects with positive dimensions, and image names share the scalar/action/collection namespace.
Invalid names, duplicates, null/stale textures and zero dimensions reject initialization before
any view or model publication. The caller can correct the configuration and
retry. `TrySetImage(Name, Texture)` replaces only an admitted image; invalid
textures or dimensions, wrong-type/unknown names and retired sessions reject without mutation.
Bind the resource with `hud-graphic image-bind="hud-frame"`. Each resource has a stable session-owned
brush and a reflected strong texture reference. Copied native image attributes
also retain the brush and texture, keeping externally held graphics safe across
session release and GC; retired attributes return null. Resource retention ends
when those native references are dropped. Release freezes native controls and
suppresses further events and updates.

Every declared Text scalar also supplies a native String binding using its
current `FText.ToString()` value. For radial selection, declare a Text scalar
such as `selected-spell` and author `selection-bind="selected-spell"`. Declare
`select-spell` and `activate-spell` in `Config.Set_ItemActions`, then author
`select="select-spell" activate="activate-spell"`. Both emit the existing
`OnItemAction(ActionName, Key)` event with a copied stable key. Event handlers
accept selection explicitly with `TrySetText("selected-spell", FText::FromString(Key))`;
activation can deploy the keyed command. The bridge does not mutate selection
on proposals. Dispatch uses the same admitted input-owner and operation guards
as ordinary item actions and remains safe when a listener replaces records or
releases the session.

The session supplies Core's Regular composite as its base font; supported CSS
size/weight choices derive from that valid source. The later native
[default-font repair](../../../CkTests/Documentation/UI/DefaultFontProof/README.md)
also normalizes missing native sources and nonfinite sizes. Its focused receipt
is separate from the original scalar-session gate.

`Config.Set_FontFamilies` optionally declares named font resources before
initialization. Each `FCk_UI_AuthoredFontFamily(Name, RegularFile)` requires a
lowercase CSS family name and a readable, nonempty regular font file. Use
`Set_BoldFile`, `Set_FallbackRegularFile` and `Set_FallbackBoldFile` for optional
faces; an empty optional path omits that face. A bold fallback requires both a
regular fallback and a primary bold face. Names follow the native font-family
grammar, are case-sensitive and unique within the font list, and cannot be
`monospace` or `sans-serif`. These declarations are separate from scalar/action
names. The existing CSS `font-family` and `font-weight` syntax selects admitted
families; no file path enters markup or CSS. An empty list retains the Core
base-font behavior.

`TryInitialize` admits the full list before publishing a view. Any invalid
name, duplicate, missing prerequisite, unreadable file or invalid font data
returns the native font error and leaves the session retryable without a partial
catalog. Registration loads and owns validated font bytes on the game thread;
the view retains an immutable catalog after the caller's config is gone. The
constructor and config setters perform no file I/O. Supply paths available to
the running host; this API does not discover installed plugin resources or
guarantee packaged-file availability. The separate
[scripted-font proof](../../../CkTests/Documentation/UI/ScriptedFontProof/README.md)
passed 15 selected checks in one fresh real-renderer Editor boot. It crosses the
actual script constructor/setters into mounted Regular/Bold faces and exact
source bytes, rejects initialization/reload failures and retains inline faces
through session release and GC. Packaged lookup remains unverified.

The direct root under each region is sized and aligned by its native mount. Do not give that root explicit dimensions, minimum/maximum dimensions, flex growth/shrink or alignment. Apply screen geometry in the UMG shell and size containers below the region root. Static XML/class/identity preflight does not validate this rule or the complete native binding/style grammar; native source admission remains required.

### Flat collections and item actions

`Config.Set_Collections` accepts `FCk_UI_AuthoredCollection(Name)` declarations.
Set each declaration's schema and seed records before initialization. A schema
field uses `FCk_UI_AuthoredFieldSchema(Name, Kind)` and is required by default;
`Set_Required(false)` permits omission. Supported `ECk_UI_AuthoredFieldKind`
values are Text, Number, Bool, Integer and Color. A record uses
`FCk_UI_AuthoredRecord(Key)` and an array of `FCk_UI_AuthoredFieldValue(Name, Kind)`;
set the matching typed value. Number uses `float32` in AngelScript. Unknown enum
values, duplicate fields, incompatible values and malformed seeds reject the
complete declaration set before a session is published.

Bind a native repeat with its collection name and bind fields inside its scoped
item template. `Config.Set_ItemActions` declares stable-key actions. A button's
`item-action` dispatches `OnItemAction(ActionName, Key)`; bind a script function
with two `const FString&` arguments. An item action is separate from a root
ordinary-button action and does not imply selection or screen policy.

`TrySetRecords(Name, Records, Errors)` validates and replaces the complete retained
collection atomically. Surviving keys retain native record identity; removed and
reinserted keys get new identity. An identical ordered replacement is a no-op.
Pure reorder advances the collection revision and notifies once without changing
unchanged record revisions. `TryGetRecords` copies supplied fields in schema
order, preserving omission of optional fields; `TryGetCollectionRevision` reads
the retained model revision. Failed getters preserve their output arguments.
`Get_Collection` is read-only native observation, not a reflected mutation API.

Native model publication precedes staged presenter refresh; an accepted setter
does not promise synchronous successful widget realization. Keyed item roots,
search controls and custom widgets can retain identity across refresh. Ordinary
row buttons are recreated even for pure reorder, so resolve the current button
before later interaction. This bridge establishes no repeated-item controller
focus continuity. Held callbacks reject removed or replaced record identities
and stale scopes/revisions before dispatch.

Synchronous collection listeners cannot recursively mutate the session. If a
listener releases the session, the outer setter returns failure after retirement
without rolling back the publication already accepted by the native model.
Retained readers may outlive the session but cannot revive its usability or
events. The script-owned [Mission Board](../../../CkTests/Documentation/UI/ScriptedCollectionProof/README.md)
is the consumer for this boundary. Its focused fresh Editor gate passed 32
selected tests with 33 inspected captures. The receipt separates reflected flat
support from native nested-collection sibling coverage and preserves the earlier
failed admission and geometry attempts.

### World maps from scripts

Opt in with `Config.Set_RegisterMapControls(true)`. Declare a `Markers` flat
collection with the [world-map schema](../CkSlateLayout/AUTHORING.md#world-maps),
an image resource through `Config.Set_Images`, a Text scalar for the selected
key, a Bool scalar for route visibility, and a keyed action through
`Config.Set_ItemActions`. The host retains the `UTexture2D` used by the image
declaration. For example:

```html
<world-map id="map-view" markers-bind="Markers" image-bind="MapImage"
           select="SelectMarker" selected-key-bind="SelectedKey"
           route-bind="RouteVisible" origin-x="0.3" origin-y="0.55"
           font-family="map-ui" class="map" />
```

`select` dispatches `OnItemAction(ActionName, Key)` with the marker's copied
stable key. The script validates current eligibility and publishes
`TrySetText("SelectedKey", FText::FromString(Key))`; the native map does not
change selection on its own. The script owns filter and route policy. Keep
filtered records in the collection with Bool `visible` if identity should
survive filter changes, and change or clear a selection before hiding its key.
The `selected-key-bind` shown above is required for the map's arrow/D-pad
selection and Enter/face-button confirmation; without it those keys are
unhandled, though pointer selection can still propose a key.
`RouteVisible` only requests a straight visual line from the authored origin;
the control does not calculate a traversable route.

An owned, active, mounted session exposes `TrySetMapCamera(Id, Center, Zoom)`,
`TryZoomMap(Id, Factor)`, `TryRevealMapMarker(Id, Key)`, and
`TryGetMapCamera(Id, OutCenter, OutZoom)`. They resolve one visible, enabled
map by its exact authored ID in the admitted surface and reject if input is
inactive, the control is ambiguous, an operation is in flight, or a pointer
gesture blocks the camera change. Zoom is restricted to `[1, 8]` and the
center is clamped to the fitted image. Reveal requires a visible enabled key.
Rejected requests preserve the current camera; a failed getter preserves both
outputs. Camera state belongs to the retained widget, so compatible reloads
keep it, while a newly created session starts from the default camera.

`TrySetRecords` validates the flat collection schema and replaces its records
atomically. The map then validates normalized coordinates, supported kinds,
and its 1024-record limit against that revision. A schema-valid but map-invalid
update can be accepted by the collection while the map keeps its last accepted
marker snapshot and refuses input. Validate the complete candidate before
publishing it; do not treat a successful collection setter alone as map
admission. An invalid source reload preserves the last accepted document;
reload during map pointer capture rejects. End owned input before hiding or
removing the screen, and release the session on terminal teardown. The native
map drains its transient capture and callbacks on ownership loss or release.

### CommonUI input and authored button focus

Set the optional `InteractionOwner` on the configuration before initialization
to bind a session to a `UCommonActivatableWidget`. Admission requires a real
local controller/player. Its getter/setter copy a nullable object handle, so
scripts can pass a derived widget directly or set `nullptr` for a passive
session; assigning a handle does not validate or activate it. Admission requires a
consistent live world and that player's existing
Slate user. The session freezes those identities and holds them weakly. An owned
surface must be inside that exact owner's UUserWidget tree. Initialization does
not activate the owner or register a missing Slate user.

After the prepared widget enters its layer and activates, call `TryBeginInput`.
It requires the admitted owner to be active and its accepted view to remain on
the admitted surface. Call `Request_EndInput` when the owner deactivates. Ending
input suspends action/edit callbacks and drains matching outstanding key/pointer
releases, while retaining the accepted model, view and revision. Model setters
remain available while covered. Reactivation calls `TryBeginInput` on that same
session. Unbound passive sessions keep their original dispatch behavior.

`TryFocusButton(Id)` resolves a current ordinary authored button for the owning
Slate user. `TryNavigateButtons(Ids, Direction)` accepts a nonempty, unique array
of current button IDs and direction `-1` or `1`; it wraps and skips disabled or
otherwise ineligible buttons. Missing/nonbutton IDs and invalid directions reject
before changing focus. Both calls refresh native attributes and recheck owner,
view, revision and widget identity around focus callbacks. They copy caller IDs
before callbacks, so mutation of the supplied string/array cannot alter the
in-flight request. A callback's focus
redirection wins; navigation does not retry another button after that attempt.
An owned session opts into the native view's stable-ID button focus restoration
on accepted reloads.

`Get_IsButtonFocused(Id)` is a read-only exact-focus query for that admitted
Slate user and the view's current ordinary button. It returns false for an
unbound/inactive session, invalid context, unknown ID or nonbutton, and does not
refresh attributes or invoke focus callbacks. A screen can query before
disabling a button, then navigate its declared button list if that button owned
focus.

`Get_IsInputActive` queries the current admission; `Get_SlateUserIndex` returns
the admitted existing user index or `INDEX_NONE` for an invalid context. Ending
input restores prior focus only while current focus remains inside this owner,
so focus already established elsewhere is preserved. For terminal CommonUI
handoff, call `Request_EndInput(false)` before child close or layer removal:
it suspends dispatch and drains matching releases, then discards saved focus
without restoring it. A later `Request_Release` cannot restore that discarded
target. The default argument is `true`, preserving ordinary covered/resumed
input behavior. The screen owns its terminal focus policy; CommonUI removal
may complete after a transition, so requesting removal is not proof that the
old admission has already disappeared. `Request_Release` remains
terminal and disables dispatch before teardown focus callbacks. The release
guard outlives that call only to finish draining and retains no UObject owner.

This is an input/focus adapter. The screen still owns pause acquisition,
entry/push rollback and terminal session cleanup. The separate
[CkUI membership companion](../CkUI/AUTHORING.md) observes one exact layer
admission and provides deferred retirement after invalidation. Deactivation also happens when CommonUI
covers a retained parent; it must not be treated as terminal release in that
case. Ordinary button callbacks carry no user payload, so supplying an owning
Slate user does not by itself prove complete rejection of foreign-user actions.

The [owned-session proof](../../../CkTests/Documentation/UI/OwnedInputProof/README.md)
passed 13 focused checks with a real CommonUI stack and nonzero local-player
Slate user. This API was invoked from C++ in that historical gate; it did not
verify the new button requirements or terminal no-restore mode. The actual
[scripted Pause consumer](../../../CkTests/Script/CkUiAuthoring/CkScriptedPause_Widget.as)
now implements screen policy around the session and membership companion,
including required buttons and a prepared native Settings child. Its new
[focused integration gate](../../../CkTests/Documentation/UI/ScriptedPauseProof/README.md) passed 15 selected checks, including the real script consumer, required-button contracts, terminal no-restore mode and compatible native Settings/Inventory. The receipt distinguishes tested scenarios from code-inspected focus rollback and remaining limits.

The later [layer membership proof](../../../CkTests/Documentation/UI/LayerMembershipProof/README.md)
verified native lifetime cases and an actual AngelScript UObject consumer of
that independent observer. It does not pair the observer with an authored
session or migrate Pause policy automatically. Check its synchronous current
query before resuming input, and compare retirement callback identity before
cleaning up an old session. Covered parents retain session/pause ownership.

The adapter does not yet reflect Image/FloatSeries fields, nested child
collections, trees, custom factories, polling or compound model transactions. Those still use native
integration. File paths and resource staging belong to the owning module;
`TryReloadFiles` does not establish installed-plugin discovery or packaged
availability. See the header for the reflected API and its failure contract.
