#pragma once

#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment_Data.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment_Data.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Fragment_Data.h"
#include "CkProceduralAnimation/Core/CkProceduralContactStaging.h"

#include <array>

class UWorld;

namespace ck
{
    enum class EProceduralContactStagingPhase : uint8
    {
        Dormant,
        Retreat,
        Replant,
        AdvanceExit,
        Exhausted,
        AuxiliaryHold,
        MoveB,
        PrepareM,
        PrepareF,
        MoveC,
        QualifyLegacy
    };

    struct FProceduralContactStagingRequiredContact
    {
        FCk_Handle_ProceduralLeg _Leg;
        int32 _Index = INDEX_NONE;
        FVector _OriginalPlant = FVector::ZeroVector;
        FVector _Query = FVector::ZeroVector;
        TOptional<FVector> _IntermediateQuery;
        FProceduralContactStagingLeg _Geometry;
    };

    enum class EProceduralContactStagingMilestone : uint8
    {
        Intermediate,
        Final,
        Complete
    };

    struct FProceduralContactStagingPlacement
    {
        bool _Started = false;
        bool _Completed = false;
        bool _Final = false;
        uint64 _Frame = 0;
        uint64 _InputSolve = 0;
        FTransform _Body = FTransform::Identity;
        FVector _Target = FVector::ZeroVector;
        TOptional<uint64> _ClaimsRevision;
        uint64 _CompletedFrame = 0;
        uint64 _CompletedSolve = 0;
        FVector _Plant = FVector::ZeroVector;
        FVector _Normal = FVector::ZeroVector;
    };

    struct FProceduralContactStagingMilestones
    {
        EProceduralContactStagingMilestone _Next = EProceduralContactStagingMilestone::Final;
        FVector _ActualPlant = FVector::ZeroVector;
        std::array<FProceduralContactStagingPlacement, 2> _Placements;
        uint8 _StartedCount = 0;
    };

    enum class EProceduralContactStagingFenceMode : uint8
    {
        Legacy,
        Preparation
    };

    // A fixed certificate is proposal geometry. Only the separate actual-start/touchdown ledger is progress.
    struct FProceduralContactStagingPreparation
    {
        FProceduralContactStagingPreparationPlan _Plan;
        std::array<FCk_Handle_ProceduralLeg, 4> _Roster;
        std::array<FName, 4> _Ids;
        std::array<FCk_ProceduralLeg_Spec, 4> _Specs;
        std::array<FProceduralContactStagingLeg, 4> _Geometry;
        std::array<FProceduralContactStagingPlacement, 4> _Placements;
        TOptional<uint64> _ClaimsRevision;
        uint8 _StartedCount = 0;
        uint8 _CompletedCount = 0;
        bool _LegacyAdopted = false;

        auto Get_IsLedgerValid() const -> bool
        {
            if (_CompletedCount > _StartedCount || _StartedCount > 4 || _StartedCount > _CompletedCount + 1)
            { return false; }
            for (auto Index = 0; Index < 4; ++Index)
            {
                if (_Placements[Index]._Started != (Index < _StartedCount)
                    || _Placements[Index]._Completed != (Index < _CompletedCount))
                { return false; }
            }
            return true;
        }

        auto Get_Query(uint8 InSlot) const -> TOptional<FVector>
        {
            switch (InSlot)
            {
                case 0: return _Plan._AuxiliaryQuery;
                case 1: return _Plan._SpanPreparationQuery;
                case 2: return _Plan._OtherPreparationQuery;
                case 3:
                    if (_Plan._LegacyPlan._RequiredCount == 1)
                    { return _Plan._LegacyPlan._Required[0]._ProposedQuery; }
                    return {};
                default: return {};
            }
        }

        auto Get_RosterIndex(uint8 InSlot) const -> int32
        {
            switch (InSlot)
            {
                case 0: return _Plan._AuxiliaryRosterIndex;
                case 1: case 3: return _Plan._SpanRosterIndex;
                case 2: return _Plan._OtherRosterIndex;
                default: return INDEX_NONE;
            }
        }

