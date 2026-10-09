#include "CkJoltBody_Utils.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Handle/CkDebugCallstack_Macros.h"
#include "CkEcs/TransientEntity/CkTransientEntity_Utils.h"

#include "CkEcsExt/PhysicsOwnership/CkPhysicsOwnership_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkJolt/Body/CkJoltBody_Fragment.h"
#include "CkJolt/CkJolt_Utils.h"
#include "CkJolt/Subsystem/CkJolt_Subsystem.h"

#include <Engine/World.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_JoltBody_UE::
    Add(
        FCk_Handle& InHandle,
        const FCk_JoltBody_Spec& InParams)
    -> FCk_Handle_JoltBody
{
    CK_ENSURE_IF_NOT(ck::IsValid(InHandle),
        TEXT("Invalid Handle passed to JoltBody Add"))
    { return {}; }

    // Writeback + kinematic push both operate on the entity's own Transform.
    CK_ENSURE_IF_NOT(UCk_Utils_Transform_UE::Has(InHandle),
        TEXT("Cannot Add a JoltBody to Entity [{}] because it does NOT have the Transform feature."), InHandle)
    { return {}; }

    // Chaos XOR Jolt per entity. A failed claim already ensured inside TryClaim_Jolt.
    if (NOT ck::physics_ownership::TryClaim_Jolt(InHandle))
    { return {}; }

    // Params never retain the point cloud: it moves into the setup-input fragment, which Setup drops once read.
    const auto IsRuntimeConvex = InParams.Get_ShapeSource() == ECk_JoltBody_ShapeSource::RuntimeConvex;
    const auto IsOversized = IsRuntimeConvex &&
        InParams.Get_RuntimeConvex().Get_PointsCm().Num() > ck::jolt_body::MaxRuntimeConvexPoints;

    auto StoredParams = InParams;
    auto RuntimeConvexPoints = MoveTemp(StoredParams._RuntimeConvex._PointsCm);

    InHandle.Add<ck::FFragment_JoltBody_Params>(MoveTemp(StoredParams));
    auto& JoltBody = InHandle.Add<ck::FFragment_JoltBody>();
    InHandle.Add<ck::FFragment_JoltBody_StepPose>();

    // Jolt never fires OnBodyDeactivated for a never-activated body, so a body composed Asleep needs the tag
    // stamped here or Get_SleepState reports Awake until its first activate-then-sleep cycle.
    if (InParams.Get_InitialSleepState() == ECk_Jolt_SleepState::Asleep)
    { InHandle.Add<ck::FTag_JoltBody_Sleeping>(); }

    switch (InParams.Get_MotionType())
    {
        case ECk_MotionType::Static:
        {
            InHandle.Add<ck::FTag_JoltBody_MotionType_Static>();
            break;
        }
        case ECk_MotionType::Kinematic:
        {
            InHandle.Add<ck::FTag_JoltBody_MotionType_Kinematic>();
            InHandle.Add<ck::FTag_JoltBody_KinematicFromECS>();
            break;
        }
        case ECk_MotionType::Dynamic:
        {
            InHandle.Add<ck::FTag_JoltBody_MotionType_Dynamic>();
            break;
        }
    }

    if (InParams.Get_PersistContacts() == ECk_EnableDisable::Enable)
    {
        InHandle.Add<ck::FTag_JoltBody_PersistContacts>();
    }

    // Admission comes last so a rejected RuntimeConvex body is still fully composed: it stays observable as Failed
    // and answers a late setup waiter immediately.
    if (IsRuntimeConvex)
    {
        const auto PointCountIsAdmissible = NOT IsOversized;
        CK_ENSURE_IF_NOT(PointCountIsAdmissible,
            TEXT("RuntimeConvex input exceeds {} points on Entity [{}]"), ck::jolt_body::MaxRuntimeConvexPoints, InHandle)
        {
            JoltBody._SetupState = ECk_JoltBody_SetupState::Failed;
            JoltBody._SetupFailure = ECk_JoltBody_SetupFailure::InvalidInput;
            JoltBody._SetupDiagnostic = ck::Format_UE(
                TEXT("RuntimeConvex input exceeds {} points"), ck::jolt_body::MaxRuntimeConvexPoints);
            return Cast(InHandle);
        }

        const auto IsConvexSpecValid = InParams.Get_RuntimeConvex().Get_IsValid();
        CK_ENSURE_IF_NOT(IsConvexSpecValid,
            TEXT("RuntimeConvex spec rejected on Entity [{}]: needs {} to {} finite points within {} cm, a positive hull tolerance and a non-negative convex radius"),
            InHandle, ck::jolt_body::MinRuntimeConvexPoints, ck::jolt_body::MaxRuntimeConvexPoints,
            ck::jolt_body::MaxRuntimeConvexPointMagnitudeCm)
        {
            JoltBody._SetupState = ECk_JoltBody_SetupState::Failed;
            JoltBody._SetupFailure = ECk_JoltBody_SetupFailure::InvalidInput;
            JoltBody._SetupDiagnostic = TEXT("RuntimeConvex spec has too few or non-finite points or invalid hull settings");
            return Cast(InHandle);
        }

        auto* World = UCk_Utils_TransientEntity_UE::Get_World(InHandle);
        auto* JoltSubsystem = ck::IsValid(World) ? World->GetSubsystem<UCk_Jolt_Subsystem>() : nullptr;
        const auto JoltWorldIsOpen = ck::IsValid(World) && NOT World->IsBeingCleanedUp() &&
            ck::IsValid(JoltSubsystem) && NOT JoltSubsystem->Get_IsClosing();
        CK_ENSURE_IF_NOT(JoltWorldIsOpen,
            TEXT("RuntimeConvex Jolt world unavailable or closing on Entity [{}]"), InHandle)
        {
            JoltBody._SetupState = ECk_JoltBody_SetupState::Failed;
            JoltBody._SetupFailure = ECk_JoltBody_SetupFailure::UnsupportedWorld;
            JoltBody._SetupDiagnostic = TEXT("Jolt world unavailable or closing");
            return Cast(InHandle);
        }

        InHandle.Add<ck::FFragment_JoltBody_RuntimeConvexInput>(MoveTemp(RuntimeConvexPoints));
    }

    InHandle.Add<ck::FTag_JoltBody_NeedsSetup>();

    return Cast(InHandle);
}

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_JoltBody_UE, FCk_Handle_JoltBody, ck::FFragment_JoltBody, ck::FFragment_JoltBody_Params);

