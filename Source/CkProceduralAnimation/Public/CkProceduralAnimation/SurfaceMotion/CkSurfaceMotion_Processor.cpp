#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Processor.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Utils.h"

#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Utils.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control.h"
#include "CkProceduralAnimation/Core/CkProceduralBodyEnvelope.h"
#include "CkProceduralAnimation/Core/CkProceduralHeightPlanning.h"
#include "CkProceduralAnimation/Core/CkProceduralContactStaging.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkJolt/Query/CkJoltQuery_Utils.h"

#include <Engine/World.h>
#include <Misc/ScopeExit.h>
#if WITH_DEV_AUTOMATION_TESTS
#include <CoreGlobals.h>
#endif

CK_REGISTER_PROCESSOR(ck::FProcessor_SurfaceMotion_Setup);
CK_REGISTER_PROCESSOR(ck::FProcessor_SurfaceMotion_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_SurfaceMotion_Update);
CK_REGISTER_PROCESSOR(ck::FProcessor_SurfaceMotion_CancelPendingRequests);

// --------------------------------------------------------------------------------------------------------------------


namespace ck_surface_motion
{
    auto
        Get_IsLive(
            const FCk_Handle& InEntity)
        -> bool
    {
        return ck::IsValid(InEntity) && NOT InEntity.Has<ck::FTag_DestroyEntity_Initiate>()
            && NOT InEntity.Has<ck::FTag_DestroyEntity_EndPlay>() && NOT InEntity.Has<ck::FTag_DestroyEntity_Teardown>()
            && NOT InEntity.Has<ck::FTag_DestroyEntity_Await>() && NOT InEntity.Has<ck::FTag_DestroyEntity_Finalize>();
    }

    auto
        Get_IsNativeVector(
            const FVector& InValue)
        -> bool
    {
        const auto Limit = static_cast<double>(TNumericLimits<float>::Max());
        return NOT InValue.ContainsNaN() && FMath::Abs(InValue.X) <= Limit
            && FMath::Abs(InValue.Y) <= Limit && FMath::Abs(InValue.Z) <= Limit;
    }

    auto
        Get_IsOwnedInWorld(
            const FCk_Handle& InEntity,
            const FCk_Handle& InTransient,
            const UWorld* InWorld,
            TArrayView<const FCk_Handle> InExcluded = {})
        -> bool
    {
        if (NOT Get_IsLive(InEntity) || NOT Get_IsLive(InTransient) || ck::Is_NOT_Valid(InWorld)
            || NOT InTransient.Has<TWeakObjectPtr<UWorld>>()
            || InTransient.Get<TWeakObjectPtr<UWorld>>().Get() != InWorld)
        { return false; }
        const auto Registry = InTransient.Get_RegistryView().Get_RegistryHandle();
        auto Current = InEntity;
        auto Visited = TArray<FCk_Handle, TInlineAllocator<64>>{};
        for (auto Depth = 0; Depth < 64; ++Depth)
        {
            if (NOT Get_IsLive(Current) || Current.Get_RegistryView().Get_RegistryHandle() != Registry
                || InExcluded.Contains(Current) || Visited.Contains(Current))
            { return false; }
            if (Current == InTransient)
            { return true; }
            if (NOT Current.Has<ck::FFragment_LifetimeOwner>())
            { return false; }
            Visited.Add(Current);
            Current = Current.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        }
        return false;
    }

    enum class EEnvelopeScope : uint8
    {
        Simulation,
        SimulationAndPresentation
    };

    auto
        DoGet_WallPolicy(
            ECk_SurfaceMotion_WallPolicy InWallPolicy)
        -> ck::EProceduralSurfaceWallPolicy
    {
        return InWallPolicy == ECk_SurfaceMotion_WallPolicy::Slide ? ck::EProceduralSurfaceWallPolicy::Slide : ck::EProceduralSurfaceWallPolicy::Climb;
    }

    auto
        DoBuild_Settings(
            const ck::FFragment_SurfaceMotion_Params& InParams)
        -> ck::FProceduralSurfaceMotionSettings
    {
        const auto& Contact = InParams.Get_Contact();
        const auto& Movement = InParams.Get_Movement();
        return ck::FProceduralSurfaceMotionSettings{}
            .Set_Clearance(Contact.Get_Clearance())
            .Set_ProbeReach(Contact.Get_ProbeReach())
            .Set_ContactGrace(Contact.Get_ContactGrace())
            .Set_ConfirmAngleDegrees(Contact.Get_ConfirmAngle())
            .Set_ConfirmTime(Contact.Get_ConfirmTime())
            .Set_SurfaceTurnRateDegrees(Movement.Get_SurfaceTurnRate())
            .Set_ClearanceSpeed(Movement.Get_ClearanceSpeed())
            .Set_Gravity(Movement.Get_Gravity())
            .Set_SteerFloor(Movement.Get_SteerFloor())
            .Set_MaxStepHeight(Contact.Get_MaxStepHeight())
            .Set_WallPolicy(DoGet_WallPolicy(Contact.Get_WallPolicy()));
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_SurfaceMotion_Setup::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_SurfaceMotion& InMotionComp,
            FFragment_SurfaceMotion_Support& InSupportComp,
            const FFragment_Transform& InTransform)
        -> void
    {
        const auto Rotation = InTransform.Get_Transform().GetRotation();
        InSupportComp._State.Set_SupportNormal(Rotation.GetAxisZ());
        InSupportComp._State.Set_TravelTangent(Rotation.GetAxisX());
        InSupportComp._SupportReturnObservation.Reset();
        InMotionComp._Direction = Rotation.GetAxisX();

        InHandle.Remove<MarkedDirtyBy>();
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SurfaceMotion_HandleRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SurfaceMotion_Params& InParams,
            FFragment_SurfaceMotion& InMotionComp,
            FFragment_SurfaceMotion_Requests& InRequestsComp)
        -> void
    {
        auto Requests = MoveTemp(InRequestsComp._Requests);
        InRequestsComp._Requests.Reset();
        for (const auto& Request : Requests)
        {
            auto Result = ECk_Request_OperationResult::Failed_Cancelled;
            const auto Guard = MakeCompletionGuard(Request, InHandle, Result);

            if (ck::Is_NOT_Valid(InHandle) || InHandle.Has<FTag_DestroyEntity_Initiate>())
            { continue; }

            InMotionComp._Direction = Request.Get_WorldDirection().GetSafeNormal();
            InMotionComp._Speed = FMath::Min(Request.Get_Speed(), InParams.Get_Movement().Get_MaxSpeed());
            Result = ECk_Request_OperationResult::Succeeded;
        }

        // A completion callback may enqueue more work; do not remove its new queue.
        if (InRequestsComp._Requests.IsEmpty())
        { InHandle.Remove<MarkedDirtyBy>(); }
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SurfaceMotion_CancelPendingRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SurfaceMotion_Requests& InRequestsComp)
        -> void
    {
        request::FireCancelledForPending(InHandle, InRequestsComp.Get_Requests());
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SurfaceMotion_Update::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SurfaceMotion_Params& InParams,
            const FFragment_SurfaceMotion& InMotionComp,
            FFragment_SurfaceMotion_Support& InSupportComp,
            const FFragment_Transform& InTransform)
        -> void
    {
#if WITH_DEV_AUTOMATION_TESTS
        auto* PreparationTest = InHandle.Has<FFragment_ProceduralBodyPreparation_Test>()
            ? &InHandle.Get<FFragment_ProceduralBodyPreparation_Test>() : nullptr;
        if (PreparationTest != nullptr)
        { *PreparationTest = FFragment_ProceduralBodyPreparation_Test{}; PreparationTest->Frame = GFrameCounter; }
        auto* HeightTest = InHandle.Has<FFragment_ProceduralHeightPlanning_Test>()
            ? &InHandle.Get<FFragment_ProceduralHeightPlanning_Test>() : nullptr;
        if (HeightTest != nullptr)
        {
            *HeightTest = FFragment_ProceduralHeightPlanning_Test{};
            HeightTest->Frame = GFrameCounter;
            HeightTest->Solve = InHandle.Has<FFragment_ProceduralGait>()
                ? InHandle.Get<FFragment_ProceduralGait>()._SolveSequence : 0;
        }
#endif
        const auto HadTopAcquisitionAtEntry = InSupportComp._AdaptiveTopAcquisition.IsSet();
        InSupportComp._AdaptiveTopAcquisition.Reset();
        auto& Staging = InSupportComp._ContactStaging;
        auto PreviousSupportReturn = MoveTemp(InSupportComp._SupportReturnObservation);
        InSupportComp._SupportReturnObservation.Reset();
#if WITH_DEV_AUTOMATION_TESTS
        ON_SCOPE_EXIT
        {
            if (InHandle.Has<FFragment_ProceduralBodyPreparation_Test>())
            { InHandle.Get<FFragment_ProceduralBodyPreparation_Test>().SupportPendingRetained = InSupportComp._SupportReturnObservation.IsSet(); }
        };
#endif
        const auto StagingWasDormantAtEntry = Staging._Phase == EProceduralContactStagingPhase::Dormant;
        Staging._PoseRayTickets.Reset();
        Staging._ReservedRays = 0;
        Staging._IntentFrame = 0;
        const auto Dt = InDeltaT.Get_Seconds();
        if (NOT FMath::IsFinite(Dt) || Dt <= 0.0 || NOT FMath::IsFinite(1.0 / Dt))
        { return; }

        auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
        if (ck::Is_NOT_Valid(World))
        { return; }

        auto Body = InTransform.Get_Transform();
        const auto BodyFinite = NOT Body.ContainsNaN();
        CK_ENSURE_IF_NOT(BodyFinite, TEXT("Surface motion [{}] body transform contains NaN; motion is failed."), InHandle)
        {
            InHandle.Add<FFragment_SurfaceMotion_Failure>(ECk_SurfaceMotion_Failure::NanBody);
            return;
        }

        auto Settings = ck_surface_motion::DoBuild_Settings(InParams);
        const auto OriginalBody = Body;
        const auto& QueryFilter = InParams.Get_Contact().Get_QueryFilter();
        const auto PreviousPaceState = InSupportComp._ReachPaceState;
        InSupportComp._ReachPaceScale = 1.0f;
        InSupportComp._ReachPaceState = ECk_SurfaceMotion_ReachPaceState::Free;
        InSupportComp._ReachPaceTrials = 0;
        InSupportComp._ReachPaceRays = 0;
        InSupportComp._AttemptedStanceSpeed = 0.0f;
        InSupportComp._ReachPaceFeedback.Reset();
        InSupportComp._HeightPlanningTrials = 0;
        InSupportComp._HeightPlanningRays = 0;
        InSupportComp._HeightEnvelopeQueries = 0;
        const auto RayCast = [&](const FVector& InStart, const FVector& InEnd) -> FProceduralSurfaceHit
        {
            ++InSupportComp._ReachPaceRays;
            const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(World, InStart, InEnd, QueryFilter);
            return FProceduralSurfaceHit{}
                .Set_Hit(Hit.Get_HasHit())
                .Set_Position(Hit.Get_Position())
                .Set_Normal(Hit.Get_Normal())
                .Set_Fraction(Hit.Get_Fraction());
        };

        // Bounded integration work, with swept fall contact in the core. A hitch advances the entire supplied duration
        // instead of silently discarding time through a delta clamp.
        constexpr auto IntegrationInterval = FCk_Time{0.016};
        const auto Substeps = FMath::Clamp(FMath::CeilToInt(FMath::Min(Dt / IntegrationInterval.Get_Seconds(), 64.0)), 1, 64);
        // The gait publishes its feet plane after this update runs, so the body rides the plane of the previous frame.
        auto FeetSupport = TOptional<FProceduralSurfaceFeetSupport>{};
        // A gait that has latched a failure no longer writes its plane, so the last one it wrote must not hold the body.
        const auto RidesPlantedFeet = InParams.Get_Contact().Get_HeightSource() == ECk_SurfaceMotion_HeightSource::PlantedFeet
            && InHandle.Has<FFragment_ProceduralGait_FeetPlane>()
            && UCk_Utils_ProceduralGait_UE::Get_Status(UCk_Utils_ProceduralGait_UE::Cast(InHandle)) == ECk_ProceduralAnimation_Status::Ready;
        if (RidesPlantedFeet)
        {
            const auto& FeetPlane = InHandle.Get<FFragment_ProceduralGait_FeetPlane>();
            if (FeetPlane.Get_State() != EProceduralGaitFeetPlane::None)
            { FeetSupport = FeetPlane.Get_Support(); }
        }

        // The gait writes this after the previous physics pass. Changes since that solve invalidate the snapshot rather
        // than making SurfaceMotion pace against a stale body transform or leg plant.
        auto ReachAnchors = TArray<FProceduralGaitReachAnchor, TInlineAllocator<64>>{};
        auto PlantedContacts = TArray<FProceduralSurfacePlantedContact, TInlineAllocator<64>>{};
        const auto HasGait = InHandle.Has<FFragment_ProceduralGait>();
        if (HasGait
            && UCk_Utils_ProceduralGait_UE::Get_Status(UCk_Utils_ProceduralGait_UE::Cast(InHandle)) == ECk_ProceduralAnimation_Status::Ready)
        {
            const auto& Gait = InHandle.Get<FFragment_ProceduralGait>();
            const auto& Stance = Gait._ReachStance;
            if (Stance.Get_HasSample() && Stance.Get_SolveSequence() == Gait._SolveSequence
                && Stance.Get_BodyAtSolve().Equals(Body, 1.0e-3))
            {
                for (const auto& Anchor : Stance.Get_Anchors())
                {
                    const auto& Leg = Anchor.Get_Leg();
                    if (NOT procedural_leg::Get_IsLive(Leg) || NOT Leg.Has<FFragment_ProceduralLeg_Params>()
                        || NOT Leg.Has<FFragment_LifetimeOwner>() || NOT procedural_leg::Get_IsSupporting(Leg)
                        || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) != InHandle.ConvertToHandle()
                        || NOT Leg.Has<FFragment_ProceduralLeg>())
                    { continue; }

                    const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                    if (Foot.Get_Phase() == ECk_ProceduralLeg_FootPhase::Planted
                        && Foot.Get_Contact() == ECk_ProceduralLeg_FootContact::Trusted
                        && Foot.Get_Position().Equals(Anchor.Get_FootWorld(), 1.0e-3))
                    {
                        ReachAnchors.Add(Anchor);
                        PlantedContacts.Emplace(Foot.Get_Position(), Foot.Get_Normal());
                    }
                }
            }
        }

