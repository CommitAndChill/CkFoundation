#include "CkCamera_Processor.h"

#include "CkCamera/CkCamera_Log.h"
#include "CkCamera/CkCamera_Stats.h"
#include "CkCamera/Camera/CkCamera_Utils.h"
#include "CkCamera_Context_Private.h"
#include "CkCamera_Admission_Private.h"
#include "CkCamera/Camera/CameraLayer/CkCameraLayer_Fragment.h"
#include "CkCamera/Camera/CameraLayer/CkCameraLayer_Utils.h"
#include "CkCamera/Camera/CameraLayer/EntityScripts/CkCameraLayer_EntityScript.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>

#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_Camera_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_Camera_CancelPendingRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_CameraLayer_Lifecycle);
CK_REGISTER_PROCESSOR(ck::FProcessor_Camera_UpdatePOV);

// --------------------------------------------------------------------------------------------------------------------

DECLARE_CYCLE_STAT(TEXT("Camera::PovRun"), STAT_Camera_PovRun, STATGROUP_CkCamera);

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    static auto DoGet_BlendRateFromTime(FCk_Time InTime) -> float
    {
        constexpr auto InstantBlendRate = 1000.0f;

        const auto Seconds = static_cast<float>(InTime.Get_Seconds());
        return Seconds > KINDA_SMALL_NUMBER ? (1.0f / Seconds) : InstantBlendRate;
    }

    // HANDLE REQUESTS

    auto
        FProcessor_Camera_HandleRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Camera_Requests& InRequestsComp) const
        -> void
    {
        InHandle.CopyAndRemove(InRequestsComp, [&](const FFragment_Camera_Requests& InRequests)
        {
            ck::algo::ForEachRequest(InRequests._Requests, ck::Visitor(
            [&](const auto& InRequest) -> void
            {
                auto Result = ECk_Request_OperationResult::Failed;
                const auto Guard = MakeCompletionGuard(InRequest, InHandle, Result);

                if (DoHandleRequest(InHandle, InRequest))
                { Result = ECk_Request_OperationResult::Succeeded; }
            }), policy::DontResetContainer{});
        });
    }

    auto
        FProcessor_Camera_HandleRequests::
        DoHandleRequest(
            HandleType InHandle,
            const FCk_Request_Camera_AddLayer& InRequest)
        -> bool
    {
        const auto LayerClass = InRequest.Get_LayerClass();

        const auto ClassIsValid = ck::IsValid(LayerClass);
        CK_ENSURE_IF_NOT(ClassIsValid,
            TEXT("AddLayer: invalid layer class on camera [{}]"), InHandle) {}
        if (NOT ClassIsValid)
        { return false; }

        const auto CameraIsValid = ck::IsValid(InHandle) &&
            InHandle.Has_All<FFragment_Camera_Params, FFragment_Camera,
                FFragment_Camera_Pov, FFragment_Transform>();
        CK_ENSURE_IF_NOT(CameraIsValid,
            TEXT("AddLayer: camera [{}] lacks required composition"), InHandle) {}
        if (NOT CameraIsValid)
        { return false; }

        auto TypedLayer = ::UCk_Utils_CameraLayer_UE::Create(InHandle, LayerClass);
        const auto NewLayerIsValid = ck::IsValid(TypedLayer);
        CK_ENSURE_IF_NOT(NewLayerIsValid,
            TEXT("AddLayer: failed to admit new layer on camera [{}]"), InHandle) {}
        if (NOT NewLayerIsValid)
        { return false; }

        const auto Priority = InRequest.Get_Priority();

        // Admit first: an invalid class, retiring owner, or rejected record must not fade an existing layer.
        if (InRequest.Get_StackingBehavior() == ECk_Camera_StackingBehavior::OneOnly)
        {
            ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(InHandle,
            [&](FCk_Handle_CameraLayer InSibling)
            {
                if (InSibling == TypedLayer)
                { return; }
                if (InSibling.Get<FFragment_CameraLayer_Params>().Get_IsDefault())
                { return; }

                if (InSibling.Get<FFragment_CameraLayer_Params>().Get_Priority() != Priority)
                { return; }

                auto& SiblingBlend = InSibling.Get<FFragment_CameraLayer_Blend>();
                SiblingBlend.Set_TargetAlpha(0.0f);
                SiblingBlend.Set_BlendRate(DoGet_BlendRateFromTime(InRequest.Get_BlendInTime()));
            });
        }

        {
            auto& Params = TypedLayer.Get<FFragment_CameraLayer_Params>();
            Params.Set_Priority(Priority);
            Params.Set_CameraTarget(InRequest.Get_CameraTarget());
        }
        {
            auto& Blend = TypedLayer.Get<FFragment_CameraLayer_Blend>();
            Blend.Set_BlendRate(DoGet_BlendRateFromTime(InRequest.Get_BlendInTime()));
        }

        camera::VeryVerbose(TEXT("[Camera] AddLayer [{}] -> entity [{}] on camera [{}]"),
            LayerClass, TypedLayer, InHandle);

        return true;
    }

    auto
        FProcessor_Camera_HandleRequests::
        DoHandleRequest(
            HandleType InHandle,
            const FCk_Request_Camera_RemoveLayer& InRequest)
        -> bool
    {
        const auto LayerClass = InRequest.Get_LayerClass();

        // Begin blend-out; the lifecycle processor prunes (destroys) the entity once its alpha reaches 0.
        ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(InHandle,
        [&](FCk_Handle_CameraLayer InLayer)
        {
            if (InLayer.Get<ck::FFragment_CameraLayer_Params>().Get_IsDefault())
            { return; }

            if (InLayer.Get<ck::FFragment_CameraLayer_Params>().Get_LayerClass() != LayerClass)
            { return; }

            auto& Blend = InLayer.Get<ck::FFragment_CameraLayer_Blend>();
            Blend.Set_TargetAlpha(0.0f);
            Blend.Set_BlendRate(DoGet_BlendRateFromTime(InRequest.Get_BlendOutTime()));
        });

        return true;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_Camera_CancelPendingRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Camera_Requests& InRequestsComp)
        -> void
    {
        request::FireCancelledForPending(InHandle, InRequestsComp.Get_Requests());
    }

    // LAYER LIFECYCLE

    auto
        FProcessor_CameraLayer_Lifecycle::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_Camera& InCamera)
        -> void
    {
        auto Context = camera::FCameraContext{};
        if (NOT camera::TryResolveCameraContext(InHandle, Context) ||
            NOT camera::CanReadCameraProfile(InHandle) ||
            NOT InHandle.Has<FFragment_RecordOfCameraLayers>()) { return; }

        // The ordinary record iterator holds a reference to its backing array. A
        // layer script may disconnect a sibling or tear down that array in Tick.
        // Snapshot all direct records and extension records before any callback.
        struct FSnapshotEntry
        {
            FCk_Handle RecordOwner;
            FCk_Handle_CameraLayer Layer;
        };
        auto Entries = TArray<FSnapshotEntry>{};
        auto PendingRecords = TArray<FCk_Handle>{ InHandle };
        auto VisitedRecords = TArray<FCk_Handle>{};
        const auto Registry = InHandle.Get_RegistryView().Get_RegistryHandle();
        while (NOT PendingRecords.IsEmpty())
        {
            const auto RecordOwner = PendingRecords.Pop(EAllowShrinking::No);
            if (NOT ck::IsValid(RecordOwner) ||
                RecordOwner.Get_RegistryView().Get_RegistryHandle() != Registry ||
                VisitedRecords.Contains(RecordOwner)) { continue; }
            VisitedRecords.Add(RecordOwner);

            if (RecordOwner.Has<FFragment_RecordOfCameraLayers>())
            {
                auto Direct = TArray<FCk_Handle_CameraLayer>{};
                if (NOT FUtils_RecordOfCameraLayers::TryGet_DirectEntries(RecordOwner, Direct))
                { return; }
                for (const auto& Layer : Direct)
                { Entries.Add(FSnapshotEntry{RecordOwner, Layer}); }
            }
            if (RecordOwner.Has<FFragment_RecordOfEntityExtensions>())
            {
                auto Extensions = TArray<FCk_Handle_EntityExtension>{};
                if (NOT RecordOfEntityExtensions_Utils::TryGet_DirectEntries(
                        RecordOwner, Extensions)) { return; }
                // Stack in reverse to keep the record utility's depth-first order.
                for (auto Index = Extensions.Num() - 1; Index >= 0; --Index)
                { PendingRecords.Add(FCk_Handle{Extensions[Index]}); }
            }
        }

        const auto CameraStillReadable = [&]() -> bool
        {
            auto CurrentContext = camera::FCameraContext{};
            return camera::TryResolveCameraContext(InHandle, CurrentContext) &&
                CurrentContext.World == Context.World &&
                camera::CanReadCameraProfile(InHandle) &&
                InHandle.Has<FFragment_RecordOfCameraLayers>();
        };
        const auto RecordStillReachable = [&](const FCk_Handle& Wanted) -> bool
        {
            // A Tick can disconnect an extension without destroying that
            // extension's own camera-layer record. Reprove the actual path.
            auto Pending = TArray<FCk_Handle>{ InHandle };
            auto Seen = TArray<FCk_Handle>{};
            while (NOT Pending.IsEmpty())
            {
                const auto Current = Pending.Pop(EAllowShrinking::No);
                if (NOT ck::IsValid(Current) ||
                    Current.Get_RegistryView().Get_RegistryHandle() != Registry ||
                    Seen.Contains(Current)) { continue; }
                if (Current == Wanted) { return true; }
                Seen.Add(Current);
                if (NOT Current.Has<FFragment_RecordOfEntityExtensions>())
                { continue; }
                auto Extensions = TArray<FCk_Handle_EntityExtension>{};
                if (NOT RecordOfEntityExtensions_Utils::TryGet_DirectEntries(
                        Current, Extensions)) { return false; }
                for (auto Index = Extensions.Num() - 1; Index >= 0; --Index)
                { Pending.Add(FCk_Handle{Extensions[Index]}); }
            }
            return false;
        };
        const auto LayerStillReadable = [&](const FSnapshotEntry& Entry) -> bool
        {
            const auto& Owner = Entry.RecordOwner;
            const auto& Layer = Entry.Layer;
            auto Direct = TArray<FCk_Handle_CameraLayer>{};
            return ck::IsValid(Owner) &&
                Owner.Get_RegistryView().Get_RegistryHandle() == Registry &&
                RecordStillReachable(Owner) &&
                FUtils_RecordOfCameraLayers::TryGet_DirectEntries(Owner, Direct) &&
                Direct.Contains(Layer) &&
                ck::IsValid(Layer) &&
                Layer.Get_RegistryView().Get_RegistryHandle() == Registry &&
                Layer.Has_All<FFragment_CameraLayer_Params, FFragment_CameraLayer_Blend>();
        };

        auto Dominant = FCk_Handle_CameraLayer{};
        auto DominantRecord = FCk_Handle{};
        auto DominantAlpha = -1.0f;
        auto DominantPriority = TNumericLimits<int32>::Lowest();
        for (const auto& Entry : Entries)
        {
            if (NOT CameraStillReadable()) { return; }
            if (NOT LayerStillReadable(Entry)) { continue; }
            auto InLayer = Entry.Layer;
            const auto Alpha = InLayer.Get<FFragment_CameraLayer_Blend>().Get_Alpha();
            const auto TargetAlpha = InLayer.Get<FFragment_CameraLayer_Blend>().Get_TargetAlpha();

            // The acquired modifiers outlive this destroy unless ExitLayer (driven by EndPlay) removes them.
            if (Alpha <= 0.0f && TargetAlpha <= 0.0f)
            {
                if (NOT InLayer.Get<FFragment_CameraLayer_Params>().Get_IsDefault())
                { UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InLayer); }
                continue;
            }

            if (Alpha <= 0.0f || NOT InLayer.Has<FTag_CameraLayer_Active>())
            { continue; }

            if (InLayer.Has<FFragment_EntityScript>())
            {
                if (auto* Script = Cast<UCk_CameraLayer_EntityScript>(
                        InLayer.Get<FFragment_EntityScript>().Get_Script().Get());
                    ck::IsValid(Script))
                {
                    if (Script->Get_TickMode() == ECk_Camera_TickMode::Tick)
                    { Script->Tick(InLayer, InDeltaT); }
                }
            }

            // Tick can invalidate the director, this layer, or its record.
            if (NOT CameraStillReadable()) { return; }
            if (NOT LayerStillReadable(Entry)) { continue; }
            const auto Priority = InLayer.Get<FFragment_CameraLayer_Params>().Get_Priority();
            if (Priority > DominantPriority || (Priority == DominantPriority && Alpha >= DominantAlpha))
            {
                DominantPriority = Priority;
                DominantAlpha = Alpha;
                Dominant = InLayer;
                DominantRecord = Entry.RecordOwner;
            }
        }

        if (NOT CameraStillReadable()) { return; }
        if (ck::IsValid(Dominant) &&
            NOT LayerStillReadable(FSnapshotEntry{DominantRecord, Dominant})) { return; }
        auto& CurrentCamera = InHandle.Get<FFragment_Camera>();

        // The tuner attributes were already recomputed earlier this frame.
        CurrentCamera._ComposedProfile = ::UCk_Utils_Camera_UE::Get_Profile(InHandle);

        CurrentCamera._DominantLookAt.Reset();
        CurrentCamera._ViewTarget         = FCk_Camera_ViewTargetResolved{};
        CurrentCamera._DominantLayerClass = nullptr;
        if (ck::IsValid(Dominant))
        {
            const auto& DominantParams = Dominant.Get<FFragment_CameraLayer_Params>();
            CurrentCamera._DominantLayerClass = DominantParams.Get_LayerClass();

            const auto& CameraTarget = DominantParams.Get_CameraTarget();
            if (const auto& TargetHandle = CameraTarget.Get_Target();
                ck::IsValid(TargetHandle) &&
                TargetHandle.Get_RegistryView().Get_RegistryHandle() == Registry &&
                TargetHandle.Has<FFragment_Transform>())
            {
                switch (CameraTarget.Get_Mode())
                {
                    case ECk_Camera_TargetMode::LookAt:
                    {
                        CurrentCamera._DominantLookAt = UCk_Utils_Transform_UE::Get_EntityCurrentLocation(TargetHandle);
                        break;
                    }
                    case ECk_Camera_TargetMode::ViewTarget:
                    {
                        CurrentCamera._ViewTarget._IsActive = true;
                        CurrentCamera._ViewTarget._Target   = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(TargetHandle);
                        CurrentCamera._ViewTarget._Alpha    = DominantAlpha;
                        break;
                    }
                }
            }
        }
    }

    // --------------------------------------------------------------------------------------------------------------------

    // UPDATE POV

    auto
        FProcessor_Camera_UpdatePOV::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_Camera& InCamera,
            FFragment_Camera_Pov& InPov)
        -> void
    {
        auto Context = ck::camera::FCameraContext{};
        if (NOT ck::camera::TryResolveCameraContext(InHandle, Context)) { return; }

        const auto& Profile = InCamera.Get_ComposedProfile();
        const auto& Sensor  = Profile.Get_Sensor();

        // Add requires a transform handle, so a director always has its input anchor — read it directly.
        auto Input = ck::camera::FPov_Input{};
        Input._AnchorTransform      = InHandle.Get<ck::FFragment_Transform>().Get_Transform();
        Input._OrientationIntention = InPov.Get_OrientationIntention();
        Input._DeltaSeconds         = static_cast<float>(InDeltaT.Get_Seconds());
        Input._LookAtLocation       = InCamera._DominantLookAt;
        Input._World                = Context.World;
        Input._TraceIgnoreActor     = Context.OwningActor;

        {
            SCOPE_CYCLE_COUNTER(STAT_Camera_PovRun);
            ck::camera::FPov::Run(Profile, Input, InPov._PovState);
        }

        // The intention is a per-frame DELTA: consume it, or it keeps re-applying after the input source stops.
        InPov._OrientationIntention = FVector::ZeroVector;

        // Overrides boom/framing, not the anchor, and leaves FOV alone (the layer's FOV modifier eases that
        // independently). Inactive = the rig POV passes through untouched.
        auto FinalXf = InPov._PovState._CameraTransform;
        if (InCamera._ViewTarget._IsActive)
        {
            FinalXf.Blend(InPov._PovState._CameraTransform, InCamera._ViewTarget._Target, InCamera._ViewTarget._Alpha);
        }

        auto ViewInfo = FMinimalViewInfo{};
        ViewInfo.Location    = FinalXf.GetLocation();
        ViewInfo.Rotation    = FinalXf.Rotator();
        ViewInfo.FOV         = Sensor.Get_FOV();
        ViewInfo.DesiredFOV  = Sensor.Get_FOV();
        if (Sensor.Get_ConstrainAspectRatio())
        {
            ViewInfo.bConstrainAspectRatio = true;
            ViewInfo.AspectRatio          = Sensor.Get_AspectRatio();
        }

        if (Sensor.Get_ProjectionMode() == ECk_Camera_ProjectionMode::Orthographic)
        {
            ViewInfo.ProjectionMode = ECameraProjectionMode::Orthographic;
            ViewInfo.OrthoWidth     = Sensor.Get_OrthoWidth();

            // Explicit planes, never the engine's auto-calculated ones: those are derived from the view rect, so
            // the scene's depth range would change with the rendering resolution.
            ViewInfo.bAutoCalculateOrthoPlanes = false;
            ViewInfo.OrthoNearClipPlane        = Sensor.Get_OrthoNearClipPlane();
            ViewInfo.OrthoFarClipPlane         = Sensor.Get_OrthoFarClipPlane();
        }

        InPov._ViewInfo = ViewInfo;

        // Publish the composed pose to the view anchor through the ordinary deferred request path. The
        // enqueue happens in FGroup_Transform_Derived, so the transform-local settle barrier drains it and
        // composes any scene-node children of the anchor before components are pushed.
        if (ck::IsValid(InCamera._ViewAnchor))
        {
            UCk_Utils_Transform_UE::Request_SetTransform(InCamera._ViewAnchor,
                FCk_Request_Transform_SetTransform{FTransform{ViewInfo.Rotation, ViewInfo.Location}}, {});
        }

        // Camera-authoritative control rotation. One-way by design: the camera reads input intention, never control
        // rotation, so this can never feed back into the POV.
        if (InHandle.Get<FFragment_Camera_Params>().Get_DriveControllerControlRotation())
        {
            if (auto* Pawn = Cast<APawn>(Context.OwningActor);
                ck::IsValid(Pawn))
            {
                if (auto* PC = Cast<APlayerController>(Pawn->GetController());
                    ck::IsValid(PC) && PC->IsLocalController())
                {
                    PC->SetControlRotation(ViewInfo.Rotation);
                }
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
