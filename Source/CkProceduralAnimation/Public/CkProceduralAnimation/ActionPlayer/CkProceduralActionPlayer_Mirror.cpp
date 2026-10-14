#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_ClipBinding.h"

#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Utils.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Utils.h"

#include "CkCore/Validation/CkIsValid.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_action_player_mirror
{
    // Maps one slot's track values into the body frame: identity for a frame that is reflected as declared, the
    // limb's rest-endpoint frame with its chain length for a rest-normalized frame.
    struct FSlotFrame
    {
        FTransform Rest = FTransform::Identity;
        double ChainLength = 1.0;
    };

    auto
    Get_RestNormalizedFrame(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FName InLimbId,
        FSlotFrame& OutFrame)
        -> bool
    {
        const auto Limb = UCk_Utils_ProceduralArticulation_UE::Get_Limb(InSubject, InLimbId);
        if (ck::Is_NOT_Valid(Limb))
        { return false; }

        auto Layout = FCk_ProceduralLimb_Layout{};
        if (NOT UCk_Utils_ProceduralLimb_UE::Try_GetLayout(Limb, Layout))
        { return false; }

        auto ChainLength = 0.0;
        for (const auto Length : Layout.Get_SegmentLengths())
        { ChainLength += Length; }

        const auto Rest = Layout.Get_RestEndpointLocal() * Layout.Get_AttachmentLocal();
        if (NOT FMath::IsFinite(ChainLength) || ChainLength <= 0.0 || Rest.ContainsNaN()
            || NOT Rest.GetScale3D().Equals(FVector::OneVector, 1.0e-6) || NOT Rest.GetRotation().IsNormalized())
        { return false; }

        OutFrame = FSlotFrame{Rest, ChainLength};
        return true;
    }

    // Source frame -> body frame -> reflection -> destination frame. With identity frames this is the draft geometry.
    struct FMirrorMap
    {
        FSlotFrame Source;
        FSlotFrame Destination;
        ECk_ProceduralAction_MirrorPlane Plane = ECk_ProceduralAction_MirrorPlane::XZ;

        auto
        Position(
            const FVector& InValue) const
            -> FVector
        {
            const auto Body = Source.Rest.TransformPositionNoScale(InValue * Source.ChainLength);
            return Destination.Rest.InverseTransformPositionNoScale(
                ck::procedural_action::ReflectVector(Body, Plane)) / Destination.ChainLength;
        }

        // A position tangent scales with the chain like the position, without the frame's offset.
        auto
        Rate(
            const FVector& InValue) const
            -> FVector
        {
            const auto Body = Source.Rest.TransformVectorNoScale(InValue * Source.ChainLength);
            return Destination.Rest.InverseTransformVectorNoScale(
                ck::procedural_action::ReflectVector(Body, Plane)) / Destination.ChainLength;
        }

        auto
        Direction(
            const FVector& InValue) const
            -> FVector
        {
            const auto Body = Source.Rest.TransformVectorNoScale(InValue);
            return Destination.Rest.InverseTransformVectorNoScale(ck::procedural_action::ReflectVector(Body, Plane));
        }

        auto
        Rotation(
            const FQuat& InValue) const
            -> FQuat
        {
            const auto Body = Source.Rest.GetRotation() * InValue;
            return (Destination.Rest.GetRotation().Inverse()
                * ck::procedural_action::ReflectRotation(Body, Plane)).GetNormalized();
        }
    };

    auto
    MapVectorKeys(
        const TArray<ck::procedural_action::FVectorKey>& InKeys,
        TFunctionRef<FVector(const FVector&)> InValue,
        TFunctionRef<FVector(const FVector&)> InTangent)
        -> TArray<ck::procedural_action::FVectorKey>
    {
        auto Keys = TArray<ck::procedural_action::FVectorKey>{};
        Keys.Reserve(InKeys.Num());
        for (const auto& Key : InKeys)
        {
            Keys.Emplace(Key.Get_Time(), InValue(Key.Get_Value()), InTangent(Key.Get_ArriveTangent()),
                InTangent(Key.Get_LeaveTangent()));
        }
        return Keys;
    }

    auto
    FindBinding(
        const TArray<FCk_ProceduralAction_SlotBinding>& InBindings,
        FName InSlot)
        -> const FCk_ProceduralAction_SlotBinding*
    {
        return InBindings.FindByPredicate([InSlot](const FCk_ProceduralAction_SlotBinding& InBinding)
        { return InBinding.Get_Slot() == InSlot; });
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action
{
    auto
    ExpandMirrors(
        const FCompiledClip& InSource,
        const TArray<FCk_ProceduralAction_MirrorSpec>& InMirrors,
        const TArray<FCk_ProceduralAction_SlotBinding>& InSlotBindings,
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCompiledClip& OutClip,
        TArray<FDiagnostic>& OutDiagnostics)
        -> bool
    {
        using namespace ck_procedural_action_player_mirror;

        auto Candidate = InSource;
        OutClip = {};
        auto Rejected = false;
        auto AddError = [&](FName InCode, const FString& InField, FName InSlot, const FString& InExpected,
            const FString& InActual) -> void
        {
            Rejected = true;
            if (OutDiagnostics.Num() < content_limits::MaxDiagnostics)
            {
                OutDiagnostics.Emplace(EDiagnosticSeverity::Error, InCode, FString{}, InField, FName{}, InSlot,
                    INDEX_NONE, InExpected, InActual);
            }
        };

        const auto& Slots = InSource.Get_Slots();
        if (InMirrors.Num() > content_limits::MaxSlots || Slots.Num() + InMirrors.Num() > content_limits::MaxSlots)
        {
            AddError(TEXT("ContentLimit"), TEXT("Mirrors"), {},
                FString::Printf(TEXT("at most %d slots with mirrors"), content_limits::MaxSlots),
                FString::FromInt(Slots.Num() + InMirrors.Num()));
            return false;
        }

        auto Maps = TArray<FMirrorMap>{};
        Maps.SetNum(InMirrors.Num());
        auto SourceIndices = TArray<int32>{};
        SourceIndices.Init(INDEX_NONE, InMirrors.Num());
        auto Destinations = TSet<FName>{};
        auto AddedKeys = int64{0};
        for (auto Index = 0; Index < InMirrors.Num(); ++Index)
        {
            const auto& Mirror = InMirrors[Index];
            const auto Path = FString::Printf(TEXT("Mirrors[%d]"), Index);
            const auto Source = Mirror.Get_SourceSlot();
            const auto Destination = Mirror.Get_DestinationSlot();
            const auto Plane = Mirror.Get_Plane();

            if (Plane != ECk_ProceduralAction_MirrorPlane::XY && Plane != ECk_ProceduralAction_MirrorPlane::XZ
                && Plane != ECk_ProceduralAction_MirrorPlane::YZ)
            { AddError(TEXT("MirrorPlane"), Path + TEXT(".Plane"), Destination, TEXT("XY, XZ or YZ"), TEXT("unsupported plane")); }

            const auto SourceIndex = Source.IsNone() ? INDEX_NONE
                : Slots.IndexOfByPredicate([Source](const FCompiledSlot& InSlot) { return InSlot.Get_Id() == Source; });
            const auto DestinationDeclared = Slots.ContainsByPredicate([Destination](const FCompiledSlot& InSlot)
            { return InSlot.Get_Id() == Destination; });
            if (Destination.IsNone() || Destination == Source || DestinationDeclared || Destinations.Contains(Destination))
            {
                AddError(TEXT("MirrorDestination"), Path + TEXT(".DestinationSlot"), Destination,
                    TEXT("a slot the clip does not declare, used by one mirror only"), Destination.ToString());
            }
            Destinations.Add(Destination);

            if (SourceIndex == INDEX_NONE)
            {
                AddError(TEXT("MirrorSource"), Path + TEXT(".SourceSlot"), Source, TEXT("a slot the clip declares"),
                    Source.ToString());
                continue;
            }
            SourceIndices[Index] = SourceIndex;

            const auto& SourceSlot = Slots[SourceIndex];
            auto SourceKeys = int64{0};
            auto SourceTracks = 0;
            for (const auto& Phase : InSource.Get_Phases())
            {
                for (const auto& Track : Phase.Get_LimbTracks())
                {
                    if (Track.Get_Slot() != Source)
                    { continue; }
                    ++SourceTracks;
                    SourceKeys += Track.Get_PositionKeys().Num() + Track.Get_OrientationKeys().Num()
                        + Track.Get_BendKeys().Num();
                }
            }
            if (SourceTracks == 0)
            {
                AddError(TEXT("MirrorSource"), Path + TEXT(".SourceSlot"), Source, TEXT("at least one source track"),
                    TEXT("none"));
            }
            AddedKeys += SourceKeys;

            const auto Space = SourceSlot.Get_Space();
            if (Space != ECk_ProceduralAction_TrackSpace::BodyLocal && Space != ECk_ProceduralAction_TrackSpace::RootAtStart
                && Space != ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
            {
                AddError(TEXT("MirrorSpace"), Path + TEXT(".SourceSlot"), Source,
                    TEXT("BodyLocal, RootAtStart or LimbRestNormalized (an anchor frame has no body mirror plane)"),
                    TEXT("anchor-relative frame"));
            }

            const auto* SourceBinding = FindBinding(InSlotBindings, Source);
            const auto* DestinationBinding = FindBinding(InSlotBindings, Destination);
            if (DestinationBinding == nullptr)
            {
                AddError(TEXT("MirrorBinding"), Path + TEXT(".DestinationSlot"), Destination,
                    TEXT("a slot binding for the destination"), TEXT("missing"));
                continue;
            }
            const auto DestinationLimb = DestinationBinding->Get_LimbId();
            if (DestinationLimb.IsNone() || ck::Is_NOT_Valid(UCk_Utils_ProceduralArticulation_UE::Get_Limb(InSubject, DestinationLimb)))
            {
                AddError(TEXT("MirrorBinding"), Path + TEXT(".DestinationSlot"), Destination, TEXT("a limb on the subject"),
                    DestinationLimb.ToString());
                continue;
            }
            if (SourceBinding != nullptr && SourceBinding->Get_LimbId() == DestinationLimb)
            {
                AddError(TEXT("MirrorBinding"), Path + TEXT(".DestinationSlot"), Destination,
                    TEXT("a limb other than the source slot's"), DestinationLimb.ToString());
                continue;
            }

            Maps[Index].Plane = Plane;
            if (Space == ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
            {
                if (SourceBinding == nullptr
                    || NOT Get_RestNormalizedFrame(InSubject, SourceBinding->Get_LimbId(), Maps[Index].Source))
                {
                    AddError(TEXT("MirrorBinding"), Path + TEXT(".SourceSlot"), Source,
                        TEXT("a bound limb with a rigid rest frame and positive chain length"),
                        SourceBinding == nullptr ? TEXT("missing") : *SourceBinding->Get_LimbId().ToString());
                }
                if (NOT Get_RestNormalizedFrame(InSubject, DestinationLimb, Maps[Index].Destination))
                {
                    AddError(TEXT("MirrorBinding"), Path + TEXT(".DestinationSlot"), Destination,
                        TEXT("a bound limb with a rigid rest frame and positive chain length"), DestinationLimb.ToString());
                }
            }
        }

        auto ExistingKeys = int64{0};
        for (auto PhaseIndex = 0; PhaseIndex < InSource.Get_Phases().Num(); ++PhaseIndex)
        {
            const auto& Phase = InSource.Get_Phases()[PhaseIndex];
            auto Tracks = Phase.Get_LimbTracks().Num();
            for (const auto& Track : Phase.Get_LimbTracks())
            {
                ExistingKeys += Track.Get_PositionKeys().Num() + Track.Get_OrientationKeys().Num()
                    + Track.Get_BendKeys().Num();
                for (const auto& Mirror : InMirrors)
                { Tracks += Track.Get_Slot() == Mirror.Get_SourceSlot() ? 1 : 0; }
            }
            if (Tracks > content_limits::MaxTracksPerPhase)
            {
                AddError(TEXT("ContentLimit"), FString::Printf(TEXT("Phases[%d].LimbTracks"), PhaseIndex), {},
                    FString::Printf(TEXT("at most %d"), content_limits::MaxTracksPerPhase), FString::FromInt(Tracks));
            }
        }
        if (ExistingKeys + AddedKeys > content_limits::MaxKeys)
        {
            AddError(TEXT("ContentLimit"), TEXT("Mirrors"), {}, FString::Printf(TEXT("at most %lld keys"), content_limits::MaxKeys),
                FString::Printf(TEXT("%lld"), ExistingKeys + AddedKeys));
        }
        if (Rejected)
        { return false; }

        auto Hash = static_cast<uint32>(InSource.Get_ContentHash());
        for (auto Index = 0; Index < InMirrors.Num(); ++Index)
        {
            const auto& Mirror = InMirrors[Index];
            const auto& Map = Maps[Index];
            const auto& SourceSlot = Slots[SourceIndices[Index]];
            const auto Destination = Mirror.Get_DestinationSlot();
            const auto Rest = SourceSlot.Get_ExplicitRestTarget();
            const auto MirroredRest = SourceSlot.Get_HasExplicitRestTarget()
                ? FTransform{Map.Rotation(Rest.GetRotation()), Map.Position(Rest.GetTranslation())}
                : Rest;
            Candidate._Slots.Emplace(Destination, SourceSlot.Get_InitialSource(), SourceSlot.Get_RequiresTerminalOrientation(),
                SourceSlot.Get_HasExplicitRestTarget(), MirroredRest, SourceSlot.Get_Space(), SourceSlot.Get_AnchorId());

            const auto Position = [&Map](const FVector& InValue) { return Map.Position(InValue); };
            const auto Rate = [&Map](const FVector& InValue) { return Map.Rate(InValue); };
            const auto Direction = [&Map](const FVector& InValue) { return Map.Direction(InValue); };
            for (auto& Phase : Candidate._Phases)
            {
                const auto* SourceTrack = Phase.Get_LimbTracks().FindByPredicate([&Mirror](const FCompiledLimbTrack& InTrack)
                { return InTrack.Get_Slot() == Mirror.Get_SourceSlot(); });
                if (SourceTrack == nullptr)
                { continue; }

                auto Mirrored = *SourceTrack;
                Mirrored.Set_Slot(Destination);
                Mirrored.Set_PositionKeys(MapVectorKeys(SourceTrack->Get_PositionKeys(), Position, Rate));
                Mirrored.Set_BendKeys(MapVectorKeys(SourceTrack->Get_BendKeys(), Direction, Direction));
                auto Orientations = TArray<FOrientationKey>{};
                Orientations.Reserve(SourceTrack->Get_OrientationKeys().Num());
                for (const auto& Key : SourceTrack->Get_OrientationKeys())
                { Orientations.Emplace(Key.Get_Time(), Map.Rotation(Key.Get_Value())); }
                Mirrored.Set_OrientationKeys(Orientations);
                Phase.Get_LimbTracks().Add(MoveTemp(Mirrored));
            }

            Hash = HashCombineFast(Hash, GetTypeHash(Mirror.Get_SourceSlot()));
            Hash = HashCombineFast(Hash, GetTypeHash(Destination));
            Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Mirror.Get_Plane())));
        }
        Candidate._ContentHash = Hash;
        OutClip = MoveTemp(Candidate);
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_action_player_service
{
    auto
    ExpandClipMirrors(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::FProceduralActionClipParams& InParams,
        ck::procedural_action::FCompiledClip& InOutClip,
        TArray<ck::procedural_action::FDiagnostic>* OutDiagnostics)
        -> bool
    {
        if (InParams.Get_Mirrors().IsEmpty())
        { return true; }

        auto Expanded = ck::procedural_action::FCompiledClip{};
        auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{};
        const auto Expands = ck::procedural_action::ExpandMirrors(InOutClip, InParams.Get_Mirrors(),
            InParams.Get_SlotBindings(), InSubject, Expanded, Diagnostics);
        if (OutDiagnostics != nullptr)
        {
            for (const auto& Diagnostic : Diagnostics)
            {
                if (OutDiagnostics->Num() >= ck::procedural_action::content_limits::MaxDiagnostics)
                { break; }
                OutDiagnostics->Add(Diagnostic);
            }
        }
        if (NOT Expands)
        { return false; }

        InOutClip = MoveTemp(Expanded);
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------