auto
    UCk_Utils_JoltBody_UE::
    Get_SetupState(
        const FCk_Handle_JoltBody& InJoltBody)
    -> ECk_JoltBody_SetupState
{
    const auto HandleIsValid = ck::IsValid(InJoltBody);
    CK_ENSURE_IF_NOT(HandleIsValid, TEXT("Invalid JoltBody Handle [{}] passed to Get_SetupState"), InJoltBody)
    { return ECk_JoltBody_SetupState::Failed; }

    return InJoltBody.Get<ck::FFragment_JoltBody>().Get_SetupState();
}

auto
    UCk_Utils_JoltBody_UE::
    Get_SetupFailure(
        const FCk_Handle_JoltBody& InJoltBody)
    -> ECk_JoltBody_SetupFailure
{
    const auto HandleIsValid = ck::IsValid(InJoltBody);
    CK_ENSURE_IF_NOT(HandleIsValid, TEXT("Invalid JoltBody Handle [{}] passed to Get_SetupFailure"), InJoltBody)
    { return ECk_JoltBody_SetupFailure::InvalidInput; }

    return InJoltBody.Get<ck::FFragment_JoltBody>().Get_SetupFailure();
}

auto
    UCk_Utils_JoltBody_UE::
    Get_SetupDiagnostic(
        const FCk_Handle_JoltBody& InJoltBody)
    -> FString
{
    const auto HandleIsValid = ck::IsValid(InJoltBody);
    CK_ENSURE_IF_NOT(HandleIsValid, TEXT("Invalid JoltBody Handle [{}] passed to Get_SetupDiagnostic"), InJoltBody)
    { return {}; }

    return InJoltBody.Get<ck::FFragment_JoltBody>().Get_SetupDiagnostic();
}

