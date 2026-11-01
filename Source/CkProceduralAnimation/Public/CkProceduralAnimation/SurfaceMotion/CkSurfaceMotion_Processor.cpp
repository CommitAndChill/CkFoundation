#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Processor.h"

#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralBodyEnvelope.h"
#include "CkProceduralAnimation/Core/CkProceduralHeightPlanning.h"
#include "CkProceduralAnimation/Core/CkProceduralContactStaging.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkJolt/Query/CkJoltQuery_Utils.h"

#include <Engine/World.h>

CK_REGISTER_PROCESSOR(ck::FProcessor_SurfaceMotion_Setup);
CK_REGISTER_PROCESSOR(ck::FProcessor_SurfaceMotion_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_SurfaceMotion_Update);
CK_REGISTER_PROCESSOR(ck::FProcessor_SurfaceMotion_CancelPendingRequests);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_surface_motion
{
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
        InSupportComp._AdaptiveTopAcquisition.Reset();
        auto& Staging = InSupportComp._ContactStaging;
        Staging._PoseRayTickets.Reset();
        Staging._ReservedRays = 0;
        Staging._IntentFrame = 0;
        const auto Dt = InDeltaT.Get_Seconds();
        if (NOT FMath::IsFinite(Dt) || Dt <= 0.0)
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
                    if (NOT procedural_leg::Get_IsSupporting(Leg)
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
            bool* OutSimulationRejected = nullptr) -> bool
        {
            if (OutSimulationRejected != nullptr)
            { *OutSimulationRejected = false; }
            if (NOT Get_IsProceduralBodyEnvelopePathClear(World, Adaptive, QueryFilter, InStart, InEnd,
                    InSupportComp._HeightEnvelopeQueries, EnvelopeQueryLimit, OutSweepHit, OutOverlap))
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
                    auto TriedPreparatoryBody = false;
                    struct FHeightGoalAttempt
                    {
                        const FProceduralGaitHeightCandidate* Candidate;
                        double Delta;
                    };
                    auto Attempts = TArray<FHeightGoalAttempt, TInlineAllocator<4>>{};
                    auto DeferredCandidates = TArray<const FProceduralGaitHeightCandidate*, TInlineAllocator<8>>{};
                    const auto TryGoal = [&](const FProceduralGaitHeightCandidate& InCandidate, double InDelta,
                        TOptional<FProceduralGaitHeightRetryWitness>* OutVisibilityRetry = nullptr) -> bool
                    {
                        if (OutVisibilityRetry != nullptr)
                        { OutVisibilityRetry->Reset(); }
                        ++InSupportComp._HeightPlanningTrials;
                        TriedWholeBodyGoal |= WholeBodyGoal;
                        Attempts.Add(FHeightGoalAttempt{&InCandidate, InDelta});
                        auto GoalBody = Body;
                        GoalBody.AddToTranslation(Up * InDelta);
                        if (NOT FitsLandings(GoalBody))
                        { return false; }
                        if (NOT Get_IsProceduralHeightPlanAdmitted(GaitHandle, GoalBody, CorePoseOffset, InCandidate,
                                InSupportComp._HeightPlanningRays, MaxPlanningRays,
                                WholeBodyGoal ? EProceduralHeightPlanStage::Recovery : EProceduralHeightPlanStage::Landing,
                                OutVisibilityRetry, &StagingObservations))
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
                            auto PartialSimulationRejected = false;
                            if (NOT FitsLandings(Partial)
                                || NOT Get_IsProceduralHeightPlanAdmitted(GaitHandle, Partial, CorePoseOffset, InCandidate,
                                    InSupportComp._HeightPlanningRays, MaxPlanningRays, EProceduralHeightPlanStage::PreparatoryBody,
                                    nullptr, &StagingObservations))
                            { return false; }
                            if (NOT EnvelopeClear(Body, Partial, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation,
                                    &PartialSweepHit, nullptr, &PartialSimulationRejected))
                            {
                                if (PartialSimulationRejected && PartialSweepHit.IsSet())
                                {
                                    StartStaging();
                                    if (StagingOwnsMotion)
                                    { FoundGoal = true; return true; }
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
                    const auto RetainedPlaneLower = NOT WholeBodyGoal
                        ? Get_ProceduralHeightRetainedPlaneLower(Body, CorePoseOffset, Up,
                            TArrayView<const FProceduralSurfaceReachPaceAnchor>{CoreAnchors})
                        : TOptional<double>{};
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
                            if (TryGoal(Candidate, Delta))
                            { break; }
                        }
                        if (FoundGoal)
                        { break; }
                    }
                    // A fixed seed may fail its interval or lie at/below a retained-foot plane even though native
                    // down replay can reproject it. Keep eligible ordinary proposals first and share their goal budget.
                    if (NOT FoundGoal && NOT WholeBodyGoal)
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
                            if (TryGoal(*Candidate, Delta.GetValue(), &VisibilityRetry))
                            { break; }
                            if (TriedVisibilityInterior || NOT VisibilityRetry.IsSet()
                                || InSupportComp._HeightPlanningTrials >= MaxHeightTrials || Delta.GetValue() >= -1.0e-3)
                            { continue; }
                            // Only this failed native call may seed one interior proposal. The original sampled
                            // candidate still owns provenance and is replayed through every unchanged admission.
                            const auto& Witness = VisibilityRetry.GetValue();
                            const auto& OriginalCandidate = Witness.Get_Candidate();
                            auto RejectedBody = Body;
                            RejectedBody.AddToTranslation(Up * Delta.GetValue());
                            if (Witness.Get_Frame() != GFrameCounter || Witness.Get_SolveSequence() != Gait._SolveSequence
                                || NOT Witness.Get_BodyAtSolve().Equals(Body, 1.0e-6)
                                || NOT Witness.Get_RejectedBody().Equals(RejectedBody, 1.0e-6)
                                || NOT Witness.Get_SupportUp().Equals(Up, 1.0e-6)
                                || Witness.Get_PoseOffset().IsSet() != CorePoseOffset.IsSet()
                                || (CorePoseOffset.IsSet() && NOT Witness.Get_PoseOffset()->Equals(CorePoseOffset.GetValue(), 1.0e-6))
                                || OriginalCandidate.Get_Leg() != Candidate->Get_Leg()
                                || OriginalCandidate.Get_HipLocal() != Candidate->Get_HipLocal()
                                || OriginalCandidate.Get_FootWorld() != Candidate->Get_FootWorld()
                                || OriginalCandidate.Get_PointWorld() != Candidate->Get_PointWorld()
                                || OriginalCandidate.Get_NormalWorld() != Candidate->Get_NormalWorld()
                                || OriginalCandidate.Get_Reach() != Candidate->Get_Reach()
                                || OriginalCandidate.Get_AdmissionReach() != Candidate->Get_AdmissionReach()
                                || OriginalCandidate.Get_RequiresRelief() != Candidate->Get_RequiresRelief()
                                || Witness.Get_Cast() != EProceduralFootholdCast::Down
                                || Witness.Get_PointWorld().ContainsNaN() || NOT Witness.Get_NormalWorld().IsNormalized())
                            { continue; }
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
                            if (TryGoal(*Candidate, Interior.GetValue()))
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
            if (Adaptive.Get_Enabled() && NOT ContactEnvelopeAccepted
                && NOT EnvelopeClear(BeforeBody, Body, ck_surface_motion::EEnvelopeScope::SimulationAndPresentation, &SweepHit, &Overlap))
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
                continue;
            }
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
