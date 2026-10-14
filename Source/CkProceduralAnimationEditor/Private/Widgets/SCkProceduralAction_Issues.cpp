#include "Widgets/SCkProceduralAction_Issues.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Text/STextBlock.h"

auto
    SCkProceduralAction_Issues::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Session = InArgs._Session;
    _Model = InArgs._Model;
    ChildSlot
    [
        SAssignNew(_List, SListView<FIssuePtr>)
        .Tag(FName{TEXT("CkActionEditor.Issues")})
        .ListItemsSource(&_Issues)
        .SelectionMode(ESelectionMode::Single)
        .OnMouseButtonClick(this, &SCkProceduralAction_Issues::OnClicked)
        .OnGenerateRow_Lambda([](FIssuePtr InIssue, const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>
        {
            return SNew(STableRow<FIssuePtr>, InOwner)
                .Tag(FName{TEXT("CkActionEditor.Issue")})
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .Text(FText::FromString(InIssue->Text))
                    .ColorAndOpacity(InIssue->IsError ? CkStyle::Err() : CkStyle::Warn())
                ];
        })
    ];
    Refresh();
}

auto
    SCkProceduralAction_Issues::
    Refresh()
    -> void
{
    _Issues.Reset();
    if (const auto Session = _Session.Pin())
    {
        for (const auto& Diagnostic : Session->GetDiagnostics())
        {
            _Issues.Add(MakeShared<FIssue>(FIssue{Diagnostic.Get_PhaseId(), Diagnostic.Get_Slot(),
                ck::procedural_action_editor::Describe(Diagnostic),
                Diagnostic.Get_Severity() == ck::procedural_action::EDiagnosticSeverity::Error}));
        }
    }
    _List->RequestListRefresh();
}

auto
    SCkProceduralAction_Issues::
    OnClicked(
        FIssuePtr InIssue)
    -> void
{
    const auto Model = _Model.Pin();
    if (NOT InIssue.IsValid() || NOT Model.IsValid())
    { return; }
    if (NOT InIssue->Slot.IsNone())
    { Model->SelectTrack(InIssue->PhaseId, InIssue->Slot); }
    else if (NOT InIssue->PhaseId.IsNone())
    { Model->SelectSection(InIssue->PhaseId); }
    else
    { Model->SelectClip(); }
}