auto
    UCk_Utils_JoltBody_UE::
    TryPromise_OnSetupResolved(
        const FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnSetupResolved& InDelegate)
    -> bool
{
    const auto HandleIsValid = ck::IsValid(InJoltBody);
    CK_ENSURE_IF_NOT(HandleIsValid, TEXT("JoltBody setup promise rejected: invalid JoltBody Handle [{}]"), InJoltBody)
    { return false; }

    const auto DelegateIsBound = InDelegate.IsBound();
    CK_ENSURE_IF_NOT(DelegateIsBound, TEXT("JoltBody setup promise rejected: unbound delegate for [{}]"), InJoltBody)
    { return false; }

    // A waiter accepted while the world is closing could never be delivered: the cancel sweep has already run.
    auto* World = UCk_Utils_TransientEntity_UE::Get_World(InJoltBody);
    auto* JoltSubsystem = ck::IsValid(World) ? World->GetSubsystem<UCk_Jolt_Subsystem>() : nullptr;
    const auto WorldIsOpen = ck::IsValid(World) && NOT World->IsBeingCleanedUp() &&
        (ck::Is_NOT_Valid(JoltSubsystem) || NOT JoltSubsystem->Get_IsClosing());
    CK_ENSURE_IF_NOT(WorldIsOpen, TEXT("JoltBody setup promise rejected during world cleanup for [{}]"), InJoltBody)
    { return false; }

    auto MutableJoltBody = InJoltBody;
    auto& JoltBody = MutableJoltBody.Get<ck::FFragment_JoltBody>();
    const auto SetupIsPending = JoltBody._SetupState == ECk_JoltBody_SetupState::Pending;

    // A pending body past BeginDestroy is about to be cancelled; a terminal one still answers immediately.
    const auto CanStillResolve = NOT SetupIsPending ||
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InJoltBody, ECk_EntityLifetime_DestructionPhase::BeginDestroy);
    CK_ENSURE_IF_NOT(CanStillResolve, TEXT("JoltBody setup promise rejected after BeginDestroy for [{}]"), InJoltBody)
    { return false; }

    if (NOT SetupIsPending)
    {
        // Copied first: the callback may rebind the caller's delegate or destroy the entity that owns these values.
        const auto State = JoltBody._SetupState;
        const auto Failure = JoltBody._SetupFailure;
        const auto Delegate = InDelegate;
        Delegate.ExecuteIfBound(InJoltBody, State, Failure);
        return true;
    }

    const auto HasJoltSubsystem = ck::IsValid(JoltSubsystem);
    CK_ENSURE_IF_NOT(HasJoltSubsystem, TEXT("JoltBody setup promise rejected: no Jolt subsystem for [{}]"), InJoltBody)
    { return false; }

    const auto HasWaiterCapacity = JoltBody._SetupWaiters.Num() < ck::jolt_body::MaxSetupWaiters;
    CK_ENSURE_IF_NOT(HasWaiterCapacity,
        TEXT("JoltBody setup waiter limit reached ({}) for [{}]"), ck::jolt_body::MaxSetupWaiters, InJoltBody)
    { return false; }

    JoltBody._SetupWaiters.Emplace(InDelegate);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_JoltBody_UE::
    Get_MotionType(
        const FCk_Handle_JoltBody& InJoltBody)
    -> ECk_MotionType
{
    return InJoltBody.Get<ck::FFragment_JoltBody_Params>().Get_MotionType();
}

auto
    UCk_Utils_JoltBody_UE::
    Get_SleepState(
        const FCk_Handle_JoltBody& InJoltBody)
    -> ECk_Jolt_SleepState
{
    return InJoltBody.Has<ck::FTag_JoltBody_Sleeping>()
        ? ECk_Jolt_SleepState::Asleep
        : ECk_Jolt_SleepState::Awake;
}

auto
    UCk_Utils_JoltBody_UE::
    Get_IsBodyAdded(
        const FCk_Handle_JoltBody& InJoltBody)
    -> bool
{
    return InJoltBody.Get<ck::FFragment_JoltBody>().Get_BodyAdded();
}

