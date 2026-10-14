#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"
#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"

#include <Algo/Sort.h>

namespace ck::procedural_action
{
    namespace validation_detail
    {
        auto
        IsFinite(
            const FVector& InValue)
            -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z);
        }

        auto
        IsUnit(
            const FQuat& InValue)
            -> bool
        {
            return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y)
                && FMath::IsFinite(InValue.Z) && FMath::IsFinite(InValue.W)
                && FMath::Abs(InValue.SizeSquared() - 1.0) <= 1.0e-4;
        }

        auto
        IsRigid(
            const FTransform& InValue)
            -> bool
        {
            return IsFinite(InValue.GetTranslation()) && IsUnit(InValue.GetRotation())
                && InValue.GetScale3D().Equals(FVector::OneVector, 1.0e-6);
        }

        auto
        IsAllowed(
            ECk_ProceduralAction_Easing InValue)
            -> bool
        {
            switch (InValue)
            {
            case ECk_ProceduralAction_Easing::Linear:
            case ECk_ProceduralAction_Easing::InQuad:
            case ECk_ProceduralAction_Easing::OutQuad:
            case ECk_ProceduralAction_Easing::InOutQuad:
            case ECk_ProceduralAction_Easing::InCubic:
            case ECk_ProceduralAction_Easing::OutCubic:
            case ECk_ProceduralAction_Easing::InOutCubic:
                return true;
            default:
                return false;
            }
        }

        auto
        IsAllowed(
            ECk_ProceduralAction_Interpolation InValue)
            -> bool
        { return InValue == ECk_ProceduralAction_Interpolation::Linear || InValue == ECk_ProceduralAction_Interpolation::Smooth; }

        auto
        IsAllowed(
            ECk_ProceduralAction_TrackSpace InValue)
            -> bool
        {
            return InValue == ECk_ProceduralAction_TrackSpace::BodyLocal
                || InValue == ECk_ProceduralAction_TrackSpace::RootAtStart
                || InValue == ECk_ProceduralAction_TrackSpace::AnchorAtStart
                || InValue == ECk_ProceduralAction_TrackSpace::LimbRestNormalized;
        }

        auto
        IsAllowed(
            ECk_ProceduralAction_ReachPolicy InValue)
            -> bool
        { return InValue == ECk_ProceduralAction_ReachPolicy::RejectUnreachable || InValue == ECk_ProceduralAction_ReachPolicy::ProjectToReach; }

        auto
        IsAllowed(
            ECk_ProceduralAction_ClearanceResponse InValue)
            -> bool
        { return InValue == ECk_ProceduralAction_ClearanceResponse::AbortOnCrossing || InValue == ECk_ProceduralAction_ClearanceResponse::ObserveCrossing; }

        auto
        IsAllowed(
            ECk_ProceduralAction_ProgressMode InValue)
            -> bool
        { return InValue == ECk_ProceduralAction_ProgressMode::Preset || InValue == ECk_ProceduralAction_ProgressMode::CustomCurve; }

        auto
        HashVector(
            uint32 InHash,
            const FVector& InValue)
            -> uint32
        {
            return HashCombineFast(HashCombineFast(HashCombineFast(InHash, GetTypeHash(InValue.X)),
                GetTypeHash(InValue.Y)), GetTypeHash(InValue.Z));
        }

        auto
        HashQuat(
            uint32 InHash,
            const FQuat& InValue)
            -> uint32
        {
            return HashCombineFast(HashCombineFast(HashCombineFast(HashCombineFast(InHash,
                GetTypeHash(InValue.X)), GetTypeHash(InValue.Y)), GetTypeHash(InValue.Z)), GetTypeHash(InValue.W));
        }

        auto
        AddHermiteExtrema(
            const FVector& InA,
            const FVector& InB,
            const FVector& InLeave,
            const FVector& InArrive,
            double InStart,
            double InEnd,
            TArray<double>& OutProgress)
            -> bool
        {
            const auto Span = InEnd - InStart;
            const auto M0 = InLeave * Span;
            const auto M1 = InArrive * Span;
            for (auto Axis = 0; Axis < 3; ++Axis)
            {
                const auto A = 2.0 * InA[Axis] - 2.0 * InB[Axis] + M0[Axis] + M1[Axis];
                const auto B = -3.0 * InA[Axis] + 3.0 * InB[Axis] - 2.0 * M0[Axis] - M1[Axis];
                const auto C = M0[Axis];
                if (NOT FMath::IsFinite(A) || NOT FMath::IsFinite(B) || NOT FMath::IsFinite(C))
                { return false; }
                const auto AddRoot = [&](double InRoot) -> void
                {
                    if (FMath::IsFinite(InRoot) && InRoot > 0.0 && InRoot < 1.0)
                    {
                        const auto Progress = InStart + Span * InRoot;
                        if (FMath::IsFinite(Progress)) { OutProgress.Add(Progress); }
                    }
                };
                if (FMath::Abs(A) <= UE_DOUBLE_SMALL_NUMBER)
                {
                    if (FMath::Abs(B) > UE_DOUBLE_SMALL_NUMBER) { AddRoot(-C / (2.0 * B)); }
                    continue;
                }
                const auto Discriminant = B * B - 3.0 * A * C;
                if (NOT FMath::IsFinite(Discriminant)) { return false; }
                if (Discriminant < 0.0) { continue; }
                const auto Root = FMath::Sqrt(Discriminant);
                AddRoot((-B - Root) / (3.0 * A));
                AddRoot((-B + Root) / (3.0 * A));
            }
            return true;
        }
    }

    auto
    MakeClipSource(
        const UCk_ProceduralAction_Data& InAsset)
        -> FClipSource
    {
        return FClipSource{InAsset.Get_CompatibilityVersion(), &InAsset.Get_BodyChannel(), &InAsset.Get_Slots(),
            &InAsset.Get_Phases(), &InAsset.Get_Markers(), &InAsset.Get_Windows(), InAsset.GetPathName()};
    }

    auto
    MakeClipSource(
        const FCk_ProceduralAction_ClipDraft& InDraft)
        -> FClipSource
    {
        return FClipSource{1, &InDraft.Get_BodyChannel(), &InDraft.Get_Slots(), &InDraft.Get_Phases(),
            &InDraft.Get_Markers(), &InDraft.Get_Windows(), TEXT("<draft>")};
    }

    auto
    Compile(
        const UCk_ProceduralAction_Data& InAsset,
        FCompiledClip& OutClip,
        TArray<FDiagnostic>& OutDiagnostics)
        -> bool
    { return Compile(MakeClipSource(InAsset), OutClip, OutDiagnostics); }

    auto
    ValidateProgress(
        const FCk_ProceduralAction_Phase& InPhase,
        const FString& InAssetPath,
        const FString& InPhasePath,
        TArray<FDiagnostic>& OutDiagnostics)
        -> bool
    {
        auto Valid = true;
        auto AddError = [&](FName InCode, const FString& InField, int32 InKey, const FString& InExpected, const FString& InActual) -> void
        {
            Valid = false;
            if (OutDiagnostics.Num() >= content_limits::MaxDiagnostics)
            { return; }
            OutDiagnostics.Add(FDiagnostic{EDiagnosticSeverity::Error, InCode, InAssetPath, InField,
                InPhase.Get_Id(), {}, InKey, InExpected, InActual});
        };
        const auto& Curve = InPhase.Get_Progress();
        const auto ProgressPath = InPhasePath + TEXT(".Progress");
        if (NOT validation_detail::IsAllowed(Curve.Get_Mode()))
        {
            AddError(TEXT("ProgressMode"), ProgressPath + TEXT(".Mode"), INDEX_NONE, TEXT("Preset or CustomCurve"), TEXT("unsupported mode"));
            return false;
        }
        if (Curve.Get_Mode() == ECk_ProceduralAction_ProgressMode::Preset)
        {
            if (NOT Curve.Get_Keys().IsEmpty())
            {
                AddError(TEXT("ProgressKeysInactive"), ProgressPath + TEXT(".Keys"), INDEX_NONE,
                    TEXT("no keys while Progress.Mode is Preset"), FString::FromInt(Curve.Get_Keys().Num()));
            }
            return Valid;
        }
        if (InPhase.Get_Easing() != ECk_ProceduralAction_Easing::Linear)
        {
            AddError(TEXT("ProgressEasing"), InPhasePath + TEXT(".Easing"), INDEX_NONE,
                TEXT("Linear when Progress.Mode is CustomCurve"), TEXT("preset easing"));
        }
        const auto& Keys = Curve.Get_Keys();
        const auto KeysPath = ProgressPath + TEXT(".Keys");
        if (Keys.Num() < 2 || Keys.Num() > 64)
        {
            AddError(TEXT("ProgressKeyCount"), KeysPath, INDEX_NONE, TEXT("2 to 64 keys"), FString::FromInt(Keys.Num()));
            return false;
        }
        auto PreviousTime = -1.0;
        auto PreviousValue = 0.0;
        for (auto KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
        {
            const auto KeyPath = FString::Printf(TEXT("%s[%d]"), *KeysPath, KeyIndex);
            const auto Time = Keys[KeyIndex].Get_Time().Get_Value();
            const auto Value = Keys[KeyIndex].Get_Value().Get_Value();
            if (NOT FMath::IsFinite(Time) || Time < 0.0 || Time > 1.0 || Time <= PreviousTime)
            { AddError(TEXT("ProgressKeyTime"), KeyPath + TEXT(".Time"), KeyIndex, TEXT("finite strictly increasing [0,1]"), FString::SanitizeFloat(Time)); }
            if (NOT FMath::IsFinite(Value) || Value < 0.0 || Value > 1.0 || (KeyIndex > 0 && Value < PreviousValue))
            { AddError(TEXT("ProgressKeyValue"), KeyPath + TEXT(".Value"), KeyIndex, TEXT("finite nondecreasing [0,1]"), FString::SanitizeFloat(Value)); }
            if (FMath::IsFinite(Time))
            { PreviousTime = Time; }
            if (FMath::IsFinite(Value))
            { PreviousValue = Value; }
        }
        if (Keys[0].Get_Time().Get_Value() != 0.0 || Keys[0].Get_Value().Get_Value() != 0.0
            || Keys.Last().Get_Time().Get_Value() != 1.0 || Keys.Last().Get_Value().Get_Value() != 1.0)
        { AddError(TEXT("ProgressEndpoints"), KeysPath, INDEX_NONE, TEXT("first key exactly (0,0) and last exactly (1,1)"), TEXT("inexact endpoint")); }
        return Valid;
    }

    auto
    Compile(
        const FClipSource& InSource,
        FCompiledClip& OutClip,
        TArray<FDiagnostic>& OutDiagnostics)
        -> bool
    {
        OutClip = {};
        OutDiagnostics.Reset();
        const auto SourceIsComplete = InSource.BodyChannel != nullptr && InSource.Slots != nullptr && InSource.Phases != nullptr
            && InSource.Markers != nullptr && InSource.Windows != nullptr;
        CK_ENSURE_IF_NOT(SourceIsComplete, TEXT("Clip source [{}] does not view all five authored fields"), InSource.AssetPath)
        {
            OutDiagnostics.Add(FDiagnostic{EDiagnosticSeverity::Error, TEXT("Source"), InSource.AssetPath, TEXT("Source"),
                {}, {}, INDEX_NONE, TEXT("body channel, slots, phases, markers and windows"), TEXT("missing field view")});
            return false;
        }
        const auto& SourceBodyChannel = *InSource.BodyChannel;
        const auto& SourceSlots = *InSource.Slots;
        const auto& SourcePhases = *InSource.Phases;
        const auto& SourceMarkers = *InSource.Markers;
        const auto& SourceWindows = *InSource.Windows;
        auto Candidate = FCompiledClip{};
        Candidate._CompatibilityVersion = InSource.CompatibilityVersion;
        Candidate._HasBodyChannel = SourceBodyChannel.Get_Enabled();
        Candidate._BodyInitialSource = SourceBodyChannel.Get_InitialSource();
        const auto& AssetPath = InSource.AssetPath;
        auto AddError = [&](FName InCode, const FString& InField, FName InPhase, FName InSlot,
            int32 InKey, const FString& InExpected, const FString& InActual) -> void
        {
            if (OutDiagnostics.Num() >= content_limits::MaxDiagnostics) { return; }
            OutDiagnostics.Add(FDiagnostic{EDiagnosticSeverity::Error, InCode, AssetPath, InField,
                InPhase, InSlot, InKey, InExpected, InActual});
        };

        // Reject oversized authored arrays before copying any key or constructing compiled events.
        auto RejectCount = [&](int64 InCount, int64 InMax, const FString& InField) -> bool
        {
            if (InCount <= InMax) { return false; }
            AddError(TEXT("ContentLimit"), InField, {}, {}, INDEX_NONE,
                FString::Printf(TEXT("at most %lld"), InMax), FString::Printf(TEXT("%lld"), InCount));
            return true;
        };
        if (RejectCount(SourceSlots.Num(), content_limits::MaxSlots, TEXT("Slots"))
            || RejectCount(SourcePhases.Num(), content_limits::MaxPhases, TEXT("Phases"))
            || RejectCount(SourceMarkers.Num(), content_limits::MaxMarkers, TEXT("Markers"))
            || RejectCount(SourceWindows.Num(), content_limits::MaxWindows, TEXT("Windows")))
        { return false; }
        auto KeyCount = int64{0};
        for (auto PhaseIndex = 0; PhaseIndex < SourcePhases.Num(); ++PhaseIndex)
        {
            const auto& Phase = SourcePhases[PhaseIndex];
            if (RejectCount(Phase.Get_LimbTracks().Num(), content_limits::MaxTracksPerPhase,
                FString::Printf(TEXT("Phases[%d].LimbTracks"), PhaseIndex)))
            { return false; }
            KeyCount += Phase.Get_BodyKeys().Num();
            for (const auto& Track : Phase.Get_LimbTracks())
            {
                KeyCount += static_cast<int64>(Track.Get_PositionKeys().Num())
                    + Track.Get_OrientationKeys().Num() + Track.Get_BendKeys().Num();
            }
            if (RejectCount(KeyCount, content_limits::MaxKeys, TEXT("Phases.Keys")))
            { return false; }
        }

        if (Candidate._CompatibilityVersion != 1)
        { AddError(TEXT("Version"), TEXT("CompatibilityVersion"), {}, {}, INDEX_NONE, TEXT("1"), FString::FromInt(Candidate._CompatibilityVersion)); }
        if (Candidate._HasBodyChannel && Candidate._BodyInitialSource != ECk_ProceduralAction_InitialSource::CapturedAcceptedPose
            && Candidate._BodyInitialSource != ECk_ProceduralAction_InitialSource::IdentityOffset)
        { AddError(TEXT("InitialSource"), TEXT("BodyChannel.InitialSource"), {}, {}, INDEX_NONE, TEXT("CapturedAcceptedPose or IdentityOffset"), TEXT("unsupported source")); }
        if (SourcePhases.IsEmpty())
        { AddError(TEXT("EmptyPhases"), TEXT("Phases"), {}, {}, INDEX_NONE, TEXT("at least one phase"), TEXT("empty")); }
        if (NOT Candidate._HasBodyChannel && SourceSlots.IsEmpty())
        { AddError(TEXT("EmptyChannels"), TEXT("BodyChannel/Slots"), {}, {}, INDEX_NONE, TEXT("body channel or one limb slot"), TEXT("none")); }

        auto SlotIds = TSet<FName>{};
        for (auto Index = 0; Index < SourceSlots.Num(); ++Index)
        {
            const auto& Slot = SourceSlots[Index];
            const auto Path = FString::Printf(TEXT("Slots[%d]"), Index);
            if (Slot.Get_Id().IsNone() || SlotIds.Contains(Slot.Get_Id()))
            { AddError(TEXT("SlotId"), Path + TEXT(".Id"), {}, Slot.Get_Id(), INDEX_NONE, TEXT("unique nonempty ID"), Slot.Get_Id().ToString()); }
            SlotIds.Add(Slot.Get_Id());
            if (Slot.Get_InitialSource() != ECk_ProceduralAction_InitialSource::CapturedAcceptedPose
                && Slot.Get_InitialSource() != ECk_ProceduralAction_InitialSource::AuthoredRest)
            { AddError(TEXT("InitialSource"), Path + TEXT(".InitialSource"), {}, Slot.Get_Id(), INDEX_NONE, TEXT("CapturedAcceptedPose or AuthoredRest"), TEXT("unsupported source")); }
            if (Slot.Get_HasExplicitRestTarget() && NOT validation_detail::IsRigid(Slot.Get_ExplicitRestTarget()))
            { AddError(TEXT("RestTarget"), Path + TEXT(".ExplicitRestTarget"), {}, Slot.Get_Id(), INDEX_NONE, TEXT("finite unit-scale rigid transform"), TEXT("malformed transform")); }
            Candidate._Slots.Add(FCompiledSlot{Slot.Get_Id(), Slot.Get_InitialSource(), Slot.Get_RequiresTerminalOrientation(),
                Slot.Get_HasExplicitRestTarget(), Slot.Get_ExplicitRestTarget(), ECk_ProceduralAction_TrackSpace::BodyLocal, {}});
        }

        auto FramedSlots = TSet<FName>{};
        auto PhaseIds = TSet<FName>{};
        auto PhaseStart = TMap<FName, double>{};
        auto PhaseDuration = TMap<FName, double>{};
        auto Hash = uint32{2166136261u};
        Hash = HashCombineFast(Hash, GetTypeHash(Candidate._CompatibilityVersion));
        Hash = HashCombineFast(Hash, GetTypeHash(Candidate._HasBodyChannel));
        Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Candidate._BodyInitialSource)));
        for (const auto& Slot : Candidate._Slots)
        {
            Hash = HashCombineFast(Hash, GetTypeHash(Slot.Get_Id()));
            Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Slot.Get_InitialSource())));
            Hash = HashCombineFast(Hash, GetTypeHash(Slot.Get_RequiresTerminalOrientation()));
            Hash = HashCombineFast(Hash, GetTypeHash(Slot.Get_HasExplicitRestTarget()));
            Hash = validation_detail::HashQuat(validation_detail::HashVector(Hash, Slot.Get_ExplicitRestTarget().GetTranslation()), Slot.Get_ExplicitRestTarget().GetRotation());
        }
        for (auto PhaseIndex = 0; PhaseIndex < SourcePhases.Num(); ++PhaseIndex)
        {
            const auto& Phase = SourcePhases[PhaseIndex];
            const auto Path = FString::Printf(TEXT("Phases[%d]"), PhaseIndex);
            if (Phase.Get_Id().IsNone() || PhaseIds.Contains(Phase.Get_Id()))
            { AddError(TEXT("PhaseId"), Path + TEXT(".Id"), Phase.Get_Id(), {}, INDEX_NONE, TEXT("unique nonempty ID"), Phase.Get_Id().ToString()); }
            PhaseIds.Add(Phase.Get_Id());
            const auto Duration = Phase.Get_Duration().Get_Seconds();
            if (NOT FMath::IsFinite(Duration) || Duration <= 0.0
                || NOT FMath::IsFinite(Candidate._Duration.Get_Seconds() + Duration))
            { AddError(TEXT("Duration"), Path + TEXT(".Duration"), Phase.Get_Id(), {}, INDEX_NONE, TEXT("finite positive duration and finite cumulative time"), FString::SanitizeFloat(Duration)); }
            if (NOT validation_detail::IsAllowed(Phase.Get_Easing()))
            { AddError(TEXT("Easing"), Path + TEXT(".Easing"), Phase.Get_Id(), {}, INDEX_NONE, TEXT("monotonic linear/quad/cubic easing"), TEXT("unsupported easing")); }
            if (NOT validation_detail::IsAllowed(Phase.Get_BodyInterpolation()))
            { AddError(TEXT("Interpolation"), Path + TEXT(".BodyInterpolation"), Phase.Get_Id(), {}, INDEX_NONE, TEXT("Linear or Smooth"), TEXT("unsupported interpolation")); }
            PhaseStart.Add(Phase.Get_Id(), Candidate._Duration.Get_Seconds());
            PhaseDuration.Add(Phase.Get_Id(), Duration);
            auto CompiledPhase = FCompiledPhase{};
            CompiledPhase.Get_Id() = Phase.Get_Id();
            CompiledPhase.Get_Start() = Candidate._Duration;
            CompiledPhase.Get_Duration() = FCk_Time{Duration};
            CompiledPhase.Get_Easing() = Phase.Get_Easing();
            CompiledPhase.Get_BodyInterpolation() = Phase.Get_BodyInterpolation();
            Candidate._Duration += FCk_Time{FMath::IsFinite(Duration) && Duration > 0.0 ? Duration : 0.0};
            Hash = HashCombineFast(Hash, GetTypeHash(Phase.Get_Id()));
            Hash = HashCombineFast(Hash, GetTypeHash(Duration));
            Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Phase.Get_Easing())));
            Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Phase.Get_BodyInterpolation())));
            auto ProgressDiagnostics = TArray<FDiagnostic>{};
            const auto ProgressIsValid = ValidateProgress(Phase, AssetPath, Path, ProgressDiagnostics);
            for (const auto& Diagnostic : ProgressDiagnostics)
            {
                AddError(Diagnostic.Get_Code(), Diagnostic.Get_FieldPath(), Phase.Get_Id(), {}, Diagnostic.Get_KeyIndex(),
                    Diagnostic.Get_Expected(), Diagnostic.Get_Actual());
            }
            // Progress joins the content hash only in CustomCurve mode, so every Preset clip keeps its revision.
            if (ProgressIsValid && Phase.Get_Progress().Get_Mode() == ECk_ProceduralAction_ProgressMode::CustomCurve)
            {
                Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Phase.Get_Progress().Get_Mode())));
                for (const auto& Key : Phase.Get_Progress().Get_Keys())
                {
                    Hash = HashCombineFast(Hash, GetTypeHash(Key.Get_Time().Get_Value()));
                    Hash = HashCombineFast(Hash, GetTypeHash(Key.Get_Value().Get_Value()));
                }
                if (NOT BuildMonotoneProgress(Phase.Get_Progress().Get_Keys(), CompiledPhase.Get_Progress()))
                {
                    AddError(TEXT("ProgressKeys"), Path + TEXT(".Progress.Keys"), Phase.Get_Id(), {}, INDEX_NONE,
                        TEXT("finite monotone tangents"), TEXT("curve build failed"));
                }
            }

            auto CriticalProgress = TArray<double>{};
            auto LastTime = -1.0;
            if (NOT Candidate._HasBodyChannel && NOT Phase.Get_BodyKeys().IsEmpty())
            { AddError(TEXT("BodyDisabled"), Path + TEXT(".BodyKeys"), Phase.Get_Id(), {}, INDEX_NONE, TEXT("empty without body channel"), TEXT("authored keys")); }
            for (auto KeyIndex = 0; KeyIndex < Phase.Get_BodyKeys().Num(); ++KeyIndex)
            {
                const auto& Key = Phase.Get_BodyKeys()[KeyIndex];
                const auto KeyPath = FString::Printf(TEXT("%s.BodyKeys[%d]"), *Path, KeyIndex);
                const auto Time = Key.Get_Time().Get_Value();
                if (FMath::IsFinite(Time) && Time >= 0.0 && Time <= 1.0) { CriticalProgress.Add(Time); }
                if (NOT FMath::IsFinite(Time) || Time <= LastTime || Time < 0.0 || Time > 1.0)
                { AddError(TEXT("KeyTime"), KeyPath + TEXT(".Time"), Phase.Get_Id(), {}, KeyIndex, TEXT("finite strictly increasing [0,1]"), FString::SanitizeFloat(Time)); }
                LastTime = Time;
                if (NOT validation_detail::IsFinite(Key.Get_Translation()) || NOT validation_detail::IsUnit(Key.Get_Rotation())
                    || NOT validation_detail::IsFinite(Key.Get_ArriveTangent()) || NOT validation_detail::IsFinite(Key.Get_LeaveTangent()))
                { AddError(TEXT("TransformKey"), KeyPath, Phase.Get_Id(), {}, KeyIndex, TEXT("finite translation/tangents and unit quaternion"), TEXT("malformed key")); }
                auto Rotation = Key.Get_Rotation();
                if (NOT CompiledPhase.Get_BodyKeys().IsEmpty() && (CompiledPhase.Get_BodyKeys().Last().Get_Rotation() | Rotation) < 0.0)
                { Rotation *= -1.0; }
                CompiledPhase.Get_BodyKeys().Add(FTransformKey{Key.Get_Time(), Key.Get_Translation(), Rotation,
                    Key.Get_ArriveTangent(), Key.Get_LeaveTangent()});
                Hash = validation_detail::HashQuat(validation_detail::HashVector(HashCombineFast(Hash, GetTypeHash(Time)), Key.Get_Translation()), Rotation);
                Hash = validation_detail::HashVector(validation_detail::HashVector(Hash, Key.Get_ArriveTangent()), Key.Get_LeaveTangent());
            }
            if (NOT CompiledPhase.Get_BodyKeys().IsEmpty() && CompiledPhase.Get_BodyKeys().Last().Get_Time().Get_Value() != 1.0)
            { AddError(TEXT("KeyEndpoints"), Path + TEXT(".BodyKeys"), Phase.Get_Id(), {}, INDEX_NONE, TEXT("a key at 1"), TEXT("missing endpoint")); }

            auto TrackSlots = TSet<FName>{};
            for (auto TrackIndex = 0; TrackIndex < Phase.Get_LimbTracks().Num(); ++TrackIndex)
            {
                const auto& Track = Phase.Get_LimbTracks()[TrackIndex];
                const auto TrackPath = FString::Printf(TEXT("%s.LimbTracks[%d]"), *Path, TrackIndex);
                if (Track.Get_Slot().IsNone() || NOT SlotIds.Contains(Track.Get_Slot()) || TrackSlots.Contains(Track.Get_Slot()))
                { AddError(TEXT("TrackSlot"), TrackPath + TEXT(".Slot"), Phase.Get_Id(), Track.Get_Slot(), INDEX_NONE, TEXT("unique declared slot"), Track.Get_Slot().ToString()); }
                TrackSlots.Add(Track.Get_Slot());
                const auto NeedsAnchor = Track.Get_Space() == ECk_ProceduralAction_TrackSpace::AnchorAtStart;
                const auto HasAnchor = NOT Track.Get_AnchorId().IsNone();
                if (NOT validation_detail::IsAllowed(Track.Get_Space()) || NeedsAnchor != HasAnchor)
                { AddError(TEXT("TrackSpace"), TrackPath + TEXT(".Space"), Phase.Get_Id(), Track.Get_Slot(), INDEX_NONE, TEXT("supported space with anchor ID iff AnchorAtStart"), TEXT("invalid space/anchor pair")); }
                if (auto* CanonicalSlot = Candidate._Slots.FindByPredicate([&Track](const FCompiledSlot& InSlot)
                    { return InSlot.Get_Id() == Track.Get_Slot(); }))
                {
                    if (FramedSlots.Contains(Track.Get_Slot()))
                    {
                        if (CanonicalSlot->Get_Space() != Track.Get_Space()
                            || CanonicalSlot->Get_AnchorId() != Track.Get_AnchorId())
                        { AddError(TEXT("SpaceChangesAcrossPhases"), TrackPath + TEXT(".Space"), Phase.Get_Id(), Track.Get_Slot(), INDEX_NONE,
                            TEXT("same Space and AnchorId for every track of this slot"), TEXT("frame changed")); }
                    }
                    else
                    {
                        CanonicalSlot->Get_Space() = Track.Get_Space();
                        CanonicalSlot->Get_AnchorId() = Track.Get_AnchorId();
                        FramedSlots.Add(Track.Get_Slot());
                    }
                }
                if (NOT validation_detail::IsAllowed(Track.Get_Interpolation()))
                { AddError(TEXT("Interpolation"), TrackPath + TEXT(".Interpolation"), Phase.Get_Id(), Track.Get_Slot(), INDEX_NONE, TEXT("Linear or Smooth"), TEXT("unsupported interpolation")); }
                if (NOT validation_detail::IsAllowed(Track.Get_ReachPolicy()) || NOT validation_detail::IsAllowed(Track.Get_ClearanceResponse()))
                { AddError(TEXT("TrackPolicy"), TrackPath, Phase.Get_Id(), Track.Get_Slot(), INDEX_NONE, TEXT("supported reach and clearance policies"), TEXT("unsupported policy")); }
                auto CompiledTrack = FCompiledLimbTrack{};
                CompiledTrack.Get_Slot() = Track.Get_Slot();
                CompiledTrack.Get_Space() = Track.Get_Space();
                CompiledTrack.Get_AnchorId() = Track.Get_AnchorId();
                CompiledTrack.Get_Interpolation() = Track.Get_Interpolation();
                CompiledTrack.Get_ReachPolicy() = Track.Get_ReachPolicy();
                CompiledTrack.Get_ClearanceResponse() = Track.Get_ClearanceResponse();
                Hash = HashCombineFast(Hash, GetTypeHash(Track.Get_Slot()));
                Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Track.Get_Space())));
                Hash = HashCombineFast(Hash, GetTypeHash(Track.Get_AnchorId()));
                Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Track.Get_Interpolation())));
                Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Track.Get_ReachPolicy())));
                Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Track.Get_ClearanceResponse())));
                auto ValidateVectorKeys = [&](const TArray<FCk_ProceduralAction_VectorKey>& InKeys,
                    TArray<FVectorKey>& OutKeys, const TCHAR* InName, bool InDirection) -> void
                {
                    auto Previous = -1.0;
                    for (auto KeyIndex = 0; KeyIndex < InKeys.Num(); ++KeyIndex)
                    {
                        const auto& Key = InKeys[KeyIndex];
                        const auto Time = Key.Get_Time().Get_Value();
                        if (FMath::IsFinite(Time) && Time >= 0.0 && Time <= 1.0) { CriticalProgress.Add(Time); }
                        const auto KeyPath = FString::Printf(TEXT("%s.%s[%d]"), *TrackPath, InName, KeyIndex);
                        if (NOT FMath::IsFinite(Time) || Time <= Previous || Time < 0.0 || Time > 1.0)
                        { AddError(TEXT("KeyTime"), KeyPath + TEXT(".Time"), Phase.Get_Id(), Track.Get_Slot(), KeyIndex, TEXT("finite strictly increasing [0,1]"), FString::SanitizeFloat(Time)); }
                        Previous = Time;
                        if (NOT validation_detail::IsFinite(Key.Get_Value()) || NOT validation_detail::IsFinite(Key.Get_ArriveTangent()) || NOT validation_detail::IsFinite(Key.Get_LeaveTangent())
                            || (InDirection && NOT FMath::IsNearlyEqual(Key.Get_Value().SizeSquared(), 1.0, 1.0e-4)))
                        { AddError(TEXT("VectorKey"), KeyPath, Phase.Get_Id(), Track.Get_Slot(), KeyIndex, InDirection ? TEXT("unit nonzero direction") : TEXT("finite value/tangents"), TEXT("malformed key")); }
                        OutKeys.Add(FVectorKey{Key.Get_Time(), Key.Get_Value(), Key.Get_ArriveTangent(), Key.Get_LeaveTangent()});
                        Hash = validation_detail::HashVector(HashCombineFast(Hash, GetTypeHash(Time)), Key.Get_Value());
                        Hash = validation_detail::HashVector(validation_detail::HashVector(Hash, Key.Get_ArriveTangent()), Key.Get_LeaveTangent());
                    }
                    // A bend track needs its start key: a limb may enter the phase with no bend direction to move from.
                    if (NOT OutKeys.IsEmpty() && ((InDirection && OutKeys[0].Get_Time().Get_Value() != 0.0)
                        || OutKeys.Last().Get_Time().Get_Value() != 1.0))
                    { AddError(TEXT("KeyEndpoints"), TrackPath + TEXT(".") + InName, Phase.Get_Id(), Track.Get_Slot(), INDEX_NONE, InDirection ? TEXT("keys at 0 and 1") : TEXT("a key at 1"), TEXT("missing endpoint")); }
                };
                ValidateVectorKeys(Track.Get_PositionKeys(), CompiledTrack.Get_PositionKeys(), TEXT("PositionKeys"), false);
                ValidateVectorKeys(Track.Get_BendKeys(), CompiledTrack.Get_BendKeys(), TEXT("BendKeys"), true);
                LastTime = -1.0;
                for (auto KeyIndex = 0; KeyIndex < Track.Get_OrientationKeys().Num(); ++KeyIndex)
                {
                    const auto& Key = Track.Get_OrientationKeys()[KeyIndex];
                    const auto Time = Key.Get_Time().Get_Value();
                    if (FMath::IsFinite(Time) && Time >= 0.0 && Time <= 1.0) { CriticalProgress.Add(Time); }
                    const auto KeyPath = FString::Printf(TEXT("%s.OrientationKeys[%d]"), *TrackPath, KeyIndex);
                    if (NOT FMath::IsFinite(Time) || Time <= LastTime || Time < 0.0 || Time > 1.0)
                    { AddError(TEXT("KeyTime"), KeyPath + TEXT(".Time"), Phase.Get_Id(), Track.Get_Slot(), KeyIndex, TEXT("finite strictly increasing [0,1]"), FString::SanitizeFloat(Time)); }
                    LastTime = Time;
                    if (NOT validation_detail::IsUnit(Key.Get_Value()))
                    { AddError(TEXT("Quaternion"), KeyPath + TEXT(".Value"), Phase.Get_Id(), Track.Get_Slot(), KeyIndex, TEXT("finite unit quaternion"), TEXT("malformed quaternion")); }
                    auto Rotation = Key.Get_Value();
                    if (NOT CompiledTrack.Get_OrientationKeys().IsEmpty() && (CompiledTrack.Get_OrientationKeys().Last().Get_Value() | Rotation) < 0.0)
                    { Rotation *= -1.0; }
                    CompiledTrack.Get_OrientationKeys().Add(FOrientationKey{Key.Get_Time(), Rotation});
                    Hash = validation_detail::HashQuat(HashCombineFast(Hash, GetTypeHash(Time)), Rotation);
                }
                if (NOT CompiledTrack.Get_OrientationKeys().IsEmpty() && CompiledTrack.Get_OrientationKeys().Last().Get_Time().Get_Value() != 1.0)
                { AddError(TEXT("KeyEndpoints"), TrackPath + TEXT(".OrientationKeys"), Phase.Get_Id(), Track.Get_Slot(), INDEX_NONE, TEXT("a key at 1"), TEXT("missing endpoint")); }
                CompiledPhase.Get_LimbTracks().Add(MoveTemp(CompiledTrack));
            }
            if (Phase.Get_BodyInterpolation() == ECk_ProceduralAction_Interpolation::Smooth)
            {
                const auto& Keys = CompiledPhase.Get_BodyKeys();
                for (auto Index = 0; Index + 1 < Keys.Num(); ++Index)
                {
                    if (NOT validation_detail::AddHermiteExtrema(Keys[Index].Get_Translation(), Keys[Index + 1].Get_Translation(),
                        Keys[Index].Get_LeaveTangent(), Keys[Index + 1].Get_ArriveTangent(),
                        Keys[Index].Get_Time().Get_Value(), Keys[Index + 1].Get_Time().Get_Value(), CriticalProgress))
                    { AddError(TEXT("CurveExtrema"), Path + TEXT(".BodyKeys"), Phase.Get_Id(), {}, Index,
                        TEXT("finite Hermite derivative coefficients"), TEXT("overflow")); }
                }
            }
            for (const auto& Track : CompiledPhase.Get_LimbTracks())
            {
                if (Track.Get_Interpolation() != ECk_ProceduralAction_Interpolation::Smooth) { continue; }
                const auto AddTrackExtrema = [&](const TArray<FVectorKey>& InKeys) -> void
                {
                    for (auto Index = 0; Index + 1 < InKeys.Num(); ++Index)
                    {
                        if (NOT validation_detail::AddHermiteExtrema(InKeys[Index].Get_Value(), InKeys[Index + 1].Get_Value(),
                            InKeys[Index].Get_LeaveTangent(), InKeys[Index + 1].Get_ArriveTangent(),
                            InKeys[Index].Get_Time().Get_Value(), InKeys[Index + 1].Get_Time().Get_Value(), CriticalProgress))
                        { AddError(TEXT("CurveExtrema"), Path + TEXT(".LimbTracks"), Phase.Get_Id(), Track.Get_Slot(), Index,
                            TEXT("finite Hermite derivative coefficients"), TEXT("overflow")); }
                    }
                };
                AddTrackExtrema(Track.Get_PositionKeys());
                AddTrackExtrema(Track.Get_BendKeys());
            }
            CriticalProgress.Sort();
            if (FMath::IsFinite(Duration) && Duration > 0.0 && validation_detail::IsAllowed(Phase.Get_Easing()) && ProgressIsValid)
            {
                for (const auto Progress : CriticalProgress)
                {
                    if (Progress <= 0.0 || Progress >= 1.0) { continue; }
                    const auto RelativeTime = InvertProgress(CompiledPhase, Progress);
                    const auto AbsoluteTime = FCk_Time{CompiledPhase.Get_Start().Get_Seconds() + Duration * RelativeTime};
                    if (CompiledPhase.Get_CriticalTimes().IsEmpty() || AbsoluteTime > CompiledPhase.Get_CriticalTimes().Last())
                    { CompiledPhase.Get_CriticalTimes().Add(AbsoluteTime); }
                }
            }
            Candidate._Phases.Add(MoveTemp(CompiledPhase));
        }

        auto ResolveTime = [&](const FCk_ProceduralAction_TimeRef& InRef, const FString& InPath) -> double
        {
            const auto Normalized = InRef.Get_Time().Get_Value();
            if (NOT FMath::IsFinite(Normalized) || Normalized < 0.0 || Normalized > 1.0)
            {
                AddError(TEXT("TimeRef"), InPath + TEXT(".Time"), InRef.Get_PhaseId(), {}, INDEX_NONE,
                    TEXT("finite normalized time in [0,1]"), FString::SanitizeFloat(Normalized));
                return 0.0;
            }
            const auto* Start = PhaseStart.Find(InRef.Get_PhaseId());
            const auto* Duration = PhaseDuration.Find(InRef.Get_PhaseId());
            if (Start == nullptr || Duration == nullptr)
            {
                AddError(TEXT("PhaseReference"), InPath, InRef.Get_PhaseId(), {}, INDEX_NONE, TEXT("declared phase ID"), InRef.Get_PhaseId().ToString());
                return 0.0;
            }
            const auto Resolved = *Start + *Duration * Normalized;
            if (NOT FMath::IsFinite(Resolved))
            {
                AddError(TEXT("TimeRef"), InPath, InRef.Get_PhaseId(), {}, INDEX_NONE,
                    TEXT("finite resolved time"), FString::SanitizeFloat(Resolved));
                return 0.0;
            }
            return Resolved;
        };
        auto MarkerIds = TSet<FName>{};
        for (auto Index = 0; Index < SourceMarkers.Num(); ++Index)
        {
            const auto& Marker = SourceMarkers[Index];
            const auto Path = FString::Printf(TEXT("Markers[%d]"), Index);
            if (Marker.Get_Id().IsNone() || MarkerIds.Contains(Marker.Get_Id()))
            { AddError(TEXT("MarkerId"), Path + TEXT(".Id"), Marker.Get_At().Get_PhaseId(), {}, INDEX_NONE, TEXT("unique nonempty ID"), Marker.Get_Id().ToString()); }
            MarkerIds.Add(Marker.Get_Id());
            Candidate._Events.Add(FCompiledEvent{FCk_Time{ResolveTime(Marker.Get_At(), Path + TEXT(".At"))}, EEventKind::Point, Marker.Get_Id(), Index});
            Hash = HashCombineFast(Hash, GetTypeHash(Marker.Get_Id()));
            Hash = HashCombineFast(Hash, GetTypeHash(Marker.Get_At().Get_PhaseId()));
            Hash = HashCombineFast(Hash, GetTypeHash(Marker.Get_At().Get_Time().Get_Value()));
        }
        auto WindowIds = TSet<FName>{};
        for (auto Index = 0; Index < SourceWindows.Num(); ++Index)
        {
            const auto& Window = SourceWindows[Index];
            const auto Path = FString::Printf(TEXT("Windows[%d]"), Index);
            if (Window.Get_Id().IsNone() || WindowIds.Contains(Window.Get_Id()))
            { AddError(TEXT("WindowId"), Path + TEXT(".Id"), Window.Get_Start().Get_PhaseId(), {}, INDEX_NONE, TEXT("unique nonempty ID"), Window.Get_Id().ToString()); }
            WindowIds.Add(Window.Get_Id());
            const auto Start = ResolveTime(Window.Get_Start(), Path + TEXT(".Start"));
            const auto End = ResolveTime(Window.Get_End(), Path + TEXT(".End"));
            if (NOT FMath::IsFinite(Start) || NOT FMath::IsFinite(End) || End <= Start)
            { AddError(TEXT("WindowInterval"), Path, {}, {}, INDEX_NONE, TEXT("finite strictly positive interval"), FString::Printf(TEXT("%g to %g"), Start, End)); }
            Candidate._Events.Add(FCompiledEvent{FCk_Time{Start}, EEventKind::WindowEnter, Window.Get_Id(), Index});
            Candidate._Events.Add(FCompiledEvent{FCk_Time{End}, EEventKind::WindowExit, Window.Get_Id(), Index});
            Hash = HashCombineFast(Hash, GetTypeHash(Window.Get_Id()));
            Hash = HashCombineFast(Hash, GetTypeHash(Window.Get_Start().Get_PhaseId()));
            Hash = HashCombineFast(Hash, GetTypeHash(Window.Get_Start().Get_Time().Get_Value()));
            Hash = HashCombineFast(Hash, GetTypeHash(Window.Get_End().Get_PhaseId()));
            Hash = HashCombineFast(Hash, GetTypeHash(Window.Get_End().Get_Time().Get_Value()));
        }
        Candidate._Events.StableSort([](const FCompiledEvent& InA, const FCompiledEvent& InB)
        {
            if (InA.Get_Time() != InB.Get_Time()) { return InA.Get_Time() < InB.Get_Time(); }
            if (InA.Get_Kind() != InB.Get_Kind()) { return static_cast<uint8>(InA.Get_Kind()) < static_cast<uint8>(InB.Get_Kind()); }
            return InA.Get_AuthoredIndex() < InB.Get_AuthoredIndex();
        });
        auto EqualTimeCount = 0;
        auto ActiveWindows = 0;
        auto PreviousTime = FCk_Time{};
        for (const auto& Event : Candidate._Events)
        {
            EqualTimeCount = Event.Get_Time() == PreviousTime ? EqualTimeCount + 1 : 1;
            PreviousTime = Event.Get_Time();
            if (EqualTimeCount > 64)
            {
                AddError(TEXT("EventCapacity"), TEXT("Markers/Windows"), {}, {}, INDEX_NONE,
                    TEXT("at most 64 crossings at one time"), FString::FromInt(EqualTimeCount));
                break;
            }
            ActiveWindows += Event.Get_Kind() == EEventKind::WindowEnter ? 1
                : Event.Get_Kind() == EEventKind::WindowExit ? -1 : 0;
            if (ActiveWindows > 32)
            {
                AddError(TEXT("WindowCapacity"), TEXT("Windows"), {}, {}, INDEX_NONE,
                    TEXT("at most 32 simultaneous windows"), FString::FromInt(ActiveWindows));
                break;
            }
        }
        if (NOT OutDiagnostics.IsEmpty()) { return false; }
        Candidate._ContentHash = Hash;
        OutClip = MoveTemp(Candidate);
        return true;
    }
}