        // The caller validates current owner/phase/native receipt and all takeoff gates before this commit.
        auto TryRecordStart(uint8 InSlot, uint64 InFrame, uint64 InInputSolve, const FTransform& InBody,
            const FVector& InTarget, const FVector& InNormal, const TOptional<uint64>& InClaimsRevision) -> bool
        {
            const auto Query = Get_Query(InSlot);
            if (NOT Get_IsLedgerValid() || InSlot >= 4 || InSlot != _StartedCount
                || _StartedCount != _CompletedCount || (InSlot == 3 && NOT _LegacyAdopted)
                || InFrame == 0 || InFrame == MAX_uint64 || InInputSolve == MAX_uint64
                || InBody.ContainsNaN() || NOT InBody.GetRotation().IsNormalized()
                || InTarget.ContainsNaN() || InNormal.ContainsNaN() || NOT InNormal.IsNormalized() || NOT Query.IsSet()
                || InTarget != Query.GetValue() || InClaimsRevision != _ClaimsRevision)
            { return false; }
            auto& Record = _Placements[InSlot];
            Record._Started = true;
            Record._Final = InSlot == 3;
            Record._Frame = InFrame;
            Record._InputSolve = InInputSolve;
            Record._Body = InBody;
            Record._Target = InTarget;
            Record._Normal = InNormal;
            Record._ClaimsRevision = InClaimsRevision;
            ++_StartedCount;
            return true;
        }

        // Called only after actual native touchdown and successful body/stance publication, including same-solve landings.
        auto TryRecordTouchdown(uint8 InSlot, uint64 InFrame, uint64 InProducedSolve, const FVector& InPlant,
            const FVector& InNormal, const TOptional<uint64>& InClaimsRevision) -> bool
        {
            if (NOT Get_IsLedgerValid() || InSlot >= 4 || InSlot != _CompletedCount
                || _StartedCount != _CompletedCount + 1 || InFrame == 0 || InFrame == MAX_uint64 || InProducedSolve == MAX_uint64
                || InPlant.ContainsNaN() || InNormal.ContainsNaN() || NOT InNormal.IsNormalized())
            { return false; }
            auto& Record = _Placements[InSlot];
            if (InFrame < Record._Frame || InProducedSolve <= Record._InputSolve
                || InPlant != Record._Target || InNormal != Record._Normal || InClaimsRevision != Record._ClaimsRevision)
            { return false; }
            Record._Completed = true;
            Record._CompletedFrame = InFrame;
            Record._CompletedSolve = InProducedSolve;
            Record._Plant = InPlant;
            Record._Normal = InNormal;
            ++_CompletedCount;
            return true;
        }
    };

    // Owner-lifetime authority: removing SurfaceMotion does not replenish a charged staging attempt.
    struct FFragment_ProceduralContactStagingFence
    {
        EProceduralContactStagingFenceMode _Mode = EProceduralContactStagingFenceMode::Legacy;
        bool _Spent = false;
        bool _AbortRecorded = false;
        FCk_Handle _Owner;
        TWeakObjectPtr<UWorld> _World;
        uint64 _Episode = 0;
        FVector _Forward = FVector::ZeroVector;
        FVector _Up = FVector::ZeroVector;
        FTransform _Exit = FTransform::Identity;
        uint64 _InitiatedFrame = 0;
        uint64 _InitiatedSolve = 0;
        uint64 _LastSurfaceFrame = 0;
        uint64 _LastAcknowledgedSolve = 0;
        uint64 _LastAcknowledgedFrame = 0;
        double _Elapsed = 0.0;
        double _ElapsedLimit = 0.0;
        double _Cycles = 0.0;
        double _CycleLimit = 0.0;
        uint8 _RequiredCount = 0;
        FCk_ProceduralGait_Timing _Timing;
        FCk_ProceduralGait_Step _Step;
        FCk_ProceduralGait_Probe _Probe;
        FCk_ProceduralGait_Foothold _Foothold;
        FCk_SurfaceMotion_Spec _Surface;
        std::array<FProceduralContactStagingMilestones, 2> _Legs;
        FProceduralContactStagingPreparation _Preparation;

