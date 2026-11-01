#pragma once

#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment_Data.h"

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
    };
}
