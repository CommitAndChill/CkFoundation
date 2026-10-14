#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FCk_ProceduralActionAuthoringSession;
class FCk_ProceduralActionEditorModel;

// --------------------------------------------------------------------------------------------------------------------

// The compiler's diagnostics, each naming its field; clicking one selects the section and channel it is about.
class SCkProceduralAction_Issues : public SCompoundWidget
{
public:
    struct FIssue
    {
        FName PhaseId;
        FName Slot;
        FString Text;
        bool IsError = true;
    };
    using FIssuePtr = TSharedPtr<FIssue>;

    SLATE_BEGIN_ARGS(SCkProceduralAction_Issues) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionAuthoringSession>, Session)
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionEditorModel>, Model)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs)
        -> void;
    auto
        Refresh()
        -> void;
    auto
        Get_Count() const
        -> int32
    { return _Issues.Num(); }

private:
    auto
        OnClicked(
            FIssuePtr InIssue)
        -> void;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TWeakPtr<FCk_ProceduralActionEditorModel> _Model;
    TSharedPtr<SListView<FIssuePtr>> _List;
    TArray<FIssuePtr> _Issues;
};

// --------------------------------------------------------------------------------------------------------------------