        auto Get_HasValidLimits() const -> bool
        {
            if (_Mode == EProceduralContactStagingFenceMode::Preparation)
            {
                return _CycleLimit == 5.0 && _Preparation.Get_IsLedgerValid()
                    && (_Preparation._LegacyAdopted ? _RequiredCount == 1 : _RequiredCount == 0);
            }
            return _Mode == EProceduralContactStagingFenceMode::Legacy
                && _RequiredCount >= 1 && _RequiredCount <= 2 && _CycleLimit == 2.0 * _RequiredCount + 1.0;
        }

        auto TryAdvanceTime(uint64 InFrame, double InDelta) -> bool
        {
            if (NOT _Spent || InFrame == 0 || InFrame == MAX_uint64 || _LastSurfaceFrame >= InFrame
                || (_LastSurfaceFrame != 0 && _LastAcknowledgedFrame != _LastSurfaceFrame)
                || NOT Get_HasValidLimits()
                || NOT FMath::IsFinite(InDelta) || InDelta <= 0.0
                || NOT FMath::IsFinite(_Elapsed) || _Elapsed < 0.0
                || NOT FMath::IsFinite(_ElapsedLimit) || _ElapsedLimit <= 0.0)
            { return false; }
            const auto Next = _Elapsed + InDelta;
            if (NOT FMath::IsFinite(Next))
            { return false; }
            _LastSurfaceFrame = InFrame;
            _Elapsed = Next;
            return _Elapsed < _ElapsedLimit;
        }

        auto TryAcknowledge(uint64 InFrame, uint64 InInputSolve, uint64 InProducedSolve, double InAdvance) -> bool
        {
            if (NOT _Spent || InFrame == 0 || InFrame == MAX_uint64 || InFrame != _LastSurfaceFrame || InInputSolve == MAX_uint64
                || InFrame <= _LastAcknowledgedFrame
                || _LastAcknowledgedSolve != InInputSolve || InProducedSolve != InInputSolve + 1
                || NOT Get_HasValidLimits()
                || NOT FMath::IsFinite(InAdvance) || InAdvance <= 0.0
                || NOT FMath::IsFinite(_Cycles) || _Cycles < 0.0)
            { return false; }
            const auto Next = _Cycles + InAdvance;
            if (NOT FMath::IsFinite(Next))
            { return false; }
            _LastAcknowledgedSolve = InProducedSolve;
            _LastAcknowledgedFrame = InFrame;
            _Cycles = Next;
            return _Cycles < _CycleLimit;
        }

        auto Get_IsBeyondExit(const FTransform& InActualBody, const FVector& InCurrentUp) const -> bool
        {
            if (NOT _Spent || InActualBody.ContainsNaN() || _Exit.ContainsNaN() || _Forward.ContainsNaN()
                || NOT _Forward.IsNormalized() || _Up.ContainsNaN() || NOT _Up.IsNormalized()
                || InCurrentUp.ContainsNaN() || NOT InCurrentUp.Equals(_Up, 1.0e-3))
            { return false; }
            const auto Passage = FVector::DotProduct(InActualBody.GetLocation() - _Exit.GetLocation(), _Forward);
            return FMath::IsFinite(Passage) && Passage > 0.0;
        }
    };

    struct FProceduralContactStagingBoundary
    {
        FCk_Handle_ProceduralLeg _Leg;
        FCk_Handle _Attribution;
        FVector _Plant = FVector::ZeroVector;
        FVector _TopPoint = FVector::ZeroVector;
        FVector _TopNormal = FVector::ZeroVector;
        FVector _FacePoint = FVector::ZeroVector;
        FVector _FaceNormal = FVector::ZeroVector;
    };

