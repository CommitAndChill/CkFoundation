#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Utils.h"

#include "CkProceduralAnimation/Core/CkProceduralActionReport.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "UObject/StrongObjectPtr.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action::builder_detail
{
    inline constexpr auto DraftAssetPath = TEXT("<draft>");
    inline constexpr auto UnitTolerance = 1.0e-4;

    auto
    AddError(
        TArray<FDiagnostic>& OutDiagnostics,
        FName InCode,
        const FString& InField,
        FName InPhase,
        FName InSlot,
        int32 InKey,
        const FString& InExpected,
        const FString& InActual)
        -> void
    {
        if (OutDiagnostics.Num() >= content_limits::MaxDiagnostics)
        { return; }
        OutDiagnostics.Add(FDiagnostic{EDiagnosticSeverity::Error, InCode, DraftAssetPath, InField,
            InPhase, InSlot, InKey, InExpected, InActual});
    }

    auto
    Reject(
        const TArray<FDiagnostic>& InDiagnostics)
        -> FCk_ProceduralAction_ValidationReport
    { return MakePublicReport(ECk_ProceduralAction_ReportStatus::Invalid, false, InDiagnostics); }

    auto
    Accept()
        -> FCk_ProceduralAction_ValidationReport
    { return MakePublicReport(ECk_ProceduralAction_ReportStatus::Valid, false, {}); }

    auto
    IsFinite(
        const FVector& InValue)
        -> bool
    { return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z); }

    auto
    IsUnit(
        const FQuat& InValue)
        -> bool
    {
        return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z)
            && FMath::IsFinite(InValue.W) && FMath::Abs(InValue.SizeSquared() - 1.0) <= UnitTolerance;
    }

    auto
    IsUnitDirection(
        const FVector& InValue)
        -> bool
    { return IsFinite(InValue) && FMath::IsNearlyEqual(InValue.SizeSquared(), 1.0, UnitTolerance); }

    auto
    IsRigid(
        const FTransform& InValue)
        -> bool
    {
        return IsFinite(InValue.GetTranslation()) && IsUnit(InValue.GetRotation())
            && InValue.GetScale3D().Equals(FVector::OneVector, 1.0e-6);
    }

    auto
    IsUnitTime(
        double InTime)
        -> bool
    { return FMath::IsFinite(InTime) && InTime >= 0.0 && InTime <= 1.0; }

    auto
    PhasePath(
        int32 InPhaseIndex)
        -> FString
    { return FString::Printf(TEXT("Phases[%d]"), InPhaseIndex); }

    auto
    TrackPath(
        int32 InPhaseIndex,
        int32 InTrackIndex)
        -> FString
    { return FString::Printf(TEXT("Phases[%d].LimbTracks[%d]"), InPhaseIndex, InTrackIndex); }

    auto
    FindPhase(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId)
        -> int32
    {
        if (InPhaseId.IsNone())
        { return INDEX_NONE; }
        return InDraft.Get_Phases().IndexOfByPredicate([InPhaseId](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    }

    auto
    HasSlot(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        FName InSlot)
        -> bool
    {
        return NOT InSlot.IsNone() && InDraft.Get_Slots().ContainsByPredicate([InSlot](const FCk_ProceduralAction_Slot& InCandidate)
        { return InCandidate.Get_Id() == InSlot; });
    }

    auto
    FindTrack(
        const FCk_ProceduralAction_Phase& InPhase,
        FName InSlot)
        -> int32
    {
        return InPhase.Get_LimbTracks().IndexOfByPredicate([InSlot](const FCk_ProceduralAction_LimbTrack& InTrack)
        { return InTrack.Get_Slot() == InSlot; });
    }

    auto
    CountKeys(
        const TArray<FCk_ProceduralAction_Phase>& InPhases)
        -> int64
    {
        auto Count = int64{0};
        for (const auto& Phase : InPhases)
        {
            Count += Phase.Get_BodyKeys().Num();
            for (const auto& Track : Phase.Get_LimbTracks())
            {
                Count += static_cast<int64>(Track.Get_PositionKeys().Num())
                    + Track.Get_OrientationKeys().Num() + Track.Get_BendKeys().Num();
            }
        }
        return Count;
    }

    struct FSlotFrame
    {
        ECk_ProceduralAction_TrackSpace Space = ECk_ProceduralAction_TrackSpace::BodyLocal;
        FName AnchorId;
    };

    // The frame a slot's tracks already use, preferring InPreferred (a phase being edited) over the draft's phases.
    auto
    Get_SlotFrame(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Phase* InPreferred,
        FName InSlot)
        -> FSlotFrame
    {
        const auto FrameOf = [InSlot](const FCk_ProceduralAction_Phase& InPhase, FSlotFrame& OutFrame) -> bool
        {
            const auto Index = FindTrack(InPhase, InSlot);
            if (Index == INDEX_NONE)
            { return false; }
            OutFrame = FSlotFrame{InPhase.Get_LimbTracks()[Index].Get_Space(), InPhase.Get_LimbTracks()[Index].Get_AnchorId()};
            return true;
        };
        auto Frame = FSlotFrame{};
        if (InPreferred != nullptr && FrameOf(*InPreferred, Frame))
        { return Frame; }
        for (const auto& Phase : InDraft.Get_Phases())
        {
            if (FrameOf(Phase, Frame))
            { return Frame; }
        }
        return Frame;
    }

    // Index a key at InTime would occupy, or INDEX_NONE when another key already sits at exactly that time.
    template <typename TKey>
    auto
    Get_InsertionIndex(
        const TArray<TKey>& InKeys,
        double InTime,
        int32& OutOccupiedIndex)
        -> int32
    {
        OutOccupiedIndex = InKeys.IndexOfByPredicate([InTime](const TKey& InKey)
        { return InKey.Get_Time().Get_Value() == InTime; });
        if (OutOccupiedIndex != INDEX_NONE)
        { return INDEX_NONE; }
        const auto Later = InKeys.IndexOfByPredicate([InTime](const TKey& InKey)
        { return InKey.Get_Time().Get_Value() > InTime; });
        return Later == INDEX_NONE ? InKeys.Num() : Later;
    }

    template <typename TKey>
    auto
    Get_KeyAt(
        const TArray<TKey>& InKeys,
        double InTime)
        -> int32
    {
        return InKeys.IndexOfByPredicate([InTime](const TKey& InKey)
        { return InKey.Get_Time().Get_Value() == InTime; });
    }

    struct FKeyBudget
    {
        int64 Remaining = 0;
        bool Exceeded = false;

        auto
        Take(
            TArray<FDiagnostic>& OutDiagnostics)
            -> bool
        {
            if (--Remaining >= 0)
            { return true; }
            if (NOT Exceeded)
            {
                AddError(OutDiagnostics, TEXT("ContentLimit"), TEXT("Phases.Keys"), {}, {}, INDEX_NONE,
                    FString::Printf(TEXT("at most %lld"), content_limits::MaxKeys), TEXT("one more key"));
            }
            Exceeded = true;
            return false;
        }
    };

    auto
    Make_BudgetFor(
        const FCk_ProceduralAction_ClipDraft& InDraft)
        -> FKeyBudget
    { return FKeyBudget{content_limits::MaxKeys - CountKeys(InDraft.Get_Phases())}; }

    template <typename TKey>
    auto
    InsertKey(
        TArray<TKey>& InOutKeys,
        double InTime,
        const TKey& InKey,
        const FString& InArrayPath,
        FName InPhaseId,
        FName InSlot,
        TArray<FDiagnostic>& OutDiagnostics)
        -> int32
    {
        auto Occupied = int32{INDEX_NONE};
        const auto Index = Get_InsertionIndex(InOutKeys, InTime, Occupied);
        if (Index == INDEX_NONE)
        {
            AddError(OutDiagnostics, TEXT("KeyTime"), FString::Printf(TEXT("%s[%d].Time"), *InArrayPath, Occupied),
                InPhaseId, InSlot, Occupied, TEXT("a time not already keyed"), FString::SanitizeFloat(InTime));
            return INDEX_NONE;
        }
        InOutKeys.Insert(InKey, Index);
        return Index;
    }

    // Validates one limb target and writes its keys into InOutPhase (a caller-owned copy that is discarded on any error).
    auto
    DoAdd_LimbTarget(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        int32 InPhaseIndex,
        FCk_ProceduralAction_Phase& InOutPhase,
        double InTime,
        const FCk_ProceduralAction_LimbPoseTarget& InTarget,
        const FString& InTargetPath,
        FKeyBudget& InOutBudget,
        TArray<FDiagnostic>& OutDiagnostics)
        -> void
    {
        const auto PhaseId = InOutPhase.Get_Id();
        const auto Slot = InTarget.Get_Slot();
        if (NOT HasSlot(InDraft, Slot))
        {
            AddError(OutDiagnostics, TEXT("UnknownSlot"), InTargetPath + TEXT(".Slot"), PhaseId, Slot, INDEX_NONE,
                TEXT("a declared slot"), Slot.ToString());
            return;
        }
        auto Tracks = InOutPhase.Get_LimbTracks();
        auto TrackIndex = FindTrack(InOutPhase, Slot);
        if (TrackIndex == INDEX_NONE)
        {
            if (Tracks.Num() >= content_limits::MaxTracksPerPhase)
            {
                AddError(OutDiagnostics, TEXT("ContentLimit"), PhasePath(InPhaseIndex) + TEXT(".LimbTracks"), PhaseId, Slot,
                    INDEX_NONE, FString::Printf(TEXT("at most %d"), content_limits::MaxTracksPerPhase), TEXT("one more track"));
                return;
            }
            const auto Frame = Get_SlotFrame(InDraft, &InOutPhase, Slot);
            auto Track = FCk_ProceduralAction_LimbTrack{Slot};
            Track.Set_Space(Frame.Space);
            Track.Set_AnchorId(Frame.AnchorId);
            TrackIndex = Tracks.Add(MoveTemp(Track));
        }
        auto& Track = Tracks[TrackIndex];
        const auto Path = TrackPath(InPhaseIndex, TrackIndex);
        const auto Time = FCk_FloatRange_0to1{InTime};

        auto Positions = Track.Get_PositionKeys();
        auto Occupied = int32{INDEX_NONE};
        const auto PositionIndex = Get_InsertionIndex(Positions, InTime, Occupied);
        if (NOT IsFinite(InTarget.Get_Position()))
        {
            AddError(OutDiagnostics, TEXT("VectorKey"), FString::Printf(TEXT("%s.PositionKeys[%d]"), *Path,
                PositionIndex == INDEX_NONE ? Occupied : PositionIndex), PhaseId, Slot, INDEX_NONE, TEXT("finite value/tangents"), TEXT("malformed key"));
        }
        else if (InOutBudget.Take(OutDiagnostics))
        {
            InsertKey(Positions, InTime, FCk_ProceduralAction_VectorKey{Time, InTarget.Get_Position()},
                Path + TEXT(".PositionKeys"), PhaseId, Slot, OutDiagnostics);
        }
        Track.Set_PositionKeys(Positions);

        if (InTarget.Get_OrientationMode() == ECk_ProceduralAction_ValueMode::Set)
        {
            auto Orientations = Track.Get_OrientationKeys();
            const auto OrientationIndex = Get_InsertionIndex(Orientations, InTime, Occupied);
            if (NOT IsUnit(InTarget.Get_Orientation()))
            {
                AddError(OutDiagnostics, TEXT("Quaternion"), FString::Printf(TEXT("%s.OrientationKeys[%d].Value"), *Path,
                    OrientationIndex == INDEX_NONE ? Occupied : OrientationIndex), PhaseId, Slot, INDEX_NONE,
                    TEXT("finite unit quaternion"), TEXT("malformed quaternion"));
            }
            else if (InOutBudget.Take(OutDiagnostics))
            {
                InsertKey(Orientations, InTime, FCk_ProceduralAction_OrientationKey{Time, InTarget.Get_Orientation()},
                    Path + TEXT(".OrientationKeys"), PhaseId, Slot, OutDiagnostics);
            }
            Track.Set_OrientationKeys(Orientations);
        }

        if (InTarget.Get_BendMode() == ECk_ProceduralAction_ValueMode::Set)
        {
            auto Bends = Track.Get_BendKeys();
            const auto BendIndex = Get_InsertionIndex(Bends, InTime, Occupied);
            if (NOT IsUnitDirection(InTarget.Get_Bend()))
            {
                AddError(OutDiagnostics, TEXT("VectorKey"), FString::Printf(TEXT("%s.BendKeys[%d]"), *Path,
                    BendIndex == INDEX_NONE ? Occupied : BendIndex), PhaseId, Slot, INDEX_NONE,
                    TEXT("unit nonzero direction"), TEXT("malformed key"));
            }
            else if (InOutBudget.Take(OutDiagnostics))
            {
                InsertKey(Bends, InTime, FCk_ProceduralAction_VectorKey{Time, InTarget.Get_Bend()},
                    Path + TEXT(".BendKeys"), PhaseId, Slot, OutDiagnostics);
            }
            Track.Set_BendKeys(Bends);
        }
        InOutPhase.Set_LimbTracks(Tracks);
    }

    auto
    DoAdd_BodyKey(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        int32 InPhaseIndex,
        FCk_ProceduralAction_Phase& InOutPhase,
        const FCk_ProceduralAction_TransformKey& InKey,
        FKeyBudget& InOutBudget,
        TArray<FDiagnostic>& OutDiagnostics)
        -> void
    {
        const auto PhaseId = InOutPhase.Get_Id();
        if (NOT InDraft.Get_BodyChannel().Get_Enabled())
        {
            AddError(OutDiagnostics, TEXT("BodyDisabled"), TEXT("BodyChannel.Enabled"), PhaseId, {}, INDEX_NONE,
                TEXT("an enabled body channel"), TEXT("disabled"));
            return;
        }
        auto Keys = InOutPhase.Get_BodyKeys();
        const auto Time = InKey.Get_Time().Get_Value();
        auto Occupied = int32{INDEX_NONE};
        const auto Index = Get_InsertionIndex(Keys, Time, Occupied);
        const auto KeyPath = FString::Printf(TEXT("%s.BodyKeys[%d]"), *PhasePath(InPhaseIndex), Index == INDEX_NONE ? Occupied : Index);
        if (NOT IsUnitTime(Time))
        {
            AddError(OutDiagnostics, TEXT("KeyTime"), KeyPath + TEXT(".Time"), PhaseId, {}, INDEX_NONE,
                TEXT("finite [0,1]"), FString::SanitizeFloat(Time));
            return;
        }
        if (NOT IsFinite(InKey.Get_Translation()) || NOT IsUnit(InKey.Get_Rotation())
            || NOT IsFinite(InKey.Get_ArriveTangent()) || NOT IsFinite(InKey.Get_LeaveTangent()))
        {
            AddError(OutDiagnostics, TEXT("TransformKey"), KeyPath, PhaseId, {}, INDEX_NONE,
                TEXT("finite translation/tangents and unit quaternion"), TEXT("malformed key"));
            return;
        }
        if (NOT InOutBudget.Take(OutDiagnostics))
        { return; }
        InsertKey(Keys, Time, InKey, PhasePath(InPhaseIndex) + TEXT(".BodyKeys"), PhaseId, {}, OutDiagnostics);
        InOutPhase.Set_BodyKeys(Keys);
    }

    auto
    ValidatePoseShape(
        const FCk_ProceduralAction_PoseKey& InPose,
        TArray<FDiagnostic>& OutDiagnostics)
        -> void
    {
        if (NOT IsUnitTime(InPose.Get_Time().Get_Value()))
        {
            AddError(OutDiagnostics, TEXT("KeyTime"), TEXT("Pose.Time"), {}, {}, INDEX_NONE, TEXT("finite [0,1]"),
                FString::SanitizeFloat(InPose.Get_Time().Get_Value()));
        }
        if (InPose.Get_BodyMode() != ECk_ProceduralAction_ValueMode::Set && InPose.Get_Limbs().IsEmpty())
        {
            AddError(OutDiagnostics, TEXT("EmptyPose"), TEXT("Pose"), {}, {}, INDEX_NONE,
                TEXT("a body value or at least one limb target"), TEXT("nothing to write"));
        }
        auto Seen = TSet<FName>{};
        for (auto Index = 0; Index < InPose.Get_Limbs().Num(); ++Index)
        {
            const auto Slot = InPose.Get_Limbs()[Index].Get_Slot();
            if (Seen.Contains(Slot))
            {
                AddError(OutDiagnostics, TEXT("DuplicateSlot"), FString::Printf(TEXT("Pose.Limbs[%d].Slot"), Index), {}, Slot,
                    INDEX_NONE, TEXT("each slot at most once per pose"), Slot.ToString());
            }
            Seen.Add(Slot);
        }
    }

    auto
    Get_PhaseStart(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        int32 InPhaseIndex)
        -> double
    {
        auto Start = 0.0;
        for (auto Index = 0; Index < InPhaseIndex; ++Index)
        { Start += InDraft.Get_Phases()[Index].Get_Duration().Get_Seconds(); }
        return Start;
    }

    auto
    ResolveTimeRef(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_TimeRef& InRef,
        const FString& InPath,
        TArray<FDiagnostic>& OutDiagnostics,
        double& OutTime)
        -> bool
    {
        const auto PhaseIndex = FindPhase(InDraft, InRef.Get_PhaseId());
        if (PhaseIndex == INDEX_NONE)
        {
            AddError(OutDiagnostics, TEXT("PhaseReference"), InPath, InRef.Get_PhaseId(), {}, INDEX_NONE,
                TEXT("declared phase ID"), InRef.Get_PhaseId().ToString());
            return false;
        }
        const auto Normalized = InRef.Get_Time().Get_Value();
        if (NOT IsUnitTime(Normalized))
        {
            AddError(OutDiagnostics, TEXT("TimeRef"), InPath + TEXT(".Time"), InRef.Get_PhaseId(), {}, INDEX_NONE,
                TEXT("finite normalized time in [0,1]"), FString::SanitizeFloat(Normalized));
            return false;
        }
        OutTime = Get_PhaseStart(InDraft, PhaseIndex)
            + InDraft.Get_Phases()[PhaseIndex].Get_Duration().Get_Seconds() * Normalized;
        return FMath::IsFinite(OutTime);
    }

    auto
    MirrorVectorKeys(
        const TArray<FCk_ProceduralAction_VectorKey>& InKeys,
        ECk_ProceduralAction_MirrorPlane InPlane)
        -> TArray<FCk_ProceduralAction_VectorKey>
    {
        auto Keys = InKeys;
        for (auto& Key : Keys)
        {
            Key.Set_Value(ReflectVector(Key.Get_Value(), InPlane));
            Key.Set_ArriveTangent(ReflectVector(Key.Get_ArriveTangent(), InPlane));
            Key.Set_LeaveTangent(ReflectVector(Key.Get_LeaveTangent(), InPlane));
        }
        return Keys;
    }

    auto
    Make_TwoKeyPhase(
        FName InId,
        FCk_Time InDuration,
        ECk_ProceduralAction_Easing InEasing)
        -> FCk_ProceduralAction_Phase
    {
        auto Phase = FCk_ProceduralAction_Phase{InId, InDuration};
        Phase.Set_Easing(InEasing);
        return Phase;
    }

    auto
    Make_ReachTrack(
        FName InSlot,
        ECk_ProceduralAction_TrackSpace InSpace,
        TArray<FCk_ProceduralAction_VectorKey> InKeys)
        -> FCk_ProceduralAction_LimbTrack
    {
        ComputeCatmullRomTangents(InKeys);
        auto Track = FCk_ProceduralAction_LimbTrack{InSlot};
        Track.Set_Space(InSpace);
        Track.Set_Interpolation(ECk_ProceduralAction_Interpolation::Smooth);
        Track.Set_PositionKeys(InKeys);
        return Track;
    }

    auto
    ValidateRecipe(
        const FCk_ProceduralAction_RecipeSpec& InRecipe,
        TArray<FDiagnostic>& OutDiagnostics)
        -> void
    {
        const auto& Slots = InRecipe.Get_Slots();
        if (Slots.Num() > content_limits::MaxSlots)
        {
            AddError(OutDiagnostics, TEXT("ContentLimit"), TEXT("Recipe.Slots"), {}, {}, INDEX_NONE,
                FString::Printf(TEXT("at most %d"), content_limits::MaxSlots), FString::FromInt(Slots.Num()));
            return;
        }
        auto Seen = TSet<FName>{};
        for (auto Index = 0; Index < Slots.Num(); ++Index)
        {
            if (Slots[Index].IsNone() || Seen.Contains(Slots[Index]))
            {
                AddError(OutDiagnostics, TEXT("SlotId"), FString::Printf(TEXT("Recipe.Slots[%d]"), Index), {}, Slots[Index],
                    INDEX_NONE, TEXT("unique nonempty ID"), Slots[Index].ToString());
            }
            Seen.Add(Slots[Index]);
        }
        const auto Recipe = InRecipe.Get_Recipe();
        switch (Recipe)
        {
        case ECk_ProceduralAction_Recipe::Blank:
            break;
        case ECk_ProceduralAction_Recipe::BodyLean:
            if (NOT Slots.IsEmpty())
            {
                AddError(OutDiagnostics, TEXT("RecipeSlots"), TEXT("Recipe.Slots"), {}, {}, INDEX_NONE,
                    TEXT("no slots: BodyLean moves the body only"), FString::FromInt(Slots.Num()));
            }
            break;
        case ECk_ProceduralAction_Recipe::Reach:
        case ECk_ProceduralAction_Recipe::Sweep:
            if (Slots.Num() != 1)
            {
                AddError(OutDiagnostics, TEXT("RecipeSlots"), TEXT("Recipe.Slots"), {}, {}, INDEX_NONE,
                    TEXT("exactly one participant slot"), FString::FromInt(Slots.Num()));
            }
            break;
        default:
            AddError(OutDiagnostics, TEXT("Recipe"), TEXT("Recipe.Recipe"), {}, {}, INDEX_NONE,
                TEXT("Blank, BodyLean, Reach or Sweep"), TEXT("unsupported recipe"));
            return;
        }
        if (Recipe == ECk_ProceduralAction_Recipe::Blank)
        {
            if (NOT FMath::IsFinite(InRecipe.Get_Execute().Get_Seconds()) || InRecipe.Get_Execute().Get_Seconds() <= 0.0)
            {
                AddError(OutDiagnostics, TEXT("Duration"), TEXT("Recipe.Execute"), {}, {}, INDEX_NONE,
                    TEXT("finite positive duration"), FString::SanitizeFloat(InRecipe.Get_Execute().Get_Seconds()));
            }
            return;
        }
        if (NOT IsUnitDirection(InRecipe.Get_Direction()))
        {
            AddError(OutDiagnostics, TEXT("RecipeDirection"), TEXT("Recipe.Direction"), {}, {}, INDEX_NONE,
                TEXT("finite unit direction"), InRecipe.Get_Direction().ToString());
        }
        else if (Recipe == ECk_ProceduralAction_Recipe::Sweep
            && FVector::CrossProduct(InRecipe.Get_Direction(), FVector::UpVector).SizeSquared() <= UnitTolerance)
        {
            AddError(OutDiagnostics, TEXT("RecipeDirection"), TEXT("Recipe.Direction"), {}, {}, INDEX_NONE,
                TEXT("a direction with a component across body up"), InRecipe.Get_Direction().ToString());
        }
        if (NOT FMath::IsFinite(InRecipe.Get_AmplitudeCm()) || InRecipe.Get_AmplitudeCm() <= 0.0f)
        {
            AddError(OutDiagnostics, TEXT("RecipeAmplitude"), TEXT("Recipe.AmplitudeCm"), {}, {}, INDEX_NONE,
                TEXT("finite positive amplitude"), FString::SanitizeFloat(InRecipe.Get_AmplitudeCm()));
        }
        const auto UsesThreePhases = Recipe != ECk_ProceduralAction_Recipe::Sweep;
        const auto CheckDuration = [&](const FCk_Time& InDuration, const TCHAR* InField) -> void
        {
            if (NOT FMath::IsFinite(InDuration.Get_Seconds()) || InDuration.Get_Seconds() <= 0.0)
            {
                AddError(OutDiagnostics, TEXT("Duration"), FString::Printf(TEXT("Recipe.%s"), InField), {}, {}, INDEX_NONE,
                    TEXT("finite positive duration"), FString::SanitizeFloat(InDuration.Get_Seconds()));
            }
        };
        CheckDuration(InRecipe.Get_Execute(), TEXT("Execute"));
        if (UsesThreePhases)
        {
            CheckDuration(InRecipe.Get_Prepare(), TEXT("Prepare"));
            CheckDuration(InRecipe.Get_Recover(), TEXT("Recover"));
        }
        if (Recipe != ECk_ProceduralAction_Recipe::BodyLean
            && InRecipe.Get_Space() != ECk_ProceduralAction_TrackSpace::BodyLocal
            && InRecipe.Get_Space() != ECk_ProceduralAction_TrackSpace::RootAtStart
            && InRecipe.Get_Space() != ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
        {
            AddError(OutDiagnostics, TEXT("TrackSpace"), TEXT("Recipe.Space"), {}, {}, INDEX_NONE,
                TEXT("BodyLocal, RootAtStart or LimbRestNormalized (an anchor needs Set_SlotFrame)"), TEXT("unsupported space"));
        }
    }

    auto
    Make_RecipeDraft(
        const FCk_ProceduralAction_RecipeSpec& InRecipe)
        -> FCk_ProceduralAction_ClipDraft
    {
        auto Draft = FCk_ProceduralAction_ClipDraft{};
        const auto Amplitude = static_cast<double>(InRecipe.Get_AmplitudeCm());
        const auto Direction = InRecipe.Get_Direction();
        const auto Easing = InRecipe.Get_Easing();
        auto Phases = TArray<FCk_ProceduralAction_Phase>{};
        switch (InRecipe.Get_Recipe())
        {
        case ECk_ProceduralAction_Recipe::Blank:
        {
            if (InRecipe.Get_Slots().IsEmpty())
            {
                auto Body = FCk_ProceduralAction_BodyChannel{};
                Body.Set_Enabled(true);
                Body.Set_InitialSource(ECk_ProceduralAction_InitialSource::IdentityOffset);
                Draft.Set_BodyChannel(Body);
            }
            Phases.Add(Make_TwoKeyPhase(TEXT("Motion"), InRecipe.Get_Execute(), Easing));
            break;
        }
        case ECk_ProceduralAction_Recipe::BodyLean:
        {
            auto Body = FCk_ProceduralAction_BodyChannel{};
            Body.Set_Enabled(true);
            Body.Set_InitialSource(ECk_ProceduralAction_InitialSource::IdentityOffset);
            Draft.Set_BodyChannel(Body);
            const auto Pullback = -0.25 * Amplitude * Direction;
            const auto Lunge = Amplitude * Direction;
            const auto MakeKeys = [](const FVector& InFrom, const FVector& InTo)
            {
                return TArray<FCk_ProceduralAction_TransformKey>{
                    FCk_ProceduralAction_TransformKey{FCk_FloatRange_0to1{0.0}, InFrom, FQuat::Identity},
                    FCk_ProceduralAction_TransformKey{FCk_FloatRange_0to1{1.0}, InTo, FQuat::Identity}};
            };
            auto Prepare = Make_TwoKeyPhase(TEXT("Prepare"), InRecipe.Get_Prepare(), Easing);
            Prepare.Set_BodyKeys(MakeKeys(FVector::ZeroVector, Pullback));
            auto Execute = Make_TwoKeyPhase(TEXT("Execute"), InRecipe.Get_Execute(), Easing);
            Execute.Set_BodyKeys(MakeKeys(Pullback, Lunge));
            auto Recover = Make_TwoKeyPhase(TEXT("Recover"), InRecipe.Get_Recover(), Easing);
            Recover.Set_BodyKeys(MakeKeys(Lunge, FVector::ZeroVector));
            Phases = {Prepare, Execute, Recover};
            break;
        }
        case ECk_ProceduralAction_Recipe::Reach:
        {
            const auto Slot = InRecipe.Get_Slots()[0];
            const auto Rest = FVector::ZeroVector;
            const auto Reached = Rest + Amplitude * Direction;
            const auto MakeKeys = [](const FVector& InFrom, const FVector& InTo)
            {
                return TArray<FCk_ProceduralAction_VectorKey>{
                    FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{0.0}, InFrom},
                    FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{1.0}, InTo}};
            };
            auto Prepare = Make_TwoKeyPhase(TEXT("Prepare"), InRecipe.Get_Prepare(), Easing);
            Prepare.Set_LimbTracks({Make_ReachTrack(Slot, InRecipe.Get_Space(), MakeKeys(Rest, Rest))});
            auto Execute = Make_TwoKeyPhase(TEXT("Execute"), InRecipe.Get_Execute(), Easing);
            Execute.Set_LimbTracks({Make_ReachTrack(Slot, InRecipe.Get_Space(), MakeKeys(Rest, Reached))});
            auto Recover = Make_TwoKeyPhase(TEXT("Recover"), InRecipe.Get_Recover(), Easing);
            Recover.Set_LimbTracks({Make_ReachTrack(Slot, InRecipe.Get_Space(), MakeKeys(Reached, Rest))});
            Phases = {Prepare, Execute, Recover};
            break;
        }
        case ECk_ProceduralAction_Recipe::Sweep:
        {
            const auto Slot = InRecipe.Get_Slots()[0];
            const auto Rest = FVector::ZeroVector;
            // Quarter and half turns about body up, exact: an axis-angle quaternion carries SinCos approximation error.
            const auto Apex = FVector{-Direction.Y, Direction.X, Direction.Z};
            const auto End = FVector{-Direction.X, -Direction.Y, Direction.Z};
            auto Execute = Make_TwoKeyPhase(TEXT("Execute"), InRecipe.Get_Execute(), Easing);
            Execute.Set_LimbTracks({Make_ReachTrack(Slot, InRecipe.Get_Space(), TArray<FCk_ProceduralAction_VectorKey>{
                FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{0.0}, Rest + Amplitude * Direction},
                FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{0.5}, Rest + Amplitude * Apex},
                FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{1.0}, Rest + Amplitude * End}})});
            Phases = {Execute};
            break;
        }
        default:
            break;
        }
        auto Slots = TArray<FCk_ProceduralAction_Slot>{};
        for (const auto Slot : InRecipe.Get_Slots())
        { Slots.Emplace(Slot); }
        Draft.Set_Slots(Slots);
        Draft.Set_Phases(Phases);
        return Draft;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Set_BodyChannel(
        FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_BodyChannel& InChannel)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    if (InChannel.Get_Enabled() && InChannel.Get_InitialSource() != ECk_ProceduralAction_InitialSource::CapturedAcceptedPose
        && InChannel.Get_InitialSource() != ECk_ProceduralAction_InitialSource::IdentityOffset)
    {
        builder_detail::AddError(Diagnostics, TEXT("InitialSource"), TEXT("BodyChannel.InitialSource"), {}, {}, INDEX_NONE,
            TEXT("CapturedAcceptedPose or IdentityOffset"), TEXT("unsupported source"));
    }
    if (NOT InChannel.Get_Enabled())
    {
        for (auto Index = 0; Index < InDraft.Get_Phases().Num(); ++Index)
        {
            if (InDraft.Get_Phases()[Index].Get_BodyKeys().IsEmpty())
            { continue; }
            builder_detail::AddError(Diagnostics, TEXT("BodyDisabled"), builder_detail::PhasePath(Index) + TEXT(".BodyKeys"),
                InDraft.Get_Phases()[Index].Get_Id(), {}, INDEX_NONE, TEXT("empty without body channel"), TEXT("authored keys"));
        }
    }
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }
    InDraft.Set_BodyChannel(InChannel);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Set_SlotFrame(
        FCk_ProceduralAction_ClipDraft& InDraft,
        FName InSlot,
        ECk_ProceduralAction_TrackSpace InSpace,
        FName InAnchorId)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    if (NOT builder_detail::HasSlot(InDraft, InSlot))
    {
        builder_detail::AddError(Diagnostics, TEXT("UnknownSlot"), TEXT("Slot"), {}, InSlot, INDEX_NONE,
            TEXT("a declared slot"), InSlot.ToString());
    }
    const auto SpaceIsKnown = InSpace == ECk_ProceduralAction_TrackSpace::BodyLocal || InSpace == ECk_ProceduralAction_TrackSpace::RootAtStart
        || InSpace == ECk_ProceduralAction_TrackSpace::AnchorAtStart || InSpace == ECk_ProceduralAction_TrackSpace::LimbRestNormalized;
    if (NOT SpaceIsKnown || (InSpace == ECk_ProceduralAction_TrackSpace::AnchorAtStart) == InAnchorId.IsNone())
    {
        builder_detail::AddError(Diagnostics, TEXT("TrackSpace"), TEXT("Space"), {}, InSlot, INDEX_NONE,
            TEXT("supported space with anchor ID iff AnchorAtStart"), TEXT("invalid space/anchor pair"));
    }
    auto Phases = InDraft.Get_Phases();
    auto HasTrack = false;
    for (const auto& Phase : Phases)
    { HasTrack |= builder_detail::FindTrack(Phase, InSlot) != INDEX_NONE; }
    if (NOT HasTrack)
    {
        if (Phases.IsEmpty())
        {
            builder_detail::AddError(Diagnostics, TEXT("EmptyPhases"), TEXT("Phases"), {}, InSlot, INDEX_NONE,
                TEXT("a phase to hold the slot frame"), TEXT("empty"));
        }
        else if (Phases[0].Get_LimbTracks().Num() >= content_limits::MaxTracksPerPhase)
        {
            builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), TEXT("Phases[0].LimbTracks"), Phases[0].Get_Id(), InSlot,
                INDEX_NONE, FString::Printf(TEXT("at most %d"), content_limits::MaxTracksPerPhase), TEXT("one more track"));
        }
    }
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }

    for (auto& Phase : Phases)
    {
        auto Tracks = Phase.Get_LimbTracks();
        for (auto& Track : Tracks)
        {
            if (Track.Get_Slot() != InSlot)
            { continue; }
            Track.Set_Space(InSpace);
            Track.Set_AnchorId(InAnchorId);
        }
        Phase.Set_LimbTracks(Tracks);
    }
    if (NOT HasTrack)
    {
        auto Tracks = Phases[0].Get_LimbTracks();
        auto Track = FCk_ProceduralAction_LimbTrack{InSlot};
        Track.Set_Space(InSpace);
        Track.Set_AnchorId(InAnchorId);
        Tracks.Add(MoveTemp(Track));
        Phases[0].Set_LimbTracks(Tracks);
    }
    InDraft.Set_Phases(Phases);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Set_PhaseProgress(
        FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        const FCk_ProceduralAction_ProgressCurve& InCurve)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto PhaseIndex = builder_detail::FindPhase(InDraft, InPhaseId);
    if (PhaseIndex == INDEX_NONE)
    {
        builder_detail::AddError(Diagnostics, TEXT("PhaseReference"), TEXT("PhaseId"), InPhaseId, {}, INDEX_NONE,
            TEXT("declared phase ID"), InPhaseId.ToString());
        return builder_detail::Reject(Diagnostics);
    }
    auto Candidate = InDraft.Get_Phases()[PhaseIndex];
    Candidate.Set_Progress(InCurve);
    if (NOT ValidateProgress(Candidate, builder_detail::DraftAssetPath, builder_detail::PhasePath(PhaseIndex), Diagnostics))
    { return builder_detail::Reject(Diagnostics); }
    InDraft.Get_Phases()[PhaseIndex] = MoveTemp(Candidate);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Add_Phase(
        FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Phase& InPhase)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto& Phases = InDraft.Get_Phases();
    if (Phases.Num() >= content_limits::MaxPhases)
    {
        builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), TEXT("Phases"), InPhase.Get_Id(), {}, INDEX_NONE,
            FString::Printf(TEXT("at most %d"), content_limits::MaxPhases), TEXT("one more phase"));
        return builder_detail::Reject(Diagnostics);
    }
    const auto Path = builder_detail::PhasePath(Phases.Num());
    if (InPhase.Get_Id().IsNone() || builder_detail::FindPhase(InDraft, InPhase.Get_Id()) != INDEX_NONE)
    {
        builder_detail::AddError(Diagnostics, TEXT("PhaseId"), Path + TEXT(".Id"), InPhase.Get_Id(), {}, INDEX_NONE,
            TEXT("unique nonempty ID"), InPhase.Get_Id().ToString());
    }
    const auto Duration = InPhase.Get_Duration().Get_Seconds();
    if (NOT FMath::IsFinite(Duration) || Duration <= 0.0
        || NOT FMath::IsFinite(builder_detail::Get_PhaseStart(InDraft, Phases.Num()) + Duration))
    {
        builder_detail::AddError(Diagnostics, TEXT("Duration"), Path + TEXT(".Duration"), InPhase.Get_Id(), {}, INDEX_NONE,
            TEXT("finite positive duration and finite cumulative time"), FString::SanitizeFloat(Duration));
    }
    if (InPhase.Get_LimbTracks().Num() > content_limits::MaxTracksPerPhase)
    {
        builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), Path + TEXT(".LimbTracks"), InPhase.Get_Id(), {}, INDEX_NONE,
            FString::Printf(TEXT("at most %d"), content_limits::MaxTracksPerPhase), FString::FromInt(InPhase.Get_LimbTracks().Num()));
    }
    if (builder_detail::CountKeys(Phases) + builder_detail::CountKeys({InPhase}) > content_limits::MaxKeys)
    {
        builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), TEXT("Phases.Keys"), InPhase.Get_Id(), {}, INDEX_NONE,
            FString::Printf(TEXT("at most %lld"), content_limits::MaxKeys), TEXT("more keys"));
    }
    ValidateProgress(InPhase, builder_detail::DraftAssetPath, Path, Diagnostics);
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }
    InDraft.Get_Phases().Add(InPhase);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Add_Slot(
        FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Slot& InSlot)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    if (InDraft.Get_Slots().Num() >= content_limits::MaxSlots)
    {
        builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), TEXT("Slots"), {}, InSlot.Get_Id(), INDEX_NONE,
            FString::Printf(TEXT("at most %d"), content_limits::MaxSlots), TEXT("one more slot"));
        return builder_detail::Reject(Diagnostics);
    }
    if (InSlot.Get_Id().IsNone() || builder_detail::HasSlot(InDraft, InSlot.Get_Id()))
    {
        builder_detail::AddError(Diagnostics, TEXT("SlotId"), FString::Printf(TEXT("Slots[%d].Id"), InDraft.Get_Slots().Num()),
            {}, InSlot.Get_Id(), INDEX_NONE, TEXT("unique nonempty ID"), InSlot.Get_Id().ToString());
        return builder_detail::Reject(Diagnostics);
    }
    InDraft.Get_Slots().Add(InSlot);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Add_BodyKey(
        FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        const FCk_ProceduralAction_TransformKey& InKey)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto PhaseIndex = builder_detail::FindPhase(InDraft, InPhaseId);
    if (PhaseIndex == INDEX_NONE)
    {
        builder_detail::AddError(Diagnostics, TEXT("PhaseReference"), TEXT("PhaseId"), InPhaseId, {}, INDEX_NONE,
            TEXT("declared phase ID"), InPhaseId.ToString());
        return builder_detail::Reject(Diagnostics);
    }
    auto Budget = builder_detail::Make_BudgetFor(InDraft);
    auto Phase = InDraft.Get_Phases()[PhaseIndex];
    builder_detail::DoAdd_BodyKey(InDraft, PhaseIndex, Phase, InKey, Budget, Diagnostics);
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }
    InDraft.Get_Phases()[PhaseIndex] = MoveTemp(Phase);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Add_LimbKey(
        FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        FCk_FloatRange_0to1 InTime,
        const FCk_ProceduralAction_LimbPoseTarget& InTarget)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto PhaseIndex = builder_detail::FindPhase(InDraft, InPhaseId);
    if (PhaseIndex == INDEX_NONE)
    {
        builder_detail::AddError(Diagnostics, TEXT("PhaseReference"), TEXT("PhaseId"), InPhaseId, {}, INDEX_NONE,
            TEXT("declared phase ID"), InPhaseId.ToString());
        return builder_detail::Reject(Diagnostics);
    }
    if (NOT builder_detail::IsUnitTime(InTime.Get_Value()))
    {
        builder_detail::AddError(Diagnostics, TEXT("KeyTime"), TEXT("Time"), InPhaseId, InTarget.Get_Slot(), INDEX_NONE,
            TEXT("finite [0,1]"), FString::SanitizeFloat(InTime.Get_Value()));
        return builder_detail::Reject(Diagnostics);
    }
    auto Budget = builder_detail::Make_BudgetFor(InDraft);
    auto Phase = InDraft.Get_Phases()[PhaseIndex];
    builder_detail::DoAdd_LimbTarget(InDraft, PhaseIndex, Phase, InTime.Get_Value(), InTarget, TEXT("Target"), Budget, Diagnostics);
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }
    InDraft.Get_Phases()[PhaseIndex] = MoveTemp(Phase);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Add_PoseKey(
        FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        const FCk_ProceduralAction_PoseKey& InPose)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto PhaseIndex = builder_detail::FindPhase(InDraft, InPhaseId);
    if (PhaseIndex == INDEX_NONE)
    {
        builder_detail::AddError(Diagnostics, TEXT("PhaseReference"), TEXT("PhaseId"), InPhaseId, {}, INDEX_NONE,
            TEXT("declared phase ID"), InPhaseId.ToString());
        return builder_detail::Reject(Diagnostics);
    }
    builder_detail::ValidatePoseShape(InPose, Diagnostics);
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }

    const auto Time = InPose.Get_Time().Get_Value();
    auto Budget = builder_detail::Make_BudgetFor(InDraft);
    auto Phase = InDraft.Get_Phases()[PhaseIndex];
    if (InPose.Get_BodyMode() == ECk_ProceduralAction_ValueMode::Set)
    {
        if (NOT builder_detail::IsRigid(InPose.Get_Body()))
        {
            builder_detail::AddError(Diagnostics, TEXT("TransformKey"), TEXT("Pose.Body"), InPhaseId, {}, INDEX_NONE,
                TEXT("finite unit-scale rigid transform"), TEXT("malformed transform"));
        }
        else
        {
            builder_detail::DoAdd_BodyKey(InDraft, PhaseIndex, Phase, FCk_ProceduralAction_TransformKey{InPose.Get_Time(),
                InPose.Get_Body().GetTranslation(), InPose.Get_Body().GetRotation()}, Budget, Diagnostics);
        }
    }
    for (auto Index = 0; Index < InPose.Get_Limbs().Num(); ++Index)
    {
        builder_detail::DoAdd_LimbTarget(InDraft, PhaseIndex, Phase, Time, InPose.Get_Limbs()[Index],
            FString::Printf(TEXT("Pose.Limbs[%d]"), Index), Budget, Diagnostics);
    }
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }
    InDraft.Get_Phases()[PhaseIndex] = MoveTemp(Phase);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Replace_PoseKey(
        FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        const FCk_ProceduralAction_PoseKey& InPose)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto PhaseIndex = builder_detail::FindPhase(InDraft, InPhaseId);
    if (PhaseIndex == INDEX_NONE)
    {
        builder_detail::AddError(Diagnostics, TEXT("PhaseReference"), TEXT("PhaseId"), InPhaseId, {}, INDEX_NONE,
            TEXT("declared phase ID"), InPhaseId.ToString());
        return builder_detail::Reject(Diagnostics);
    }
    builder_detail::ValidatePoseShape(InPose, Diagnostics);
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }

    const auto Time = InPose.Get_Time().Get_Value();
    const auto TimeText = FString::SanitizeFloat(Time);
    const auto Path = builder_detail::PhasePath(PhaseIndex);
    auto Phase = InDraft.Get_Phases()[PhaseIndex];
    if (InPose.Get_BodyMode() == ECk_ProceduralAction_ValueMode::Set)
    {
        auto Keys = Phase.Get_BodyKeys();
        const auto KeyIndex = builder_detail::Get_KeyAt(Keys, Time);
        if (NOT InDraft.Get_BodyChannel().Get_Enabled())
        {
            builder_detail::AddError(Diagnostics, TEXT("BodyDisabled"), TEXT("BodyChannel.Enabled"), InPhaseId, {}, INDEX_NONE,
                TEXT("an enabled body channel"), TEXT("disabled"));
        }
        else if (KeyIndex == INDEX_NONE)
        {
            builder_detail::AddError(Diagnostics, TEXT("MissingKey"), Path + TEXT(".BodyKeys"), InPhaseId, {}, INDEX_NONE,
                TEXT("an existing key at the pose time"), TimeText);
        }
        else if (NOT builder_detail::IsRigid(InPose.Get_Body()))
        {
            builder_detail::AddError(Diagnostics, TEXT("TransformKey"), TEXT("Pose.Body"), InPhaseId, {}, KeyIndex,
                TEXT("finite unit-scale rigid transform"), TEXT("malformed transform"));
        }
        else
        {
            Keys[KeyIndex].Set_Translation(InPose.Get_Body().GetTranslation());
            Keys[KeyIndex].Set_Rotation(InPose.Get_Body().GetRotation());
            Phase.Set_BodyKeys(Keys);
        }
    }
    auto Tracks = Phase.Get_LimbTracks();
    for (auto Index = 0; Index < InPose.Get_Limbs().Num(); ++Index)
    {
        const auto& Target = InPose.Get_Limbs()[Index];
        const auto TargetPath = FString::Printf(TEXT("Pose.Limbs[%d]"), Index);
        const auto Slot = Target.Get_Slot();
        const auto TrackIndex = builder_detail::FindTrack(Phase, Slot);
        if (NOT builder_detail::HasSlot(InDraft, Slot))
        {
            builder_detail::AddError(Diagnostics, TEXT("UnknownSlot"), TargetPath + TEXT(".Slot"), InPhaseId, Slot, INDEX_NONE,
                TEXT("a declared slot"), Slot.ToString());
            continue;
        }
        if (TrackIndex == INDEX_NONE)
        {
            builder_detail::AddError(Diagnostics, TEXT("MissingKey"), Path + TEXT(".LimbTracks"), InPhaseId, Slot, INDEX_NONE,
                TEXT("an existing track for the slot"), Slot.ToString());
            continue;
        }
        auto& Track = Tracks[TrackIndex];
        const auto TrackPath = builder_detail::TrackPath(PhaseIndex, TrackIndex);
        auto Positions = Track.Get_PositionKeys();
        const auto PositionIndex = builder_detail::Get_KeyAt(Positions, Time);
        if (PositionIndex == INDEX_NONE)
        {
            builder_detail::AddError(Diagnostics, TEXT("MissingKey"), TrackPath + TEXT(".PositionKeys"), InPhaseId, Slot, INDEX_NONE,
                TEXT("an existing key at the pose time"), TimeText);
        }
        else if (NOT builder_detail::IsFinite(Target.Get_Position()))
        {
            builder_detail::AddError(Diagnostics, TEXT("VectorKey"), FString::Printf(TEXT("%s.PositionKeys[%d]"), *TrackPath, PositionIndex),
                InPhaseId, Slot, PositionIndex, TEXT("finite value/tangents"), TEXT("malformed key"));
        }
        else
        { Positions[PositionIndex].Set_Value(Target.Get_Position()); }
        Track.Set_PositionKeys(Positions);
        if (Target.Get_OrientationMode() == ECk_ProceduralAction_ValueMode::Set)
        {
            auto Orientations = Track.Get_OrientationKeys();
            const auto OrientationIndex = builder_detail::Get_KeyAt(Orientations, Time);
            if (OrientationIndex == INDEX_NONE)
            {
                builder_detail::AddError(Diagnostics, TEXT("MissingKey"), TrackPath + TEXT(".OrientationKeys"), InPhaseId, Slot, INDEX_NONE,
                    TEXT("an existing key at the pose time"), TimeText);
            }
            else if (NOT builder_detail::IsUnit(Target.Get_Orientation()))
            {
                builder_detail::AddError(Diagnostics, TEXT("Quaternion"), FString::Printf(TEXT("%s.OrientationKeys[%d].Value"), *TrackPath,
                    OrientationIndex), InPhaseId, Slot, OrientationIndex, TEXT("finite unit quaternion"), TEXT("malformed quaternion"));
            }
            else
            { Orientations[OrientationIndex].Set_Value(Target.Get_Orientation()); }
            Track.Set_OrientationKeys(Orientations);
        }
        if (Target.Get_BendMode() == ECk_ProceduralAction_ValueMode::Set)
        {
            auto Bends = Track.Get_BendKeys();
            const auto BendIndex = builder_detail::Get_KeyAt(Bends, Time);
            if (BendIndex == INDEX_NONE)
            {
                builder_detail::AddError(Diagnostics, TEXT("MissingKey"), TrackPath + TEXT(".BendKeys"), InPhaseId, Slot, INDEX_NONE,
                    TEXT("an existing key at the pose time"), TimeText);
            }
            else if (NOT builder_detail::IsUnitDirection(Target.Get_Bend()))
            {
                builder_detail::AddError(Diagnostics, TEXT("VectorKey"), FString::Printf(TEXT("%s.BendKeys[%d]"), *TrackPath, BendIndex),
                    InPhaseId, Slot, BendIndex, TEXT("unit nonzero direction"), TEXT("malformed key"));
            }
            else
            { Bends[BendIndex].Set_Value(Target.Get_Bend()); }
            Track.Set_BendKeys(Bends);
        }
    }
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }
    Phase.Set_LimbTracks(Tracks);
    InDraft.Get_Phases()[PhaseIndex] = MoveTemp(Phase);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Add_Marker(
        FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Marker& InMarker)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto& Markers = InDraft.Get_Markers();
    if (Markers.Num() >= content_limits::MaxMarkers)
    {
        builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), TEXT("Markers"), {}, {}, INDEX_NONE,
            FString::Printf(TEXT("at most %d"), content_limits::MaxMarkers), TEXT("one more marker"));
        return builder_detail::Reject(Diagnostics);
    }
    const auto Path = FString::Printf(TEXT("Markers[%d]"), Markers.Num());
    if (InMarker.Get_Id().IsNone() || Markers.ContainsByPredicate([&InMarker](const FCk_ProceduralAction_Marker& InExisting)
        { return InExisting.Get_Id() == InMarker.Get_Id(); }))
    {
        builder_detail::AddError(Diagnostics, TEXT("MarkerId"), Path + TEXT(".Id"), InMarker.Get_At().Get_PhaseId(), {}, INDEX_NONE,
            TEXT("unique nonempty ID"), InMarker.Get_Id().ToString());
    }
    auto Resolved = 0.0;
    builder_detail::ResolveTimeRef(InDraft, InMarker.Get_At(), Path + TEXT(".At"), Diagnostics, Resolved);
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }
    InDraft.Get_Markers().Add(InMarker);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Add_Window(
        FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Window& InWindow)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto& Windows = InDraft.Get_Windows();
    if (Windows.Num() >= content_limits::MaxWindows)
    {
        builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), TEXT("Windows"), {}, {}, INDEX_NONE,
            FString::Printf(TEXT("at most %d"), content_limits::MaxWindows), TEXT("one more window"));
        return builder_detail::Reject(Diagnostics);
    }
    const auto Path = FString::Printf(TEXT("Windows[%d]"), Windows.Num());
    if (InWindow.Get_Id().IsNone() || Windows.ContainsByPredicate([&InWindow](const FCk_ProceduralAction_Window& InExisting)
        { return InExisting.Get_Id() == InWindow.Get_Id(); }))
    {
        builder_detail::AddError(Diagnostics, TEXT("WindowId"), Path + TEXT(".Id"), InWindow.Get_Start().Get_PhaseId(), {}, INDEX_NONE,
            TEXT("unique nonempty ID"), InWindow.Get_Id().ToString());
    }
    auto Start = 0.0;
    auto End = 0.0;
    const auto StartResolved = builder_detail::ResolveTimeRef(InDraft, InWindow.Get_Start(), Path + TEXT(".Start"), Diagnostics, Start);
    const auto EndResolved = builder_detail::ResolveTimeRef(InDraft, InWindow.Get_End(), Path + TEXT(".End"), Diagnostics, End);
    if (StartResolved && EndResolved && End <= Start)
    {
        builder_detail::AddError(Diagnostics, TEXT("WindowInterval"), Path, {}, {}, INDEX_NONE,
            TEXT("finite strictly positive interval"), FString::Printf(TEXT("%g to %g"), Start, End));
    }
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }
    InDraft.Get_Windows().Add(InWindow);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Mirror_Slot(
        FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_MirrorSpec& InMirror)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    auto Diagnostics = TArray<FDiagnostic>{};
    const auto Source = InMirror.Get_SourceSlot();
    const auto Destination = InMirror.Get_DestinationSlot();
    const auto Plane = InMirror.Get_Plane();
    if (NOT builder_detail::HasSlot(InDraft, Source))
    {
        builder_detail::AddError(Diagnostics, TEXT("UnknownSlot"), TEXT("Mirror.SourceSlot"), {}, Source, INDEX_NONE,
            TEXT("a declared slot"), Source.ToString());
    }
    if (NOT builder_detail::HasSlot(InDraft, Destination) || Destination == Source)
    {
        builder_detail::AddError(Diagnostics, TEXT("UnknownSlot"), TEXT("Mirror.DestinationSlot"), {}, Destination, INDEX_NONE,
            TEXT("a declared slot other than the source"), Destination.ToString());
    }
    if (Plane != ECk_ProceduralAction_MirrorPlane::XY && Plane != ECk_ProceduralAction_MirrorPlane::XZ
        && Plane != ECk_ProceduralAction_MirrorPlane::YZ)
    {
        builder_detail::AddError(Diagnostics, TEXT("MirrorPlane"), TEXT("Mirror.Plane"), {}, {}, INDEX_NONE,
            TEXT("XY, XZ or YZ"), TEXT("unsupported plane"));
    }
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }

    auto Phases = InDraft.Get_Phases();
    auto MirroredKeys = int64{0};
    auto Written = 0;
    for (auto PhaseIndex = 0; PhaseIndex < Phases.Num(); ++PhaseIndex)
    {
        const auto& Phase = Phases[PhaseIndex];
        const auto SourceIndex = builder_detail::FindTrack(Phase, Source);
        if (SourceIndex == INDEX_NONE)
        { continue; }
        ++Written;
        const auto& SourceTrack = Phase.Get_LimbTracks()[SourceIndex];
        const auto Space = SourceTrack.Get_Space();
        if ((Space != ECk_ProceduralAction_TrackSpace::BodyLocal && Space != ECk_ProceduralAction_TrackSpace::RootAtStart)
            || NOT SourceTrack.Get_AnchorId().IsNone())
        {
            builder_detail::AddError(Diagnostics, TEXT("MirrorSpace"), builder_detail::TrackPath(PhaseIndex, SourceIndex) + TEXT(".Space"),
                Phase.Get_Id(), Source, INDEX_NONE, TEXT("BodyLocal or RootAtStart (other frames need the subject)"), TEXT("subject-relative frame"));
            continue;
        }
        const auto DestinationIndex = builder_detail::FindTrack(Phase, Destination);
        if (DestinationIndex != INDEX_NONE)
        {
            builder_detail::AddError(Diagnostics, TEXT("MirrorDestination"), builder_detail::TrackPath(PhaseIndex, DestinationIndex),
                Phase.Get_Id(), Destination, INDEX_NONE, TEXT("no existing destination track"), TEXT("occupied"));
            continue;
        }
        if (Phase.Get_LimbTracks().Num() >= content_limits::MaxTracksPerPhase)
        {
            builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), builder_detail::PhasePath(PhaseIndex) + TEXT(".LimbTracks"),
                Phase.Get_Id(), Destination, INDEX_NONE, FString::Printf(TEXT("at most %d"), content_limits::MaxTracksPerPhase),
                TEXT("one more track"));
            continue;
        }
        for (auto OtherIndex = 0; OtherIndex < Phases.Num(); ++OtherIndex)
        {
            const auto OtherTrack = builder_detail::FindTrack(Phases[OtherIndex], Destination);
            if (OtherTrack == INDEX_NONE)
            { continue; }
            const auto& Existing = Phases[OtherIndex].Get_LimbTracks()[OtherTrack];
            if (Existing.Get_Space() != Space || NOT Existing.Get_AnchorId().IsNone())
            {
                builder_detail::AddError(Diagnostics, TEXT("SpaceChangesAcrossPhases"),
                    builder_detail::TrackPath(PhaseIndex, Phase.Get_LimbTracks().Num()) + TEXT(".Space"), Phase.Get_Id(), Destination,
                    INDEX_NONE, TEXT("the destination slot's existing frame"), TEXT("frame changed"));
                break;
            }
        }
        MirroredKeys += static_cast<int64>(SourceTrack.Get_PositionKeys().Num())
            + SourceTrack.Get_OrientationKeys().Num() + SourceTrack.Get_BendKeys().Num();
    }
    if (Written == 0)
    {
        builder_detail::AddError(Diagnostics, TEXT("MirrorSource"), TEXT("Mirror.SourceSlot"), {}, Source, INDEX_NONE,
            TEXT("at least one source track"), TEXT("none"));
    }
    if (builder_detail::CountKeys(Phases) + MirroredKeys > content_limits::MaxKeys)
    {
        builder_detail::AddError(Diagnostics, TEXT("ContentLimit"), TEXT("Phases.Keys"), {}, Destination, INDEX_NONE,
            FString::Printf(TEXT("at most %lld"), content_limits::MaxKeys), FString::Printf(TEXT("%lld more"), MirroredKeys));
    }
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }

    for (auto& Phase : Phases)
    {
        const auto SourceIndex = builder_detail::FindTrack(Phase, Source);
        if (SourceIndex == INDEX_NONE)
        { continue; }
        auto Tracks = Phase.Get_LimbTracks();
        const auto& SourceTrack = Tracks[SourceIndex];
        auto Mirrored = FCk_ProceduralAction_LimbTrack{Destination};
        Mirrored.Set_Space(SourceTrack.Get_Space());
        Mirrored.Set_Interpolation(SourceTrack.Get_Interpolation());
        Mirrored.Set_ReachPolicy(SourceTrack.Get_ReachPolicy());
        Mirrored.Set_ClearanceResponse(SourceTrack.Get_ClearanceResponse());
        Mirrored.Set_PositionKeys(builder_detail::MirrorVectorKeys(SourceTrack.Get_PositionKeys(), Plane));
        Mirrored.Set_BendKeys(builder_detail::MirrorVectorKeys(SourceTrack.Get_BendKeys(), Plane));
        auto Orientations = SourceTrack.Get_OrientationKeys();
        for (auto& Key : Orientations)
        { Key.Set_Value(ReflectRotation(Key.Get_Value(), Plane)); }
        Mirrored.Set_OrientationKeys(Orientations);
        Tracks.Add(MoveTemp(Mirrored));
        Phase.Set_LimbTracks(Tracks);
    }
    InDraft.Set_Phases(Phases);
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Make_FromRecipe(
        const FCk_ProceduralAction_RecipeSpec& InRecipe,
        FCk_ProceduralAction_ClipDraft& OutDraft)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    OutDraft = {};
    auto Diagnostics = TArray<FDiagnostic>{};
    builder_detail::ValidateRecipe(InRecipe, Diagnostics);
    if (NOT Diagnostics.IsEmpty())
    { return builder_detail::Reject(Diagnostics); }

    auto Draft = builder_detail::Make_RecipeDraft(InRecipe);
    auto Compiled = FCompiledClip{};
    if (NOT Compile(MakeClipSource(Draft), Compiled, Diagnostics))
    { return builder_detail::Reject(Diagnostics); }

    auto Warnings = TArray<FDiagnostic>{};
    const auto MovesALimbFromRest = InRecipe.Get_Recipe() == ECk_ProceduralAction_Recipe::Reach
        || InRecipe.Get_Recipe() == ECk_ProceduralAction_Recipe::Sweep;
    if (MovesALimbFromRest && InRecipe.Get_Space() != ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
    {
        Warnings.Add(FDiagnostic{EDiagnosticSeverity::Warning, TEXT("RecipeRest"), builder_detail::DraftAssetPath,
            TEXT("Slots[0].ExplicitRestTarget"), {}, InRecipe.Get_Slots()[0], INDEX_NONE,
            TEXT("an AuthoredRest slot with an ExplicitRestTarget, or LimbRestNormalized space"),
            TEXT("rest taken as the slot frame's origin")});
    }
    OutDraft = MoveTemp(Draft);
    return MakePublicReport(ECk_ProceduralAction_ReportStatus::Valid, false, Warnings);
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Make_Draft_FromData(
        UCk_ProceduralAction_Data* InSource,
        FCk_ProceduralAction_ClipDraft& OutDraft)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    OutDraft = {};
    const auto Pinned = TStrongObjectPtr<UCk_ProceduralAction_Data>{InSource};
    auto Diagnostics = TArray<FDiagnostic>{};
    if (NOT Pinned.IsValid())
    {
        builder_detail::AddError(Diagnostics, TEXT("NullData"), TEXT("Data"), {}, {}, INDEX_NONE,
            TEXT("valid loaded clip object"), TEXT("null or invalid"));
        return builder_detail::Reject(Diagnostics);
    }
    if (Pinned->Get_CompatibilityVersion() != 1)
    {
        builder_detail::AddError(Diagnostics, TEXT("Version"), TEXT("CompatibilityVersion"), {}, {}, INDEX_NONE,
            TEXT("1"), FString::FromInt(Pinned->Get_CompatibilityVersion()));
        return builder_detail::Reject(Diagnostics);
    }
    OutDraft.Set_BodyChannel(Pinned->Get_BodyChannel());
    OutDraft.Set_Slots(Pinned->Get_Slots());
    OutDraft.Set_Phases(Pinned->Get_Phases());
    OutDraft.Set_Markers(Pinned->Get_Markers());
    OutDraft.Set_Windows(Pinned->Get_Windows());
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Build_Into(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        UCk_ProceduralAction_Data* InDestination)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    const auto Pinned = TStrongObjectPtr<UCk_ProceduralAction_Data>{InDestination};
    auto Diagnostics = TArray<FDiagnostic>{};
    if (NOT Pinned.IsValid())
    {
        builder_detail::AddError(Diagnostics, TEXT("NullData"), TEXT("Destination"), {}, {}, INDEX_NONE,
            TEXT("valid clip object"), TEXT("null or invalid"));
        return builder_detail::Reject(Diagnostics);
    }
    if (Pinned->Get_CompatibilityVersion() != 1)
    {
        builder_detail::AddError(Diagnostics, TEXT("Version"), TEXT("Destination.CompatibilityVersion"), {}, {}, INDEX_NONE,
            TEXT("1"), FString::FromInt(Pinned->Get_CompatibilityVersion()));
        return builder_detail::Reject(Diagnostics);
    }
    auto Compiled = FCompiledClip{};
    if (NOT Compile(MakeClipSource(InDraft), Compiled, Diagnostics))
    { return builder_detail::Reject(Diagnostics); }
    Pinned->Set_BodyChannel(InDraft.Get_BodyChannel());
    Pinned->Set_Slots(InDraft.Get_Slots());
    Pinned->Set_Phases(InDraft.Get_Phases());
    Pinned->Set_Markers(InDraft.Get_Markers());
    Pinned->Set_Windows(InDraft.Get_Windows());
    return builder_detail::Accept();
}

