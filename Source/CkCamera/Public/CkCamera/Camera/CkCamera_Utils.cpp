#include "CkCamera/Camera/CkCamera_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkCamera/CkCamera_Log.h"
#include "CkCamera/Camera/CkCamera_Component.h"
#include "CkCamera_Context_Private.h"
#include "CkCamera_Admission_Private.h"
#include "CkCamera/Camera/CkCamera_GameplayTags.h"
#include "CkCamera/Camera/CameraLayer/CkCameraLayer_Fragment.h"
#include "CkCamera/Camera/CameraLayer/CkCameraLayer_Utils.h"
#include "CkCamera/Camera/CameraLayer/EntityScripts/CkCameraLayer_EntityScript.h"

#include "CkCore/Game/CkGame_Utils.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Snapshot/CkSnapshot_RestoreMarker.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

#include "CkAttribute/FloatAttribute/CkFloatAttribute_Utils.h"
#include "CkAttribute/VectorAttribute/CkVectorAttribute_Utils.h"
#include "CkAttribute/RotatorAttribute/CkRotatorAttribute_Utils.h"
#include "CkAttribute/IntegerAttribute/CkIntegerAttribute_Utils.h"

#include <Camera/PlayerCameraManager.h>
#include <GameFramework/Actor.h>
#include <GameFramework/PlayerController.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Camera_UE::
    Add(
        FCk_Handle_Transform& InHandle,
        const FCk_Camera_Spec& InParams)
    -> FCk_Handle_Camera
{
    const auto DirectorIsValid = ck::IsValid(InHandle);
    CK_ENSURE_IF_NOT(DirectorIsValid, TEXT("Camera Add requires a live transform entity")) {}
    if (NOT DirectorIsValid) { return {}; }

    const auto HasTransform = InHandle.Has<ck::FFragment_Transform>();
    CK_ENSURE_IF_NOT(HasTransform, TEXT("Camera Add requires a transform on entity [{}]"), InHandle) {}
    if (NOT HasTransform) { return {}; }
    const auto& InitialTransform = InHandle.Get<ck::FFragment_Transform>().Get_Transform();
    const auto TransformIsValid = NOT InitialTransform.ContainsNaN() &&
        InitialTransform.IsValid() && InitialTransform.GetRotation().IsNormalized();
    CK_ENSURE_IF_NOT(TransformIsValid, TEXT("Camera Add requires a finite normalized transform on [{}]"), InHandle) {}
    if (NOT TransformIsValid) { return {}; }

    auto Context = ck::camera::FCameraContext{};
    const auto ContextIsValid = ck::camera::TryResolveCameraContext(InHandle, Context);
    CK_ENSURE_IF_NOT(ContextIsValid, TEXT("Camera Add requires a live exact world/owner context on [{}]"), InHandle) {}
    if (NOT ContextIsValid) { return {}; }

    auto* const OutputComponent = InParams.Get_OutputComponent().Get();
    const auto OutputIsLive = ck::IsValid(OutputComponent) &&
        NOT OutputComponent->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
    CK_ENSURE_IF_NOT(OutputIsLive, TEXT("Camera Add requires a live output component")) {}
    if (NOT OutputIsLive) { return {}; }
    auto* const OutputOwner = OutputComponent->GetOwner();
    const auto OutputOwnerIsLive = OutputOwner == nullptr ||
        (ck::IsValid(OutputOwner) &&
         NOT OutputOwner->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) &&
         NOT OutputOwner->IsActorBeingDestroyed());
    CK_ENSURE_IF_NOT(OutputOwnerIsLive, TEXT("Camera Add output owner is retiring")) {}
    if (NOT OutputOwnerIsLive) { return {}; }
    const auto OutputWorldMatches = OutputComponent->GetWorld() == Context.World &&
        (OutputOwner == nullptr || OutputOwner->GetWorld() == Context.World);
    CK_ENSURE_IF_NOT(OutputWorldMatches,
        TEXT("Camera Add output component is outside the director's exact world")) {}
    if (NOT OutputWorldMatches) { return {}; }

    // A failed admission may close only the composition begun by this call.
    // Snapshot tuner children may already exist; an active director, queued request,
    // or old layer record may not be disturbed by that rollback.
    const auto FreshComposition =
        NOT InHandle.Has<ck::FTag_Camera_AdmissionClosed>() &&
        NOT InHandle.Has<ck::FFragment_Camera_Params>() &&
        NOT InHandle.Has<ck::FFragment_Camera_Requests>() &&
        NOT InHandle.Has<ck::FFragment_Camera>() &&
        NOT InHandle.Has<ck::FFragment_Camera_Pov>() &&
        NOT InHandle.Has<ck::FFragment_Camera_Rig>() &&
        NOT InHandle.Has<ck::FFragment_Camera_Springs>() &&
        NOT InHandle.Has<ck::FFragment_Camera_Sensor>() &&
        NOT InHandle.Has<ck::FFragment_Camera_Noise>() &&
        NOT InHandle.Has<ck::FFragment_Camera_OrientationControl>() &&
        NOT InHandle.Has<ck::FFragment_Camera_AutoReorient>() &&
        NOT InHandle.Has<ck::FFragment_Camera_Collision>() &&
        NOT InHandle.Has<ck::FFragment_Camera_DepthOfField>() &&
        NOT InHandle.Has<ck::FFragment_RecordOfCameraLayers>();
    CK_ENSURE_IF_NOT(FreshComposition,
        TEXT("Camera Add requires a fresh unclosed director on [{}]"), InHandle) {}
    if (NOT FreshComposition) { return {}; }

    auto DefaultLayer = FCk_Handle_CameraLayer{};
    auto ViewAnchor = FCk_Handle_Transform{};
    const auto CloseFailedComposition = [&]()
    {
        // This never destroys the caller-owned transform or adopted tuner children.
        // A returned child is retired only while it is still our exact direct child.
        const auto RetireOwned = [&](const FCk_Handle& Child)
        {
            if (ck::IsValid(Child) && Child.Has<ck::FFragment_LifetimeOwner>() &&
                Child.Get_RegistryView().Get_RegistryHandle() ==
                    InHandle.Get_RegistryView().Get_RegistryHandle() &&
                Child.Get<ck::FFragment_LifetimeOwner>().Get_Entity() == InHandle)
            {
                auto MutableChild = Child;
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(MutableChild);
            }
        };
        if (NOT ck::IsValid(InHandle)) { return; }
        InHandle.AddOrGet<ck::FTag_Camera_AdmissionClosed>();
        auto PendingRequests = ck::FFragment_Camera_Requests::Requests{};
        if (InHandle.Has<ck::FFragment_Camera_Requests>())
        { PendingRequests = InHandle.Get<ck::FFragment_Camera_Requests>().Get_Requests(); }
        InHandle.Try_Remove<ck::FFragment_Camera_Requests>();
        InHandle.Try_Remove<ck::FFragment_Camera_Pov>();
        InHandle.Try_Remove<ck::FFragment_Camera>();
        InHandle.Try_Remove<ck::FFragment_Camera_Params>();
        InHandle.Try_Remove<ck::FFragment_Camera_Rig>();
        InHandle.Try_Remove<ck::FFragment_Camera_Springs>();
        InHandle.Try_Remove<ck::FFragment_Camera_Sensor>();
        InHandle.Try_Remove<ck::FFragment_Camera_Noise>();
        InHandle.Try_Remove<ck::FFragment_Camera_OrientationControl>();
        InHandle.Try_Remove<ck::FFragment_Camera_AutoReorient>();
        InHandle.Try_Remove<ck::FFragment_Camera_Collision>();
        InHandle.Try_Remove<ck::FFragment_Camera_DepthOfField>();
        InHandle.Try_Remove<ck::FFragment_RecordOfCameraLayers>();
        // Reentrant requests were not present at admission, but if any arrived
        // during composition they must complete as cancelled, not disappear.
        ck::request::FireCancelledForPending(InHandle, PendingRequests);
        if (NOT ck::IsValid(InHandle)) { return; }
        RetireOwned(DefaultLayer);
        if (ck::IsValid(InHandle)) { RetireOwned(ViewAnchor); }
    };

    InHandle.Add<ck::FFragment_Camera_Params>(InParams.Get_DriveControllerControlRotation());
    InHandle.AddOrGet<ck::FFragment_Camera>();
    InHandle.AddOrGet<ck::FFragment_Camera_Pov>();

    ck::FUtils_RecordOfCameraLayers::AddIfMissing(InHandle);

    auto Director = Cast(InHandle);

    const auto AttributesReady = DoMaterializeAttributes(Director, InParams.Get_Profile());
    CK_ENSURE_IF_NOT(AttributesReady,
        TEXT("Camera Add could not materialize the complete profile on [{}]"), InHandle) {}
    if (NOT AttributesReady) { CloseFailedComposition(); return {}; }

    // External construction callbacks can alter the source transform. Never
    // read it after a callback without proving the live current pose again.
    const auto HasLivePose = [&]() -> bool
    {
        if (NOT ck::IsValid(InHandle) || NOT InHandle.Has<ck::FFragment_Transform>())
        { return false; }
        const auto Pose = InHandle.Get<ck::FFragment_Transform>().Get_Transform();
        return NOT Pose.ContainsNaN() && Pose.IsValid() && Pose.GetRotation().IsNormalized();
    };

    // The persistent base layer acquires no modifiers — the resting state IS the tuner base values just materialized.
    DefaultLayer = UCk_Utils_CameraLayer_UE::Create(
        Director, UCk_CameraLayer_Default_EntityScript::StaticClass());
    // Layer construction can invoke script and record callbacks. Recheck the
    // exact director before reading either its record or the returned layer.
    auto AfterLayerContext = ck::camera::FCameraContext{};
    const auto DirectorAfterLayer = ck::IsValid(InHandle) &&
        ck::camera::TryResolveCameraContext(InHandle, AfterLayerContext) &&
        AfterLayerContext.World == Context.World &&
        HasLivePose() &&
        NOT InHandle.Has<ck::FTag_Camera_AdmissionClosed>() &&
        InHandle.Has_All<ck::FFragment_Camera_Params, ck::FFragment_Camera,
            ck::FFragment_Camera_Pov, ck::FFragment_Camera_Rig,
            ck::FFragment_Camera_Springs, ck::FFragment_Camera_Sensor,
            ck::FFragment_Camera_Noise, ck::FFragment_Camera_OrientationControl,
            ck::FFragment_Camera_AutoReorient, ck::FFragment_Camera_Collision,
            ck::FFragment_Camera_DepthOfField, ck::FFragment_RecordOfCameraLayers>();
    const auto DefaultReady = DirectorAfterLayer && ck::IsValid(DefaultLayer) &&
        DefaultLayer.Get_RegistryView().Get_RegistryHandle() ==
            InHandle.Get_RegistryView().Get_RegistryHandle() &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            DefaultLayer, ECk_EntityLifetime_DestructionPhase::BeginDestroy) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            DefaultLayer, ECk_EntityLifetime_DestructionPhase::Teardown) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            DefaultLayer, ECk_EntityLifetime_DestructionPhase::Destroyed) &&
        DefaultLayer.Has_All<ck::FFragment_CameraLayer_Params,
            ck::FFragment_CameraLayer_Blend, ck::FFragment_LifetimeOwner>() &&
        DefaultLayer.Get<ck::FFragment_LifetimeOwner>().Get_Entity() == InHandle &&
        DefaultLayer.Get<ck::FFragment_CameraLayer_Params>().Get_LayerClass() ==
            UCk_CameraLayer_Default_EntityScript::StaticClass() &&
        ck::FUtils_RecordOfCameraLayers::Get_ContainsEntry(Director, DefaultLayer);
    CK_ENSURE_IF_NOT(DefaultReady,
        TEXT("Camera Add could not admit its default layer on [{}]"), InHandle) {}
    if (NOT DefaultReady) { CloseFailedComposition(); return {}; }
    {
        auto& Params = DefaultLayer.Get<ck::FFragment_CameraLayer_Params>();
        Params.Set_IsDefault(true);
        Params.Set_Priority(TNumericLimits<int32>::Lowest());

        auto& Blend = DefaultLayer.Get<ck::FFragment_CameraLayer_Blend>();
        Blend.Set_Alpha(1.0f);
        Blend.Set_TargetAlpha(1.0f);
    }

    // The composed view's attachable presence (FFragment_Camera::_ViewAnchor): a plain child
    // transform, seeded from the director's current pose so a first-frame attach composes from something
    // sane. Every later pose arrives as an ordinary transform request enqueued by the POV processor and
    // drained by the transform-local settle barrier.
    {
        const auto SeedPose = Director.Get<ck::FFragment_Transform>().Get_Transform();

        ViewAnchor = UCk_Utils_Transform_UE::Create(InHandle,
            FTransform{SeedPose.GetRotation(), SeedPose.GetLocation()}, ECk_Replication::DoesNotReplicate);
        // Create broadcasts TransformAdded synchronously in editor builds.
        // Never retain an ECS section reference across that callback.
        auto AfterAnchorContext = ck::camera::FCameraContext{};
        const auto DirectorAfterAnchor = ck::IsValid(InHandle) &&
            ck::camera::TryResolveCameraContext(InHandle, AfterAnchorContext) &&
            AfterAnchorContext.World == Context.World &&
            HasLivePose() &&
            NOT InHandle.Has<ck::FTag_Camera_AdmissionClosed>() &&
            InHandle.Has_All<ck::FFragment_Camera_Params, ck::FFragment_Camera,
                ck::FFragment_Camera_Pov, ck::FFragment_Camera_Rig,
                ck::FFragment_Camera_Springs, ck::FFragment_Camera_Sensor,
                ck::FFragment_Camera_Noise, ck::FFragment_Camera_OrientationControl,
                ck::FFragment_Camera_AutoReorient, ck::FFragment_Camera_Collision,
                ck::FFragment_Camera_DepthOfField, ck::FFragment_RecordOfCameraLayers>();
        CK_ENSURE_IF_NOT(DirectorAfterAnchor,
            TEXT("Camera Add director changed during view-anchor creation")) {}
        if (NOT DirectorAfterAnchor) { CloseFailedComposition(); return {}; }
        Director.Get<ck::FFragment_Camera>()._ViewAnchor = ViewAnchor;
    }
    const auto ViewAnchorReady = ck::IsValid(ViewAnchor) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            ViewAnchor, ECk_EntityLifetime_DestructionPhase::BeginDestroy) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            ViewAnchor, ECk_EntityLifetime_DestructionPhase::Teardown) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            ViewAnchor, ECk_EntityLifetime_DestructionPhase::Destroyed) &&
        ViewAnchor.Has<ck::FFragment_Transform>() &&
        ViewAnchor.Has<ck::FFragment_LifetimeOwner>() &&
        ViewAnchor.Get_RegistryView().Get_RegistryHandle() ==
            InHandle.Get_RegistryView().Get_RegistryHandle() &&
        ViewAnchor.Get<ck::FFragment_LifetimeOwner>().Get_Entity() == InHandle;
    CK_ENSURE_IF_NOT(ViewAnchorReady,
        TEXT("Camera Add could not create its direct view anchor on [{}]"), InHandle) {}
    if (NOT ViewAnchorReady) { CloseFailedComposition(); return {}; }

    // The FIRST GetCameraView can land before FProcessor_Camera_UpdatePOV ever ticks; without this seed it would
    // return a default (origin) FMinimalViewInfo and the camera would snap on the first frame after Add. The
    // transform is guaranteed — Add requires a transform handle.
    {
        auto& Current = Director.Get<ck::FFragment_Camera>();
        auto& Pov = Director.Get<ck::FFragment_Camera_Pov>();

        auto Input = ck::camera::FPov_Input{};
        Input._AnchorTransform  = Director.Get<ck::FFragment_Transform>().Get_Transform();
        Input._World            = Context.World;
        Input._TraceIgnoreActor = Context.OwningActor;

        ck::camera::FPov::Run(Current.Get_ComposedProfile(), Input, Pov._PovState);

        auto ViewInfo = FMinimalViewInfo{};
        ViewInfo.Location   = Pov._PovState._CameraTransform.GetLocation();
        ViewInfo.Rotation   = Pov._PovState._CameraTransform.Rotator();
        ViewInfo.FOV        = Current.Get_ComposedProfile().Get_Sensor().Get_FOV();
        ViewInfo.DesiredFOV = ViewInfo.FOV;
        Pov._ViewInfo   = ViewInfo;
    }

    // Construction may have called external code. Prove the same exact world,
    // retained children, and live sink again before the one publication write.
    auto FinalContext = ck::camera::FCameraContext{};
    const auto FinalDirectorReady = ck::IsValid(InHandle) &&
        ck::camera::TryResolveCameraContext(InHandle, FinalContext) &&
        FinalContext.World == Context.World &&
        HasLivePose() &&
        InHandle.Has_All<ck::FFragment_Camera_Params, ck::FFragment_Camera,
            ck::FFragment_Camera_Pov, ck::FFragment_Camera_Rig,
            ck::FFragment_Camera_Springs, ck::FFragment_Camera_Sensor,
            ck::FFragment_Camera_Noise, ck::FFragment_Camera_OrientationControl,
            ck::FFragment_Camera_AutoReorient, ck::FFragment_Camera_Collision,
            ck::FFragment_Camera_DepthOfField, ck::FFragment_RecordOfCameraLayers>() &&
        NOT InHandle.Has<ck::FTag_Camera_AdmissionClosed>();
    const auto FinalOutputLive = ck::IsValid(OutputComponent) &&
        NOT OutputComponent->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
    auto* const FinalOutputOwner = FinalOutputLive ? OutputComponent->GetOwner() : nullptr;
    const auto FinalOwnerLive = FinalOutputLive &&
        (FinalOutputOwner == nullptr ||
            (ck::IsValid(FinalOutputOwner) &&
             NOT FinalOutputOwner->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) &&
             NOT FinalOutputOwner->IsActorBeingDestroyed()));
    const auto FinalReady = FinalDirectorReady &&
        ck::IsValid(DefaultLayer) &&
        DefaultLayer.Get_RegistryView().Get_RegistryHandle() ==
            InHandle.Get_RegistryView().Get_RegistryHandle() &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            DefaultLayer, ECk_EntityLifetime_DestructionPhase::BeginDestroy) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            DefaultLayer, ECk_EntityLifetime_DestructionPhase::Teardown) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            DefaultLayer, ECk_EntityLifetime_DestructionPhase::Destroyed) &&
        DefaultLayer.Has_All<ck::FFragment_CameraLayer_Params,
            ck::FFragment_CameraLayer_Blend, ck::FFragment_LifetimeOwner>() &&
        DefaultLayer.Get<ck::FFragment_LifetimeOwner>().Get_Entity() == InHandle &&
        DefaultLayer.Get<ck::FFragment_CameraLayer_Params>().Get_LayerClass() ==
            UCk_CameraLayer_Default_EntityScript::StaticClass() &&
        ck::FUtils_RecordOfCameraLayers::Get_ContainsEntry(Director, DefaultLayer) &&
        ck::IsValid(ViewAnchor) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            ViewAnchor, ECk_EntityLifetime_DestructionPhase::BeginDestroy) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            ViewAnchor, ECk_EntityLifetime_DestructionPhase::Teardown) &&
        NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            ViewAnchor, ECk_EntityLifetime_DestructionPhase::Destroyed) &&
        ViewAnchor.Get_RegistryView().Get_RegistryHandle() ==
            InHandle.Get_RegistryView().Get_RegistryHandle() &&
        ViewAnchor.Has_All<ck::FFragment_Transform, ck::FFragment_LifetimeOwner>() &&
        ViewAnchor.Get<ck::FFragment_LifetimeOwner>().Get_Entity() == InHandle &&
        InHandle.Get<ck::FFragment_Camera>()._ViewAnchor == ViewAnchor &&
        FinalOwnerLive && OutputComponent->GetWorld() == Context.World &&
        (FinalOutputOwner == nullptr || FinalOutputOwner->GetWorld() == Context.World);
    CK_ENSURE_IF_NOT(FinalReady,
        TEXT("Camera Add lost its exact composition before output publication")) {}
    if (NOT FinalReady) { CloseFailedComposition(); return {}; }

    // The supplied component is the sole external publication point. It is
    // never rebound on any synchronous failure above.
    OutputComponent->Set_DirectorEntity(Director);
    return Director;
}


