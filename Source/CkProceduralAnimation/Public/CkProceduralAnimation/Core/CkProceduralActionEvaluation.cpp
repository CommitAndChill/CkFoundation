#include "CkProceduralAnimation/Core/CkProceduralActionEvaluation.h"

#include "CkProceduralAnimation/Core/CkProceduralRigidTransform.h"
#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"

namespace ck::procedural_action
{
    namespace evaluation_detail
    {
        auto
        IsFinite(
            const FVector& InValue)
            -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z);
        }

        template<typename TKey>
        auto
        Segment(
            const TArray<TKey>& InKeys,
            double InTime)
            -> int32
        {
            for (auto Index = 0; Index + 1 < InKeys.Num(); ++Index)
            {
                if (InTime <= InKeys[Index + 1].Get_Time().Get_Value())
                { return Index; }
            }
            return FMath::Max(0, InKeys.Num() - 2);
        }

        auto
        BlendVector(
            const FVectorKey& InA,
            const FVectorKey& InB,
            double InTime,
            ECk_ProceduralAction_Interpolation InInterpolation)
            -> FVector
        {
            const auto Span = InB.Get_Time().Get_Value() - InA.Get_Time().Get_Value();
            const auto Alpha = Span > 0.0 ? FMath::Clamp((InTime - InA.Get_Time().Get_Value()) / Span, 0.0, 1.0) : 0.0;
            if (InInterpolation == ECk_ProceduralAction_Interpolation::Linear)
            { return FMath::Lerp(InA.Get_Value(), InB.Get_Value(), Alpha); }
            const auto Raw = FMath::CubicInterp(InA.Get_Value(), InA.Get_LeaveTangent() * Span,
                InB.Get_Value(), InB.Get_ArriveTangent() * Span, Alpha);
            // Bound each axis between its keys. An authored tangent cannot create an unbudgeted overshoot.
            return FVector{FMath::Clamp(Raw.X, FMath::Min(InA.Get_Value().X, InB.Get_Value().X), FMath::Max(InA.Get_Value().X, InB.Get_Value().X)),
                FMath::Clamp(Raw.Y, FMath::Min(InA.Get_Value().Y, InB.Get_Value().Y), FMath::Max(InA.Get_Value().Y, InB.Get_Value().Y)),
                FMath::Clamp(Raw.Z, FMath::Min(InA.Get_Value().Z, InB.Get_Value().Z), FMath::Max(InA.Get_Value().Z, InB.Get_Value().Z))};
        }

        // A track whose first key is after 0 moves from InCarried, the channel's value when the phase starts.
        auto
        SampleVector(
            const TArray<FVectorKey>& InKeys,
            double InTime,
            ECk_ProceduralAction_Interpolation InInterpolation,
            const FVector& InCarried)
            -> FVector
        {
            const auto& First = InKeys[0];
            if (First.Get_Time().Get_Value() > 0.0 && InTime < First.Get_Time().Get_Value())
            {
                const auto Carried = FVectorKey{FCk_FloatRange_0to1{0.0}, InCarried, FVector::ZeroVector, FVector::ZeroVector};
                return BlendVector(Carried, First, InTime, InInterpolation);
            }
            if (InKeys.Num() == 1 || InTime <= First.Get_Time().Get_Value())
            { return First.Get_Value(); }
            if (InTime >= InKeys.Last().Get_Time().Get_Value())
            { return InKeys.Last().Get_Value(); }
            const auto Index = Segment(InKeys, InTime);
            return BlendVector(InKeys[Index], InKeys[Index + 1], InTime, InInterpolation);
        }

        auto
        SampleRotation(
            const TArray<FOrientationKey>& InKeys,
            double InTime,
            const FQuat& InCarried)
            -> FQuat
        {
            const auto& First = InKeys[0];
            const auto FirstTime = First.Get_Time().Get_Value();
            if (FirstTime > 0.0 && InTime < FirstTime)
            {
                const auto Carried = (InCarried | First.Get_Value()) < 0.0 ? -InCarried : InCarried;
                return FQuat::Slerp(Carried, First.Get_Value(), FMath::Clamp(InTime / FirstTime, 0.0, 1.0)).GetNormalized();
            }
            if (InKeys.Num() == 1 || InTime <= FirstTime)
            { return First.Get_Value(); }
            if (InTime >= InKeys.Last().Get_Time().Get_Value())
            { return InKeys.Last().Get_Value(); }
            const auto Index = Segment(InKeys, InTime);
            const auto Span = InKeys[Index + 1].Get_Time().Get_Value() - InKeys[Index].Get_Time().Get_Value();
            const auto Alpha = Span > 0.0 ? FMath::Clamp((InTime - InKeys[Index].Get_Time().Get_Value()) / Span, 0.0, 1.0) : 0.0;
            return FQuat::Slerp(InKeys[Index].Get_Value(), InKeys[Index + 1].Get_Value(), Alpha).GetNormalized();
        }

        auto
        BlendBody(
            const FTransformKey& A,
            const FTransformKey& B,
            double InTime,
            ECk_ProceduralAction_Interpolation InInterpolation)
            -> FTransform
        {
            const auto Span = B.Get_Time().Get_Value() - A.Get_Time().Get_Value();
            const auto Alpha = Span > 0.0 ? FMath::Clamp((InTime - A.Get_Time().Get_Value()) / Span, 0.0, 1.0) : 0.0;
            auto Translation = FVector::ZeroVector;
            if (InInterpolation == ECk_ProceduralAction_Interpolation::Linear)
            { Translation = FMath::Lerp(A.Get_Translation(), B.Get_Translation(), Alpha); }
            else
            {
                const auto Raw = FMath::CubicInterp(A.Get_Translation(), A.Get_LeaveTangent() * Span,
                    B.Get_Translation(), B.Get_ArriveTangent() * Span, Alpha);
                Translation = FVector{FMath::Clamp(Raw.X, FMath::Min(A.Get_Translation().X, B.Get_Translation().X), FMath::Max(A.Get_Translation().X, B.Get_Translation().X)),
                    FMath::Clamp(Raw.Y, FMath::Min(A.Get_Translation().Y, B.Get_Translation().Y), FMath::Max(A.Get_Translation().Y, B.Get_Translation().Y)),
                    FMath::Clamp(Raw.Z, FMath::Min(A.Get_Translation().Z, B.Get_Translation().Z), FMath::Max(A.Get_Translation().Z, B.Get_Translation().Z))};
            }
            return FTransform{FQuat::Slerp(A.Get_Rotation(), B.Get_Rotation(), Alpha).GetNormalized(), Translation, FVector::OneVector};
        }

        auto
        SampleBody(
            const TArray<FTransformKey>& InKeys,
            double InTime,
            ECk_ProceduralAction_Interpolation InInterpolation,
            const FTransform& InCarried)
            -> FTransform
        {
            const auto& First = InKeys[0];
            if (First.Get_Time().Get_Value() > 0.0 && InTime < First.Get_Time().Get_Value())
            {
                const auto Rotation = (InCarried.GetRotation() | First.Get_Rotation()) < 0.0 ? -InCarried.GetRotation() : InCarried.GetRotation();
                const auto Carried = FTransformKey{FCk_FloatRange_0to1{0.0}, InCarried.GetTranslation(), Rotation,
                    FVector::ZeroVector, FVector::ZeroVector};
                return BlendBody(Carried, First, InTime, InInterpolation);
            }
            const auto& Last = InKeys.Last();
            if (InKeys.Num() == 1 || InTime >= Last.Get_Time().Get_Value())
            { return FTransform{Last.Get_Rotation().GetNormalized(), Last.Get_Translation(), FVector::OneVector}; }
            const auto Index = Segment(InKeys, InTime);
            return BlendBody(InKeys[Index], InKeys[Index + 1], InTime, InInterpolation);
        }

        auto
        AngularDeltaDegrees(
            const FQuat& InA,
            const FQuat& InB)
            -> double
        {
            const auto Dot = FMath::Clamp(FMath::Abs(InA | InB), 0.0, 1.0);
            return FMath::RadiansToDegrees(2.0 * FMath::Acos(Dot));
        }

        auto
        WithinAuthoredBudget(
            const FEvaluatedPose& InA,
            const FEvaluatedPose& InB,
            const FCk_ProceduralAction_Sampling& InBudget)
            -> bool
        {
            const auto MaxTravel = InBudget.Get_MaxEndpointTravelCm();
            const auto MaxAngle = InBudget.Get_MaxSegmentAngularStepDegrees();
            if (FVector::Dist(InA.Get_Body().GetTranslation(), InB.Get_Body().GetTranslation()) > MaxTravel
                || AngularDeltaDegrees(InA.Get_Body().GetRotation(), InB.Get_Body().GetRotation()) > MaxAngle)
            { return false; }
            if (InA.Get_Limbs().Num() != InB.Get_Limbs().Num())
            { return false; }
            for (auto Index = 0; Index < InA.Get_Limbs().Num(); ++Index)
            {
                if (InA.Get_Limbs()[Index].Get_Slot() != InB.Get_Limbs()[Index].Get_Slot()
                    || FVector::Dist(InA.Get_Limbs()[Index].Get_Position(), InB.Get_Limbs()[Index].Get_Position()) > MaxTravel
                    || (InA.Get_Limbs()[Index].Get_HasOrientation() && InB.Get_Limbs()[Index].Get_HasOrientation()
                        && AngularDeltaDegrees(InA.Get_Limbs()[Index].Get_Orientation(), InB.Get_Limbs()[Index].Get_Orientation()) > MaxAngle))
                { return false; }
            }
            return true;
        }
    }

    auto
    Evaluate(
        const FCompiledClip& InClip,
        const FInitialPose& InInitial,
        FCk_Time InTime,
        FEvaluatedPose& OutPose)
        -> bool
    {
        OutPose = {};
        const auto InTimeSeconds = InTime.Get_Seconds();
        if (InClip.Get_Phases().IsEmpty() || NOT FMath::IsFinite(InTimeSeconds)
            || InTimeSeconds < 0.0 || InTime > InClip.Get_Duration())
        { return false; }
        if (InClip.Get_BodyInitialSource() == ECk_ProceduralAction_InitialSource::CapturedAcceptedPose
            && NOT procedural_animation::IsRigid(InInitial.Get_Body(), procedural_animation::AuthoredRotationTolerance))
        { return false; }
        OutPose.Get_Body() = InClip.Get_BodyInitialSource() == ECk_ProceduralAction_InitialSource::IdentityOffset
            ? FTransform::Identity : InInitial.Get_Body();
        OutPose.Get_Time() = InTime;
        for (const auto& Slot : InClip.Get_Slots())
        {
            const auto* InitialLimb = InInitial.Get_Limbs().Find(Slot.Get_Id());
            if (InitialLimb == nullptr
                || NOT procedural_animation::IsRigid(*InitialLimb, procedural_animation::AuthoredRotationTolerance))
            { OutPose = {}; return false; }
            const auto& InitialTransform = *InitialLimb;
            auto Limb = FLimbPose{};
            Limb.Get_Slot() = Slot.Get_Id();
            Limb.Get_Position() = InitialTransform.GetTranslation();
            Limb.Get_Orientation() = InitialTransform.GetRotation();
            if (const auto* Bend = InInitial.Get_BendDirections().Find(Slot.Get_Id()))
            {
                if (NOT evaluation_detail::IsFinite(*Bend) || Bend->SizeSquared() <= 1.0e-8)
                { OutPose = {}; return false; }
                Limb.Get_BendDirection() = Bend->GetSafeNormal();
            }
            OutPose.Get_Limbs().Add(Limb);
        }
        for (auto PhaseIndex = 0; PhaseIndex < InClip.Get_Phases().Num(); ++PhaseIndex)
        {
            const auto& Phase = InClip.Get_Phases()[PhaseIndex];
            const auto PhaseEnd = Phase.Get_Start().Get_Seconds() + Phase.Get_Duration().Get_Seconds();
            const auto SampleTime = FMath::Min(InTimeSeconds, PhaseEnd);
            const auto Raw = FMath::Clamp((SampleTime - Phase.Get_Start().Get_Seconds()) / Phase.Get_Duration().Get_Seconds(), 0.0, 1.0);
            const auto Progress = EvaluateProgress(Phase, Raw);
            if (NOT Phase.Get_BodyKeys().IsEmpty())
            { OutPose.Get_Body() = evaluation_detail::SampleBody(Phase.Get_BodyKeys(), Progress, Phase.Get_BodyInterpolation(), OutPose.Get_Body()); }
            for (const auto& Track : Phase.Get_LimbTracks())
            {
                auto* Limb = OutPose.Get_Limbs().FindByPredicate([&](const FLimbPose& InLimb) { return InLimb.Get_Slot() == Track.Get_Slot(); });
                if (Limb == nullptr)
                { OutPose = {}; return false; }
                Limb->Get_ReachPolicy() = Track.Get_ReachPolicy();
                if (NOT Track.Get_PositionKeys().IsEmpty())
                { Limb->Get_Position() = evaluation_detail::SampleVector(Track.Get_PositionKeys(), Progress, Track.Get_Interpolation(), Limb->Get_Position()); }
                if (NOT Track.Get_OrientationKeys().IsEmpty())
                { Limb->Get_Orientation() = evaluation_detail::SampleRotation(Track.Get_OrientationKeys(), Progress, Limb->Get_Orientation()); Limb->Get_HasOrientation() = true; }
                if (NOT Track.Get_BendKeys().IsEmpty())
                {
                    const auto Bend = evaluation_detail::SampleVector(Track.Get_BendKeys(), Progress, Track.Get_Interpolation(), Limb->Get_BendDirection());
                    if (Bend.SizeSquared() <= 1.0e-8)
                    { OutPose = {}; return false; }
                    Limb->Get_BendDirection() = Bend.GetSafeNormal();
                    Limb->Get_HasBend() = true;
                }
            }
            OutPose.Get_PhaseIndex() = PhaseIndex;
            if (InTimeSeconds <= PhaseEnd)
            { break; }
        }
        return true;
    }

    auto
    SampleInterval(
        const FCompiledClip& InClip,
        const FInitialPose& InInitial,
        FCk_Time InStart,
        FCk_Time InRequestedEnd,
        const FCk_ProceduralAction_Sampling& InBudget,
        FSampledInterval& OutInterval)
        -> bool
    {
        OutInterval = {};
        const auto InStartSeconds = InStart.Get_Seconds();
        const auto InRequestedEndSeconds = InRequestedEnd.Get_Seconds();
        if (InClip.Get_Phases().IsEmpty() || NOT FMath::IsFinite(InStartSeconds) || NOT FMath::IsFinite(InRequestedEndSeconds)
            || InStartSeconds < 0.0 || InStart > InClip.Get_Duration() || InRequestedEnd < InStart
            || NOT FMath::IsFinite(InBudget.Get_MaxAdvance().Get_Seconds()) || InBudget.Get_MaxAdvance().Get_Seconds() <= 0.0
            || NOT FMath::IsFinite(InBudget.Get_MaxEndpointTravelCm()) || InBudget.Get_MaxEndpointTravelCm() <= 0.0f
            || NOT FMath::IsFinite(InBudget.Get_MaxSegmentAngularStepDegrees()) || InBudget.Get_MaxSegmentAngularStepDegrees() <= 0.0f
            || InBudget.Get_MaxSamples() <= 0)
        { return false; }
        auto Current = FEvaluatedPose{};
        if (NOT Evaluate(InClip, InInitial, InStart, Current))
        { return false; }
        auto Accepted = InStartSeconds;
        const auto Requested = FMath::Min(InRequestedEndSeconds, InClip.Get_Duration().Get_Seconds());
        while (Accepted < Requested && OutInterval.Get_Samples().Num() < InBudget.Get_MaxSamples())
        {
            auto End = FMath::Min(Requested, Accepted + InBudget.Get_MaxAdvance().Get_Seconds());
            for (const auto& Phase : InClip.Get_Phases())
            {
                const auto Boundary = (Phase.Get_Start() + Phase.Get_Duration()).Get_Seconds();
                if (Boundary > Accepted && Boundary < End)
                { End = Boundary; }
                for (const auto CriticalTime : Phase.Get_CriticalTimes())
                {
                    const auto Time = CriticalTime.Get_Seconds();
                    if (Time > Accepted && Time < End)
                    { End = Time; }
                }
            }
            for (const auto& Event : InClip.Get_Events())
            {
                const auto EventTime = Event.Get_Time().Get_Seconds();
                if (EventTime > Accepted && EventTime < End)
                { End = EventTime; }
            }
            if (End <= Accepted)
            { OutInterval = {}; return false; }
            auto Next = FEvaluatedPose{};
            if (NOT Evaluate(InClip, InInitial, FCk_Time{End}, Next))
            { OutInterval = {}; return false; }
            auto Refinements = 0;
            while (NOT evaluation_detail::WithinAuthoredBudget(Current, Next, InBudget) && Refinements++ < 32)
            {
                End = (Accepted + End) * 0.5;
                if (End <= Accepted || NOT Evaluate(InClip, InInitial, FCk_Time{End}, Next))
                { OutInterval = {}; return false; }
            }
            if (NOT evaluation_detail::WithinAuthoredBudget(Current, Next, InBudget))
            { OutInterval = {}; return false; }
            for (const auto& Event : InClip.Get_Events())
            {
                const auto EventTime = Event.Get_Time().Get_Seconds();
                if ((EventTime > Accepted
                    || (Accepted == 0.0 && OutInterval.Get_Samples().IsEmpty() && EventTime == 0.0))
                    && EventTime <= End)
                { OutInterval.Get_CrossedEvents().Add(Event); }
            }
            OutInterval.Get_Samples().Add(MoveTemp(Next));
            Accepted = End;
            Current = OutInterval.Get_Samples().Last();
            for (const auto& Phase : InClip.Get_Phases())
            {
                if (End == (Phase.Get_Start() + Phase.Get_Duration()).Get_Seconds()
                    && End < InClip.Get_Duration().Get_Seconds())
                { OutInterval.Get_StoppedAtPhaseBoundary() = true; break; }
            }
            for (const auto& Event : InClip.Get_Events())
            {
                if (Event.Get_Time().Get_Seconds() == End && Event.Get_Kind() != EEventKind::Point)
                { OutInterval.Get_StoppedAtWindowBoundary() = true; break; }
            }
            if (OutInterval.Get_StoppedAtPhaseBoundary() || OutInterval.Get_StoppedAtWindowBoundary())
            { break; }
        }
        OutInterval.Get_AcceptedEnd() = FCk_Time{Accepted};
        OutInterval.Get_Backlog() = FCk_Time{FMath::Max(0.0, Requested - Accepted)};
        OutInterval.Get_BudgetExhausted() = OutInterval.Get_Samples().Num() >= InBudget.Get_MaxSamples()
            && Accepted < Requested;
        return true;
    }
}
