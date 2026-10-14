#pragma once

#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"

// --------------------------------------------------------------------------------------------------------------------

// A pose group as the editor shows it. Persisted groups come from the clip's editor metadata; the others are derived
// from key times that no persisted group claims and exist only until the author edits them.
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionPoseGroupView
{
    CK_GENERATED_BODY(FCk_ProceduralActionPoseGroupView);

private:
    FCk_ProceduralAction_PoseGroup _Group;
    bool _Persisted = false;
    FString _BrokenReason;

public:
    CK_PROPERTY_GET(_Group);
    CK_PROPERTY_GET(_Persisted);
    CK_PROPERTY_GET(_BrokenReason);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralActionPoseGroupView, _Group, _Persisted, _BrokenReason);

    auto
        Get_IsBroken() const
        -> bool
    { return NOT _BrokenReason.IsEmpty(); }
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    // A channel joins a group at a key time when it has a body key (the body channel) or a position key (a limb slot)
    // at exactly that time; orientation and bend keys at the same time ride with their slot.
    CKPROCEDURALANIMATIONEDITOR_API auto
        DerivePoseGroups(
            const TArray<FCk_ProceduralAction_Phase>& InPhases,
            const TArray<FCk_ProceduralAction_PoseGroup>& InPersisted,
            TArray<FCk_ProceduralActionPoseGroupView>& OutGroups)
        -> void;

    CKPROCEDURALANIMATIONEDITOR_API auto
        DerivePoseGroups(
            const UCk_ProceduralAction_Data& InClip,
            TArray<FCk_ProceduralActionPoseGroupView>& OutGroups)
        -> void;

    CKPROCEDURALANIMATIONEDITOR_API auto
        FindPoseGroup(
            const UCk_ProceduralAction_Data& InClip,
            const FGuid& InId,
            FCk_ProceduralActionPoseGroupView& OutGroup)
        -> bool;

    // Reads the group's key values back into a pose; orientation and bend are Set only where keyed at the group time.
    CKPROCEDURALANIMATIONEDITOR_API auto
        ReadPoseKey(
            const TArray<FCk_ProceduralAction_Phase>& InPhases,
            const FCk_ProceduralAction_PoseGroup& InGroup,
            FCk_ProceduralAction_PoseKey& OutPose,
            FString& OutError)
        -> bool;

    CKPROCEDURALANIMATIONEDITOR_API auto
        Get_PoseChannels(
            const FCk_ProceduralAction_PoseKey& InPose)
        -> TArray<FName>;
}

// --------------------------------------------------------------------------------------------------------------------
