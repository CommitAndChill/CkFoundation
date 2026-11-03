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
        Exhausted
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

    // Owner-lifetime authority: removing SurfaceMotion does not replenish a charged staging attempt.
    struct FFragment_ProceduralContactStagingFence
    {
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

        auto TryAdvanceTime(uint64 InFrame, double InDelta) -> bool
        {
            if (NOT _Spent || InFrame == 0 || InFrame == MAX_uint64 || _LastSurfaceFrame >= InFrame
                || (_LastSurfaceFrame != 0 && _LastAcknowledgedFrame != _LastSurfaceFrame)
                || _RequiredCount < 1 || _RequiredCount > 2 || _CycleLimit != 2.0 * _RequiredCount + 1.0
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
                || _RequiredCount < 1 || _RequiredCount > 2 || _CycleLimit != 2.0 * _RequiredCount + 1.0
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
}