// --------------------------------------------------------------------------------------------------------------------

namespace camera_admission_detail
{
    // Private admission preflight shared by queued and immediate Camera entry points.
    // No rejected handle is dereferenced: each term is ordered by its prerequisite.
    template <typename T_Feature>
    auto CanUseCameraFeature(const FCk_Handle_Camera& InCamera) -> bool
    {
        return ck::IsValid(InCamera) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
                InCamera, ECk_EntityLifetime_DestructionPhase::BeginDestroy) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
                InCamera, ECk_EntityLifetime_DestructionPhase::Teardown) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
                InCamera, ECk_EntityLifetime_DestructionPhase::Destroyed) &&
            InCamera.Has_All<ck::FFragment_Camera_Params, T_Feature>() &&
            NOT InCamera.Has<ck::FTag_Camera_AdmissionClosed>();
    }


    // Get_Profile reads final values from every materialized tuner. Validate
    // all required sections and typed receipts before its first final-value Get.
    static auto CanReadProfile(const FCk_Handle_Camera& InCamera) -> bool
    {
        if (NOT CanUseCameraFeature<ck::FFragment_Camera>(InCamera)) { return false; }
        const auto HasSections =
            InCamera.Has_All<ck::FFragment_Camera_Rig,
            ck::FFragment_Camera_Springs, ck::FFragment_Camera_Sensor,
            ck::FFragment_Camera_Noise, ck::FFragment_Camera_OrientationControl,
            ck::FFragment_Camera_AutoReorient, ck::FFragment_Camera_Collision,
            ck::FFragment_Camera_DepthOfField>();
        if (NOT HasSections) { return false; }
        const auto ReadyRotation = [](const ck::FCameraAttr_Rotation& R)
        {
            return ck::IsValid(R._Speed) && ck::IsValid(R._Limits) &&
                ck::IsValid(R._NormalSpeedRange) && ck::IsValid(R._OutOfRangeMultiplier);
        };
        const auto& Rig = InCamera.Get<ck::FFragment_Camera_Rig>();
        const auto& Springs = InCamera.Get<ck::FFragment_Camera_Springs>();
        const auto& Sensor = InCamera.Get<ck::FFragment_Camera_Sensor>();
        const auto& Noise = InCamera.Get<ck::FFragment_Camera_Noise>();
        const auto& Orientation = InCamera.Get<ck::FFragment_Camera_OrientationControl>();
        const auto& Reorient = InCamera.Get<ck::FFragment_Camera_AutoReorient>();
        const auto& Collision = InCamera.Get<ck::FFragment_Camera_Collision>();
        const auto& DoF = InCamera.Get<ck::FFragment_Camera_DepthOfField>();
        const auto AllAttributesReady =
            ck::IsValid(Rig._BoomArmPivotOffset) && ck::IsValid(Rig._BoomArmLength) &&
            ck::IsValid(Rig._FramingOffset) && ck::IsValid(Rig._FramingPitch) &&
            ck::IsValid(Rig._FramingYaw) && ck::IsValid(Rig._FixedBoomRotation) &&
            ck::IsValid(Springs._GroupBaseLocationInterpSpeed) &&
            ck::IsValid(Springs._LookAtLocationInterpSpeed) &&
            ck::IsValid(Sensor._FOV) && ck::IsValid(Sensor._AspectRatio) &&
            ck::IsValid(Sensor._OrthoWidth) &&
            ReadyRotation(Noise._Pitch) && ReadyRotation(Noise._Yaw) &&
            ReadyRotation(Noise._Roll) && ReadyRotation(Orientation._Pitch) &&
            ReadyRotation(Orientation._Yaw) && ReadyRotation(Reorient._Pitch) &&
            ReadyRotation(Reorient._Yaw) &&
            ck::IsValid(Reorient._LookAtTargetLocationPitchOffset) &&
            ck::IsValid(Reorient._LookAtTargetLocationYawOffset) &&
            ck::IsValid(Reorient._LookAtCameraLocationPitchOffset) &&
            ck::IsValid(Reorient._LookAtCameraLocationYawOffset) &&
            ck::IsValid(Collision._NumPredictionWhiskers) &&
            ck::IsValid(Collision._MaxWhiskerAngle) &&
            ck::IsValid(Collision._AdditionalOffsetFromHit) &&
            ck::IsValid(Collision._ClippingSpeed) &&
            ck::IsValid(Collision._ReturningSpeed) &&
            ck::IsValid(DoF._Fstop) && ck::IsValid(DoF._FocalDistance) &&
            ck::IsValid(DoF._SensorWidth);
        return AllAttributesReady;
    }

    static auto CanReadLayerEntry(const FCk_Handle_CameraLayer& InLayer) -> bool
    {
        return ck::IsValid(InLayer) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
                InLayer, ECk_EntityLifetime_DestructionPhase::BeginDestroy) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
                InLayer, ECk_EntityLifetime_DestructionPhase::Teardown) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
                InLayer, ECk_EntityLifetime_DestructionPhase::Destroyed) &&
            InLayer.Has<ck::FFragment_CameraLayer_Params>();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto ck::camera::CanReadCameraProfile(const FCk_Handle_Camera& InCamera) -> bool
{
    return camera_admission_detail::CanReadProfile(InCamera);
}

