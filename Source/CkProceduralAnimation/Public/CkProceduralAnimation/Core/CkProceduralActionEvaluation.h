#pragma once

#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action
{
    struct CKPROCEDURALANIMATION_API FInitialPose
    {
        CK_GENERATED_BODY(FInitialPose);
    private:
        FTransform _Body = FTransform::Identity;
        TMap<FName, FTransform> _Limbs;
        TMap<FName, FVector> _BendDirections;
    public:
        CK_PROPERTY(_Body);
        CK_PROPERTY(_Limbs);
        CK_PROPERTY(_BendDirections);
    };

    struct CKPROCEDURALANIMATION_API FLimbPose
    {
        CK_GENERATED_BODY(FLimbPose);
    private:
        FName _Slot;
        FVector _Position = FVector::ZeroVector;
        FQuat _Orientation = FQuat::Identity;
        FVector _BendDirection = FVector::UpVector;
        bool _HasOrientation = false;
        bool _HasBend = false;
        ECk_ProceduralAction_ReachPolicy _ReachPolicy = ECk_ProceduralAction_ReachPolicy::RejectUnreachable;
    public:
        CK_PROPERTY(_Slot);
        CK_PROPERTY(_Position);
        CK_PROPERTY(_Orientation);
        CK_PROPERTY(_BendDirection);
        CK_PROPERTY(_HasOrientation);
        CK_PROPERTY(_HasBend);
        CK_PROPERTY(_ReachPolicy);
    };

    struct CKPROCEDURALANIMATION_API FEvaluatedPose
    {
        CK_GENERATED_BODY(FEvaluatedPose);
    private:
        FTransform _Body = FTransform::Identity;
        TArray<FLimbPose> _Limbs;
        int32 _PhaseIndex = INDEX_NONE;
        FCk_Time _Time;
    public:
        CK_PROPERTY(_Body);
        CK_PROPERTY(_Limbs);
        CK_PROPERTY(_PhaseIndex);
        CK_PROPERTY(_Time);
    };

    struct CKPROCEDURALANIMATION_API FSampledInterval
    {
        CK_GENERATED_BODY(FSampledInterval);
    private:
        FCk_Time _AcceptedEnd;
        FCk_Time _Backlog;
        bool _StoppedAtPhaseBoundary = false;
        bool _StoppedAtWindowBoundary = false;
        bool _BudgetExhausted = false;
        TArray<FEvaluatedPose> _Samples;
        TArray<FCompiledEvent> _CrossedEvents;
    public:
        CK_PROPERTY(_AcceptedEnd);
        CK_PROPERTY(_Backlog);
        CK_PROPERTY(_StoppedAtPhaseBoundary);
        CK_PROPERTY(_StoppedAtWindowBoundary);
        CK_PROPERTY(_BudgetExhausted);
        CK_PROPERTY(_Samples);
        CK_PROPERTY(_CrossedEvents);
    };

    CKPROCEDURALANIMATION_API auto
        Evaluate(
            const FCompiledClip& InClip,
            const FInitialPose& InInitial,
            FCk_Time InTime,
            FEvaluatedPose& OutPose)
        -> bool;

    // Authored-space candidates only. Runtime must refine/reject each prefix after space resolution and IK:
    // this does not certify solved segment angles, root motion or settled transforms.
    // A publication stops at phase and window boundaries; point markers may remain interior.
    CKPROCEDURALANIMATION_API auto
        SampleInterval(
            const FCompiledClip& InClip,
            const FInitialPose& InInitial,
            FCk_Time InStart,
            FCk_Time InRequestedEnd,
            const FCk_ProceduralAction_Sampling& InBudget,
            FSampledInterval& OutInterval)
        -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