auto
    UCk_Utils_JoltBody_UE::
    Get_LinearVelocity(
        const FCk_Handle_JoltBody& InJoltBody)
    -> FVector
{
    CK_ENSURE_IF_NOT(ck::IsValid(InJoltBody),
        TEXT("Invalid JoltBody Handle passed to Get_LinearVelocity"))
    { return FVector::ZeroVector; }

    // Not-yet-added is a legitimate transient state (setup is deferred) — report zero, never ensure.
    const auto& Current = InJoltBody.Get<ck::FFragment_JoltBody>();
    if (NOT Current.Get_BodyAdded())
    { return FVector::ZeroVector; }

    const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InJoltBody);
    if (ck::Is_NOT_Valid(World))
    { return FVector::ZeroVector; }

    const auto JoltSubsystem = World->GetSubsystem<UCk_Jolt_Subsystem>();
    if (ck::Is_NOT_Valid(JoltSubsystem))
    { return FVector::ZeroVector; }

    const auto PhysicsSystem = JoltSubsystem->Get_PhysicsSystem().Pin();
    if (ck::Is_NOT_Valid(PhysicsSystem))
    { return FVector::ZeroVector; }

    return ck::jolt::Conv(PhysicsSystem->GetBodyInterface().GetLinearVelocity(Current.Get_BodyId()));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_JoltBody_UE::
    Request_SetSleepState(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_SetSleepState& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_AddForce(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_AddForce& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_AddForceAtLocation(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_AddForceAtLocation& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_AddTorque(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_AddTorque& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_AddImpulse(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_AddImpulse& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_AddImpulseAtLocation(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_AddImpulseAtLocation& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_AddAngularImpulse(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_AddAngularImpulse& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_SetLinearVelocity(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_SetLinearVelocity& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_SetAngularVelocity(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_SetAngularVelocity& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_Teleport(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_Teleport& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_SetMotionType(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_SetMotionType& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    Request_SetCollisionProfile(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Request_JoltBody_SetCollisionProfile& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_CALLSTACK_RECORD(ck::FFragment_JoltBody_Requests, InJoltBody);

    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InJoltBody.AddOrGet<ck::FFragment_JoltBody_Requests>()._Requests.Emplace(InRequest);

    return InJoltBody;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_JoltBody_UE::
    BindTo_OnJoltBodyContactAdded(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnContact& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_JoltBody
{
    CK_SIGNAL_BIND(ck::UUtils_Signal_OnJoltBodyContactAdded, InJoltBody, InDelegate, InBindingPolicy, InPostFireBehavior);
    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    UnbindFrom_OnJoltBodyContactAdded(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnContact& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_SIGNAL_UNBIND(ck::UUtils_Signal_OnJoltBodyContactAdded, InJoltBody, InDelegate);
    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    BindTo_OnJoltBodyContactPersisted(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnContact& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_JoltBody
{
    CK_SIGNAL_BIND(ck::UUtils_Signal_OnJoltBodyContactPersisted, InJoltBody, InDelegate, InBindingPolicy, InPostFireBehavior);
    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    UnbindFrom_OnJoltBodyContactPersisted(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnContact& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_SIGNAL_UNBIND(ck::UUtils_Signal_OnJoltBodyContactPersisted, InJoltBody, InDelegate);
    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    BindTo_OnJoltBodyContactRemoved(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnContactRemoved& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_JoltBody
{
    CK_SIGNAL_BIND(ck::UUtils_Signal_OnJoltBodyContactRemoved, InJoltBody, InDelegate, InBindingPolicy, InPostFireBehavior);
    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    UnbindFrom_OnJoltBodyContactRemoved(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnContactRemoved& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_SIGNAL_UNBIND(ck::UUtils_Signal_OnJoltBodyContactRemoved, InJoltBody, InDelegate);
    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    BindTo_OnJoltBodySleepStateChanged(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnSleepStateChanged& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> FCk_Handle_JoltBody
{
    CK_SIGNAL_BIND(ck::UUtils_Signal_OnJoltBodySleepStateChanged, InJoltBody, InDelegate, InBindingPolicy, InPostFireBehavior);
    return InJoltBody;
}

auto
    UCk_Utils_JoltBody_UE::
    UnbindFrom_OnJoltBodySleepStateChanged(
        FCk_Handle_JoltBody& InJoltBody,
        const FCk_Delegate_JoltBody_OnSleepStateChanged& InDelegate)
    -> FCk_Handle_JoltBody
{
    CK_SIGNAL_UNBIND(ck::UUtils_Signal_OnJoltBodySleepStateChanged, InJoltBody, InDelegate);
    return InJoltBody;
}

// --------------------------------------------------------------------------------------------------------------------