auto
    UCk_Utils_Camera_UE::
    Get_LayerCount(
        const FCk_Handle_Camera& InCamera)
    -> int32
{
    const auto CanRead = camera_admission_detail::CanUseCameraFeature<ck::FFragment_RecordOfCameraLayers>(InCamera);
    CK_ENSURE_IF_NOT(CanRead, TEXT("Camera layer count requires an admitted live director")) {}
    if (NOT CanRead) { return {}; }

    auto Count = int32{0};

    ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(InCamera,
    [&](FCk_Handle_CameraLayer InLayer)
    {
        if (NOT camera_admission_detail::CanReadLayerEntry(InLayer)) { return; }
        if (InLayer.Get<ck::FFragment_CameraLayer_Params>().Get_IsDefault())
        { return; }

        ++Count;
    });

    return Count;
}

auto
    UCk_Utils_Camera_UE::
    Has_Layer(
        const FCk_Handle_Camera& InCamera,
        TSubclassOf<UCk_CameraLayer_EntityScript> InLayerClass)
    -> bool
{
    const auto CanRead = camera_admission_detail::CanUseCameraFeature<ck::FFragment_RecordOfCameraLayers>(InCamera);
    CK_ENSURE_IF_NOT(CanRead, TEXT("Camera layer query requires an admitted live director")) {}
    if (NOT CanRead) { return {}; }

    auto Found = false;

    ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(InCamera,
    [&](FCk_Handle_CameraLayer InLayer)
    {
        if (NOT camera_admission_detail::CanReadLayerEntry(InLayer)) { return; }
        if (Found)
        { return; }

        if (InLayer.Get<ck::FFragment_CameraLayer_Params>().Get_LayerClass() == InLayerClass)
        { Found = true; }
    });

    return Found;
}