auto
    UCk_Utils_ProceduralActionBuilder_UE::
    Write_Into(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        UCk_ProceduralAction_Data* InDestination)
    -> FCk_ProceduralAction_ValidationReport
{
    using namespace ck::procedural_action;
    const auto Pinned = TStrongObjectPtr<UCk_ProceduralAction_Data>{InDestination};
    auto Diagnostics = TArray<FDiagnostic>{};
    if (NOT Pinned.IsValid())
    {
        builder_detail::AddError(Diagnostics, TEXT("NullData"), TEXT("Destination"), {}, {}, INDEX_NONE,
            TEXT("valid clip object"), TEXT("null or invalid"));
        return builder_detail::Reject(Diagnostics);
    }
    if (Pinned->Get_CompatibilityVersion() != 1)
    {
        builder_detail::AddError(Diagnostics, TEXT("Version"), TEXT("Destination.CompatibilityVersion"), {}, {}, INDEX_NONE,
            TEXT("1"), FString::FromInt(Pinned->Get_CompatibilityVersion()));
        return builder_detail::Reject(Diagnostics);
    }
    Pinned->Set_BodyChannel(InDraft.Get_BodyChannel());
    Pinned->Set_Slots(InDraft.Get_Slots());
    Pinned->Set_Phases(InDraft.Get_Phases());
    Pinned->Set_Markers(InDraft.Get_Markers());
    Pinned->Set_Windows(InDraft.Get_Windows());
    return builder_detail::Accept();
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action
{
    auto
    ReflectVector(
        const FVector& InVector,
        ECk_ProceduralAction_MirrorPlane InPlane)
        -> FVector
    {
        switch (InPlane)
        {
        case ECk_ProceduralAction_MirrorPlane::XY: return FVector{InVector.X, InVector.Y, -InVector.Z};
        case ECk_ProceduralAction_MirrorPlane::XZ: return FVector{InVector.X, -InVector.Y, InVector.Z};
        case ECk_ProceduralAction_MirrorPlane::YZ: return FVector{-InVector.X, InVector.Y, InVector.Z};
        default:
            CK_INVALID_ENUM(InPlane);
            return InVector;
        }
    }

    auto
    ReflectRotation(
        const FQuat& InRotation,
        ECk_ProceduralAction_MirrorPlane InPlane)
        -> FQuat
    {
        const auto Reflected = ReflectVector(FVector{InRotation.X, InRotation.Y, InRotation.Z}, InPlane);
        return FQuat{-Reflected.X, -Reflected.Y, -Reflected.Z, InRotation.W}.GetNormalized();
    }

    auto
    ComputeCatmullRomTangents(
        TArray<FCk_ProceduralAction_VectorKey>& InOutKeys)
        -> void
    {
        for (auto Index = 0; Index < InOutKeys.Num(); ++Index)
        {
            auto Tangent = FVector::ZeroVector;
            if (Index > 0 && Index + 1 < InOutKeys.Num())
            {
                const auto& Previous = InOutKeys[Index - 1];
                const auto& Next = InOutKeys[Index + 1];
                const auto Span = Next.Get_Time().Get_Value() - Previous.Get_Time().Get_Value();
                if (Span > 0.0)
                { Tangent = (Next.Get_Value() - Previous.Get_Value()) / Span; }
            }
            InOutKeys[Index].Set_ArriveTangent(Tangent);
            InOutKeys[Index].Set_LeaveTangent(Tangent);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
