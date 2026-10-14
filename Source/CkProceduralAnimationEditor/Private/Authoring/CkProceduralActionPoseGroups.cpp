#include "CkProceduralAnimationEditor/CkProceduralActionPoseGroups.h"

#include "Misc/Crc.h"

namespace ck_procedural_action_pose_groups
{
    struct FKeyedTime
    {
        FName PhaseId;
        double Time = 0.0;
        TArray<FName> Channels;
    };

    auto
        AddChannel(
            TArray<FKeyedTime>& InOutTimes,
            FName InPhaseId,
            double InTime,
            FName InChannel)
        -> void
    {
        auto* Entry = InOutTimes.FindByPredicate([InPhaseId, InTime](const FKeyedTime& InEntry)
            { return InEntry.PhaseId == InPhaseId && InEntry.Time == InTime; });
        if (Entry == nullptr)
        { Entry = &InOutTimes.Add_GetRef(FKeyedTime{InPhaseId, InTime, {}}); }
        Entry->Channels.AddUnique(InChannel);
    }

    auto
        CollectKeyedTimes(
            const TArray<FCk_ProceduralAction_Phase>& InPhases)
        -> TArray<FKeyedTime>
    {
        const auto Body = FCk_ProceduralAction_PoseGroup::Get_BodyChannelName();
        auto Times = TArray<FKeyedTime>{};
        for (const auto& Phase : InPhases)
        {
            for (const auto& Key : Phase.Get_BodyKeys())
            { AddChannel(Times, Phase.Get_Id(), Key.Get_Time().Get_Value(), Body); }
            for (const auto& Track : Phase.Get_LimbTracks())
            {
                for (const auto& Key : Track.Get_PositionKeys())
                { AddChannel(Times, Phase.Get_Id(), Key.Get_Time().Get_Value(), Track.Get_Slot()); }
            }
        }
        const auto PhaseIndex = [&InPhases](FName InId) -> int32
        {
            return InPhases.IndexOfByPredicate([InId](const FCk_ProceduralAction_Phase& InPhase)
                { return InPhase.Get_Id() == InId; });
        };
        Times.StableSort([&PhaseIndex](const FKeyedTime& InA, const FKeyedTime& InB)
        {
            const auto A = PhaseIndex(InA.PhaseId);
            const auto B = PhaseIndex(InB.PhaseId);
            return A != B ? A < B : InA.Time < InB.Time;
        });
        return Times;
    }

    // Stable across sessions and machines: derived from the phase name and the exact key time, never from FName indices.
    auto
        MakeDerivedId(
            FName InPhaseId,
            double InTime)
        -> FGuid
    {
        auto Bits = uint64{0};
        FMemory::Memcpy(&Bits, &InTime, sizeof(Bits));
        return FGuid{FCrc::StrCrc32(*InPhaseId.ToString()), static_cast<uint32>(Bits), static_cast<uint32>(Bits >> 32),
            0x5053u};
    }

    auto
        MakeDerivedLabel(
            FName InPhaseId,
            double InTime)
        -> FName
    { return FName{*FString::Printf(TEXT("%s %.3g"), *InPhaseId.ToString(), InTime)}; }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    auto
        DerivePoseGroups(
            const TArray<FCk_ProceduralAction_Phase>& InPhases,
            const TArray<FCk_ProceduralAction_PoseGroup>& InPersisted,
            TArray<FCk_ProceduralActionPoseGroupView>& OutGroups)
        -> void
    {
        using namespace ck_procedural_action_pose_groups;
        OutGroups.Reset();
        auto Times = CollectKeyedTimes(InPhases);
        auto Claimed = TMap<FString, FGuid>{};
        const auto ClaimKey = [](FName InPhaseId, double InTime, FName InChannel)
        { return FString::Printf(TEXT("%s|%.17g|%s"), *InPhaseId.ToString(), InTime, *InChannel.ToString()); };

        for (const auto& Group : InPersisted)
        {
            const auto Time = Group.Get_Time().Get_Value();
            const auto* Keyed = Times.FindByPredicate([&Group, Time](const FKeyedTime& InEntry)
                { return InEntry.PhaseId == Group.Get_PhaseId() && InEntry.Time == Time; });
            auto Broken = FString{};
            if (Group.Get_Channels().IsEmpty())
            { Broken = TEXT("The pose lists no channels."); }
            for (const auto Channel : Group.Get_Channels())
            {
                if (Keyed == nullptr || NOT Keyed->Channels.Contains(Channel))
                {
                    Broken += FString::Printf(TEXT("%s has no key at %s %g. "), *Channel.ToString(),
                        *Group.Get_PhaseId().ToString(), Time);
                    continue;
                }
                const auto Key = ClaimKey(Group.Get_PhaseId(), Time, Channel);
                if (const auto* Owner = Claimed.Find(Key))
                {
                    Broken += FString::Printf(TEXT("%s at %s %g is already grouped by pose %s. "), *Channel.ToString(),
                        *Group.Get_PhaseId().ToString(), Time, *Owner->ToString());
                    continue;
                }
                Claimed.Add(Key, Group.Get_Id());
            }
            OutGroups.Add(FCk_ProceduralActionPoseGroupView{Group, true, Broken.TrimEnd()});
        }

        for (const auto& Entry : Times)
        {
            auto Channels = Entry.Channels.FilterByPredicate([&](FName InChannel)
                { return NOT Claimed.Contains(ClaimKey(Entry.PhaseId, Entry.Time, InChannel)); });
            if (Channels.IsEmpty())
            { continue; }
            auto Group = FCk_ProceduralAction_PoseGroup{MakeDerivedId(Entry.PhaseId, Entry.Time), Entry.PhaseId,
                FCk_FloatRange_0to1{Entry.Time}, Channels};
            Group.Set_Label(MakeDerivedLabel(Entry.PhaseId, Entry.Time));
            OutGroups.Add(FCk_ProceduralActionPoseGroupView{Group, false, FString{}});
        }
    }