auto
    UCk_Utils_Camera_UE::
    Get_DominantLayerClass(
        const FCk_Handle_Camera& InCamera)
    -> TSubclassOf<UCk_CameraLayer_EntityScript>
{
    const auto CanRead = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanRead, TEXT("Camera dominant layer requires an admitted live director")) {}
    if (NOT CanRead) { return {}; }

    return InCamera.Get<ck::FFragment_Camera>().Get_DominantLayerClass();
}

auto
    UCk_Utils_Camera_UE::
    Get_ComposedProfile(
        const FCk_Handle_Camera& InCamera)
    -> FCk_CameraProfile
{
    const auto CanRead = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanRead, TEXT("Camera composed profile requires an admitted live director")) {}
    if (NOT CanRead) { return {}; }

    return InCamera.Get<ck::FFragment_Camera>().Get_ComposedProfile();
}

auto
    UCk_Utils_Camera_UE::
    Get_ViewInfo(
        const FCk_Handle_Camera& InCamera)
    -> FMinimalViewInfo
{
    const auto CanRead = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera_Pov>(InCamera);
    CK_ENSURE_IF_NOT(CanRead, TEXT("Camera view info requires an admitted live director")) {}
    if (NOT CanRead) { return {}; }

    return InCamera.Get<ck::FFragment_Camera_Pov>().Get_ViewInfo();
}

auto
    UCk_Utils_Camera_UE::
    Get_ViewAnchor(
        const FCk_Handle_Camera& InCamera)
    -> FCk_Handle_Transform
{
    const auto CanRead = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanRead, TEXT("Camera view anchor requires an admitted live director")) {}
    if (NOT CanRead) { return {}; }

    return InCamera.Get<ck::FFragment_Camera>().Get_ViewAnchor();
}

auto
    UCk_Utils_Camera_UE::
    Get_OwningActor(
        const FCk_Handle_Camera& InCamera)
    -> AActor*
{
    return UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor_Recursive(InCamera);
}

auto
    UCk_Utils_Camera_UE::
    TryGet_LocalViewInfo(
        const FCk_Handle& InAnyHandleInWorld,
        FMinimalViewInfo& OutViewInfo)
    -> bool
{
    if (ck::Is_NOT_Valid(InAnyHandleInWorld))
    { return false; }

    auto Found = false;
    auto AnyHandle = InAnyHandleInWorld;
    AnyHandle.View<ck::FFragment_Camera_Pov, CK_IGNORE_PENDING_KILL>().ForEach(
    [&](FCk_Entity InEntity, const ck::FFragment_Camera_Pov& InPov)
    {
        if (Found)
        { return; }

        const auto CameraEntity = ck::MakeHandle(InEntity, InAnyHandleInWorld);
        if (UCk_Utils_Net_UE::Get_IsEntityLocallyControlled_ByPlayer(CameraEntity)
            != ECk_Utils_Net_IsLocallyControlled_Result::IsLocallyControlled)
        { return; }

        OutViewInfo = InPov.Get_ViewInfo();
        Found = true;
    });

    if (Found)
    { return true; }

    const auto PlayerController = UCk_Utils_Game_UE::Get_PrimaryPlayerController(
        UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InAnyHandleInWorld));
    if (ck::Is_NOT_Valid(PlayerController) || ck::Is_NOT_Valid(PlayerController->PlayerCameraManager))
    { return false; }

    const auto CameraManager = PlayerController->PlayerCameraManager;
    OutViewInfo = FMinimalViewInfo{};
    OutViewInfo.Location = CameraManager->GetCameraLocation();
    OutViewInfo.Rotation = CameraManager->GetCameraRotation();
    OutViewInfo.FOV      = CameraManager->GetFOVAngle();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_Camera_UE, FCk_Handle_Camera, ck::FFragment_Camera_Params);

// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Camera_UE::
    Request_AddLayer(
        FCk_Handle_Camera& InCamera,
        const FCk_Request_Camera_AddLayer& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanQueue = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanQueue, TEXT("Camera layer request requires an admitted live director")) {}
    if (NOT CanQueue)
    {
        auto Rejected = InRequest;
        if (InDelegate.IsBound()) { Rejected.Set_CompletionDelegate(InDelegate); }
        Rejected.TryFireCompletion(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }
    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InCamera.AddOrGet<ck::FFragment_Camera_Requests>()._Requests.Emplace(InRequest);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_RemoveLayer(
        FCk_Handle_Camera& InCamera,
        const FCk_Request_Camera_RemoveLayer& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanQueue = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanQueue, TEXT("Camera layer request requires an admitted live director")) {}
    if (NOT CanQueue)
    {
        auto Rejected = InRequest;
        if (InDelegate.IsBound()) { Rejected.Set_CompletionDelegate(InDelegate); }
        Rejected.TryFireCompletion(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }
    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InCamera.AddOrGet<ck::FFragment_Camera_Requests>()._Requests.Emplace(InRequest);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_SetOrientationIntention(
        FCk_Handle_Camera& InCamera,
        FVector InOrientationIntention,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera_Pov>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InCamera.Get<ck::FFragment_Camera_Pov>().Set_OrientationIntention(InOrientationIntention);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_SnapBoomRotation(
        FCk_Handle_Camera& InCamera,
        FRotator InWorldRotation,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera_Pov>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InWorldRotation.Roll = 0.0f;

    auto& Pov = InCamera.Get<ck::FFragment_Camera_Pov>();
    // _Initialized so FPov::Run's seed-from-anchor branch cannot clobber this seed on a first frame.
    Pov._PovState._BoomArmRotation = InWorldRotation;
    Pov._PovState._Initialized     = true;

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_Set_OrientationYawLimits(
        FCk_Handle_Camera& InCamera,
        float InMinYaw,
        float InMaxYaw,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera_OrientationControl>(InCamera) &&
        ck::IsValid(InCamera.Get<ck::FFragment_Camera_OrientationControl>()._Yaw._Limits);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    auto& Limits = InCamera.Get<ck::FFragment_Camera_OrientationControl>()._Yaw._Limits;
    UCk_Utils_FloatAttribute_UE::Request_Override(Limits, InMinYaw, ECk_MinMaxCurrent::Min, {});
    UCk_Utils_FloatAttribute_UE::Request_Override(Limits, InMaxYaw, ECk_MinMaxCurrent::Max, {});

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Camera_UE::
    Request_Set_UseFixedBoomRotation(FCk_Handle_Camera& InCamera, bool bInEnabled, const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InCamera.Get<ck::FFragment_Camera>().Set_UseFixedBoomRotation(bInEnabled);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_Set_ConstrainAspectRatio(FCk_Handle_Camera& InCamera, bool bInEnabled, const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InCamera.Get<ck::FFragment_Camera>().Set_ConstrainAspectRatio(bInEnabled);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_SetProjectionMode(FCk_Handle_Camera& InCamera, const FCk_Request_Camera_SetProjectionMode& InRequest, const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    auto& CurrentFrag = InCamera.Get<ck::FFragment_Camera>();
    CurrentFrag.Set_ProjectionMode(InRequest.Get_ProjectionMode());

    if (InRequest.Get_OrthoNearClipPlane().IsSet())
    { CurrentFrag.Set_OrthoNearClipPlane(*InRequest.Get_OrthoNearClipPlane()); }

    if (InRequest.Get_OrthoFarClipPlane().IsSet())
    { CurrentFrag.Set_OrthoFarClipPlane(*InRequest.Get_OrthoFarClipPlane()); }

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_Set_HasOrientationControl(FCk_Handle_Camera& InCamera, bool bInEnabled, const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InCamera.Get<ck::FFragment_Camera>().Set_HasOrientationControl(bInEnabled);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_Set_HasAutoReorient(FCk_Handle_Camera& InCamera, bool bInEnabled, const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InCamera.Get<ck::FFragment_Camera>().Set_HasAutoReorient(bInEnabled);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_Set_HasCollision(FCk_Handle_Camera& InCamera, bool bInEnabled, const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InCamera.Get<ck::FFragment_Camera>().Set_HasCollision(bInEnabled);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_Set_UseAsyncTrace(FCk_Handle_Camera& InCamera, bool bInEnabled, const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InCamera.Get<ck::FFragment_Camera>().Set_UseAsyncTrace(bInEnabled);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

auto
    UCk_Utils_Camera_UE::
    Request_Set_UsePostProcess(FCk_Handle_Camera& InCamera, bool bInEnabled, const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_Camera
{
    const auto CanMutate = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera>(InCamera);
    CK_ENSURE_IF_NOT(CanMutate, TEXT("Camera setter requires an admitted live director")) {}
    if (NOT CanMutate)
    {
        InDelegate.ExecuteIfBound(FCk_Handle{}, ECk_Request_OperationResult::Failed_NotEnqueued);
        return {};
    }

    InCamera.Get<ck::FFragment_Camera>().Set_UsePostProcess(bInEnabled);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InCamera, ECk_Request_OperationResult::Succeeded);
    return InCamera;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Camera_UE::
    Get_ViewRotation(
        const FCk_Handle_Camera& InCamera)
    -> FRotator
{
    return Get_ViewInfo(InCamera).Rotation;
}

auto
    UCk_Utils_Camera_UE::
    Get_BoomRotation(
        const FCk_Handle_Camera& InCamera)
    -> FRotator
{
    const auto CanRead = camera_admission_detail::CanUseCameraFeature<ck::FFragment_Camera_Pov>(InCamera);
    CK_ENSURE_IF_NOT(CanRead, TEXT("Camera boom rotation requires an admitted live director")) {}
    if (NOT CanRead) { return {}; }
    return InCamera.Get<ck::FFragment_Camera_Pov>().Get_PovState()._BoomArmRotation;
}

// --------------------------------------------------------------------------------------------------------------------
// MATERIALIZE + ASSEMBLE
// --------------------------------------------------------------------------------------------------------------------

namespace camera_materialize_detail
{
    static constexpr ECk_Replication   NoRep   = ECk_Replication::DoesNotReplicate;
    static constexpr ECk_MinMaxCurrent Current = ECk_MinMaxCurrent::Current;
    static constexpr ECk_MinMaxCurrent Min     = ECk_MinMaxCurrent::Min;
    static constexpr ECk_MinMaxCurrent Max     = ECk_MinMaxCurrent::Max;

    // Every AddX below is adopt-or-add; rationale in CkCamera/CLAUDE.md.

    // Tuners are reconstruct-only: a camera is client-local per-viewer state whose profile plus current
    // possession own the authoritative post-load values, so hydration would run before the restored pawn is
    // locally possessed. Mark new and legacy-adopted children alike so future saves omit them.
    template <typename T_AttributeHandle>
    static auto MarkReconstructOnly(T_AttributeHandle InAttribute) -> T_AttributeHandle
    {
        const auto AttributeIsValid = ck::IsValid(InAttribute);
        CK_ENSURE_IF_NOT(AttributeIsValid,
            TEXT("Camera attribute materialization returned an invalid handle")) {}
        if (NOT AttributeIsValid) { return InAttribute; }
        InAttribute.AddOrGet<ck::FTag_Snapshot_ReconstructOnly>();
        return InAttribute;
    }

    static auto AddFloat(FCk_Handle& InOwner, const FGameplayTag& InTag, float InBase) -> FCk_Handle_FloatAttribute
    {
        if (auto Existing = UCk_Utils_FloatAttribute_UE::TryGet(InOwner, InTag);
            ck::IsValid(Existing))
        {
            UCk_Utils_FloatAttribute_UE::Request_Override(Existing, InBase, Current, {});
            return MarkReconstructOnly(Existing);
        }

        return MarkReconstructOnly(UCk_Utils_FloatAttribute_UE::Add(InOwner,
            FCk_FloatAttribute_Spec{InTag, InBase}, NoRep));
    }

    static auto AddRange(FCk_Handle& InOwner, const FGameplayTag& InTag, const FCk_FloatRange& InRange) -> FCk_Handle_FloatAttribute
    {
        if (auto Existing = UCk_Utils_FloatAttribute_UE::TryGet(InOwner, InTag);
            ck::IsValid(Existing))
        {
            UCk_Utils_FloatAttribute_UE::Request_Override(Existing, static_cast<float>(InRange.Get_Min()), Min, {});
            UCk_Utils_FloatAttribute_UE::Request_Override(Existing, static_cast<float>(InRange.Get_Max()), Max, {});
            return MarkReconstructOnly(Existing);
        }

        auto Params = FCk_FloatAttribute_Spec{InTag, 0.0f};
        Params.Set_MinMax(ECk_MinMax::MinMax);
        Params.Set_MinValue(static_cast<float>(InRange.Get_Min()));
        Params.Set_MaxValue(static_cast<float>(InRange.Get_Max()));
        return MarkReconstructOnly(UCk_Utils_FloatAttribute_UE::Add(InOwner, Params, NoRep));
    }

    static auto AddVector(FCk_Handle& InOwner, const FGameplayTag& InTag, const FVector& InBase) -> FCk_Handle_VectorAttribute
    {
        if (auto Existing = UCk_Utils_VectorAttribute_UE::TryGet(InOwner, InTag);
            ck::IsValid(Existing))
        {
            UCk_Utils_VectorAttribute_UE::Request_Override(Existing, InBase, Current, {});
            return MarkReconstructOnly(Existing);
        }

        return MarkReconstructOnly(UCk_Utils_VectorAttribute_UE::Add(InOwner,
            FCk_VectorAttribute_Spec{InTag, InBase}, NoRep));
    }

    static auto AddRotator(FCk_Handle& InOwner, const FGameplayTag& InTag, const FRotator& InBase) -> FCk_Handle_RotatorAttribute
    {
        if (auto Existing = UCk_Utils_RotatorAttribute_UE::TryGet(InOwner, InTag);
            ck::IsValid(Existing))
        {
            UCk_Utils_RotatorAttribute_UE::Request_Override(Existing, InBase, Current, {});
            return MarkReconstructOnly(Existing);
        }

        return MarkReconstructOnly(UCk_Utils_RotatorAttribute_UE::Add(InOwner,
            FCk_RotatorAttribute_Spec{InTag, InBase}, NoRep));
    }

    static auto AddInt(FCk_Handle& InOwner, const FGameplayTag& InTag, int32 InBase) -> FCk_Handle_IntegerAttribute
    {
        if (auto Existing = UCk_Utils_IntegerAttribute_UE::TryGet(InOwner, InTag);
            ck::IsValid(Existing))
        {
            UCk_Utils_IntegerAttribute_UE::Request_Override(Existing, InBase, Current, {});
            return MarkReconstructOnly(Existing);
        }

        return MarkReconstructOnly(UCk_Utils_IntegerAttribute_UE::Add(InOwner,
            FCk_IntegerAttribute_Spec{InTag, InBase}, NoRep));
    }

    static auto AddRotation(
        FCk_Handle& InOwner,
        const FGameplayTag& InSpeedTag,
        const FGameplayTag& InLimitsTag,
        const FGameplayTag& InNormalSpeedRangeTag,
        const FGameplayTag& InOutOfRangeMultiplierTag,
        const FCk_CameraProfile_Rotation& InRot) -> ck::FCameraAttr_Rotation
    {
        auto Out = ck::FCameraAttr_Rotation{};
        Out._Speed                = AddFloat(InOwner, InSpeedTag, InRot.Get_Speed());
        Out._Limits               = AddRange(InOwner, InLimitsTag, InRot.Get_Limits());
        Out._NormalSpeedRange     = AddRange(InOwner, InNormalSpeedRangeTag, InRot.Get_NormalSpeedRange());
        Out._OutOfRangeMultiplier = AddFloat(InOwner, InOutOfRangeMultiplierTag, InRot.Get_OutOfRangeMultiplier());
        return Out;
    }

    static auto ReadFloat(const FCk_Handle_FloatAttribute& InAttr) -> float
    {
        return UCk_Utils_FloatAttribute_UE::Get_FinalValue(InAttr, ECk_MinMaxCurrent::Current);
    }

    static auto ReadRange(const FCk_Handle_FloatAttribute& InAttr) -> FCk_FloatRange
    {
        return FCk_FloatRange{UCk_Utils_FloatAttribute_UE::Get_FinalValue(InAttr, Min),
                              UCk_Utils_FloatAttribute_UE::Get_FinalValue(InAttr, Max)};
    }

    static auto ReadRotation(const ck::FCameraAttr_Rotation& InAttr) -> FCk_CameraProfile_Rotation
    {
        auto Out = FCk_CameraProfile_Rotation{};
        Out.Set_Speed(ReadFloat(InAttr._Speed));
        Out.Set_Limits(ReadRange(InAttr._Limits));
        Out.Set_NormalSpeedRange(ReadRange(InAttr._NormalSpeedRange));
        Out.Set_OutOfRangeMultiplier(ReadFloat(InAttr._OutOfRangeMultiplier));
        return Out;
    }
}

auto
    UCk_Utils_Camera_UE::
    DoMaterializeAttributes(
        FCk_Handle_Camera& InCamera,
        const FCk_CameraProfile& InDefaults)
    -> bool
{
    using namespace camera_materialize_detail;

    FCk_Handle& Owner = InCamera;

    // Fail at the first incomplete section, before any later tuner is created.
    const auto ReadyRotation = [](const ck::FCameraAttr_Rotation& R)
    {
        return ck::IsValid(R._Speed) && ck::IsValid(R._Limits) &&
            ck::IsValid(R._NormalSpeedRange) && ck::IsValid(R._OutOfRangeMultiplier);
    };

    // ---- Rig ----
    {
        const auto& Rig = InDefaults.Get_Rig();
        auto& Section = InCamera.AddOrGet<ck::FFragment_Camera_Rig>();
        Section._BoomArmPivotOffset = AddVector(Owner,  TAG_Camera_Rig_BoomArmPivotOffset, Rig.Get_BoomArmPivotOffset());
        Section._BoomArmLength      = AddFloat(Owner,   TAG_Camera_Rig_BoomArmLength,      Rig.Get_BoomArmLength());
        Section._FramingOffset      = AddVector(Owner,  TAG_Camera_Rig_FramingOffset,      Rig.Get_FramingOffset());
        Section._FramingPitch       = AddFloat(Owner,   TAG_Camera_Rig_FramingPitch,       Rig.Get_FramingPitch());
        Section._FramingYaw         = AddFloat(Owner,   TAG_Camera_Rig_FramingYaw,         Rig.Get_FramingYaw());
        Section._FixedBoomRotation  = AddRotator(Owner, TAG_Camera_Rig_FixedBoomRotation,  Rig.Get_FixedBoomRotation());
        if (NOT (ck::IsValid(Section._BoomArmPivotOffset) &&
            ck::IsValid(Section._BoomArmLength) &&
            ck::IsValid(Section._FramingOffset) &&
            ck::IsValid(Section._FramingPitch) &&
            ck::IsValid(Section._FramingYaw) &&
            ck::IsValid(Section._FixedBoomRotation))) { return false; }
    }

    // ---- Springs ----
    {
        const auto& Springs = InDefaults.Get_Springs();
        auto& Section = InCamera.AddOrGet<ck::FFragment_Camera_Springs>();
        Section._GroupBaseLocationInterpSpeed = AddFloat(Owner, TAG_Camera_Springs_GroupBaseLocationInterpSpeed, Springs.Get_GroupBaseLocationInterpSpeed());
        Section._LookAtLocationInterpSpeed    = AddFloat(Owner, TAG_Camera_Springs_LookAtLocationInterpSpeed,    Springs.Get_LookAtLocationInterpSpeed());
        if (NOT (ck::IsValid(Section._GroupBaseLocationInterpSpeed) &&
            ck::IsValid(Section._LookAtLocationInterpSpeed))) { return false; }
    }

    // ---- Sensor ----
    {
        const auto& Sensor = InDefaults.Get_Sensor();
        auto& Section = InCamera.AddOrGet<ck::FFragment_Camera_Sensor>();
        Section._FOV         = AddFloat(Owner, TAG_Camera_Sensor_FOV,         Sensor.Get_FOV());
        Section._AspectRatio = AddFloat(Owner, TAG_Camera_Sensor_AspectRatio, Sensor.Get_AspectRatio());
        Section._OrthoWidth  = AddFloat(Owner, TAG_Camera_Sensor_OrthoWidth,  Sensor.Get_OrthoWidth());
        if (NOT (ck::IsValid(Section._FOV) &&
            ck::IsValid(Section._AspectRatio) &&
            ck::IsValid(Section._OrthoWidth))) { return false; }
    }

    // ---- Noise ----
    {
        const auto& Noise = InDefaults.Get_Noise();
        auto& Section = InCamera.AddOrGet<ck::FFragment_Camera_Noise>();
        Section._Pitch = AddRotation(Owner, TAG_Camera_Noise_Pitch_Speed, TAG_Camera_Noise_Pitch_Limits, TAG_Camera_Noise_Pitch_NormalSpeedRange, TAG_Camera_Noise_Pitch_OutOfRangeMultiplier, Noise.Get_Pitch());
        Section._Yaw   = AddRotation(Owner, TAG_Camera_Noise_Yaw_Speed,   TAG_Camera_Noise_Yaw_Limits,   TAG_Camera_Noise_Yaw_NormalSpeedRange,   TAG_Camera_Noise_Yaw_OutOfRangeMultiplier,   Noise.Get_Yaw());
        Section._Roll  = AddRotation(Owner, TAG_Camera_Noise_Roll_Speed,  TAG_Camera_Noise_Roll_Limits,  TAG_Camera_Noise_Roll_NormalSpeedRange,  TAG_Camera_Noise_Roll_OutOfRangeMultiplier,  Noise.Get_Roll());
        if (NOT (ReadyRotation(Section._Pitch) &&
            ReadyRotation(Section._Yaw) &&
            ReadyRotation(Section._Roll))) { return false; }
    }

    // ---- Orientation Control ----
    {
        const auto& OC = InDefaults.Get_OrientationControl();
        auto& Section = InCamera.AddOrGet<ck::FFragment_Camera_OrientationControl>();
        Section._Pitch = AddRotation(Owner, TAG_Camera_OrientationControl_Pitch_Speed, TAG_Camera_OrientationControl_Pitch_Limits, TAG_Camera_OrientationControl_Pitch_NormalSpeedRange, TAG_Camera_OrientationControl_Pitch_OutOfRangeMultiplier, OC.Get_Pitch());
        Section._Yaw   = AddRotation(Owner, TAG_Camera_OrientationControl_Yaw_Speed,   TAG_Camera_OrientationControl_Yaw_Limits,   TAG_Camera_OrientationControl_Yaw_NormalSpeedRange,   TAG_Camera_OrientationControl_Yaw_OutOfRangeMultiplier,   OC.Get_Yaw());
        if (NOT (ReadyRotation(Section._Pitch) &&
            ReadyRotation(Section._Yaw))) { return false; }
    }

    // ---- Auto Reorient ----
    {
        const auto& AR = InDefaults.Get_AutoReorient();
        auto& Section = InCamera.AddOrGet<ck::FFragment_Camera_AutoReorient>();
        Section._Pitch = AddRotation(Owner, TAG_Camera_AutoReorient_Pitch_Speed, TAG_Camera_AutoReorient_Pitch_Limits, TAG_Camera_AutoReorient_Pitch_NormalSpeedRange, TAG_Camera_AutoReorient_Pitch_OutOfRangeMultiplier, AR.Get_Pitch());
        Section._Yaw   = AddRotation(Owner, TAG_Camera_AutoReorient_Yaw_Speed,   TAG_Camera_AutoReorient_Yaw_Limits,   TAG_Camera_AutoReorient_Yaw_NormalSpeedRange,   TAG_Camera_AutoReorient_Yaw_OutOfRangeMultiplier,   AR.Get_Yaw());
        Section._LookAtTargetLocationPitchOffset = AddFloat(Owner, TAG_Camera_AutoReorient_LookAtTargetLocationPitchOffset, AR.Get_LookAtTargetLocationPitchOffset());
        Section._LookAtTargetLocationYawOffset   = AddFloat(Owner, TAG_Camera_AutoReorient_LookAtTargetLocationYawOffset,   AR.Get_LookAtTargetLocationYawOffset());
        Section._LookAtCameraLocationPitchOffset = AddFloat(Owner, TAG_Camera_AutoReorient_LookAtCameraLocationPitchOffset, AR.Get_LookAtCameraLocationPitchOffset());
        Section._LookAtCameraLocationYawOffset   = AddFloat(Owner, TAG_Camera_AutoReorient_LookAtCameraLocationYawOffset,   AR.Get_LookAtCameraLocationYawOffset());
        if (NOT (ReadyRotation(Section._Pitch) &&
            ReadyRotation(Section._Yaw) &&
            ck::IsValid(Section._LookAtTargetLocationPitchOffset) &&
            ck::IsValid(Section._LookAtTargetLocationYawOffset) &&
            ck::IsValid(Section._LookAtCameraLocationPitchOffset) &&
            ck::IsValid(Section._LookAtCameraLocationYawOffset))) { return false; }
    }

    // ---- Collision ----
    {
        const auto& Collision = InDefaults.Get_Collision();
        auto& Section = InCamera.AddOrGet<ck::FFragment_Camera_Collision>();
        Section._NumPredictionWhiskers   = AddInt(Owner,   TAG_Camera_Collision_NumPredictionWhiskers,   Collision.Get_NumPredictionWhiskers());
        Section._MaxWhiskerAngle         = AddFloat(Owner, TAG_Camera_Collision_MaxWhiskerAngle,         Collision.Get_MaxWhiskerAngle());
        Section._AdditionalOffsetFromHit = AddFloat(Owner, TAG_Camera_Collision_AdditionalOffsetFromHit, Collision.Get_AdditionalOffsetFromHit());
        Section._ClippingSpeed           = AddFloat(Owner, TAG_Camera_Collision_ClippingSpeed,           Collision.Get_ClippingSpeed());
        Section._ReturningSpeed          = AddFloat(Owner, TAG_Camera_Collision_ReturningSpeed,          Collision.Get_ReturningSpeed());
        if (NOT (ck::IsValid(Section._NumPredictionWhiskers) &&
            ck::IsValid(Section._MaxWhiskerAngle) &&
            ck::IsValid(Section._AdditionalOffsetFromHit) &&
            ck::IsValid(Section._ClippingSpeed) &&
            ck::IsValid(Section._ReturningSpeed))) { return false; }
    }

    // ---- Depth Of Field ----
    {
        const auto& DoF = InDefaults.Get_DepthOfField();
        auto& Section = InCamera.AddOrGet<ck::FFragment_Camera_DepthOfField>();
        Section._Fstop         = AddFloat(Owner, TAG_Camera_DepthOfField_Fstop,         DoF.Get_Fstop());
        Section._FocalDistance = AddFloat(Owner, TAG_Camera_DepthOfField_FocalDistance, DoF.Get_FocalDistance());
        Section._SensorWidth   = AddFloat(Owner, TAG_Camera_DepthOfField_SensorWidth,   DoF.Get_SensorWidth());
        if (NOT (ck::IsValid(Section._Fstop) &&
            ck::IsValid(Section._FocalDistance) &&
            ck::IsValid(Section._SensorWidth))) { return false; }
    }

    // Validate every materialized tuner before publishing the profile cache.
    // Request_Override on an adopted tuner is asynchronous: this proves only
    // structural completion, not that its final value has already settled.
    if (NOT camera_admission_detail::CanReadProfile(InCamera)) { return false; }

    // ---- Non-attribute leaves (bools + curves) → Current ----
    {
        auto& CurrentFrag = InCamera.AddOrGet<ck::FFragment_Camera>();
        CurrentFrag.Set_UseFixedBoomRotation(InDefaults.Get_Rig().Get_UseFixedBoomRotation());
        CurrentFrag.Set_ConstrainAspectRatio(InDefaults.Get_Sensor().Get_ConstrainAspectRatio());
        CurrentFrag.Set_ProjectionMode(InDefaults.Get_Sensor().Get_ProjectionMode());
        CurrentFrag.Set_OrthoNearClipPlane(InDefaults.Get_Sensor().Get_OrthoNearClipPlane());
        CurrentFrag.Set_OrthoFarClipPlane(InDefaults.Get_Sensor().Get_OrthoFarClipPlane());
        CurrentFrag.Set_HasOrientationControl(InDefaults.Get_HasOrientationControl());
        CurrentFrag.Set_HasAutoReorient(InDefaults.Get_HasAutoReorient());
        CurrentFrag.Set_HasCollision(InDefaults.Get_HasCollision());
        CurrentFrag.Set_UseAsyncTrace(InDefaults.Get_Collision().Get_UseAsyncTrace());
        CurrentFrag.Set_UsePostProcess(InDefaults.Get_UsePostProcess());
        CurrentFrag.Set_XIntentionCurve(InDefaults.Get_OrientationControl().Get_XIntentionCurve());
        CurrentFrag.Set_YIntentionCurve(InDefaults.Get_OrientationControl().Get_YIntentionCurve());

        // Seed the read-cache so the first frame (before the lifecycle processor runs) has a sane profile.
        CurrentFrag.Set_ComposedProfile(InDefaults);
    }
    return true;
}

auto
    UCk_Utils_Camera_UE::
    Get_Profile(
        const FCk_Handle_Camera& InCamera)
    -> FCk_CameraProfile
{
    const auto CanRead = camera_admission_detail::CanReadProfile(InCamera);
    CK_ENSURE_IF_NOT(CanRead, TEXT("Camera tuner profile requires a complete admitted director")) {}
    if (NOT CanRead) { return {}; }

    using namespace camera_materialize_detail;

    auto Profile = FCk_CameraProfile{};

    const auto& CurrentFrag = InCamera.Get<ck::FFragment_Camera>();

    // ---- Rig ----
    {
        const auto& Section = InCamera.Get<ck::FFragment_Camera_Rig>();
        auto Rig = FCk_CameraProfile_Rig{};
        Rig.Set_BoomArmPivotOffset(UCk_Utils_VectorAttribute_UE::Get_FinalValue(Section._BoomArmPivotOffset, Current));
        Rig.Set_BoomArmLength(ReadFloat(Section._BoomArmLength));
        Rig.Set_FramingOffset(UCk_Utils_VectorAttribute_UE::Get_FinalValue(Section._FramingOffset, Current));
        Rig.Set_FramingPitch(ReadFloat(Section._FramingPitch));
        Rig.Set_FramingYaw(ReadFloat(Section._FramingYaw));
        Rig.Set_UseFixedBoomRotation(CurrentFrag.Get_UseFixedBoomRotation());
        Rig.Set_FixedBoomRotation(UCk_Utils_RotatorAttribute_UE::Get_FinalValue(Section._FixedBoomRotation, Current));
        Profile.Set_Rig(Rig);
    }

    // ---- Springs ----
    {
        const auto& Section = InCamera.Get<ck::FFragment_Camera_Springs>();
        auto Springs = FCk_CameraProfile_Springs{};
        Springs.Set_GroupBaseLocationInterpSpeed(ReadFloat(Section._GroupBaseLocationInterpSpeed));
        Springs.Set_LookAtLocationInterpSpeed(ReadFloat(Section._LookAtLocationInterpSpeed));
        Profile.Set_Springs(Springs);
    }

    // ---- Sensor ----
    {
        const auto& Section = InCamera.Get<ck::FFragment_Camera_Sensor>();
        auto Sensor = FCk_CameraProfile_Sensor{};
        Sensor.Set_FOV(ReadFloat(Section._FOV));
        Sensor.Set_ConstrainAspectRatio(CurrentFrag.Get_ConstrainAspectRatio());
        Sensor.Set_AspectRatio(ReadFloat(Section._AspectRatio));
        Sensor.Set_ProjectionMode(CurrentFrag.Get_ProjectionMode());
        Sensor.Set_OrthoWidth(ReadFloat(Section._OrthoWidth));
        Sensor.Set_OrthoNearClipPlane(CurrentFrag.Get_OrthoNearClipPlane());
        Sensor.Set_OrthoFarClipPlane(CurrentFrag.Get_OrthoFarClipPlane());
        Profile.Set_Sensor(Sensor);
    }

    // ---- Noise ----
    {
        const auto& Section = InCamera.Get<ck::FFragment_Camera_Noise>();
        auto Noise = FCk_CameraProfile_Noise{};
        Noise.Set_Pitch(ReadRotation(Section._Pitch));
        Noise.Set_Yaw(ReadRotation(Section._Yaw));
        Noise.Set_Roll(ReadRotation(Section._Roll));
        Profile.Set_Noise(Noise);
    }

    // ---- Orientation Control ----
    {
        Profile.Set_HasOrientationControl(CurrentFrag.Get_HasOrientationControl());
        const auto& Section = InCamera.Get<ck::FFragment_Camera_OrientationControl>();
        auto OC = FCk_CameraProfile_OrientationControl{};
        OC.Set_Pitch(ReadRotation(Section._Pitch));
        OC.Set_Yaw(ReadRotation(Section._Yaw));
        OC.Set_XIntentionCurve(CurrentFrag.Get_XIntentionCurve());
        OC.Set_YIntentionCurve(CurrentFrag.Get_YIntentionCurve());
        Profile.Set_OrientationControl(OC);
    }

    // ---- Auto Reorient ----
    {
        Profile.Set_HasAutoReorient(CurrentFrag.Get_HasAutoReorient());
        const auto& Section = InCamera.Get<ck::FFragment_Camera_AutoReorient>();
        auto AR = FCk_CameraProfile_AutoReorient{};
        AR.Set_Pitch(ReadRotation(Section._Pitch));
        AR.Set_Yaw(ReadRotation(Section._Yaw));
        AR.Set_LookAtTargetLocationPitchOffset(ReadFloat(Section._LookAtTargetLocationPitchOffset));
        AR.Set_LookAtTargetLocationYawOffset(ReadFloat(Section._LookAtTargetLocationYawOffset));
        AR.Set_LookAtCameraLocationPitchOffset(ReadFloat(Section._LookAtCameraLocationPitchOffset));
        AR.Set_LookAtCameraLocationYawOffset(ReadFloat(Section._LookAtCameraLocationYawOffset));
        Profile.Set_AutoReorient(AR);
    }

    // ---- Collision ----
    {
        Profile.Set_HasCollision(CurrentFrag.Get_HasCollision());
        const auto& Section = InCamera.Get<ck::FFragment_Camera_Collision>();
        auto Collision = FCk_CameraProfile_Collision{};
        Collision.Set_UseAsyncTrace(CurrentFrag.Get_UseAsyncTrace());
        Collision.Set_NumPredictionWhiskers(UCk_Utils_IntegerAttribute_UE::Get_FinalValue(Section._NumPredictionWhiskers, Current));
        Collision.Set_MaxWhiskerAngle(ReadFloat(Section._MaxWhiskerAngle));
        Collision.Set_AdditionalOffsetFromHit(ReadFloat(Section._AdditionalOffsetFromHit));
        Collision.Set_ClippingSpeed(ReadFloat(Section._ClippingSpeed));
        Collision.Set_ReturningSpeed(ReadFloat(Section._ReturningSpeed));
        Profile.Set_Collision(Collision);
    }

    // ---- Depth Of Field ----
    {
        Profile.Set_UsePostProcess(CurrentFrag.Get_UsePostProcess());
        const auto& Section = InCamera.Get<ck::FFragment_Camera_DepthOfField>();
        auto DoF = FCk_CameraProfile_DepthOfField{};
        DoF.Set_Fstop(ReadFloat(Section._Fstop));
        DoF.Set_FocalDistance(ReadFloat(Section._FocalDistance));
        DoF.Set_SensorWidth(ReadFloat(Section._SensorWidth));
        Profile.Set_DepthOfField(DoF);
    }

    return Profile;
}

// --------------------------------------------------------------------------------------------------------------------
