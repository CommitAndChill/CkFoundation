#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FCk_ProceduralActionAuthoringSession;
class FCk_ProceduralActionEditorModel;

// --------------------------------------------------------------------------------------------------------------------

/** The Channels panel: the body, each clip slot with the subject limb it is bound to, the subject's other limbs (under the
 *  gait, or unbound), and a subject summary. The tick on a channel is what Record covers; clicking a channel selects its
 *  key at the playhead. */
class SCkProceduralAction_Channels : public SCompoundWidget
{
public:
    struct FItem
    {
        enum class EKind : uint8 { Body, Slot, Limb, Summary };
        EKind Kind = EKind::Body;
        FName Id;
        FText Label;
    };
    using FItemPtr = TSharedPtr<FItem>;

    SLATE_BEGIN_ARGS(SCkProceduralAction_Channels) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionAuthoringSession>, Session)
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionEditorModel>, Model)
        SLATE_ARGUMENT(TFunction<void(bool, const FString&)>, OnResult)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs)
        -> void;
    // Rebuilds the rows from the clip and the subject; rows keep their identity while their channel exists.
    auto
        Refresh()
        -> void;
    auto
        SyncSelection()
        -> void;

private:
    auto
        MakeRow(
            FItemPtr InItem,
            const TSharedRef<STableViewBase>& InOwner)
        -> TSharedRef<ITableRow>;
    auto
        MakeBindingMenu(
            FName InSlot)
        -> TSharedRef<SWidget>;
    auto
        MakeContextMenu()
        -> TSharedPtr<SWidget>;
    auto
        OnSelectionChanged(
            FItemPtr InItem,
            ESelectInfo::Type InInfo)
        -> void;
    auto
        Get_BoundLimb(
            FName InSlot) const
        -> FName;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TWeakPtr<FCk_ProceduralActionEditorModel> _Model;
    TFunction<void(bool, const FString&)> _OnResult;
    TSharedPtr<STreeView<FItemPtr>> _Tree;
    TArray<FItemPtr> _Items;
    TMap<FString, FItemPtr> _ItemsByKey;
};

// --------------------------------------------------------------------------------------------------------------------
