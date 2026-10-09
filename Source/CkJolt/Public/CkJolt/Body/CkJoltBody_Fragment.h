#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Tag/CkTag.h"
#include "CkEcs/Handle/CkDebugCallstack_Macros.h"
#include "CkEcs/Signal/CkSignal_Macros.h"
#include "CkEcs/Snapshot/CkSnapshot_Posture.h"

#include "CkJolt/Body/CkJoltBody_Fragment_Data.h"

#include "CkResourceLoader/CkResourceLoader_Fragment_Data.h"

#include <variant>

#include <CoreMinimal.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

// --------------------------------------------------------------------------------------------------------------------

class UCk_Utils_JoltBody_UE;
class UCk_Jolt_Subsystem;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class FProcessor_JoltBody_Setup;
    class FProcessor_JoltBody_HandleRequests;
    class FProcessor_JoltBody_SleepStateMirror;
    class FProcessor_JoltBody_KinematicPush;
    class FProcessor_JoltBody_WritebackInterpolated;
    class FProcessor_JoltBody_EndPlay;
    class FProcessor_JoltBody_CancelSetupWaiters;

    // --------------------------------------------------------------------------------------------------------------------

    // Set by FJoltWorld::DoApplyPoseBuffer_GameThread when an active body's StepPose was refreshed this frame;
    // cleared by FProcessor_JoltBody_WritebackInterpolated once the interpolated pose reaches the Transform.
    CK_DEFINE_ECS_TAG(FTag_JoltBody_TransformDirty);

    CK_DEFINE_ECS_TAG(FTag_JoltBody_NeedsSetup);
    // Present while a StaticMeshAsset body's mesh preload batch is still loading (Setup re-polls,
    // keeping NeedsSetup). Observability only — nothing gates on it.
    CK_DEFINE_ECS_TAG(FTag_JoltBody_PendingAssetLoad);
    CK_DEFINE_ECS_TAG(FTag_JoltBody_MotionType_Static);
    CK_DEFINE_ECS_TAG(FTag_JoltBody_MotionType_Kinematic);
    CK_DEFINE_ECS_TAG(FTag_JoltBody_MotionType_Dynamic);
    CK_DEFINE_ECS_TAG(FTag_JoltBody_KinematicFromECS);
    CK_DEFINE_ECS_TAG(FTag_JoltBody_Sleeping);
    CK_DEFINE_ECS_TAG(FTag_JoltBody_PersistContacts);

    // --------------------------------------------------------------------------------------------------------------------

    using FFragment_JoltBody_Params = FCk_JoltBody_Spec;

    // --------------------------------------------------------------------------------------------------------------------

    // Post-step simulated pose (UE-space); Prev/Curr let FProcessor_JoltBody_WritebackInterpolated blend by the step alpha.
    struct CKJOLT_API FFragment_JoltBody_StepPose
    {
    public:
        CK_GENERATED_BODY(FFragment_JoltBody_StepPose);

    private:
        FVector _PrevLocation = FVector::ZeroVector;
        FQuat   _PrevRotation = FQuat::Identity;
        FVector _CurrLocation = FVector::ZeroVector;
        FQuat   _CurrRotation = FQuat::Identity;

    public:
        CK_PROPERTY(_PrevLocation);
        CK_PROPERTY(_PrevRotation);
        CK_PROPERTY(_CurrLocation);
        CK_PROPERTY(_CurrRotation);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_JoltBody_StepPose, _PrevLocation, _PrevRotation, _CurrLocation, _CurrRotation);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKJOLT_API FFragment_JoltBody
    {
    public:
        CK_GENERATED_BODY(FFragment_JoltBody);

    public:
        friend class FProcessor_JoltBody_Setup;
        friend class FProcessor_JoltBody_HandleRequests;
        friend class FProcessor_JoltBody_SleepStateMirror;
        friend class FProcessor_JoltBody_KinematicPush;
        friend class FProcessor_JoltBody_WritebackInterpolated;
        friend class FProcessor_JoltBody_EndPlay;
        friend class FProcessor_JoltBody_CancelSetupWaiters;
        friend class ::UCk_Utils_JoltBody_UE;
        friend class ::UCk_Jolt_Subsystem;

    private:
        JPH::BodyID          _BodyId;
        JPH::Ref<JPH::Shape> _Shape;
        bool                 _BodyAdded = false;

        // Roots the StaticMeshAsset mesh from Setup's kick until the entity dies. The cooked Jolt
        // shape owns its own geometry, but the batch is the only thing keeping the soft-authored
        // mesh resident across the load window.
        FCk_ResourceLoader_RootedAssetBatch _MeshPreloadBatch;

        ECk_JoltBody_SetupState _SetupState = ECk_JoltBody_SetupState::Pending;
        ECk_JoltBody_SetupFailure _SetupFailure = ECk_JoltBody_SetupFailure::None;
        FString _SetupDiagnostic;

        // Drained exactly once, by whichever path makes _SetupState terminal.
        TArray<FCk_Delegate_JoltBody_OnSetupResolved> _SetupWaiters;

    public:
        CK_PROPERTY_GET(_BodyId);
        CK_PROPERTY_GET(_BodyAdded);
        CK_PROPERTY_GET(_SetupState);
        CK_PROPERTY_GET(_SetupFailure);
        CK_PROPERTY_GET(_SetupDiagnostic);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // The RuntimeConvex point cloud, owned until Setup consumes it; the retained Params never hold it.
    struct CKJOLT_API FFragment_JoltBody_RuntimeConvexInput : FCk_Snapshot_Session
    {
    public:
        CK_GENERATED_BODY(FFragment_JoltBody_RuntimeConvexInput);

    public:
        friend class FProcessor_JoltBody_Setup;
        friend class ::UCk_Utils_JoltBody_UE;

    private:
        TArray<FVector> _PointsCm;

    public:
        CK_PROPERTY_GET(_PointsCm);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_JoltBody_RuntimeConvexInput, _PointsCm);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKJOLT_API FFragment_JoltBody_Requests
    {
    public:
        CK_GENERATED_BODY(FFragment_JoltBody_Requests);

    public:
        friend class FProcessor_JoltBody_HandleRequests;
        friend class ::UCk_Utils_JoltBody_UE;

    public:
        using RequestType = std::variant<
            FCk_Request_JoltBody_SetSleepState,
            FCk_Request_JoltBody_AddForce,
            FCk_Request_JoltBody_AddForceAtLocation,
            FCk_Request_JoltBody_AddTorque,
            FCk_Request_JoltBody_AddImpulse,
            FCk_Request_JoltBody_AddImpulseAtLocation,
            FCk_Request_JoltBody_AddAngularImpulse,
            FCk_Request_JoltBody_SetLinearVelocity,
            FCk_Request_JoltBody_SetAngularVelocity,
            FCk_Request_JoltBody_Teleport,
            FCk_Request_JoltBody_SetMotionType,
            FCk_Request_JoltBody_SetCollisionProfile>;
        using RequestList = TArray<RequestType>;

    private:
        RequestList _Requests;

    public:
        CK_PROPERTY_GET(_Requests);
    };

    // --------------------------------------------------------------------------------------------------------------------

    CK_ECS_DEFINE_CALLSTACK_FRAGMENT_FOR(FFragment_JoltBody_Requests);

    // --------------------------------------------------------------------------------------------------------------------

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
        CKJOLT_API,
        OnJoltBodyContactAdded,
        FCk_Delegate_JoltBody_OnContact,
        FCk_Handle_JoltBody,
        FCk_JoltBody_Payload_OnContact);

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
        CKJOLT_API,
        OnJoltBodyContactPersisted,
        FCk_Delegate_JoltBody_OnContact,
        FCk_Handle_JoltBody,
        FCk_JoltBody_Payload_OnContact);

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
        CKJOLT_API,
        OnJoltBodyContactRemoved,
        FCk_Delegate_JoltBody_OnContactRemoved,
        FCk_Handle_JoltBody,
        FCk_JoltBody_Payload_OnContactRemoved);

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
        CKJOLT_API,
        OnJoltBodySleepStateChanged,
        FCk_Delegate_JoltBody_OnSleepStateChanged,
        FCk_Handle_JoltBody,
        ECk_Jolt_SleepState);
}

// --------------------------------------------------------------------------------------------------------------------