    auto
        DerivePoseGroups(
            const UCk_ProceduralAction_Data& InClip,
            TArray<FCk_ProceduralActionPoseGroupView>& OutGroups)
        -> void
    { DerivePoseGroups(InClip.Get_Phases(), InClip.Get_EditorPoseGroups(), OutGroups); }

    auto
        FindPoseGroup(
            const UCk_ProceduralAction_Data& InClip,
            const FGuid& InId,
            FCk_ProceduralActionPoseGroupView& OutGroup)
        -> bool
    {
        auto Groups = TArray<FCk_ProceduralActionPoseGroupView>{};
        DerivePoseGroups(InClip, Groups);
        const auto* Found = Groups.FindByPredicate([&InId](const FCk_ProceduralActionPoseGroupView& InView)
            { return InView.Get_Group().Get_Id() == InId; });
        if (Found == nullptr)
        { return false; }
        OutGroup = *Found;
        return true;
    }

    auto
        ReadPoseKey(
            const TArray<FCk_ProceduralAction_Phase>& InPhases,
            const FCk_ProceduralAction_PoseGroup& InGroup,
            FCk_ProceduralAction_PoseKey& OutPose,
            FString& OutError)
        -> bool
    {
        const auto* Phase = InPhases.FindByPredicate([&InGroup](const FCk_ProceduralAction_Phase& InPhase)
            { return InPhase.Get_Id() == InGroup.Get_PhaseId(); });
        if (Phase == nullptr)
        {
            OutError = FString::Printf(TEXT("Pose phase %s no longer exists."), *InGroup.Get_PhaseId().ToString());
            return false;
        }
        const auto Time = InGroup.Get_Time().Get_Value();
        const auto AtTime = [Time](const auto& InKey) { return InKey.Get_Time().Get_Value() == Time; };
        auto Pose = FCk_ProceduralAction_PoseKey{InGroup.Get_Time()};
        auto Limbs = TArray<FCk_ProceduralAction_LimbPoseTarget>{};
        for (const auto Channel : InGroup.Get_Channels())
        {
            if (Channel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
            {
                const auto* Key = Phase->Get_BodyKeys().FindByPredicate(AtTime);
                if (Key == nullptr)
                {
                    OutError = FString::Printf(TEXT("The body has no key at %s %g."), *Phase->Get_Id().ToString(), Time);
                    return false;
                }
                Pose.Set_BodyMode(ECk_ProceduralAction_ValueMode::Set);
                Pose.Set_Body(FTransform{Key->Get_Rotation(), Key->Get_Translation()});
                continue;
            }
            const auto* Track = Phase->Get_LimbTracks().FindByPredicate([Channel](const FCk_ProceduralAction_LimbTrack& InTrack)
                { return InTrack.Get_Slot() == Channel; });
            const auto* Position = Track == nullptr ? nullptr : Track->Get_PositionKeys().FindByPredicate(AtTime);
            if (Position == nullptr)
            {
                OutError = FString::Printf(TEXT("Slot %s has no position key at %s %g."), *Channel.ToString(),
                    *Phase->Get_Id().ToString(), Time);
                return false;
            }
            auto Target = FCk_ProceduralAction_LimbPoseTarget{Channel, Position->Get_Value()};
            if (const auto* Orientation = Track->Get_OrientationKeys().FindByPredicate(AtTime))
            {
                Target.Set_OrientationMode(ECk_ProceduralAction_ValueMode::Set);
                Target.Set_Orientation(Orientation->Get_Value());
            }
            if (const auto* Bend = Track->Get_BendKeys().FindByPredicate(AtTime))
            {
                Target.Set_BendMode(ECk_ProceduralAction_ValueMode::Set);
                Target.Set_Bend(Bend->Get_Value());
            }
            Limbs.Add(MoveTemp(Target));
        }
        Pose.Set_Limbs(Limbs);
        OutPose = MoveTemp(Pose);
        return true;
    }

    auto
        Get_PoseChannels(
            const FCk_ProceduralAction_PoseKey& InPose)
        -> TArray<FName>
    {
        auto Channels = TArray<FName>{};
        if (InPose.Get_BodyMode() == ECk_ProceduralAction_ValueMode::Set)
        { Channels.Add(FCk_ProceduralAction_PoseGroup::Get_BodyChannelName()); }
        for (const auto& Limb : InPose.Get_Limbs())
        { Channels.Add(Limb.Get_Slot()); }
        return Channels;
    }
}