        const auto HasPoseFeature = InHandle.Has<FFragment_ProceduralBodyPose>();
        if (InParams.Get_AdaptiveHeight().Get_Enabled() && HasPoseFeature)
        {
            const auto PoseStatus = UCk_Utils_ProceduralBodyPose_UE::Get_Status(UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle));
            if (PoseStatus != ECk_ProceduralAnimation_Status::Ready)
            {
                if (PoseStatus == ECk_ProceduralAnimation_Status::Failed)
                { InHandle.Add<FFragment_SurfaceMotion_Failure>(ECk_SurfaceMotion_Failure::BodyObstructed); }
                return;
            }
        }
        const auto HasPose = HasPoseFeature
            && UCk_Utils_ProceduralBodyPose_UE::Get_Status(UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle))
                == ECk_ProceduralAnimation_Status::Ready;
        const auto PoseOffset = HasPose
            ? UCk_Utils_ProceduralBodyPose_UE::Get_Offset(UCk_Utils_ProceduralBodyPose_UE::CastChecked(InHandle))
            : FTransform::Identity;
        auto CoreAnchors = TArray<FProceduralSurfaceReachPaceAnchor, TInlineAllocator<64>>{};
        CoreAnchors.Reserve(ReachAnchors.Num());
        for (const auto& Anchor : ReachAnchors)
        {
            CoreAnchors.Add(FProceduralSurfaceReachPaceAnchor{
                Anchor.Get_FootWorld(), Anchor.Get_HipLocal(), Anchor.Get_Reach()});
        }
        const auto CorePoseOffset = HasPose ? TOptional<FTransform>{PoseOffset} : TOptional<FTransform>{};
        auto CoreLandings = TArray<FProceduralSurfaceLandingConstraint>{};
        if (HasGait)
        {
            for (const auto& Landing : Get_ProceduralGaitLandingConstraints(
                     UCk_Utils_ProceduralGait_UE::Cast(InHandle), Body, EProceduralGaitLandingConsumer::SurfaceMotion))
            { CoreLandings.Add(Landing.Get_Constraint()); }
        }
        const auto FitsLandings = [&](const FTransform& InCandidate) -> bool
        {
            for (const auto& Landing : CoreLandings)
            {
                if (NOT Get_IsProceduralSurfaceLandingReachValid(InCandidate, CorePoseOffset, Landing))
                { return false; }
            }
            return true;
        };
        const auto PlantedContactView = TArrayView<const FProceduralSurfacePlantedContact>{PlantedContacts};
        const auto CorePlantedContacts = HasGait
            ? TOptional<TArrayView<const FProceduralSurfacePlantedContact>>{PlantedContactView}
            : TOptional<TArrayView<const FProceduralSurfacePlantedContact>>{};

        const auto& Adaptive = InParams.Get_AdaptiveHeight();
        auto ObservedTop = TOptional<FProceduralSurfaceAdaptiveTopReceipt>{};
        constexpr auto MaxEnvelopeQueries = 64;
        constexpr auto MaxPlanningRays = 2048;
        constexpr auto MaxHeightTrials = 4;
        const auto EnvelopeQueriesPerStep = HasPose ? 12 : 6;
        auto EnvelopeQueryLimit = MaxEnvelopeQueries - EnvelopeQueriesPerStep;
        const auto EnvelopeClear = [&](const FTransform& InStart, const FTransform& InEnd, ck_surface_motion::EEnvelopeScope InScope,
            TOptional<FCk_Jolt_HitResult>* OutSweepHit = nullptr,
            TOptional<FProceduralBodyEnvelopeOverlapObservation>* OutOverlap = nullptr,
            bool* OutSimulationRejected = nullptr,
            FProceduralBodyEnvelopeEnclosureObservation* OutEnclosure = nullptr) -> bool
        {
            if (OutSimulationRejected != nullptr)
            { *OutSimulationRejected = false; }
            if (NOT Get_IsProceduralBodyEnvelopePathClear(World, Adaptive, QueryFilter, InStart, InEnd,
                    InSupportComp._HeightEnvelopeQueries, EnvelopeQueryLimit, OutSweepHit, OutOverlap,
                    EProceduralBodyEnvelopeEndpointValidation::ConservativeEnclosure, OutEnclosure))
            {
                if (OutSimulationRejected != nullptr)
                { *OutSimulationRejected = true; }
                return false;
            }
            if (NOT HasPose || InScope == ck_surface_motion::EEnvelopeScope::Simulation)
            { return true; }
            auto PosedEnvelope = Adaptive;
            const auto Turn = InStart.GetRotation().AngularDistance(InEnd.GetRotation());
            const auto Orbit = PoseOffset.GetTranslation().Size() * (1.0 - FMath::Cos(Turn * 0.5));
            PosedEnvelope.Set_BodyHalfExtents(Adaptive.Get_BodyHalfExtents() + FVector{Orbit});
            return Get_IsProceduralBodyEnvelopePathClear(World, PosedEnvelope, QueryFilter,
                PoseOffset * InStart, PoseOffset * InEnd, InSupportComp._HeightEnvelopeQueries, EnvelopeQueryLimit, OutSweepHit, OutOverlap);
        };

        const auto Up = InSupportComp._State.Get_SupportNormal();
        const auto Transient = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InHandle);
        const auto ReadRecoveryAuthority = [&](FProceduralSupportReturnObservation& Out) -> bool
        {
            if (NOT IsInGameThread() || NOT Adaptive.Get_Enabled() || NOT HasGait || NOT ck_surface_motion::Get_IsLive(InHandle)
                || InHandle.Has<FFragment_ProceduralBodyPose>() != HasPoseFeature
                || NOT ck_surface_motion::Get_IsOwnedInWorld(InHandle, Transient, World)
                || NOT InHandle.Has<FFragment_ProceduralGait_Tunables>()
                || InHandle.Has<FTag_ProceduralGait_NeedsSetup>() || InHandle.Has<FFragment_ProceduralGait_Failure>()
                || InHandle.Has<FTag_SurfaceMotion_NeedsSetup>() || InHandle.Has<FFragment_SurfaceMotion_Failure>()
                || UCk_Utils_ProceduralGait_UE::Get_Status(UCk_Utils_ProceduralGait_UE::Cast(InHandle)) != ECk_ProceduralAnimation_Status::Ready
                || GFrameCounter == 0 || NOT Up.IsNormalized() || NOT InMotionComp._Direction.IsNormalized()
                || NOT FMath::IsFinite(InMotionComp._Speed) || InMotionComp._Speed <= 0.0f
                || OriginalBody.ContainsNaN() || NOT ck_surface_motion::Get_IsNativeVector(OriginalBody.GetLocation())
                || NOT OriginalBody.GetRotation().IsNormalized() || NOT OriginalBody.GetScale3D().Equals(FVector::OneVector, 1.0e-4)
                || NOT InTransform.Get_Transform().Equals(OriginalBody, 1.0e-3)
                || NOT StagingWasDormantAtEntry || Staging._Phase != EProceduralContactStagingPhase::Dormant
                || InSupportComp._HeightPlanningRays < 0 || InSupportComp._HeightPlanningRays > MaxPlanningRays
                || InSupportComp._HeightPlanningTrials < 0 || InSupportComp._HeightPlanningTrials > MaxHeightTrials
                || InSupportComp._HeightEnvelopeQueries < 0 || InSupportComp._HeightEnvelopeQueries > MaxEnvelopeQueries)
            { return false; }
            if (HasPose && (NOT InHandle.Has<FFragment_ProceduralBodyPose_Params>()
                || UCk_Utils_ProceduralBodyPose_UE::Get_Status(UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle))
                    != ECk_ProceduralAnimation_Status::Ready
                || NOT UCk_Utils_ProceduralBodyPose_UE::Get_Offset(
                    UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle)).Equals(PoseOffset, 1.0e-6)
                || PoseOffset.ContainsNaN() || NOT PoseOffset.GetRotation().IsNormalized()
                || NOT PoseOffset.GetScale3D().Equals(FVector::OneVector, 1.0e-4)
                || NOT ck_surface_motion::Get_IsNativeVector(PoseOffset.GetLocation())))
            { return false; }
            const auto& Gait = InHandle.Get<FFragment_ProceduralGait>();
            const auto& Plan = Gait._HeightPlanning;
            const auto& Stance = Gait._ReachStance;
            if (Gait._Legs.Num() < 2 || Gait._Legs.Num() > 64 || Gait._Solver.NumLegs() != Gait._Legs.Num()
                || Gait._Solver.IsAirborne() || NOT Gait._ParticipationLeases.IsEmpty() || Gait._UnplantedMask != 0
                || NOT Plan.Get_HasSample() || Plan.Get_Frame() != GFrameCounter - 1 || Plan.Get_SolveSequence() != Gait._SolveSequence
                || NOT Plan.Get_BodyAtSolve().Equals(OriginalBody, 1.0e-3)
                || NOT Plan.Get_Direction().Equals(InMotionComp._Direction, 1.0e-3)
                || NOT FMath::IsNearlyEqual(Plan.Get_Speed(), InMotionComp._Speed)
                || NOT Stance.Get_HasSample() || Stance.Get_SolveSequence() != Gait._SolveSequence
                || NOT Stance.Get_BodyAtSolve().Equals(OriginalBody, 1.0e-3)
                || InSupportComp._EvaluatedFrame != Plan.Get_Frame()
                || NOT InSupportComp._EvaluatedBody.Equals(OriginalBody, 1.0e-3)
                || Stance.Get_Anchors().IsEmpty() || Stance.Get_Anchors().Num() > 64
                || ReachAnchors.Num() != Stance.Get_Anchors().Num()
                || NOT Gait._LandingPlanning.Get_HasSample()
                || Gait._LandingPlanning.Get_Frame() != Plan.Get_Frame()
                || Gait._LandingPlanning.Get_SolveSequence() != Gait._SolveSequence
                || NOT Gait._LandingPlanning.Get_BodyAtSolve().Equals(OriginalBody, 1.0e-3))
            { return false; }
            auto Result = FProceduralSupportReturnObservation{};
            auto Required = TArray<FCk_Handle>{};
            auto Plants = 0;
            for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
            {
                const auto& Leg = Gait._Legs[Index];
                if (NOT ck_surface_motion::Get_IsLive(Leg) || NOT Leg.Has<FFragment_ProceduralLeg>()
                    || Leg.Has<FTag_ProceduralLeg_Detached>() || NOT Leg.Has<FFragment_ProceduralLeg_Params>() || NOT Leg.Has<FFragment_LifetimeOwner>()
                    || Leg.Get_RegistryView().Get_RegistryHandle() != InHandle.Get_RegistryView().Get_RegistryHandle()
                    || Leg.Get<FFragment_LifetimeOwner>().Get_Entity() != InHandle.ConvertToHandle()
                    || Result.Roster.Contains(Leg.ConvertToHandle()))
                { return false; }
                Result.Roster.Add(Leg.ConvertToHandle());
                const auto Bit = uint64{1} << Index;
                if (procedural_leg::Get_IsStructurallyEnabled(Leg))
                { Result.EnabledMask |= Bit; }
                if (NOT procedural_leg::Get_IsSupporting(Leg))
                { continue; }
                if (NOT Gait._Solver.IsLegParticipating(Index)
                    || Gait._Solver.GetLegState(Index).Get_AutomaticRecovery() != EProceduralGaitAutomaticRecovery::None)
                { return false; }
                Result.SupportingMask |= Bit;
                Required.Add(Leg.ConvertToHandle());
                const auto& Swing = Gait._Solver.GetLegState(Index).Get_Swing();
                if (Swing.Get_Active() && NOT Swing.Get_CatchStep() && Swing.Get_TargetTrusted())
                {
                    auto LandingMatches = 0;
                    for (const auto& Landing : Gait._LandingPlanning.Get_Landings())
                    {
                        if (Landing.Get_Leg() == Leg)
                        { ++LandingMatches; }
                    }
                    if (LandingMatches != 1)
                    { return false; }
                }
                const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
                const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                double CheckedReach = 0.0;
                if (Params.Get_Chain().Get_SegmentLengths().IsEmpty() || Params.Get_Chain().Get_SegmentLengths().Num() > 64)
                { return false; }
                for (const auto Length : Params.Get_Chain().Get_SegmentLengths())
                {
                    if (NOT FMath::IsFinite(Length) || Length <= 0.0f)
                    { return false; }
                    CheckedReach += Length;
                }
                if (CheckedReach > TNumericLimits<float>::Max() || NOT ck_surface_motion::Get_IsNativeVector(Params.Get_Placement().Get_HipLocal()))
                { return false; }
                const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
                if (NOT FMath::IsFinite(Reach) || Reach <= 0.0f)
                { return false; }
                if (Foot.Get_Phase() != ECk_ProceduralLeg_FootPhase::Planted || Foot.Get_Contact() != ECk_ProceduralLeg_FootContact::Trusted)
                { continue; }
                if (NOT ck_surface_motion::Get_IsNativeVector(Foot.Get_Position()) || NOT Foot.Get_Normal().IsNormalized())
                { return false; }
                auto Matches = 0;
                for (const auto& Anchor : Stance.Get_Anchors())
                {
                    if (Anchor.Get_Leg() != Leg)
                    { continue; }
                    if (NOT Anchor.Get_FootWorld().Equals(Foot.Get_Position(), 1.0e-3)
                        || NOT Anchor.Get_HipLocal().Equals(Params.Get_Placement().Get_HipLocal(), 1.0e-6) || Anchor.Get_Reach() != Reach)
                    { return false; }
                    ++Matches;
                }
                if (Matches != 1)
                { return false; }
                ++Plants;
            }
            if (Plants != Stance.Get_Anchors().Num() || Result.EnabledMask != Plan.Get_EnabledMask()
                || Result.SupportingMask != Plan.Get_SupportingMask() || Plan.Get_Candidates().Num() > 2 * Gait._Legs.Num())
            { return false; }
            const auto ForceFraction = InHandle.Get<FFragment_ProceduralGait_Tunables>().Get_Step().Get_ForceStepReachFraction();
            if (NOT FMath::IsFinite(ForceFraction) || ForceFraction <= 0.0f)
            { return false; }
            for (const auto& Candidate : Plan.Get_Candidates())
            {
                const auto& Leg = Candidate.Get_Leg();
                const auto LegIndex = Gait._Legs.IndexOfByKey(Leg);
                if (LegIndex < 0 || LegIndex >= 64 || (Result.SupportingMask & (uint64{1} << LegIndex)) == 0)
                { return false; }
                const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
                const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
                if (NOT FMath::IsFinite(Reach) || Reach <= 0.0f
                    || NOT Candidate.Get_FootWorld().Equals(Foot.Get_Position(), 1.0e-3)
                    || NOT Candidate.Get_HipLocal().Equals(Params.Get_Placement().Get_HipLocal(), 1.0e-6)
                    || NOT FMath::IsFinite(Candidate.Get_AdmissionReach()) || NOT FMath::IsFinite(ForceFraction * Reach)
                    || Candidate.Get_Reach() != Reach || Candidate.Get_AdmissionReach() != ForceFraction * Reach
                    || NOT ck_surface_motion::Get_IsNativeVector(Candidate.Get_PointWorld()) || NOT Candidate.Get_NormalWorld().IsNormalized())
                { return false; }
            }
            if (Gait._LandingPlanning.Get_HasSample() && Gait._LandingPlanning.Get_Landings().Num() != CoreLandings.Num())
            { return false; }
            if (NOT FCk_ProceduralArticulation_Control::TryGet_UnclaimedState(InHandle, Required, Result.ClaimsRevision, Result.Parts)
                || (Gait._LandingPlanning.Get_HasSample() && Gait._LandingPlanning.Get_ClaimsRevision() != Result.ClaimsRevision))
            { return false; }
            Result.Root = InHandle.ConvertToHandle(); Result.World = World;
            Result.Body = OriginalBody; Result.Pose = CorePoseOffset; Result.Up = Up;
            Result.Direction = InMotionComp._Direction; Result.Speed = InMotionComp._Speed;
            Result.Frame = GFrameCounter; Result.Solve = Gait._SolveSequence;
            Result.WorldSeconds = World->GetTimeSeconds();
            if (NOT FMath::IsFinite(Result.WorldSeconds))
            { return false; }
            Out = MoveTemp(Result);
            return true;
        };
        auto StagingOwnsMotion = false;
        auto StagingBodyAccepted = false;
        auto StagingObservations = TArray<FProceduralContactStagingBoundary>{};
        const auto ExhaustStaging = [&]()
        {
            Staging._Phase = EProceduralContactStagingPhase::Exhausted;
            Staging._IntentFrame = 0;
            Staging._PoseRayTickets.Reset();
            Staging._ReservedRays = 0;
            Staging._PendingBody.Reset();
        };
        const auto RunStagingPhase = [&]() -> bool
        {
            StagingOwnsMotion = true;
            if (NOT HasGait || NOT Adaptive.Get_Enabled() || NOT InSupportComp._State.Get_Grounded()
                || GFrameCounter == 0 || GFrameCounter == MAX_uint64
                || PreviousPaceState == ECk_SurfaceMotion_ReachPaceState::PhysicalOverride
                || Staging._Owner != InHandle.ConvertToHandle() || Staging._Required.Num() < 1 || Staging._Required.Num() > 2
                || NOT Staging._Direction.Equals(InMotionComp._Direction, 1.0e-3)
                || NOT FMath::IsNearlyEqual(Staging._Speed, InMotionComp._Speed)
                || NOT Staging._Up.Equals(Up, 1.0e-3) || CoreAnchors.IsEmpty())
            { ExhaustStaging(); return false; }
            const auto& Gait = InHandle.Get<FFragment_ProceduralGait>();
            if (Gait._SolveSequence == MAX_uint64)
            { ExhaustStaging(); return false; }
            if (Staging._PendingBody.IsSet())
            {
                if (NOT Body.Equals(Staging._PendingBody.GetValue(), 1.0e-6)
                    || Staging._ProducedFrame < Staging._PendingFrame || Staging._ProducedSolve <= Staging._PendingSolve
                    || Staging._ProducedSolve != Gait._SolveSequence
                    || NOT Staging._ProducedBody.Equals(Body, 1.0e-6)
                    || NOT Gait._ReachStance.Get_HasSample()
                    || NOT Gait._ReachStance.Get_BodyAtSolve().Equals(Body, 1.0e-6))
                { ExhaustStaging(); return false; }
                Staging._PendingBody.Reset();
            }
            if (Staging._Phase == EProceduralContactStagingPhase::Retreat && Body.Equals(Staging._RetreatBody, 1.0e-6))
            {
                Staging._Phase = EProceduralContactStagingPhase::Replant;
                Staging._LastClock = Gait._Solver.GetGaitClock();
                Staging._ClockAtProgress = Staging._LastClock;
                Staging._ClockWrapsWithoutProgress = 0;
                Staging._ClockTravelWithoutProgress = 0.0;
            }
            if (Staging._Phase == EProceduralContactStagingPhase::Replant)
            {
                for (auto Index = 0; Index < Staging._Required.Num(); ++Index)
                {
                    const auto& Required = Staging._Required[Index];
                    if (NOT ck::IsValid(Required._Leg) || NOT Required._Leg.Has<FFragment_ProceduralLeg>())
                    { ExhaustStaging(); return false; }
                    const auto& Foot = Required._Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                    if (Foot.Get_Phase() != ECk_ProceduralLeg_FootPhase::Planted
                        || Foot.Get_Contact() != ECk_ProceduralLeg_FootContact::Trusted
                        || NOT ReachAnchors.ContainsByPredicate([&](const auto& InAnchor)
                            { return InAnchor.Get_Leg() == Required._Leg && InAnchor.Get_FootWorld() == Foot.Get_Position(); }))
                    { Staging._LandedMask &= static_cast<uint8>(~(1u << Index)); }
                }
                if (Staging._LandedMask == static_cast<uint8>((1u << Staging._Required.Num()) - 1u))
                { Staging._Phase = EProceduralContactStagingPhase::AdvanceExit; }
            }
            if (Staging._Phase == EProceduralContactStagingPhase::AdvanceExit && Body.Equals(Staging._ExitBody, 1.0e-6))
            {
                Staging._Phase = EProceduralContactStagingPhase::Dormant;
                Staging._IntentFrame = 0;
                StagingBodyAccepted = true;
                return true;
            }
            if (InSupportComp._HeightPlanningTrials >= MaxHeightTrials)
            { ExhaustStaging(); return false; }
            ++InSupportComp._HeightPlanningTrials;
            Staging._IntentFrame = GFrameCounter;
            Staging._ConsumerSolve = Gait._SolveSequence;
            Staging._IntentBody = Body;
            const auto GaitHandle = UCk_Utils_ProceduralGait_UE::CastChecked(InHandle);
            const auto Holding = Staging._Phase == EProceduralContactStagingPhase::Replant;
            const auto& Goal = Holding ? Body : Staging._Phase == EProceduralContactStagingPhase::Retreat
                ? Staging._RetreatBody : Staging._ExitBody;
            const auto Proofs = Holding ? 1 : 2;
            const auto RequiredRays = 3 * CoreAnchors.Num() * Proofs;
            const auto RequiredEnvelope = (HasPose ? 6 : 3) * Proofs;
            if (InSupportComp._HeightPlanningRays > MaxPlanningRays - RequiredRays
                || InSupportComp._HeightEnvelopeQueries > EnvelopeQueryLimit - RequiredEnvelope
                || NOT FitsLandings(Goal)
                || NOT Get_IsProceduralContactStagingBodyAdmitted(GaitHandle, Goal, CorePoseOffset,
                    InSupportComp._HeightPlanningRays, MaxPlanningRays)
                || NOT EnvelopeClear(Body, Goal, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation))
            { ExhaustStaging(); return false; }
            if (Holding)
            {
                const auto ExitPosed = HasPose ? PoseOffset * Staging._ExitBody : Staging._ExitBody;
                auto CertifiedMask = uint8{0};
                for (auto Index = 0; Index < Staging._Required.Num(); ++Index)
                {
                    const auto& Required = Staging._Required[Index];
                    const auto* Anchor = ReachAnchors.FindByPredicate([&](const auto& InAnchor)
                        { return InAnchor.Get_Leg() == Required._Leg; });
                    if (Anchor == nullptr || Anchor->Get_FootWorld() == Required._OriginalPlant)
                    { continue; }
                    const auto PhysicalReach = static_cast<double>(Anchor->Get_Reach()) + 1.0e-3;
                    if (FVector::Dist(Staging._ExitBody.TransformPosition(Anchor->Get_HipLocal()), Anchor->Get_FootWorld()) <= PhysicalReach
                        && FVector::Dist(ExitPosed.TransformPosition(Anchor->Get_HipLocal()), Anchor->Get_FootWorld()) <= PhysicalReach)
                    { CertifiedMask |= static_cast<uint8>(1u << Index); }
                }
                Staging._LandedMask = CertifiedMask;
                for (auto Index = 0; Index < Staging._Required.Num(); ++Index)
                {
                    const auto Bit = static_cast<uint8>(1u << Index);
                    const auto RosterIndex = Staging._Required[Index]._Index;
                    if ((Staging._StartedMask & Bit) != 0 && (CertifiedMask & Bit) == 0
                        && (RosterIndex < 0 || RosterIndex >= Gait._Solver.NumLegs()
                            || (NOT Gait._Solver.GetLegState(RosterIndex).Get_Swing().Get_Active()
                                && NOT Gait._Solver.HasPendingStep(RosterIndex))))
                    { ExhaustStaging(); return false; }
                }
                if (InSupportComp._HeightPlanningRays <= MaxPlanningRays - Staging._Required.Num())
                {
                    for (const auto& Required : Staging._Required)
                    { Staging._PoseRayTickets.Add(Required._Leg); }
                    Staging._ReservedRays = Staging._PoseRayTickets.Num();
                }
                StagingBodyAccepted = true;
                return true;
            }
            const auto StepDistance = FMath::Min(static_cast<double>(InMotionComp._Speed),
                static_cast<double>(Settings.Get_ClearanceSpeed())) * Dt;
            const auto Remaining = FVector::DotProduct(Goal.GetLocation() - Body.GetLocation(), Staging._Direction);
            if (NOT FMath::IsFinite(StepDistance) || StepDistance <= 0.0 || NOT FMath::IsFinite(Remaining))
            { ExhaustStaging(); return false; }
            auto Increment = Body;
            Increment.AddToTranslation(Staging._Direction * FMath::Clamp(Remaining, -StepDistance, StepDistance));
            if (NOT FitsLandings(Increment)
                || NOT Get_IsProceduralContactStagingBodyAdmitted(GaitHandle, Increment, CorePoseOffset,
                    InSupportComp._HeightPlanningRays, MaxPlanningRays)
                || NOT EnvelopeClear(Body, Increment, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation))
            { ExhaustStaging(); return false; }
            Body = Increment;
            Staging._PendingBody = Body;
            Staging._PendingFrame = GFrameCounter;
            Staging._PendingSolve = Gait._SolveSequence;
            Staging._IntentBody = Body;
            StagingBodyAccepted = true;
            return true;
        };
        if (Staging._Phase != EProceduralContactStagingPhase::Dormant
            && (Staging._Owner != InHandle.ConvertToHandle()
                || NOT Staging._Direction.Equals(InMotionComp._Direction, 1.0e-3)
                || NOT FMath::IsNearlyEqual(Staging._Speed, InMotionComp._Speed)
                || (Staging._Phase == EProceduralContactStagingPhase::Exhausted
                    && FVector::DotProduct(Body.GetLocation() - Staging._ExitBody.GetLocation(), Staging._Direction) > 0.0)))
        {
            const auto Episode = Staging._Episode;
            Staging = FProceduralContactStagingState{};
            Staging._Episode = Episode;
        }
        if (NOT Adaptive.Get_Enabled() && Staging._Phase != EProceduralContactStagingPhase::Dormant)
        {
            const auto Episode = Staging._Episode;
            Staging = FProceduralContactStagingState{};
            Staging._Episode = Episode;
        }
        const auto StagingWasActive = Staging._Phase == EProceduralContactStagingPhase::Retreat
            || Staging._Phase == EProceduralContactStagingPhase::Replant
            || Staging._Phase == EProceduralContactStagingPhase::AdvanceExit;
        const auto StartStaging = [&]() -> bool
        {
            if (Staging._Phase != EProceduralContactStagingPhase::Dormant || NOT HasGait
                || InSupportComp._HeightPlanningTrials >= MaxHeightTrials || Staging._Episode == MAX_uint64)
            { return false; }
            const auto GaitHandle = UCk_Utils_ProceduralGait_UE::CastChecked(InHandle);
            const auto Legs = Get_ProceduralContactStagingLegs(GaitHandle, StagingObservations);
            for (const auto& Observation : StagingObservations)
            {
                if (NOT ck::IsValid(Observation._Attribution) || NOT Observation._FaceNormal.Equals(InMotionComp._Direction, 0.0))
                { continue; }
                auto Input = FProceduralContactStagingInput{};
                Input._Body = Body;
                Input._PosedBody = HasPose ? TOptional<FTransform>{PoseOffset * Body} : TOptional<FTransform>{};
                Input._Up = Up;
                Input._Forward = InMotionComp._Direction;
                Input._BodyHalfExtents = Adaptive.Get_BodyHalfExtents();
                Input._CollisionMargin = Adaptive.Get_CollisionMargin();
                Input._Face = {Observation._FacePoint, Observation._FaceNormal};
                Input._Legs = MakeArrayView(Legs);
                Input._Landings = MakeArrayView(CoreLandings);
                const auto Plan = Get_ProceduralContactStagingPlan(Input);
                if (NOT Plan.IsSet())
                { return false; }
                const auto& Gait = InHandle.Get<FFragment_ProceduralGait>();
                for (auto Index = 0; Index < Plan->_RequiredCount; ++Index)
                {
                    const auto& Required = Plan->_Required[Index];
                    if (NOT Gait._Legs.IsValidIndex(Required._RosterIndex))
                    { return false; }
                    const auto* NativeTop = StagingObservations.FindByPredicate([&](const auto& InObservation)
                    {
                        return InObservation._Leg == Gait._Legs[Required._RosterIndex]
                            && InObservation._Plant == Required._OriginalPlant
                            && InObservation._Attribution == Observation._Attribution
                            && InObservation._TopNormal == Up;
                    });
                    if (NativeTop == nullptr)
                    { return false; }
                }
                Staging._Owner = InHandle.ConvertToHandle();
                ++Staging._Episode;
                Staging._Phase = EProceduralContactStagingPhase::Retreat;
                Staging._Direction = InMotionComp._Direction;
                Staging._Up = Up;
                Staging._Speed = InMotionComp._Speed;
                Staging._BlockedBody = Body;
                Staging._RetreatBody = Plan->_RetreatBody;
                Staging._ExitBody = Plan->_ExitBody;
                Staging._Required.Reset();
                Staging._LandedMask = 0;
                Staging._StartedMask = 0;
                for (auto Index = 0; Index < Plan->_RequiredCount; ++Index)
                {
                    const auto& Required = Plan->_Required[Index];
                    if (NOT Gait._Legs.IsValidIndex(Required._RosterIndex))
                    { ExhaustStaging(); return false; }
                    Staging._Required.Add({Gait._Legs[Required._RosterIndex], Required._RosterIndex,
                        Required._OriginalPlant, Required._ProposedQuery});
                }
                return RunStagingPhase();
            }
            return false;
        };
        if (NOT Adaptive.Get_Enabled() || NOT InSupportComp._State.Get_Grounded()
            || CoreAnchors.IsEmpty() || FVector::DotProduct(Up, InSupportComp._AdaptiveHeightUp) < 0.999)
        {
            InSupportComp._AdaptiveHeightActive = false;
            InSupportComp._AdaptiveHeightPreparing = false;
        }
        if (Adaptive.Get_Enabled())
        {
            if (NOT EnvelopeClear(Body, Body, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation))
            {
                InHandle.Add<FFragment_SurfaceMotion_Failure>(ECk_SurfaceMotion_Failure::BodyObstructed);
                return;
            }
            if (StagingWasActive)
            { RunStagingPhase(); }
            auto ObstacleHeightDelta = TOptional<double>{};
            if (NOT StagingWasActive && InSupportComp._HeightObstructionPoint.IsSet() && InSupportComp._State.Get_Grounded()
                && PreviousPaceState != ECk_SurfaceMotion_ReachPaceState::PhysicalOverride
                && InSupportComp._HeightObstructionBody.Equals(Body, 1.0e-3)
                && InSupportComp._HeightObstructionDirection.Equals(InMotionComp._Direction, 1.0e-3)
                && InSupportComp._HeightObstructionUp.Equals(Up, 1.0e-3)
                && GFrameCounter >= InSupportComp._HeightObstructionFrame
                && GFrameCounter - InSupportComp._HeightObstructionFrame <= 1
                && FVector::DotProduct(InSupportComp._HeightObstructionNormal, InMotionComp._Direction) < -KINDA_SMALL_NUMBER)
            {
                const auto Inset = InSupportComp._HeightObstructionPoint.GetValue()
                    - InSupportComp._HeightObstructionNormal * Adaptive.Get_CollisionMargin();
                const auto Top = UCk_Utils_JoltQuery_UE::Get_RayCast(World, Inset + Up * Settings.Get_ProbeReach(),
                    Inset - Up * Settings.Get_ProbeReach(), QueryFilter);
                ++InSupportComp._HeightPlanningRays;
                if (Top.Get_HasHit() && Top.Get_Fraction() > 0.0f && Top.Get_Fraction() <= 1.0f
                    && NOT Top.Get_Position().ContainsNaN() && NOT Top.Get_Normal().ContainsNaN()
                    && FVector::DotProduct(Top.Get_Normal(), Up) > KINDA_SMALL_NUMBER)
                {
                    if (HasGait && FMath::IsFinite(Top.Get_Fraction()) && Top.Get_Normal().IsNormalized()
                        && FMath::IsFinite(InMotionComp._Speed) && InMotionComp._Speed > 0.0f
                        && NOT InMotionComp._Direction.ContainsNaN() && InMotionComp._Direction.IsNormalized()
                        && NOT Up.ContainsNaN() && Up.IsNormalized())
                    {
                        ObservedTop = FProceduralSurfaceAdaptiveTopReceipt{Top.Get_Position(), Top.Get_Normal(),
                            Body, InMotionComp._Direction, Up, InMotionComp._Speed, GFrameCounter};
                    }
                    const auto& Extents = Adaptive.Get_BodyHalfExtents();
                    const auto Rotation = Body.GetRotation();
                    const auto Radius = FMath::Abs(FVector::DotProduct(Rotation.GetAxisX(), Up)) * Extents.X
                        + FMath::Abs(FVector::DotProduct(Rotation.GetAxisY(), Up)) * Extents.Y
                        + FMath::Abs(FVector::DotProduct(Rotation.GetAxisZ(), Up)) * Extents.Z;
                    constexpr auto ConvexPadding = 0.05;
                    constexpr auto HeightGoalSeparation = 0.1;
                    const auto Delta = FVector::DotProduct(Top.Get_Position() - Body.GetLocation(), Up)
                        + Radius + Adaptive.Get_CollisionMargin() + ConvexPadding + HeightGoalSeparation;
                    if (Delta > 1.0e-3 && Delta <= Settings.Get_Clearance())
                    { ObstacleHeightDelta = Delta; }
                }
            }
            InSupportComp._HeightObstructionPoint.Reset();
            const auto CanPlan = HasGait && NOT CoreAnchors.IsEmpty() && InSupportComp._State.Get_Grounded()
                && (InSupportComp._AdaptiveHeightActive || PreviousPaceState == ECk_SurfaceMotion_ReachPaceState::Pacing
                    || PreviousPaceState == ECk_SurfaceMotion_ReachPaceState::Blocked || ObstacleHeightDelta.IsSet());
            if (CanPlan && NOT StagingWasActive)
            {
                auto FoundGoal = false;
                const auto& Gait = InHandle.Get<FFragment_ProceduralGait>();
                const auto& Planning = Gait._HeightPlanning;
                if (Planning.Get_HasSample() && Planning.Get_SolveSequence() == Gait._SolveSequence
                    && Planning.Get_BodyAtSolve().Equals(Body, 1.0e-3)
                    && Planning.Get_Direction().Equals(InMotionComp._Direction, 1.0e-3)
                    && FMath::IsNearlyEqual(Planning.Get_Speed(), InMotionComp._Speed))
                {
                    const auto NominalDelta = Settings.Get_Clearance()
                        - FVector::DotProduct(Body.GetLocation() - InSupportComp._State.Get_SupportPoint(), Up);
                    const auto GaitHandle = UCk_Utils_ProceduralGait_UE::CastChecked(InHandle);
                    const auto HasRelief = Planning.Get_Candidates().ContainsByPredicate(
                        [](const auto& InCandidate) { return InCandidate.Get_RequiresRelief(); });
                    const auto WholeBodyGoal = ObstacleHeightDelta.IsSet() || (InSupportComp._AdaptiveHeightActive && NOT HasRelief);
                    auto TriedWholeBodyGoal = false;
                    auto TriedRecoveryRetry = false;
                    auto TriedPreparatoryBody = false;
                    auto TriedNativePlaneClip = false;
                    auto RecoveryBoundaryConsumed = false;
                    enum class EHeightGoalRoute : uint8
                    {
                        Ordinary,
                        Deferred,
                        Interior,
                        Cooperative
                    };
                    struct FHeightGoalAttempt
                    {
                        const FProceduralGaitHeightCandidate* Candidate;
                        double Delta;
                        EProceduralHeightPlanStage Stage;
                        EHeightGoalRoute Route;
                        EProceduralHeightSeedFailure SeedFailure = EProceduralHeightSeedFailure::None;
                        bool Completed = false;
                        bool Accepted = false;
                    };
                    auto Attempts = TArray<FHeightGoalAttempt, TInlineAllocator<4>>{};
                    auto DeferredCandidates = TArray<const FProceduralGaitHeightCandidate*, TInlineAllocator<8>>{};
                    const auto RunGoal = [&](const FProceduralGaitHeightCandidate& InCandidate, double InDelta,
                        TOptional<FProceduralGaitHeightRetryWitness>* OutVisibilityRetry,
                        EProceduralHeightSeedFailure* OutSeedFailure) -> bool
                    {
                        auto GoalBody = Body;
                        GoalBody.AddToTranslation(Up * InDelta);
                        if (NOT FitsLandings(GoalBody))
                        { return false; }
                        auto Continuation = FProceduralHeightRecoveryContinuation{};
                        auto* ContinuationPointer = static_cast<FProceduralHeightRecoveryContinuation*>(nullptr);
                        const auto EntryRays = InSupportComp._HeightPlanningRays;
                        const auto CandidateCount = Planning.Get_Candidates().Num();
                        const auto AnchorCount = CoreAnchors.Num();
                        const auto ExtraDemand = CandidateCount > 0 && CandidateCount <= 128 && AnchorCount > 0 && AnchorCount <= 64
                            ? 9 * CandidateCount + 8 + 3 * AnchorCount + 1 + 3 * AnchorCount : MaxPlanningRays + 1;
                        const auto RequiredEnvelopes = HasPose ? 15 : 9;
                        auto Authority = FProceduralSupportReturnObservation{};
                        if (WholeBodyGoal && ObstacleHeightDelta.IsSet() && ObservedTop.IsSet()
                            && ObservedTop->Get_Frame() == GFrameCounter && ObservedTop->Get_Body().Equals(OriginalBody, 1.0e-3)
                            && ObservedTop->Get_Up().Equals(Up, 1.0e-3)
                            && InDelta > 0.0 && InDelta == ObstacleHeightDelta.GetValue()
                            && NOT HasRelief && NOT RecoveryBoundaryConsumed && Attempts.Num() == 1
                            && InSupportComp._HeightPlanningTrials == 1
                            && PreviousPaceState == ECk_SurfaceMotion_ReachPaceState::Blocked
                            && CandidateCount > 0 && CandidateCount <= 128 && AnchorCount > 0 && AnchorCount <= 64
                            && EntryRays >= 0 && EntryRays <= MaxPlanningRays && ExtraDemand <= MaxPlanningRays - EntryRays
                            && InSupportComp._HeightEnvelopeQueries <= EnvelopeQueryLimit - RequiredEnvelopes
                            && Body.Equals(OriginalBody, 1.0e-3) && ReadRecoveryAuthority(Authority))
                        {
                            Continuation._IsCurrent = [&]() -> bool
                            {
                                auto Current = FProceduralSupportReturnObservation{};
                                return ReadRecoveryAuthority(Current) && Current.ClaimsRevision == Authority.ClaimsRevision
                                    && Current.Roster == Authority.Roster && Current.Parts == Authority.Parts
                                    && Current.Solve == Authority.Solve && Body.Equals(OriginalBody, 1.0e-3);
                            };
                            Continuation._TryConsume = [&](int32 InCurrentRays) -> bool
                            {
                                if (RecoveryBoundaryConsumed || InCurrentRays < EntryRays || InCurrentRays > MaxPlanningRays
                                    || InCurrentRays - EntryRays > 9 * CandidateCount
                                    || ExtraDemand - (InCurrentRays - EntryRays) > MaxPlanningRays - InCurrentRays
                                    || InSupportComp._HeightEnvelopeQueries > EnvelopeQueryLimit - RequiredEnvelopes
                                    || NOT Continuation._IsCurrent())
                                { return false; }
                                RecoveryBoundaryConsumed = true;
                                return true;
                            };
#if WITH_DEV_AUTOMATION_TESTS
                            Continuation._TestReceipt = HeightTest != nullptr && HeightTest->Count < 4
                                ? &HeightTest->Attempts[HeightTest->Count].Recovery : nullptr;
                            if (PreparationTest != nullptr)
                            { PreparationTest->ObstacleContinuationIssued = true; }
#endif
                            ContinuationPointer = &Continuation;
                        }
                        if (NOT Get_IsProceduralHeightPlanAdmitted(GaitHandle, GoalBody, CorePoseOffset, InCandidate,
                                InSupportComp._HeightPlanningRays, MaxPlanningRays,
                                WholeBodyGoal ? EProceduralHeightPlanStage::Recovery : EProceduralHeightPlanStage::Landing,
                                OutVisibilityRetry, &StagingObservations, OutSeedFailure, ContinuationPointer))
                        { return false; }
                        auto FullSweepHit = TOptional<FCk_Jolt_HitResult>{};
                        if (NOT EnvelopeClear(Body, GoalBody, ck_surface_motion::EEnvelopeScope::Simulation, &FullSweepHit))
                        {
                            if (WholeBodyGoal || NOT InCandidate.Get_RequiresRelief() || NOT FMath::IsFinite(InDelta)
                                || InDelta >= 0.0 || NOT FullSweepHit.IsSet() || TriedPreparatoryBody
                                || PreviousPaceState == ECk_SurfaceMotion_ReachPaceState::PhysicalOverride)
                            { return false; }
                            TriedPreparatoryBody = true;
                            const auto MaxStep = Settings.Get_ClearanceSpeed() * Dt;
                            if (NOT FMath::IsFinite(MaxStep) || MaxStep <= 0.0)
                            { return false; }
                            const auto PartialDelta = FMath::Clamp(InDelta, -MaxStep, MaxStep);
                            if (NOT (InDelta < PartialDelta && PartialDelta < 0.0))
                            { return false; }
                            auto Partial = Body;
                            Partial.AddToTranslation(Up * PartialDelta);
                            auto PartialSweepHit = TOptional<FCk_Jolt_HitResult>{};
                            auto PartialEnclosure = FProceduralBodyEnvelopeEnclosureObservation{};
                            auto PartialSimulationRejected = false;
                            if (NOT FitsLandings(Partial)
                                || NOT Get_IsProceduralHeightPlanAdmitted(GaitHandle, Partial, CorePoseOffset, InCandidate,
                                    InSupportComp._HeightPlanningRays, MaxPlanningRays, EProceduralHeightPlanStage::PreparatoryBody,
                                    nullptr, &StagingObservations))
                            { return false; }
                            if (NOT EnvelopeClear(Body, Partial, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation,
                                    &PartialSweepHit, nullptr, &PartialSimulationRejected, &PartialEnclosure))
                            {
                                if (PartialSimulationRejected && PartialSweepHit.IsSet())
                                {
                                    StartStaging();
                                    if (StagingOwnsMotion)
                                    {
                                        FoundGoal = true; return true;
                                    }
                                    if (NOT TriedNativePlaneClip && StagingWasDormantAtEntry && NOT StagingWasActive
                                        && Staging._Phase == EProceduralContactStagingPhase::Dormant)
                                    {
                                        TriedNativePlaneClip = true;
#if WITH_DEV_AUTOMATION_TESTS
                                        if (PreparationTest != nullptr)
                                        {
                                            PreparationTest->ClipConsumed = true;
                                            PreparationTest->ClipTrial = InSupportComp._HeightPlanningTrials;
                                            PreparationTest->ClipFullGoal = GoalBody;
                                            PreparationTest->ClipFullHit = FullSweepHit.GetValue();
                                            PreparationTest->ClipPartialHit = PartialSweepHit.GetValue();
                                            PreparationTest->ClipEntryDormant = StagingWasDormantAtEntry;
                                            PreparationTest->ClipStagingDormant = true;
                                            PreparationTest->ClipBefore = Body;
                                            PreparationTest->ClipRejected = Partial;
                                            PreparationTest->ClipRaysBefore = InSupportComp._HeightPlanningRays;
                                            PreparationTest->ClipEnvelopesBefore = InSupportComp._HeightEnvelopeQueries;
                                        }
                                        ON_SCOPE_EXIT
                                        {
                                            if (PreparationTest != nullptr)
                                            {
                                                PreparationTest->ClipRaysAfter = InSupportComp._HeightPlanningRays;
                                                PreparationTest->ClipEnvelopesAfter = InSupportComp._HeightEnvelopeQueries;
                                            }
                                        };
#endif
                                        const auto& Hit = PartialSweepHit.GetValue();
                                        const auto HitEntity = Hit.Get_Entity();
                                        const auto ClipEnvelopeQueries = HasPose ? 6 : 3;
                                        if (InSupportComp._HeightEnvelopeQueries > EnvelopeQueryLimit - ClipEnvelopeQueries
                                            || NOT FullSweepHit->Get_HasHit() || NOT FMath::IsFinite(FullSweepHit->Get_Fraction())
                                            || FullSweepHit->Get_Fraction() <= 0.0f || FullSweepHit->Get_Fraction() > 1.0f
                                            || NOT ck_surface_motion::Get_IsLive(HitEntity)
                                            || NOT ck_surface_motion::Get_IsOwnedInWorld(HitEntity, Transient, World))
                                        { return false; }
                                        auto ClipAuthority = FProceduralSupportReturnObservation{};
                                        if (NOT ReadRecoveryAuthority(ClipAuthority))
                                        { return false; }
                                        auto Excluded = ClipAuthority.Parts;
                                        Excluded.Append(ClipAuthority.Roster);
                                        Excluded.Add(InHandle.ConvertToHandle());
                                        if (NOT ck_surface_motion::Get_IsOwnedInWorld(HitEntity, Transient, World, Excluded))
                                        { return false; }
                                        const auto Clipped = TryGet_ProceduralBodyPlaneClip(PartialEnclosure, Body, Partial, Hit, Up);
                                        if (NOT Clipped.IsSet() || NOT FitsLandings(Clipped.GetValue()))
                                        { return false; }
#if WITH_DEV_AUTOMATION_TESTS
                                        if (PreparationTest != nullptr)
                                        { PreparationTest->ClipCandidate = Clipped.GetValue(); }
#endif
                                        if (NOT Get_IsProceduralHeightPlanAdmitted(GaitHandle, Clipped.GetValue(), CorePoseOffset,
                                                InCandidate, InSupportComp._HeightPlanningRays, MaxPlanningRays,
                                                EProceduralHeightPlanStage::PreparatoryBody)
                                            || NOT EnvelopeClear(Body, Clipped.GetValue(), ck_surface_motion::EEnvelopeScope::SimulationAndPresentation))
                                        { return false; }
                                        auto CurrentClipAuthority = FProceduralSupportReturnObservation{};
                                        if (NOT ReadRecoveryAuthority(CurrentClipAuthority)
                                            || CurrentClipAuthority.ClaimsRevision != ClipAuthority.ClaimsRevision
                                            || CurrentClipAuthority.Roster != ClipAuthority.Roster || CurrentClipAuthority.Parts != ClipAuthority.Parts)
                                        { return false; }
                                        Body = Clipped.GetValue();
                                        InSupportComp._AdaptiveHeightActive = true;
                                        InSupportComp._AdaptiveHeightUp = Up;
                                        InSupportComp._AdaptiveHeightPreparing = true;
                                        InSupportComp._AdaptiveHeightGoal = Body.GetLocation();
                                        FoundGoal = true;
#if WITH_DEV_AUTOMATION_TESTS
                                        if (PreparationTest != nullptr)
                                        { PreparationTest->ClipAccepted = true; }
#endif
                                        return true;
                                    }
                                }
                                return false;
                            }
                            Body = Partial;
                            InSupportComp._AdaptiveHeightActive = true;
                            InSupportComp._AdaptiveHeightUp = Up;
                            InSupportComp._AdaptiveHeightPreparing = true;
                            InSupportComp._AdaptiveHeightGoal = Body.GetLocation();
                            FoundGoal = true;
                            return true;
                        }
                        if (NOT EnvelopeClear(Body, GoalBody, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation))
                        {
                            if (ContinuationPointer != nullptr && RecoveryBoundaryConsumed && NOT Continuation._IsCurrent())
                            { return false; }
                            InSupportComp._AdaptiveHeightActive = true;
                            InSupportComp._AdaptiveHeightUp = Up;
                            InSupportComp._AdaptiveHeightPreparing = true;
                            InSupportComp._AdaptiveHeightGoal = Body.GetLocation();
                            FoundGoal = true;
                            return true;
                        }
                        auto Partial = Body;
                        Partial.AddToTranslation(Up * FMath::Clamp(InDelta,
                            -Settings.Get_ClearanceSpeed() * Dt, Settings.Get_ClearanceSpeed() * Dt));
                        if (NOT FitsLandings(Partial))
                        { return false; }
                        if ((NOT Partial.Equals(GoalBody, 1.0e-6)
                                && NOT Get_IsProceduralHeightPlanAdmitted(GaitHandle, Partial, CorePoseOffset, InCandidate,
                                    InSupportComp._HeightPlanningRays, MaxPlanningRays, EProceduralHeightPlanStage::Transit))
                            || NOT EnvelopeClear(Body, Partial, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation))
                        { return false; }
                        if (ContinuationPointer != nullptr && RecoveryBoundaryConsumed && NOT Continuation._IsCurrent())
                        { return false; }
                        Body = Partial;
                        InSupportComp._AdaptiveHeightActive = true;
                        InSupportComp._AdaptiveHeightUp = Up;
                        InSupportComp._AdaptiveHeightPreparing = false;
                        InSupportComp._AdaptiveHeightGoal = Body.GetLocation();
                        FoundGoal = true;
                        if (WholeBodyGoal && NOT ObstacleHeightDelta.IsSet()
                            && FMath::Abs(InDelta - NominalDelta) <= 1.0e-3 && FMath::Abs(InDelta) <= Settings.Get_ClearanceSpeed() * Dt)
                        { InSupportComp._AdaptiveHeightActive = false; }
                        return true;
                    };
                    const auto TryGoal = [&](const FProceduralGaitHeightCandidate& InCandidate, double InDelta,
                        EHeightGoalRoute InRoute,
                        TOptional<FProceduralGaitHeightRetryWitness>* OutVisibilityRetry = nullptr) -> bool
                    {
                        if (OutVisibilityRetry != nullptr)
                        { OutVisibilityRetry->Reset(); }
                        ++InSupportComp._HeightPlanningTrials;
                        TriedWholeBodyGoal |= WholeBodyGoal;
                        const auto AttemptIndex = Attempts.Add(FHeightGoalAttempt{&InCandidate, InDelta,
                            WholeBodyGoal ? EProceduralHeightPlanStage::Recovery : EProceduralHeightPlanStage::Landing,
                            InRoute});
                        auto SeedFailure = EProceduralHeightSeedFailure::None;
#if WITH_DEV_AUTOMATION_TESTS
                        const auto BodyBeforeAttempt = Body;
                        if (HeightTest != nullptr && WholeBodyGoal && HeightTest->Count < 4)
                        { HeightTest->ActiveRecoveryReceipt = &HeightTest->Attempts[HeightTest->Count].Recovery; }
                        ON_SCOPE_EXIT { if (HeightTest != nullptr) { HeightTest->ActiveRecoveryReceipt = nullptr; } };
#endif
                        const auto Accepted = RunGoal(InCandidate, InDelta, OutVisibilityRetry, &SeedFailure);
                        auto& Attempt = Attempts[AttemptIndex];
                        Attempt.SeedFailure = Accepted ? EProceduralHeightSeedFailure::None : SeedFailure;
                        Attempt.Completed = true;
                        Attempt.Accepted = Accepted;
#if WITH_DEV_AUTOMATION_TESTS
                        if (HeightTest != nullptr)
                        {
                            if (HeightTest->Count >= 4)
                            { HeightTest->Overflow = true; }
                            else
                            {
                                auto& Receipt = HeightTest->Attempts[HeightTest->Count++];
                                Receipt.Candidate = InCandidate;
                                Receipt.Delta = InDelta;
                                Receipt.Route = static_cast<uint8>(InRoute);
                                Receipt.Stage = Attempt.Stage;
                                Receipt.SeedFailure = Attempt.SeedFailure;
                                Receipt.Accepted = Accepted;
                                Receipt.Completed = Attempt.Completed;
                                Receipt.WitnessRequested = OutVisibilityRetry != nullptr;
                                Receipt.Witness = OutVisibilityRetry != nullptr ? *OutVisibilityRetry
                                    : TOptional<FProceduralGaitHeightRetryWitness>{};
                                Receipt.Rays = InSupportComp._HeightPlanningRays;
                                Receipt.Envelopes = InSupportComp._HeightEnvelopeQueries;
                                Receipt.Trials = InSupportComp._HeightPlanningTrials;
                                Receipt.BodyBefore = BodyBeforeAttempt;
                                Receipt.BodyAfter = Body;
                                Receipt.Preparing = InSupportComp._AdaptiveHeightPreparing;
                            }
                        }
#endif
                        return Accepted;
                    };
                    const auto RecoveryRetryDelta = [&](const FProceduralGaitHeightCandidate& InCandidate,
                        double InFailedDelta) -> TOptional<double>
                    {
                        if (TriedRecoveryRetry || NOT WholeBodyGoal || NOT InSupportComp._AdaptiveHeightActive
                            || HasRelief || ObstacleHeightDelta.IsSet() || StagingWasActive
                            || Staging._Phase != EProceduralContactStagingPhase::Dormant
                            || PreviousPaceState != ECk_SurfaceMotion_ReachPaceState::Blocked
                            || NOT InSupportComp._State.Get_Grounded() || InSupportComp._HeightPlanningTrials >= MaxHeightTrials
                            || NOT FMath::IsFinite(InFailedDelta) || InFailedDelta <= 1.0e-3 || Attempts.Num() != 1)
                        { return {}; }
                        const auto First = Attempts[0];
                        if (NOT First.Completed || First.Accepted || First.Stage != EProceduralHeightPlanStage::Recovery
                            || First.Route != EHeightGoalRoute::Ordinary || First.Candidate != &InCandidate
                            || First.Delta != InFailedDelta
                            || First.SeedFailure != EProceduralHeightSeedFailure::TrustedDownFinalForceReject)
                        { return {}; }
                        if (ck::Is_NOT_Valid(InHandle) || NOT InHandle.Has<FFragment_ProceduralGait_Tunables>()
                            || UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle) != World
                            || UCk_Utils_ProceduralGait_UE::Get_Status(GaitHandle) != ECk_ProceduralAnimation_Status::Ready
                            || GFrameCounter == 0 || NOT Planning.Get_HasSample()
                            || Planning.Get_Frame() != GFrameCounter - 1
                            || Planning.Get_SolveSequence() != Gait._SolveSequence
                            || NOT Planning.Get_BodyAtSolve().Equals(Body, 1.0e-3)
                            || NOT Planning.Get_Direction().Equals(InMotionComp._Direction, 1.0e-3)
                            || NOT FMath::IsNearlyEqual(Planning.Get_Speed(), InMotionComp._Speed)
                            || NOT InTransform.Get_Transform().Equals(Body, 1.0e-3)
                            || Gait._Legs.Num() < 2 || Gait._Legs.Num() > 64
                            || Gait._Solver.NumLegs() != Gait._Legs.Num() || Gait._Solver.IsAirborne()
                            || Planning.Get_Candidates().Num() > 2 * Gait._Legs.Num())
                        { return {}; }
                        if (HasPose && (UCk_Utils_ProceduralBodyPose_UE::Get_Status(
                                UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle)) != ECk_ProceduralAnimation_Status::Ready
                            || NOT UCk_Utils_ProceduralBodyPose_UE::Get_Offset(
                                UCk_Utils_ProceduralBodyPose_UE::CastChecked(InHandle)).Equals(PoseOffset, 1.0e-3)))
                        { return {}; }
                        const auto& Stance = Gait._ReachStance;
                        if (NOT Stance.Get_HasSample() || Stance.Get_SolveSequence() != Gait._SolveSequence
                            || NOT Stance.Get_BodyAtSolve().Equals(Body, 1.0e-3) || Stance.Get_Anchors().Num() > 64
                            || CoreAnchors.Num() != Stance.Get_Anchors().Num())
                        { return {}; }
                        auto EnabledMask = uint64{0};
                        auto SupportingMask = uint64{0};
                        auto Plants = 0;
                        const auto& Step = InHandle.Get<FFragment_ProceduralGait_Tunables>().Get_Step();
                        auto Geometry = TArray<FProceduralHeightRecoveryLegGeometry, TInlineAllocator<64>>{};
                        for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
                        {
                            const auto& Leg = Gait._Legs[Index];
                            if (NOT procedural_leg::Get_IsLive(Leg) || NOT Leg.Has<FFragment_ProceduralLeg>()
                                || NOT Leg.Has<FFragment_ProceduralLeg_Params>()
                                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) != InHandle.ConvertToHandle())
                            { return {}; }
                            for (auto Previous = 0; Previous < Index; ++Previous)
                            {
                                if (Gait._Legs[Previous] == Leg)
                                { return {}; }
                            }
                            const auto Bit = uint64{1} << Index;
                            if (procedural_leg::Get_IsStructurallyEnabled(Leg))
                            { EnabledMask |= Bit; }
                            if (NOT procedural_leg::Get_IsSupporting(Leg))
                            { continue; }
                            SupportingMask |= Bit;
                            if (NOT Gait._Solver.IsLegParticipating(Index))
                            { return {}; }
                            const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
                            const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                            const auto HipLocal = Params.Get_Placement().Get_HipLocal();
                            const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
                            const auto ForceReach = Reach * Step.Get_ForceStepReachFraction();
                            if (HipLocal.ContainsNaN() || Foot.Get_Position().ContainsNaN()
                                || NOT FMath::IsFinite(Reach) || Reach <= 0.0f
                                || NOT FMath::IsFinite(ForceReach) || ForceReach <= 0.0f)
                            { return {}; }
                            if (Foot.Get_Phase() == ECk_ProceduralLeg_FootPhase::Planted
                                && Foot.Get_Contact() == ECk_ProceduralLeg_FootContact::Trusted)
                            {
                                ++Plants;
                                auto MatchingAnchors = 0;
                                for (const auto& Anchor : Stance.Get_Anchors())
                                {
                                    if (Anchor.Get_Leg() != Leg)
                                    { continue; }
                                    if (NOT Anchor.Get_FootWorld().Equals(Foot.Get_Position(), 1.0e-3)
                                        || NOT Anchor.Get_HipLocal().Equals(HipLocal, 1.0e-3) || Anchor.Get_Reach() != Reach)
                                    { return {}; }
                                    ++MatchingAnchors;
                                }
                                if (MatchingAnchors != 1)
                                { return {}; }
                            }
                            auto FirstPoint = TOptional<FVector>{};
                            auto SecondPoint = TOptional<FVector>{};
                            for (const auto& Observed : Planning.Get_Candidates())
                            {
                                if (Observed.Get_Leg() != Leg)
                                { continue; }
                                if (NOT Observed.Get_FootWorld().Equals(Foot.Get_Position(), 1.0e-3)
                                    || NOT Observed.Get_HipLocal().Equals(HipLocal, 1.0e-3)
                                    || Observed.Get_Reach() != Reach || Observed.Get_AdmissionReach() != ForceReach
                                    || Observed.Get_PointWorld().ContainsNaN() || NOT Observed.Get_NormalWorld().IsNormalized())
                                { return {}; }
                                if (NOT FirstPoint.IsSet())
                                { FirstPoint = Observed.Get_PointWorld(); }
                                else if (NOT SecondPoint.IsSet())
                                { SecondPoint = Observed.Get_PointWorld(); }
                                else
                                { return {}; }
                            }
                            if (NOT FirstPoint.IsSet())
                            { return {}; }
                            Geometry.Emplace(HipLocal, FirstPoint.GetValue(), SecondPoint, ForceReach);
                        }
                        if (EnabledMask != Planning.Get_EnabledMask() || SupportingMask != Planning.Get_SupportingMask()
                            || Plants != Stance.Get_Anchors().Num())
                        { return {}; }
                        for (const auto& Observed : Planning.Get_Candidates())
                        {
                            const auto& Leg = Observed.Get_Leg();
                            if (ck::Is_NOT_Valid(Leg) || NOT Gait._Legs.Contains(Leg))
                            { return {}; }
                            const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
                            const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                            const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
                            if (NOT Observed.Get_FootWorld().Equals(Foot.Get_Position(), 1.0e-3)
                                || NOT Observed.Get_HipLocal().Equals(Params.Get_Placement().Get_HipLocal(), 1.0e-3)
                                || Observed.Get_Reach() != Reach
                                || Observed.Get_AdmissionReach() != Reach * Step.Get_ForceStepReachFraction()
                                || Observed.Get_PointWorld().ContainsNaN() || NOT Observed.Get_NormalWorld().IsNormalized())
                            { return {}; }
                        }
                        return Get_ProceduralHeightRecoveryRetryDelta(Body, CorePoseOffset, Up,
                            TArrayView<const FProceduralSurfaceReachPaceAnchor>{CoreAnchors},
                            TArrayView<const FProceduralHeightRecoveryLegGeometry>{Geometry}, Settings.Get_Clearance(), InFailedDelta);
                    };
                    const auto RetainedPlaneLower = NOT WholeBodyGoal
                        ? Get_ProceduralHeightRetainedPlaneLower(Body, CorePoseOffset, Up,
                            TArrayView<const FProceduralSurfaceReachPaceAnchor>{CoreAnchors})
                        : TOptional<double>{};
                    auto CooperativeConsumed = false;
                    struct FHeightGoalOffer
                    {
                        const FProceduralGaitHeightCandidate* Candidate;
                        double Delta;
                    };
                    struct FHeightGoalDispatch
                    {
                        bool Accepted;
                        bool ReplacedOriginal;
                        const FProceduralGaitHeightCandidate* Candidate;
                        double Delta;
                        EHeightGoalRoute Route;
                        bool PlanningConsumed;
                    };
                    const auto VisibilityRetryMatches = [&](const TOptional<FProceduralGaitHeightRetryWitness>& InVisibilityRetry,
                        const FProceduralGaitHeightCandidate& InCandidate, double InDelta) -> bool
                    {
                        if (NOT InVisibilityRetry.IsSet())
                        { return false; }
                        const auto& Witness = InVisibilityRetry.GetValue();
                        const auto& OriginalCandidate = Witness.Get_Candidate();
                        auto RejectedBody = Body;
                        RejectedBody.AddToTranslation(Up * InDelta);
                        if (Witness.Get_Frame() != GFrameCounter || Witness.Get_SolveSequence() != Gait._SolveSequence
                            || NOT Witness.Get_BodyAtSolve().Equals(Body, 1.0e-6)
                            || NOT Witness.Get_RejectedBody().Equals(RejectedBody, 1.0e-6)
                            || NOT Witness.Get_SupportUp().Equals(Up, 1.0e-6)
                            || Witness.Get_PoseOffset().IsSet() != CorePoseOffset.IsSet()
                            || (CorePoseOffset.IsSet() && NOT Witness.Get_PoseOffset()->Equals(CorePoseOffset.GetValue(), 1.0e-6))
                            || OriginalCandidate.Get_Leg() != InCandidate.Get_Leg()
                            || OriginalCandidate.Get_HipLocal() != InCandidate.Get_HipLocal()
                            || OriginalCandidate.Get_FootWorld() != InCandidate.Get_FootWorld()
                            || OriginalCandidate.Get_PointWorld() != InCandidate.Get_PointWorld()
                            || OriginalCandidate.Get_NormalWorld() != InCandidate.Get_NormalWorld()
                            || OriginalCandidate.Get_Reach() != InCandidate.Get_Reach()
                            || OriginalCandidate.Get_AdmissionReach() != InCandidate.Get_AdmissionReach()
                            || OriginalCandidate.Get_RequiresRelief() != InCandidate.Get_RequiresRelief()
                            || Witness.Get_Cast() != EProceduralFootholdCast::Down
                            || Witness.Get_PointWorld().ContainsNaN() || NOT Witness.Get_NormalWorld().IsNormalized())
                        { return false; }
                        return true;
                    };
                    const auto SelectCooperative = [&](int32 InRequiredHistory) -> TOptional<FHeightGoalOffer>
                    {
                        if (NOT Adaptive.Get_Enabled() || NOT CanPlan
                            || PreviousPaceState != ECk_SurfaceMotion_ReachPaceState::Blocked
                            || NOT HasRelief || WholeBodyGoal || ObstacleHeightDelta.IsSet()
                            || Staging._Phase != EProceduralContactStagingPhase::Dormant
                            || (InRequiredHistory != 2 && InRequiredHistory != MaxHeightTrials - 1)
                            || InSupportComp._HeightPlanningTrials != InRequiredHistory
                            || Attempts.Num() != InRequiredHistory || CooperativeConsumed
                            || NOT RetainedPlaneLower.IsSet() || NOT FMath::IsFinite(RetainedPlaneLower.GetValue()))
                        { return {}; }
                        for (const auto& Attempt : Attempts)
                        {
                            if (NOT Attempt.Completed || Attempt.Accepted
                                || Attempt.Stage != EProceduralHeightPlanStage::Landing
                                || (Attempt.SeedFailure != EProceduralHeightSeedFailure::TrustedInitialDownClampReprobeMiss
                                    && Attempt.SeedFailure != EProceduralHeightSeedFailure::TrustedDownFinalForceReject))
                            { return {}; }
                        }
                        const auto LegCount = Gait._Legs.Num();
                        const auto CandidateCount = Planning.Get_Candidates().Num();
                        if (LegCount < 2 || LegCount > 64 || CandidateCount < 0 || CandidateCount > 2 * LegCount)
                        { return {}; }
                        const auto AnchorView = TArrayView<const FProceduralSurfaceReachPaceAnchor>{CoreAnchors};
                        for (auto Index = 0; Index < FMath::Min(8, CandidateCount); ++Index)
                        {
                            const auto& Candidate = Planning.Get_Candidates()[Index];
                            const auto& Leg = Candidate.Get_Leg();
                            if (Candidate.Get_RequiresRelief() || ck::Is_NOT_Valid(Leg)
                                || NOT Gait._Legs.Contains(Leg) || NOT Leg.Has<FFragment_LifetimeOwner>()
                                || NOT Leg.Has<FFragment_ProceduralLeg>() || NOT Leg.Has<FFragment_ProceduralLeg_Params>()
                                || NOT procedural_leg::Get_IsSupporting(Leg)
                                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) != InHandle.ConvertToHandle()
                                || Candidate.Get_HipLocal().ContainsNaN() || Candidate.Get_FootWorld().ContainsNaN()
                                || Candidate.Get_PointWorld().ContainsNaN() || Candidate.Get_NormalWorld().ContainsNaN()
                                || NOT Candidate.Get_NormalWorld().IsNormalized()
                                || NOT FMath::IsFinite(Candidate.Get_Reach()) || Candidate.Get_Reach() <= 0.0f
                                || NOT FMath::IsFinite(Candidate.Get_AdmissionReach()) || Candidate.Get_AdmissionReach() <= 0.0f)
                            { continue; }
                            const auto Interval = Get_ProceduralHeightPlanningInterval(Body, CorePoseOffset, Up, AnchorView,
                                Candidate.Get_HipLocal(), Candidate.Get_PointWorld(), Candidate.Get_AdmissionReach(), Settings.Get_Clearance());
                            const auto Delta = Get_ProceduralHeightReprojectionDelta(Body, CorePoseOffset, Up, AnchorView,
                                Candidate.Get_HipLocal(), Candidate.Get_PointWorld(), Candidate.Get_AdmissionReach(), Settings.Get_Clearance());
                            if (NOT Interval.IsSet() || NOT FMath::IsFinite(Interval->X) || NOT FMath::IsFinite(Interval->Y)
                                || Interval->X > Interval->Y || NOT Delta.IsSet() || NOT FMath::IsFinite(Delta.GetValue())
                                || Delta.GetValue() >= -1.0e-3 || Delta.GetValue() < Interval->X || Delta.GetValue() > Interval->Y
                                || Delta.GetValue() < RetainedPlaneLower.GetValue())
                            { continue; }
                            const auto AlreadyAttempted = Attempts.ContainsByPredicate([&](const auto& InAttempt)
                            {
                                return InAttempt.Candidate->Get_Leg() == Candidate.Get_Leg()
                                    && InAttempt.Candidate->Get_PointWorld() == Candidate.Get_PointWorld()
                                    && InAttempt.Candidate->Get_NormalWorld() == Candidate.Get_NormalWorld()
                                    && InAttempt.Delta == Delta.GetValue();
                            });
                            if (NOT AlreadyAttempted)
                            {
                                return FHeightGoalOffer{&Candidate, Delta.GetValue()};
                            }
                        }
                        return {};
                    };
                    const auto DispatchGoal = [&](const FProceduralGaitHeightCandidate& InCandidate, double InDelta,
                        EHeightGoalRoute InRoute,
                        TOptional<FProceduralGaitHeightRetryWitness>* OutVisibilityRetry = nullptr) -> FHeightGoalDispatch
                    {
                        const auto ThirdCooperative = SelectCooperative(2);
                        if (ThirdCooperative.IsSet())
                        {
                            if (OutVisibilityRetry != nullptr)
                            { OutVisibilityRetry->Reset(); }
                            const auto& Offer = ThirdCooperative.GetValue();
                            CooperativeConsumed = true;
#if WITH_DEV_AUTOMATION_TESTS
                            if (HeightTest != nullptr)
                            {
                                HeightTest->CooperativeThirdOffered = true;
                                HeightTest->PlanningConsumed = true;
                            }
#endif
                            auto VisibilityRetry = TOptional<FProceduralGaitHeightRetryWitness>{};
                            const auto Accepted = TryGoal(*Offer.Candidate, Offer.Delta,
                                EHeightGoalRoute::Cooperative, &VisibilityRetry);
                            const auto FinishDispatch = [&](bool InAccepted, const FProceduralGaitHeightCandidate& InActualCandidate,
                                double InActualDelta, EHeightGoalRoute InActualRoute, bool InReplacedOriginal) -> FHeightGoalDispatch
                            {
                                return {InAccepted, InReplacedOriginal, &InActualCandidate,
                                    InActualDelta, InActualRoute, true};
                            };
                            if (Accepted)
                            { return FinishDispatch(true, *Offer.Candidate, Offer.Delta, EHeightGoalRoute::Cooperative, true); }

                            auto Interior = TOptional<double>{};
                            if (VisibilityRetryMatches(VisibilityRetry, *Offer.Candidate, Offer.Delta))
                            {
#if WITH_DEV_AUTOMATION_TESTS
                                if (HeightTest != nullptr)
                                { HeightTest->ValidatedWitnessTrial = InSupportComp._HeightPlanningTrials; }
#endif
                                Interior = Get_ProceduralHeightInteriorRetryDelta(Body, CorePoseOffset, Up,
                                    TArrayView<const FProceduralSurfaceReachPaceAnchor>{CoreAnchors},
                                    Offer.Candidate->Get_HipLocal(), VisibilityRetry->Get_PointWorld(),
                                    Offer.Candidate->Get_AdmissionReach(), Settings.Get_Clearance(), Offer.Delta);
                            }
                            const auto AlreadyAttempted = Interior.IsSet() && Attempts.ContainsByPredicate([&](const auto& InAttempt)
                            {
                                return InAttempt.Candidate->Get_Leg() == Offer.Candidate->Get_Leg()
                                    && InAttempt.Candidate->Get_PointWorld() == Offer.Candidate->Get_PointWorld()
                                    && InAttempt.Candidate->Get_NormalWorld() == Offer.Candidate->Get_NormalWorld()
                                    && InAttempt.Delta == Interior.GetValue();
                            });
                            if (Interior.IsSet() && NOT AlreadyAttempted)
                            {
#if WITH_DEV_AUTOMATION_TESTS
                                if (HeightTest != nullptr)
                                { HeightTest->InteriorOffered = true; }
#endif
                                const auto InteriorAccepted = TryGoal(*Offer.Candidate, Interior.GetValue(), EHeightGoalRoute::Interior);
                                return FinishDispatch(InteriorAccepted, *Offer.Candidate, Interior.GetValue(),
                                    EHeightGoalRoute::Interior, true);
                            }
                            const auto OriginalAccepted = TryGoal(InCandidate, InDelta, InRoute);
                            return FinishDispatch(OriginalAccepted, InCandidate, InDelta, InRoute, false);
                        }
                        const auto Cooperative = SelectCooperative(MaxHeightTrials - 1);
                        if (Cooperative.IsSet())
                        {
                            if (OutVisibilityRetry != nullptr)
                            { OutVisibilityRetry->Reset(); }
                            const auto& Offer = Cooperative.GetValue();
                            const auto Accepted = TryGoal(*Offer.Candidate, Offer.Delta, EHeightGoalRoute::Cooperative);
                            CooperativeConsumed = true;
#if WITH_DEV_AUTOMATION_TESTS
                            if (HeightTest != nullptr)
                            { HeightTest->PlanningConsumed = true; }
#endif
                            return {Accepted, true, Offer.Candidate, Offer.Delta, EHeightGoalRoute::Cooperative, true};
                        }
                        const auto Accepted = TryGoal(InCandidate, InDelta, InRoute, OutVisibilityRetry);
                        return {Accepted, false, &InCandidate, InDelta, InRoute, false};
                    };
                    for (const auto& Candidate : Planning.Get_Candidates())
                    {
                        if (InSupportComp._HeightPlanningTrials >= MaxHeightTrials)
                        { break; }
                        if (NOT WholeBodyGoal && NOT Candidate.Get_RequiresRelief())
                        { continue; }
                        const auto AnchorView = TArrayView<const FProceduralSurfaceReachPaceAnchor>{CoreAnchors};
                        const auto Interval = WholeBodyGoal
                            ? Get_ProceduralHeightStanceInterval(Body, CorePoseOffset, Up, AnchorView, Settings.Get_Clearance())
                            : Get_ProceduralHeightPlanningInterval(Body, CorePoseOffset, Up, AnchorView, Candidate.Get_HipLocal(),
                                Candidate.Get_PointWorld(), Candidate.Get_AdmissionReach(), Settings.Get_Clearance());
                        if (NOT Interval.IsSet())
                        {
                            if (NOT WholeBodyGoal)
                            { DeferredCandidates.Add(&Candidate); }
                            continue;
                        }
                        const auto Span = Interval->Y - Interval->X;
                        const auto RequestedDelta = ObstacleHeightDelta.IsSet()
                            ? ObstacleHeightDelta.GetValue() : NominalDelta;
                        const auto Desired = FMath::Clamp(RequestedDelta, Interval->X, Interval->Y);
                        const auto AllowClampedWholeBodyRecovery = WholeBodyGoal && InSupportComp._AdaptiveHeightActive
                            && NOT HasRelief && NOT ObstacleHeightDelta.IsSet()
                            && FMath::IsFinite(RequestedDelta) && FMath::IsFinite(Desired)
                            && RequestedDelta > Interval->Y && Desired > 1.0e-3;
                        if (WholeBodyGoal && (TriedWholeBodyGoal
                            || (FMath::Abs(Desired - RequestedDelta) > 1.0e-3 && NOT AllowClampedWholeBodyRecovery)))
                        { continue; }
                        const auto NominalRecovery = InSupportComp._AdaptiveHeightActive && FMath::Abs(NominalDelta) <= 1.0e-3;
                        if (FMath::Abs(Desired) <= 1.0e-3 && NOT NominalRecovery)
                        { continue; }
                        const double GoalDeltas[] = {Desired,
                            FMath::Clamp(Desired - FMath::Min(1.0, Span * 0.5), Interval->X, Interval->Y)};
                        for (const auto Delta : GoalDeltas)
                        {
                            if (WholeBodyGoal && TriedWholeBodyGoal)
                            { break; }
                            if (InSupportComp._HeightPlanningTrials >= MaxHeightTrials)
                            { break; }
                            if (NOT WholeBodyGoal && Delta < 0.0)
                            {
                                if (NOT RetainedPlaneLower.IsSet())
                                { continue; }
                                // Boundary goals need the deferred path's native retry witness. Do not spend a
                                // normal slot or record an attempt that would suppress that same boundary replay.
                                if (Delta <= RetainedPlaneLower.GetValue())
                                {
                                    DeferredCandidates.AddUnique(&Candidate);
                                    continue;
                                }
                            }
                            const auto Dispatch = DispatchGoal(Candidate, Delta, EHeightGoalRoute::Ordinary);
                            if (Dispatch.Accepted || Dispatch.PlanningConsumed)
                            { break; }
                            const auto RecoveryDelta = RecoveryRetryDelta(Candidate, Delta);
                            if (RecoveryDelta.IsSet())
                            {
                                TriedRecoveryRetry = true;
                                TryGoal(Candidate, RecoveryDelta.GetValue(), EHeightGoalRoute::Ordinary);
                                break;
                            }
                        }
                        if (FoundGoal || CooperativeConsumed || TriedRecoveryRetry)
                        { break; }
                    }
                    // A fixed seed may fail its interval or lie at/below a retained-foot plane even though native
                    // down replay can reproject it. Keep eligible ordinary proposals first and share their goal budget.
                    if (NOT FoundGoal && NOT WholeBodyGoal && NOT CooperativeConsumed)
                    {
                        const auto AnchorView = TArrayView<const FProceduralSurfaceReachPaceAnchor>{CoreAnchors};
                        auto TriedVisibilityInterior = false;
                        for (const auto* Candidate : DeferredCandidates)
                        {
                            if (InSupportComp._HeightPlanningTrials >= MaxHeightTrials)
                            { break; }
                            const auto Delta = Get_ProceduralHeightReprojectionDelta(Body, CorePoseOffset, Up, AnchorView,
                                Candidate->Get_HipLocal(), Candidate->Get_PointWorld(), Candidate->Get_AdmissionReach(),
                                Settings.Get_Clearance());
                            if (NOT Delta.IsSet() || NOT FMath::IsFinite(Delta.GetValue()) || FMath::Abs(Delta.GetValue()) <= 1.0e-3)
                            { continue; }
                            const auto AlreadyAttempted = Attempts.ContainsByPredicate([&](const auto& InAttempt)
                            {
                                return InAttempt.Candidate->Get_Leg() == Candidate->Get_Leg()
                                    && InAttempt.Candidate->Get_PointWorld() == Candidate->Get_PointWorld()
                                    && InAttempt.Candidate->Get_NormalWorld() == Candidate->Get_NormalWorld()
                                    && InAttempt.Delta == Delta.GetValue();
                            });
                            if (AlreadyAttempted)
                            { continue; }
                            auto VisibilityRetry = TOptional<FProceduralGaitHeightRetryWitness>{};
                            const auto Dispatch = DispatchGoal(*Candidate, Delta.GetValue(), EHeightGoalRoute::Deferred, &VisibilityRetry);
                            if (Dispatch.Accepted || Dispatch.PlanningConsumed)
                            { break; }
                            if (TriedVisibilityInterior || NOT VisibilityRetry.IsSet()
                                || InSupportComp._HeightPlanningTrials >= MaxHeightTrials || Delta.GetValue() >= -1.0e-3)
                            { continue; }
                            // Only this failed native call may seed one interior proposal. The original sampled
                            // candidate still owns provenance and is replayed through every unchanged admission.
                            if (NOT VisibilityRetryMatches(VisibilityRetry, *Candidate, Delta.GetValue()))
                            { continue; }
                            const auto& Witness = VisibilityRetry.GetValue();
                            const auto Interior = Get_ProceduralHeightInteriorRetryDelta(Body, CorePoseOffset, Up, AnchorView,
                                Candidate->Get_HipLocal(), Witness.Get_PointWorld(), Candidate->Get_AdmissionReach(),
                                Settings.Get_Clearance(), Delta.GetValue());
                            if (NOT Interior.IsSet())
                            { continue; }
                            const auto AlreadyTriedInterior = Attempts.ContainsByPredicate([&](const auto& InAttempt)
                            {
                                return InAttempt.Candidate->Get_Leg() == Candidate->Get_Leg()
                                    && InAttempt.Candidate->Get_PointWorld() == Candidate->Get_PointWorld()
                                    && InAttempt.Candidate->Get_NormalWorld() == Candidate->Get_NormalWorld()
                                    && InAttempt.Delta == Interior.GetValue();
                            });
                            if (AlreadyTriedInterior)
                            { continue; }
                            TriedVisibilityInterior = true;
                            const auto InteriorDispatch = DispatchGoal(*Candidate, Interior.GetValue(), EHeightGoalRoute::Interior);
                            if (InteriorDispatch.Accepted || InteriorDispatch.PlanningConsumed)
                            { break; }
                        }
                    }
                }
            }
            if (InSupportComp._AdaptiveHeightActive)
            { Settings.Set_HeightPlane(TOptional<FVector>{InSupportComp._AdaptiveHeightGoal}); }
        }
        if (StagingOwnsMotion)
        {
            InSupportComp._ReachPaceState = ECk_SurfaceMotion_ReachPaceState::ContactStaging;
            InSupportComp._ReachPaceScale = 0.0f;
            InSupportComp._ReachPaceFeedback.Reset();
            InSupportComp._AttemptedStanceSpeed = 0.0f;
            if (StagingBodyAccepted)
            {
                InSupportComp._State.Set_Velocity((Body.GetLocation() - OriginalBody.GetLocation()) / Dt);
                InSupportComp._EvaluatedBody = Body;
                InSupportComp._EvaluatedFrame = GFrameCounter;
                auto TransformHandle = UCk_Utils_Transform_UE::CastChecked(InHandle);
                UCk_Utils_Transform_UE::Request_SetTransform(TransformHandle, FCk_Request_Transform_SetTransform{Body}, {});
            }
            return;
        }
        const auto PlannedHeightVelocity = (Body.GetLocation() - OriginalBody.GetLocation()) / Dt;
        EnvelopeQueryLimit = MaxEnvelopeQueries;
        const auto AcceptedSubsteps = Adaptive.Get_Enabled()
            ? FMath::Min(Substeps, FMath::Max(1,
                (MaxEnvelopeQueries - InSupportComp._HeightEnvelopeQueries) / EnvelopeQueriesPerStep))
            : Substeps;
        const auto Step = FCk_Time{Dt / AcceptedSubsteps};
        auto AttemptedStanceDistance = 0.0;
        const auto TrySupportReturn = [&](const FTransform& InBefore, const FProceduralSurfaceMotionState& InBeforeState,
            const FTransform& InAttempted, const FCk_Jolt_HitResult& InHit) -> bool
        {
            if (PreviousPaceState != ECk_SurfaceMotion_ReachPaceState::Blocked || NOT Adaptive.Get_Enabled()
                || InSupportComp._HeightPlanningTrials != 0 || InSupportComp._AdaptiveHeightActive
                || InSupportComp._AdaptiveHeightPreparing || Settings.Get_HeightPlane().IsSet()
                || StagingOwnsMotion || NOT StagingWasDormantAtEntry || HadTopAcquisitionAtEntry
                || ObservedTop.IsSet() || InSupportComp._HeightObstructionPoint.IsSet()
                || NOT Body.Equals(InBefore, 1.0e-3) || NOT InBefore.Equals(OriginalBody, 1.0e-3)
                || NOT InSupportComp._State.Get_SupportNormal().Equals(Up, 1.0e-3)
                || NOT InBeforeState.Get_Grounded() || NOT InBeforeState.Get_ContactTrusted()
                || NOT InHit.Get_HasHit() || NOT FMath::IsFinite(InHit.Get_Fraction())
                || InHit.Get_Fraction() <= 0.0f || InHit.Get_Fraction() > 1.0f
                || NOT InHit.Get_Normal().IsNormalized() || NOT ck_surface_motion::Get_IsNativeVector(InHit.Get_Position())
                || FVector::DotProduct(InHit.Get_Normal(), InMotionComp._Direction) < -KINDA_SMALL_NUMBER
                || NOT (FVector::DotProduct(InAttempted.GetLocation() - InBefore.GetLocation(), InHit.Get_Normal()) < 0.0)
                || NOT (FVector::DotProduct(InSupportComp._State.Get_StepForward(), InHit.Get_Normal()) < 0.0))
            { return false; }
            auto Authority = FProceduralSupportReturnObservation{};
            if (NOT ReadRecoveryAuthority(Authority))
            { return false; }
            const auto& Planning = InHandle.Get<FFragment_ProceduralGait>()._HeightPlanning;
            if (Planning.Get_Candidates().ContainsByPredicate([](const auto& InCandidate) { return InCandidate.Get_RequiresRelief(); }))
            { return false; }
            auto Excluded = Authority.Parts;
            Excluded.Append(Authority.Roster); Excluded.Add(InHandle.ConvertToHandle());
            const auto Collider = InHit.Get_Entity();
            if (NOT ck_surface_motion::Get_IsOwnedInWorld(Collider, Transient, World, Excluded))
            { return false; }
            const auto Contact = FProceduralSurfaceHit{}.Set_Hit(true).Set_Position(InHit.Get_Position())
                .Set_Normal(InHit.Get_Normal()).Set_Fraction(InHit.Get_Fraction());
            if (NOT Get_IsProceduralSupportReturnContactEligible(Settings, FeetSupport, PlantedContactView, Body,
                    InSupportComp._State, Contact))
            { return false; }
            const auto RemainingReserve = (AcceptedSubsteps - 1) * EnvelopeQueriesPerStep;
            const auto ProofQueries = HasPose ? 6 : 3;
#if WITH_DEV_AUTOMATION_TESTS
            if (PreparationTest != nullptr)
            {
                PreparationTest->SupportBudgetEnvelopesBefore = InSupportComp._HeightEnvelopeQueries;
                PreparationTest->SupportRemainingSubsteps = AcceptedSubsteps - 1;
                PreparationTest->SupportProofQueries = ProofQueries;
                PreparationTest->SupportAcceptedSubsteps = AcceptedSubsteps;
            }
#endif
            if (RemainingReserve > MaxEnvelopeQueries - ProofQueries
                || InSupportComp._HeightEnvelopeQueries > MaxEnvelopeQueries - RemainingReserve - ProofQueries
                || InSupportComp._HeightPlanningRays >= MaxPlanningRays)
            {
#if WITH_DEV_AUTOMATION_TESTS
                if (PreparationTest != nullptr) { PreparationTest->SupportBudgetDeclined = true; }
#endif
                return false;
            }
            ++InSupportComp._HeightPlanningTrials;
#if WITH_DEV_AUTOMATION_TESTS
            if (PreparationTest != nullptr)
            {
                PreparationTest->SupportEligible = true; PreparationTest->SupportBefore = Body;
                PreparationTest->SupportAfter = Body; PreparationTest->SupportNormal = Contact.Get_Normal();
                PreparationTest->SupportStateBefore = InSupportComp._State;
                PreparationTest->SupportStateAfter = InSupportComp._State;
            }
            ON_SCOPE_EXIT
            {
                if (PreparationTest != nullptr)
                {
                    PreparationTest->SupportTrials = InSupportComp._HeightPlanningTrials;
                    PreparationTest->SupportRays = InSupportComp._HeightPlanningRays;
                    PreparationTest->SupportEnvelopes = InSupportComp._HeightEnvelopeQueries;
                }
            };
#endif
            const auto RoomRay = [&](const FVector& InStart, const FVector& InEnd) -> FProceduralSurfaceHit
            {
                ++InSupportComp._HeightPlanningRays;
#if WITH_DEV_AUTOMATION_TESTS
                if (PreparationTest != nullptr) { ++PreparationTest->SupportRoomCalls; }
#endif
                const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(World, InStart, InEnd, QueryFilter);
                return FProceduralSurfaceHit{}.Set_Hit(Hit.Get_HasHit()).Set_Position(Hit.Get_Position())
                    .Set_Normal(Hit.Get_Normal()).Set_Fraction(Hit.Get_Fraction());
            };
            if (NOT Get_HasProceduralSupportReturnRoom(Settings, RoomRay, Contact, Up))
            { return false; }
#if WITH_DEV_AUTOMATION_TESTS
            if (PreparationTest != nullptr) { PreparationTest->SupportRoom = true; }
#endif
            const auto SameOptionalPose = [](const TOptional<FTransform>& InLeft, const TOptional<FTransform>& InRight) -> bool
            {
                return InLeft.IsSet() == InRight.IsSet() && (NOT InLeft.IsSet() || InLeft->Equals(InRight.GetValue(), 1.0e-6));
            };
            const auto SameBinding = [&](const FProceduralSupportReturnObservation& InPrevious,
                const FProceduralSupportReturnObservation& InCurrent) -> bool
            {
                return InPrevious.Root == InCurrent.Root && InPrevious.World.Get() == World
                    && InPrevious.Roster == InCurrent.Roster && InPrevious.Parts == InCurrent.Parts
                    && InPrevious.ClaimsRevision == InCurrent.ClaimsRevision
                    && InPrevious.Body.Equals(InCurrent.Body, 1.0e-3) && SameOptionalPose(InPrevious.Pose, InCurrent.Pose)
                    && InPrevious.Up.Equals(InCurrent.Up, 1.0e-3) && InPrevious.Direction.Equals(InCurrent.Direction, 1.0e-3)
                    && InPrevious.Speed == InCurrent.Speed && InPrevious.EnabledMask == InCurrent.EnabledMask
                    && InPrevious.SupportingMask == InCurrent.SupportingMask;
            };
            Authority.Collider = Collider; Authority.Normal = Contact.Get_Normal();
            const auto Seconds = Step.Get_Seconds();
            if (NOT FMath::IsFinite(Seconds) || Seconds <= 0.0 || Seconds > Dt || NOT FMath::IsFinite(1.0 / Seconds))
            { return false; }
            auto Seen = 0.0;
            if (PreviousSupportReturn.IsSet())
            {
                const auto& Previous = PreviousSupportReturn.GetValue();
                if (SameBinding(Previous, Authority) && Previous.Collider == Collider
                    && Previous.Frame < GFrameCounter && GFrameCounter - Previous.Frame == 1
                    && Previous.Solve < Authority.Solve && FMath::IsFinite(Previous.WorldSeconds)
                    && Authority.WorldSeconds > Previous.WorldSeconds
                    && Get_IsProceduralSupportReturnNormalConsistent(Previous.Normal, Authority.Normal)
                    && FMath::IsFinite(Previous.SeenSeconds) && Previous.SeenSeconds >= 0.0)
                { Seen = Previous.SeenSeconds; }
            }
            if (NOT Get_IsProceduralSupportReturnConfirmed(Settings, Seen))
            {
                if (Seconds > TNumericLimits<double>::Max() - Seen)
                { return false; }
                Seen += Seconds;
            }
            Authority.SeenSeconds = Seen;
            auto CurrentAuthority = FProceduralSupportReturnObservation{};
            if (NOT ReadRecoveryAuthority(CurrentAuthority) || NOT SameBinding(Authority, CurrentAuthority))
            { return false; }
            InSupportComp._SupportReturnObservation = Authority;
#if WITH_DEV_AUTOMATION_TESTS
            if (PreparationTest != nullptr) { PreparationTest->SupportPendingRetained = true; }
#endif
            if (NOT Get_IsProceduralSupportReturnConfirmed(Settings, Seen))
            {
#if WITH_DEV_AUTOMATION_TESTS
                if (PreparationTest != nullptr) { PreparationTest->SupportPending = true; }
#endif
                return false;
            }
            auto Candidate = FTransform::Identity;
            auto CandidateState = FProceduralSurfaceMotionState{};
            if (NOT TryMake_ProceduralSupportReturn(Settings, Contact, Step, Body, InSupportComp._State, Candidate, CandidateState))
            { return false; }
            const auto CandidatePosed = HasPose ? PoseOffset * Candidate : Candidate;
            if (CandidatePosed.ContainsNaN() || NOT ck_surface_motion::Get_IsNativeVector(CandidatePosed.GetLocation()))
            { return false; }
            for (const auto& Anchor : CoreAnchors)
            {
                const auto Reach = Anchor.Get_Reach() + 1.0e-3;
                if (FVector::Dist(Candidate.TransformPosition(Anchor.Get_HipLocal()), Anchor.Get_FootWorld()) > Reach
                    || (HasPose && FVector::Dist(CandidatePosed.TransformPosition(Anchor.Get_HipLocal()), Anchor.Get_FootWorld()) > Reach))
                { return false; }
            }
#if WITH_DEV_AUTOMATION_TESTS
            if (PreparationTest != nullptr) { PreparationTest->SupportPhysical = true; }
#endif
            if (NOT FitsLandings(Candidate))
            { return false; }
#if WITH_DEV_AUTOMATION_TESTS
            if (PreparationTest != nullptr) { PreparationTest->SupportLandings = true; }
#endif
            const auto PreviousLimit = EnvelopeQueryLimit;
            EnvelopeQueryLimit = MaxEnvelopeQueries - RemainingReserve;
            ON_SCOPE_EXIT { EnvelopeQueryLimit = PreviousLimit; };
            if (NOT EnvelopeClear(Body, Candidate, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation))
            { return false; }
#if WITH_DEV_AUTOMATION_TESTS
            if (PreparationTest != nullptr) { PreparationTest->SupportEnvelope = true; }
#endif
            auto FinalAuthority = FProceduralSupportReturnObservation{};
            if (NOT ReadRecoveryAuthority(FinalAuthority) || NOT SameBinding(Authority, FinalAuthority)
                || NOT ck_surface_motion::Get_IsOwnedInWorld(Collider, Transient, World, Excluded))
            { InSupportComp._SupportReturnObservation.Reset(); return false; }
            Body = Candidate; InSupportComp._State = CandidateState;
            InSupportComp._SupportReturnObservation.Reset(); InSupportComp._HeightObstructionPoint.Reset();
            InSupportComp._AdaptiveTopAcquisition.Reset();
#if WITH_DEV_AUTOMATION_TESTS
            if (PreparationTest != nullptr)
            {
                PreparationTest->SupportCommitted = true; PreparationTest->SupportAfter = Body;
                PreparationTest->SupportStateAfter = InSupportComp._State;
                PreparationTest->SupportPublicSource = UCk_Utils_SurfaceMotion_UE::Get_ContactSource(
                    UCk_Utils_SurfaceMotion_UE::Cast(InHandle));
            }
#endif
            return true;
        };
        for (auto Iteration = 0; Iteration < AcceptedSubsteps; ++Iteration)
        {
            const auto BeforeBody = Body;
            const auto BeforeState = InSupportComp._State;
            auto ContactEnvelopeAttempted = false;
            auto ContactEnvelopeAccepted = false;
            const auto AdmitContactCorrection = [&](const FTransform& InCandidate) -> bool
            {
                ContactEnvelopeAttempted = true;
                ContactEnvelopeAccepted = EnvelopeClear(BeforeBody, InCandidate,
                    ck_surface_motion::EEnvelopeScope::SimulationAndPresentation);
                return ContactEnvelopeAccepted;
            };
            const auto ContactAdmission = Adaptive.Get_Enabled()
                ? TOptional<FProceduralSurfaceContactCorrectionAdmission>{AdmitContactCorrection}
                : TOptional<FProceduralSurfaceContactCorrectionAdmission>{};
            const auto Outcome = StepProceduralSurfaceMotionPaced(Settings, InMotionComp._Direction, InMotionComp._Speed,
                Step, RayCast, FeetSupport, TArrayView<const FProceduralSurfaceReachPaceAnchor>{CoreAnchors},
                CorePoseOffset, Body, InSupportComp._State, CorePlantedContacts, ContactAdmission,
                TArrayView<const FProceduralSurfaceLandingConstraint>{CoreLandings});
            for (auto Index = CoreLandings.Num() - 1; Index >= 0; --Index)
            {
                if (Outcome.Get_WithdrawnLandingIndices().Contains(Index))
                { CoreLandings.RemoveAt(Index); }
            }
            auto SweepHit = TOptional<FCk_Jolt_HitResult>{};
            auto Overlap = TOptional<FProceduralBodyEnvelopeOverlapObservation>{};
            auto ForwardSimulationRejected = false;
            if (Adaptive.Get_Enabled() && NOT ContactEnvelopeAccepted
                && NOT EnvelopeClear(BeforeBody, Body, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation,
                    &SweepHit, &Overlap, &ForwardSimulationRejected))
            {
                // A rejected correction already returned the cached zero trial. Its failed native proof is final:
                // replaying that same zero would add a ninth core step and a third envelope proof to this route.
                if (ContactEnvelopeAttempted)
                {
                    InHandle.Add<FFragment_SurfaceMotion_Failure>(ECk_SurfaceMotion_Failure::BodyObstructed);
                    return;
                }
                // Both observations only seed the existing fresh top probe. Neither grants support or path admission.
                const auto IsOpposingContact = [&](const FCk_Jolt_HitResult& InHit) -> bool
                {
                    return InHit.Get_HasHit() && NOT InHit.Get_Position().ContainsNaN()
                        && NOT InHit.Get_Normal().ContainsNaN() && InHit.Get_Normal().IsNormalized()
                        && FVector::DotProduct(InHit.Get_Normal(), InMotionComp._Direction) < -KINDA_SMALL_NUMBER;
                };
                const FCk_Jolt_HitResult* Obstacle = nullptr;
                if (SweepHit.IsSet() && SweepHit->Get_Fraction() > 0.0f && IsOpposingContact(*SweepHit))
                { Obstacle = &SweepHit.GetValue(); }
                else if (Overlap.IsSet())
                {
                    const auto LexicographicLess = [](const FVector& InLeft, const FVector& InRight) -> bool
                    {
                        if (InLeft.X != InRight.X)
                        { return InLeft.X < InRight.X; }
                        if (InLeft.Y != InRight.Y)
                        { return InLeft.Y < InRight.Y; }
                        return InLeft.Z < InRight.Z;
                    };
                    for (const auto& Hit : Overlap->Get_Hits())
                    {
                        if (NOT IsOpposingContact(Hit))
                        { continue; }
                        const auto Dot = FVector::DotProduct(Hit.Get_Normal(), InMotionComp._Direction);
                        const auto BestDot = Obstacle != nullptr
                            ? FVector::DotProduct(Obstacle->Get_Normal(), InMotionComp._Direction) : 0.0;
                        if (Obstacle == nullptr || Dot < BestDot
                            || (Dot == BestDot && (LexicographicLess(Hit.Get_Position(), Obstacle->Get_Position())
                                || (Hit.Get_Position() == Obstacle->Get_Position()
                                    && LexicographicLess(Hit.Get_Normal(), Obstacle->Get_Normal())))))
                        { Obstacle = &Hit; }
                    }
                }
                if (Obstacle != nullptr && BeforeState.Get_Grounded() && NOT Outcome.Get_PhysicalOverride())
                {
                    InSupportComp._HeightObstructionPoint = Obstacle->Get_Position();
                    InSupportComp._HeightObstructionNormal = Obstacle->Get_Normal();
                    InSupportComp._HeightObstructionBody = BeforeBody;
                    InSupportComp._HeightObstructionDirection = InMotionComp._Direction;
                    InSupportComp._HeightObstructionUp = BeforeState.Get_SupportNormal();
                    InSupportComp._HeightObstructionFrame = GFrameCounter;
                }
                auto Attempted = FVector::Dist(BeforeBody.GetLocation(), Body.GetLocation());
                for (const auto& Anchor : CoreAnchors)
                {
                    Attempted = FMath::Max(Attempted, FVector::Dist(BeforeBody.TransformPosition(Anchor.Get_HipLocal()),
                        Body.TransformPosition(Anchor.Get_HipLocal())));
                }
                AttemptedStanceDistance += Attempted;
                const auto RejectedBody = Body;
                Body = BeforeBody;
                InSupportComp._State = BeforeState;
                constexpr auto ZeroVoluntaryMotion = 0.0f;
                StepProceduralSurfaceMotion(Settings, InMotionComp._Direction, InMotionComp._Speed,
                    Step, RayCast, FeetSupport, Body, InSupportComp._State, ZeroVoluntaryMotion, CorePlantedContacts);
                CoreLandings.RemoveAll([&](const auto& InLanding)
                    { return NOT Get_IsProceduralSurfaceLandingReachValid(Body, CorePoseOffset, InLanding); });
                if (NOT EnvelopeClear(BeforeBody, Body, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation))
                {
                    InHandle.Add<FFragment_SurfaceMotion_Failure>(ECk_SurfaceMotion_Failure::BodyObstructed);
                    return;
                }
                auto PhysicalOverride = false;
                const auto ZeroPosed = HasPose ? PoseOffset * Body : Body;
                for (const auto& Anchor : CoreAnchors)
                {
                    const auto Reach = Anchor.Get_Reach() + 1.0e-3;
                    PhysicalOverride |= FVector::Dist(Body.TransformPosition(Anchor.Get_HipLocal()), Anchor.Get_FootWorld()) > Reach
                        || (HasPose && FVector::Dist(ZeroPosed.TransformPosition(Anchor.Get_HipLocal()), Anchor.Get_FootWorld()) > Reach);
                }
                InSupportComp._ReachPaceState = PhysicalOverride
                    || InSupportComp._ReachPaceState == ECk_SurfaceMotion_ReachPaceState::PhysicalOverride
                    ? ECk_SurfaceMotion_ReachPaceState::PhysicalOverride : ECk_SurfaceMotion_ReachPaceState::Blocked;
                InSupportComp._ReachPaceScale = 0.0f;
                if (Iteration == 0 && ForwardSimulationRejected && SweepHit.IsSet() && NOT PhysicalOverride
                    && NOT Outcome.Get_PhysicalOverride() && Outcome.Get_Scale() == 1.0f
                    && Outcome.Get_RejectedAnchorIndices().IsEmpty() && NOT ContactEnvelopeAttempted)
                {
#if WITH_DEV_AUTOMATION_TESTS
                    if (PreparationTest != nullptr)
                    {
                        PreparationTest->SupportSweep = SweepHit.GetValue();
                        PreparationTest->SupportZeroAccepted = true;
                        PreparationTest->SupportBefore = Body; PreparationTest->SupportAfter = Body;
                        PreparationTest->SupportStateBefore = InSupportComp._State;
                        PreparationTest->SupportStateAfter = InSupportComp._State;
                    }
#endif
                    TrySupportReturn(BeforeBody, BeforeState, RejectedBody, SweepHit.GetValue());
                }
                continue;
            }
            InSupportComp._SupportReturnObservation.Reset();
            // Earlier rejected trials are not unioned with the final accepted-body stamp. Only the final substep can
            // request release, and any physical override in this frame withdraws that request below.
            if (Iteration == AcceptedSubsteps - 1 && Outcome.Get_RejectedFullBody().IsSet())
            {
                const auto& TrialBody = Outcome.Get_RejectedFullBody().GetValue();
                const auto TrialPosedBody = HasPose ? PoseOffset * TrialBody : FTransform::Identity;
                for (const auto AnchorIndex : Outcome.Get_RejectedAnchorIndices())
                {
                    const auto& Anchor = ReachAnchors[AnchorIndex];
                    InSupportComp._ReachPaceFeedback.Emplace(Anchor.Get_Leg(), Anchor.Get_FootWorld(),
                        TrialBody.TransformPosition(Anchor.Get_HipLocal()), HasPose
                            ? TOptional<FVector>{TrialPosedBody.TransformPosition(Anchor.Get_HipLocal())} : TOptional<FVector>{});
                }
            }
            InSupportComp._ReachPaceTrials += Outcome.Get_Trials();
            AttemptedStanceDistance += Outcome.Get_AttemptedStanceSpeed() * Step.Get_Seconds();
            InSupportComp._ReachPaceScale = FMath::Min(InSupportComp._ReachPaceScale, Outcome.Get_Scale());
            if (Outcome.Get_PhysicalOverride())
            { InSupportComp._ReachPaceState = ECk_SurfaceMotion_ReachPaceState::PhysicalOverride; }
            else if (InSupportComp._ReachPaceState != ECk_SurfaceMotion_ReachPaceState::PhysicalOverride)
            {
                if (Outcome.Get_Scale() == 0.0f)
                { InSupportComp._ReachPaceState = ECk_SurfaceMotion_ReachPaceState::Blocked; }
                else if (Outcome.Get_Scale() < 1.0f && InSupportComp._ReachPaceState != ECk_SurfaceMotion_ReachPaceState::Blocked)
                { InSupportComp._ReachPaceState = ECk_SurfaceMotion_ReachPaceState::Pacing; }
            }
        }
        if (InSupportComp._ReachPaceState == ECk_SurfaceMotion_ReachPaceState::PhysicalOverride)
        {
            InSupportComp._SupportReturnObservation.Reset();
            InSupportComp._ReachPaceFeedback.Reset();
            InSupportComp._AdaptiveHeightActive = false;
            InSupportComp._AdaptiveHeightPreparing = false;
        }
        InSupportComp._AttemptedStanceSpeed = static_cast<float>(AttemptedStanceDistance / Dt);
        InSupportComp._State.Set_Velocity(InSupportComp._State.Get_Velocity() + PlannedHeightVelocity);
        if (ObservedTop.IsSet() && HasGait
            && UCk_Utils_ProceduralGait_UE::Get_Status(UCk_Utils_ProceduralGait_UE::Cast(InHandle))
                == ECk_ProceduralAnimation_Status::Ready
            && InSupportComp._State.Get_Grounded()
            && InSupportComp._ReachPaceState == ECk_SurfaceMotion_ReachPaceState::Blocked
            && InSupportComp._HeightObstructionPoint.IsSet()
            && InSupportComp._HeightObstructionFrame == GFrameCounter
            && InSupportComp._HeightObstructionBody.Equals(Body, 1.0e-3)
            && InSupportComp._HeightObstructionDirection.Equals(InMotionComp._Direction, 1.0e-3)
            && InSupportComp._HeightObstructionUp.Equals(InSupportComp._State.Get_SupportNormal(), 1.0e-3)
            && ObservedTop->Get_Body().Equals(Body, 1.0e-3)
            && ObservedTop->Get_Direction().Equals(InMotionComp._Direction, 1.0e-3)
            && ObservedTop->Get_Up().Equals(InSupportComp._State.Get_SupportNormal(), 1.0e-3)
            && ObservedTop->Get_Speed() == InMotionComp._Speed)
        { InSupportComp._AdaptiveTopAcquisition = ObservedTop; }
        InSupportComp._EvaluatedBody = Body;
        InSupportComp._EvaluatedFrame = GFrameCounter;
        auto TransformHandle = UCk_Utils_Transform_UE::CastChecked(InHandle);
        UCk_Utils_Transform_UE::Request_SetTransform(TransformHandle, FCk_Request_Transform_SetTransform{Body}, {});
    }
}