    struct FProceduralContactStagingState
    {
        EProceduralContactStagingPhase _Phase = EProceduralContactStagingPhase::Dormant;
        FCk_Handle _Owner;
        uint64 _Episode = 0;
        FVector _Direction = FVector::ZeroVector;
        FVector _Up = FVector::UpVector;
        float _Speed = 0.0f;
        FTransform _BlockedBody = FTransform::Identity;
        FTransform _RetreatBody = FTransform::Identity;
        FTransform _ExitBody = FTransform::Identity;
        TArray<FProceduralContactStagingRequiredContact, TInlineAllocator<2>> _Required;
        TOptional<FTransform> _PendingBody;
        uint64 _PendingFrame = 0;
        uint64 _PendingSolve = 0;
        uint64 _IntentFrame = 0;
        uint64 _ConsumerSolve = 0;
        FTransform _IntentBody = FTransform::Identity;
        TOptional<FTransform> _IntentPoseOffset;
        TArray<FCk_Handle_ProceduralLeg, TInlineAllocator<2>> _PoseRayTickets;
        int32 _ReservedRays = 0;
        uint64 _ProducedFrame = 0;
        uint64 _ProducedSolve = 0;
        FTransform _ProducedBody = FTransform::Identity;
        uint8 _LandedMask = 0;
        uint8 _StartedMask = 0;
        float _LastClock = 0.0f;
        float _ClockAtProgress = 0.0f;
        double _ClockTravelWithoutProgress = 0.0;
        uint8 _ClockWrapsWithoutProgress = 0;
        bool _TwoPlacement = false;
    };

#if WITH_DEV_AUTOMATION_TESTS
    // Test-owned passive copies only. Presence never grants a query, candidate, start or phase transition.
    struct FProceduralContactPreparationTestSelection
    {
        uint64 Frame = 0;
        uint64 InputSolve = 0;
        uint64 Episode = 0;
        int32 LegIndex = INDEX_NONE;
        uint8 Slot = 0;
        FTransform Body = FTransform::Identity;
        FVector OrdinaryPoint = FVector::ZeroVector;
        FVector NativePoint = FVector::ZeroVector;
        FVector NativeNormal = FVector::ZeroVector;
        uint8 OrdinarySource = 0;
        bool OrdinaryValid = false;
        bool OrdinaryTrusted = false;
        bool NativeAccepted = false;
        bool ReusedNative = false;
        bool PosedTicketConsumed = false;
        bool OverlaySelected = false;
        int32 SearchBefore = 0;
        int32 SearchAfter = 0;
        int32 OrdinaryBoundaryLimit = 0;
        int32 OrdinaryBoundaryUsed = 0;
        int32 FixedSamples = 0;
        int32 RaysBefore = 0;
        int32 RaysAfter = 0;
        bool ActualBegin = false;
        bool Emergency = false;
        bool WindowOpen = false;
        bool Budget = false;
        bool OnSchedule = false;
        bool CatchStep = false;
        bool RecoveryWants = false;
        bool ReachPaceRelief = false;
        bool BeyondSchedule = false;
    };

    struct FFragment_ProceduralContactPreparation_Test
    {
        CK_GENERATED_BODY(FFragment_ProceduralContactPreparation_Test);
        FProceduralContactPreparationTestSelection Last;
        std::array<FProceduralContactPreparationTestSelection, 4> Starts;
        uint8 StartedMask = 0;
        uint32 Revocations = 0;
        uint32 DeniedTakeoffs = 0;
        bool Overflow = false;
        uint64 ChargedEpisode = 0;
        uint64 ChargedFrame = 0;
        uint64 ChargedInputSolve = 0;
        int32 ChargedRays = 0;
        int32 ChargedEnvelopes = 0;
        int32 ChargedTrials = 0;
    };
#endif
}
