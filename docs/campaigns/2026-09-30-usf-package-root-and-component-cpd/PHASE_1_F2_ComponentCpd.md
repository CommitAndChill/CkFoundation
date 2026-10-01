# PHASE 1 / F2 — `Request_SetCustomPrimitiveData` on CkUnrealComponent

Read `PROMPT.md` in this folder first. You edit C++ and write AngelScript test files. You do not build, run
the editor, commit or stage.
You own: `Source/CkUnrealComponent/**`, the CkUnrealComponent rows of `Source/CLAUDE.md`,
`Plugins/CkTests/Script/CkUnrealComponent/**`. Another executor is editing `Source/CkUsf*/**`; do not touch it.

## Entry criteria

- `git -C D:/Repo/Mars/Plugins/CkFoundation status --short` shows nothing under `Source/CkUnrealComponent`.
- You have read the CkTimer quartet and the root `CLAUDE.md` § Requests / Request completion.
- Anything else: STOP, record in `PROGRESS.md` Blockers.

## Pre-designed surface

`CkUnrealComponent_Fragment_Data.h` (after the Spec):

```cpp
USTRUCT(BlueprintType)
struct CKUNREALCOMPONENT_API FCk_Request_UnrealComponent_SetCustomPrimitiveData : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_UnrealComponent_SetCustomPrimitiveData);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_UnrealComponent_SetCustomPrimitiveData);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_CustomPrimitiveData _Data;

public:
    CK_PROPERTY_GET(_Data);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_UnrealComponent_SetCustomPrimitiveData, _Data);
};
```

`CkUnrealComponent_Utils.h`:

```cpp
    // Writes custom primitive data on the hosted PRIMITIVE component. A request made before the component
    // has been set up is held and applied once it exists. Fails when the hosted component is not a
    // primitive, or when the value's floats do not fit the engine's custom primitive data.
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|UnrealComponent",
              DisplayName = "[Ck][UnrealComponent] Request Set Custom Primitive Data",
              meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_UnrealComponent
    Request_SetCustomPrimitiveData(
        UPARAM(ref) FCk_Handle_UnrealComponent& InUnrealComponent,
        const FCk_Request_UnrealComponent_SetCustomPrimitiveData& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    // The float currently stored at InIndex on the hosted primitive component; 0 when nothing has been
    // written there or the component does not exist yet.
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|UnrealComponent",
              DisplayName = "[Ck][UnrealComponent] Get Custom Primitive Data Float")
    static float
    Get_CustomPrimitiveDataFloat(
        const FCk_Handle_UnrealComponent& InUnrealComponent,
        int32 InIndex);
```

Runtime pieces (names fixed; internal shape mimics CkTimer and the IsmProxy handler):

- `ck::FFragment_UnrealComponent_Requests` in `CkUnrealComponent_Fragment.h`, holding the queued requests;
  friends: the handler processor, the EndPlay processor, the Utils.
- `ck::FProcessor_UnrealComponent_HandleRequests` in `CkUnrealComponent_Processor.h/.cpp`,
  `Group = FGroup_PostTransform`, `RunAfter = TDepList<FProcessor_UnrealComponent_PushTransform>`,
  registered with `CK_REGISTER_PROCESSOR`. Its view excludes `FTag_UnrealComponent_NeedsSetup`, pending-kill
  and entities tagged `ck::FTag_DestroyEntity_Initiate`.
- Pending requests complete `Failed_Cancelled` at EndPlay (`ck::request::FireCancelledForPending`), in the
  module's existing `FProcessor_UnrealComponent_EndPlay` or a sibling EndPlay processor, whichever matches
  how CkTimer does it.
- `CkUnrealComponent.Build.cs`: add `CkGraphics`.

## Behaviour contract

| Situation | Result |
|---|---|
| Invalid handle | ensure; `Failed_NotEnqueued`; returns the handle unchanged |
| `_CustomDataIndex < 0`, or `index + Get_FloatCount() > FCustomPrimitiveData::NumCustomPrimitiveDataFloats` | ensure naming index and count; `Failed_NotEnqueued`; nothing queued |
| Valid, component not set up yet | queued; applied the first tick after setup; `Succeeded` |
| Valid, component set up, is a `UPrimitiveComponent` | written with the engine setter matching the value type (Float / Vector2 / Vector3 / Vector4; LinearColor as Vector4); `Succeeded` |
| Hosted component is not a `UPrimitiveComponent`, or is gone | ensure naming the component class; `Failed`; no write |
| Entity destroyed with requests pending | each completes `Failed_Cancelled` |
| Several requests in one frame | applied in enqueue order; a later write to the same slot wins |

