#include "CkProceduralAnimationEditor/CkProceduralActionEditorData.h"

#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_ProceduralActionEditorData::
    Get_Subject() const
    -> FCk_ProceduralActionEditor_Subject
{ return _Subject; }

auto
    UCk_ProceduralActionEditorData::
    Set_Subject(
        const FCk_ProceduralActionEditor_Subject& InSubject)
    -> void
{ _Subject = InSubject; }

auto
    UCk_ProceduralActionEditorData::
    Get_PoseLabels() const
    -> TArray<FCk_ProceduralActionEditor_PoseLabel>
{ return _PoseLabels; }

auto
    UCk_ProceduralActionEditorData::
    Set_PoseLabels(
        const TArray<FCk_ProceduralActionEditor_PoseLabel>& InLabels)
    -> void
{ _PoseLabels = InLabels; }

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralActionEditor_UE::
    Get_EditorData(
        UCk_ProceduralAction_Data* InClip)
    -> UCk_ProceduralActionEditorData*
{
    const auto ClipIsValid = ck::IsValid(InClip, ck::IsValid_Policy_NullptrOnly{});
    CK_ENSURE_IF_NOT(ClipIsValid, TEXT("Get_EditorData needs a clip"))
    { return nullptr; }

    if (auto* Existing = TryGet_EditorData(InClip))
    { return Existing; }

    InClip->Modify();
    auto* Created = NewObject<UCk_ProceduralActionEditorData>(InClip, NAME_None, RF_Transactional);
    InClip->AddAssetUserData(Created);
    return Created;
}

auto
    UCk_Utils_ProceduralActionEditor_UE::
    TryGet_EditorData(
        const UCk_ProceduralAction_Data* InClip)
    -> UCk_ProceduralActionEditorData*
{
    const auto* UserData = InClip == nullptr ? nullptr : InClip->GetAssetUserDataArray();
    if (UserData == nullptr)
    { return nullptr; }
    for (auto* Datum : *UserData)
    {
        if (auto* EditorData = Cast<UCk_ProceduralActionEditorData>(Datum))
        { return EditorData; }
    }
    return nullptr;
}

auto
    UCk_Utils_ProceduralActionEditor_UE::
    MigrateLegacyEditorData(
        UCk_ProceduralAction_Data& InClip)
    -> void
{
    if (NOT InClip.Get_HasLegacyEditorData())
    { return; }

    auto* EditorData = Get_EditorData(&InClip);
    const auto& Legacy = InClip.Get_LegacyEditorSubject();
    auto Subject = FCk_ProceduralActionEditor_Subject{};
    Subject.Set_WalkingRig(Legacy.Get_WalkingRig());
    Subject.Set_WalkingGait(Legacy.Get_WalkingGait());
    Subject.Set_Layout(Legacy.Get_Layout());
    Subject.Set_SlotBindings(Legacy.Get_SlotBindings());
    Subject.Set_Scenario(TSoftObjectPtr<UCk_ProceduralActionPreview_Config>{Legacy.Get_Scenario().ToSoftObjectPath()});
    EditorData->Set_Subject(Subject);

    auto Labels = EditorData->Get_PoseLabels();
    for (const auto& Group : InClip.Get_LegacyEditorPoseGroups())
    {
        const auto AlreadyLabelled = Labels.ContainsByPredicate([&Group](const FCk_ProceduralActionEditor_PoseLabel& InLabel)
        {
            return InLabel.Get_PhaseId() == Group.Get_PhaseId()
                && ck::procedural_action_editor::IsSamePoseTime(InLabel.Get_Time().Get_Value(), Group.Get_Time().Get_Value());
        });
        if (AlreadyLabelled)
        { continue; }
        Labels.Add(FCk_ProceduralActionEditor_PoseLabel{Group.Get_PhaseId(), Group.Get_Time(), Group.Get_Label()});
    }
    EditorData->Set_PoseLabels(Labels);
    InClip.Clear_LegacyEditorData();
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    auto
        Get_Subject(
            const UCk_ProceduralAction_Data& InClip)
        -> FCk_ProceduralActionEditor_Subject
    {
        const auto* EditorData = UCk_Utils_ProceduralActionEditor_UE::TryGet_EditorData(&InClip);
        return EditorData != nullptr ? EditorData->Get_Subject() : FCk_ProceduralActionEditor_Subject{};
    }

    auto
        Get_PoseLabels(
            const UCk_ProceduralAction_Data& InClip)
        -> TArray<FCk_ProceduralActionEditor_PoseLabel>
    {
        const auto* EditorData = UCk_Utils_ProceduralActionEditor_UE::TryGet_EditorData(&InClip);
        return EditorData != nullptr ? EditorData->Get_PoseLabels() : TArray<FCk_ProceduralActionEditor_PoseLabel>{};
    }

    auto
        FindPoseLabel(
            const TArray<FCk_ProceduralActionEditor_PoseLabel>& InLabels,
            FName InPhaseId,
            double InTime)
        -> const FCk_ProceduralActionEditor_PoseLabel*
    {
        return InLabels.FindByPredicate([InPhaseId, InTime](const FCk_ProceduralActionEditor_PoseLabel& InLabel)
            { return InLabel.Get_PhaseId() == InPhaseId && IsSamePoseTime(InLabel.Get_Time().Get_Value(), InTime); });
    }

    auto
        Get_BodyChannelName()
        -> FName
    { return FName{TEXT("(Body)")}; }

    auto
        IsSamePoseTime(
            double InA,
            double InB)
        -> bool
    { return FMath::IsNearlyEqual(InA, InB, 1.0e-6); }

    auto
        Get_PoseChannels(
            const FCk_ProceduralAction_PoseKey& InPose)
        -> TArray<FName>
    {
        auto Channels = TArray<FName>{};
        if (InPose.Get_BodyMode() == ECk_ProceduralAction_ValueMode::Set)
        { Channels.Add(Get_BodyChannelName()); }
        for (const auto& Limb : InPose.Get_Limbs())
        { Channels.Add(Limb.Get_Slot()); }
        return Channels;
    }

    auto
        Get_ChannelsKeyedAt(
            const FCk_ProceduralAction_Phase& InPhase,
            double InTime)
        -> TArray<FName>
    {
        const auto KeyedAtTime = [InTime](const auto& InKey) { return IsSamePoseTime(InKey.Get_Time().Get_Value(), InTime); };
        auto Channels = TArray<FName>{};
        if (InPhase.Get_BodyKeys().ContainsByPredicate(KeyedAtTime))
        { Channels.Add(Get_BodyChannelName()); }
        for (const auto& Track : InPhase.Get_LimbTracks())
        {
            const auto Keyed = Track.Get_PositionKeys().ContainsByPredicate(KeyedAtTime)
                || Track.Get_OrientationKeys().ContainsByPredicate(KeyedAtTime)
                || Track.Get_BendKeys().ContainsByPredicate(KeyedAtTime);
            if (Keyed)
            { Channels.AddUnique(Track.Get_Slot()); }
        }
        return Channels;
    }
}

// --------------------------------------------------------------------------------------------------------------------