Clear the request storage BEFORE completing/handling, so a re-entrant `Request_*` from a completion delegate survives (root doctrine; CkTimer does this).

## Steps

1. Request struct, fragment, Utils declarations and definitions.
2. Handler processor + EndPlay cancellation + registration.
3. `Get_CustomPrimitiveDataFloat` (reads `UPrimitiveComponent::GetCustomPrimitiveData().Data`; out-of-range index returns 0).
4. Build.cs dependency.
5. AngelScript AutoTests, one scenario per file, in `Plugins/CkTests/Script/CkUnrealComponent/`, following
   `Plugins/CkTests/CLAUDE.md`, `ck-tests-authoring-and-running` and a neighbouring CkTests AutoTest folder
   for shape (isolated band, `DoesNotReplicate`, settle steps, hand-written `A..._Actor` wrapper with
   `Get_ExpectedLogErrors` only where an ensure is deliberate):
   - `CkAutoTest_UnrealComponent_SetCustomPrimitiveData_Applies.as` — add a `UStaticMeshComponent` archetype
     (engine cube), wait for the component, request `FCk_CustomPrimitiveData(4, FCk_CustomPrimitiveData_Value(0.75f))`,
     wait until `Get_CustomPrimitiveDataFloat(Handle, 4)` is 0.75; also a Vector4 at 8 lands in 8..11; the completion delegate reports `Succeeded` exactly once.
   - `..._AppliesAfterSetup.as` — request in the SAME step as `Add`; assert it lands after the component appears.
   - `..._RejectsNonPrimitive.as` — archetype is a plain `USceneComponent`; expect the ensure, `Failed`, and no crash.
   - `..._RejectsOutOfRange.as` — index 36 Float and index 33 Vector4 (and index -1): expect the ensures, `Failed_NotEnqueued`, slot values unchanged.
   - `..._CancelledOnDestroy.as` — request then destroy the owner in the same step; completion is `Failed_Cancelled`.
6. Docs: create `Source/CkUnrealComponent/Claude.md` (purpose, key API including the existing Add/Remove/bake/transform-push
   surface as it is in the header today, the new request and its contract table, anti-patterns: do not write the
   hosted component's render state from game code when a request exists; the component is null until setup).
   In `Source/CLAUDE.md`: add Graphics to the CkUnrealComponent tier row, remove CkUnrealComponent from both
   "no doc yet" mentions, and add a decision-tree row
   "write custom primitive data on an entity-owned component | `CkUnrealComponent` — `Request_SetCustomPrimitiveData`".

## Decision gates

| After | Expect | Otherwise |
|---|---|---|
| Reading CkTimer + the scheduler docs (`Source/CkEcs/Claude.md`) | You know whether a `MarkedDirtyBy` processor re-runs for an entity whose excluded tag (`NeedsSetup`) is removed later | If it does NOT (or you cannot establish it from code), do not use `MarkedDirtyBy` for the handler: iterate the requests fragment's presence and remove the fragment once drained. The `AppliesAfterSetup` test is the proof either way. |
| Step 2 | `FTag_DestroyEntity_Initiate` and `ck::request::FireCancelledForPending` exist as the root `CLAUDE.md` describes | If either is missing or shaped differently: STOP, Blocker with what you found. |
| Step 5 | AS exposes `FCk_CustomPrimitiveData(int32, FCk_CustomPrimitiveData_Value)` and `FCk_CustomPrimitiveData_Value(float32)` constructors (see `CK_ANGELSCRIPT_CTOR_REGISTRATION` in `CkGraphics_Common.h`) | If `FCk_CustomPrimitiveData` has no AS constructor registration, add the matching `CK_ANGELSCRIPT_CTOR_REGISTRATION` to it in `CkGraphics_Common.h` (additive) and say so in your report. |

## Exit criteria

- Steps 1-6 done. The new test files exist and each names its isolated band.
- `PROGRESS.md` updated: files touched, which request-storage shape you used and why, INFERRED items.
- Blueprint `[EDITOR-VERIFY]` checklist for the two new nodes written into `VALIDATION.md` § Blueprint.
- Comment audit done.

## Fences

- Do NOT add a material, MID or "set parameter" API.
- Do NOT change `Add`, `Request_Remove` or the existing immediate requests.
- Do NOT write to the component from the Utils function; the write happens in the processor only.
- Do NOT make the request silently succeed for a non-primitive component.
- Do NOT hand-edit `Script/Generated/*`.
