#include "Toolkit/CkProceduralAction_EditorToolkit.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "Authoring/CkProceduralActionObservedReach.h"
#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionGuidedCreation.h"
#include "CkProceduralAnimationEditor/CkProceduralActionSolvedCapture.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditor_UserSettings.h"
#include "CkProceduralAnimationEditor/CkProceduralAction_Factory.h"

#include "Widgets/SCkProceduralAction_AuthoredPlot.h"
#include "Widgets/SCkProceduralAction_AuthoredViewport.h"
#include "Widgets/SCkProceduralAction_ProgressPlot.h"
#include "Widgets/SCkProceduralAction_Timeline.h"
#include "Curve/CkProceduralAction_CurveAdapter.h"
#include "Session/CkProceduralActionEditorModes.h"

#include "Editor.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CoreGlobals.h"
#include "Misc/PackageName.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Containers/Ticker.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionEditorToolkit"

namespace ck_procedural_action_editor_toolkit
{
    template <typename TWidget>
    auto
        Tagged(
            const TSharedRef<TWidget>& InWidget,
            const TCHAR* InTag)
        -> TSharedRef<TWidget>
    {
        InWidget->SetTag(FName{InTag});
        return InWidget;
    }
}

const FName FCk_ProceduralAction_EditorToolkit::EditorTabId(TEXT("CkProceduralAction_Editor"));

FCk_ProceduralAction_EditorToolkit::FCk_ProceduralAction_EditorToolkit() = default;

FCk_ProceduralAction_EditorToolkit::~FCk_ProceduralAction_EditorToolkit()
{
    OnClose();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    Init(
        EToolkitMode::Type InMode,
        const TSharedPtr<IToolkitHost>& InHost,
        UCk_ProceduralAction_Data* InClip)
    -> void
{
    _Session = MakeShared<FCk_ProceduralActionAuthoringSession>(InClip);
    _Modes = MakeUnique<FCkProceduralActionEditorModes>(_Session);
    _ObservationText = LOCTEXT("AuthoredEvidence", "Authored: requested motion only; no accepted-contact evidence.");

    auto& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    auto DetailArgs = FDetailsViewArgs{};
    DetailArgs.bAllowSearch = true;
    DetailArgs.bLockable = false;
    DetailArgs.bUpdatesFromSelection = false;
    DetailArgs.bHideSelectionTip = true;
    DetailArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    _ClipDetails = PropertyEditor.CreateDetailView(DetailArgs);
    _PreviewDetails = PropertyEditor.CreateDetailView(DetailArgs);

    const auto SavedPreviewPath = GetDefault<UCk_ProceduralActionEditor_UserSettings>()
        ->GetPreviewPath(InClip->GetPathName());
    if (NOT SavedPreviewPath.IsEmpty())
    {
        if (auto* Preview = LoadObject<UCk_ProceduralActionPreview_Config>(nullptr, *SavedPreviewPath))
        {
            _Session->SetPreviewConfig(Preview);
        }
        else
        {
            _PreviewSelectionError = FString::Printf(
                TEXT("Saved preview config is unavailable: %s"), *SavedPreviewPath);
        }
    }
    else
    {
        // A preview saved beside the clip travels with the content, so a clip opens on its subject on any machine.
        const auto SiblingPackage = ck::procedural_action_editor::Get_PreviewSiblingPackageName(InClip->GetOutermost()->GetName());
        if (FPackageName::DoesPackageExist(SiblingPackage))
        {
            const auto SiblingPath = SiblingPackage + TEXT(".") + FPackageName::GetLongPackageAssetName(SiblingPackage);
            if (auto* Preview = LoadObject<UCk_ProceduralActionPreview_Config>(nullptr, *SiblingPath))
            { _Session->SetPreviewConfig(Preview); }
        }
    }

    const auto Layout = FTabManager::NewLayout("CkProceduralAction_Authored_v1")
        ->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
            ->Split(FTabManager::NewStack()->AddTab(EditorTabId, ETabState::OpenedTab)));
    InitAssetEditor(InMode, InHost, FName(TEXT("CkProceduralActionEditor")), Layout,
        true, true, InClip);
    _ClipDetails->SetObject(InClip, true);
    if (auto* Preview = _Session->GetPreviewConfig())
    { _PreviewDetails->SetObject(Preview, true); }

    if (GEditor != nullptr)
    {
        GEditor->RegisterForUndo(this);
        _UndoRegistered = true;
    }
    _TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FCk_ProceduralAction_EditorToolkit::OnTicker));
    _SourceAssetPropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
        this, &FCk_ProceduralAction_EditorToolkit::OnSourceAssetChanged);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetToolkitFName() const
    -> FName
{
    return FName(TEXT("CkProceduralActionEditor"));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetBaseToolkitName() const
    -> FText
{
    return LOCTEXT("ToolkitName", "Procedural Action");
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetWorldCentricTabPrefix() const
    -> FString
{
    return TEXT("Procedural Action ");
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetWorldCentricTabColorScale() const
    -> FLinearColor
{
    return FLinearColor{0.12f, 0.67f, 0.78f};
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RegisterTabSpawners(
        const TSharedRef<FTabManager>& InTabManager)
    -> void
{
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
    InTabManager->RegisterTabSpawner(EditorTabId,
        FOnSpawnTab::CreateSP(this, &FCk_ProceduralAction_EditorToolkit::SpawnEditorTab))
        .SetDisplayName(LOCTEXT("AuthoredTab", "Authored"));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    UnregisterTabSpawners(
        const TSharedRef<FTabManager>& InTabManager)
    -> void
{
    InTabManager->UnregisterTabSpawner(EditorTabId);
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SpawnEditorTab(
        const FSpawnTabArgs&)
    -> TSharedRef<SDockTab>
{
    return SNew(SDockTab).TabRole(ETabRole::PanelTab)[BuildEditorContent()];
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildEditorContent()
    -> TSharedRef<SWidget>
{
    _PlotContainer = SNew(SVerticalBox);
    RebuildPlots();
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    _CurveAdapter = MakeShared<FCkProceduralAction_CurveAdapter>();
    const auto CurvePanel = _CurveAdapter->Init(_Session,
        FSimpleDelegate::CreateLambda([WeakToolkit]()
        {
            if (const auto Toolkit = WeakToolkit.Pin())
            { Toolkit->RefreshFromCurve(); }
        }));

    const auto Content = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 4.0f, 4.0f, 0.0f)
        [BuildHeader()]
        + SVerticalBox::Slot().AutoHeight().Padding(4.0f)
        [BuildTransport()]
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f, 0.0f)
        [ck_procedural_action_editor_toolkit::Tagged(SNew(STextBlock).AutoWrapText(true)
            .Text_Lambda([WeakToolkit]() -> FText
            {
                const auto Toolkit = WeakToolkit.Pin();
                if (NOT Toolkit.IsValid() || NOT Toolkit->_Session.IsValid() || NOT Toolkit->_Modes.IsValid())
                { return FText::GetEmpty(); }
                return Toolkit->_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Authored
                    ? Toolkit->_Session->GetStatusText() : Toolkit->_Modes->GetStatusText();
            }), TEXT("CkActionEditorStatus"))]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
        [
            SNew(SSplitter)
            + SSplitter::Slot().Value(0.2f)
            [BuildSubjectPanel()]
            + SSplitter::Slot().Value(0.5f)
            [BuildViewportPanel()]
            + SSplitter::Slot().Value(0.3f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(0.55f)
                [BuildInspectorPanel()]
                + SVerticalBox::Slot().FillHeight(0.45f)
                [BuildTimingPanel()]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 0.0f)
        [
            SNew(SBox).HeightOverride(124.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Text(LOCTEXT("PoseTimeline", "Timeline: named poses per phase (markers and windows on elapsed phase time)"))]
                + SVerticalBox::Slot().FillHeight(1.0f)
                [SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(_PoseRows, SVerticalBox)]]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 2.0f)
        [BuildPoseActions()]
        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 2.0f)
        [SNew(SBox).HeightOverride(76.0f)[BuildIssuePanel()]]
        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 2.0f, 4.0f, 4.0f)
        [BuildAdvanced(CurvePanel)];

    RebuildWorkspaceRows();
    SyncStagingTimeFromTransport();
    if (NOT _SelectedPose.IsValid() && NOT _Session->GetPoseGroups().IsEmpty())
    { SelectPose(_Session->GetPoseGroups()[0].Get_Group().Get_Id()); }
    return Content;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildHeader()
    -> TSharedRef<SWidget>
{
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
        [SNew(STextBlock).Text(LOCTEXT("SubjectLabel", "Subject"))]
        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
        [
            SNew(SBox).WidthOverride(300.0f)
            [
                SNew(SObjectPropertyEntryBox)
                .AllowedClass(UCk_ProceduralActionPreview_Config::StaticClass())
                .ObjectPath_Lambda([WeakToolkit]() -> FString
                {
                    const auto Toolkit = WeakToolkit.Pin();
                    return Toolkit.IsValid() ? Toolkit->GetPreviewPath() : FString{};
                })
                .OnObjectChanged_Lambda([WeakToolkit](const FAssetData& InAsset)
                {
                    if (const auto Toolkit = WeakToolkit.Pin())
                    { Toolkit->OnPreviewPicked(InAsset); }
                })
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
        [SNew(SButton).Text(LOCTEXT("CreatePreviewConfig", "Create Preview Config"))
            .ToolTipText(LOCTEXT("CreatePreviewTip", "Select the preview config next to this clip, creating it when it does not exist."))
            .OnClicked_Lambda([WeakToolkit]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { return Toolkit->OnCreatePreviewClicked(); }
                return FReply::Unhandled();
            })]
        + SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
        [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton).Text(LOCTEXT("SavePair", "Save clip + preview"))
            .ToolTipText(LOCTEXT("SavePairTip", "Save the clip and its preview config as two assets and report each result."))
            .OnClicked_Lambda([WeakToolkit]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { return Toolkit->OnSavePairClicked(); }
                return FReply::Unhandled();
            }), TEXT("CkActionEditorSavePair"))]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6.0f, 0.0f)
        [SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakToolkit]() -> FText
        {
            const auto Toolkit = WeakToolkit.Pin();
            if (NOT Toolkit.IsValid())
            { return FText::GetEmpty(); }
            return FText::FromString(Toolkit->_PreviewSelectionError.IsEmpty()
                ? Toolkit->_SaveReport : Toolkit->_PreviewSelectionError);
        })];
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildTransport()
    -> TSharedRef<SWidget>
{
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    auto ResetButton = TSharedPtr<SButton>{};
    auto TransportSlider = TSharedPtr<SSlider>{};
    const auto Transport = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton)
            .Text_Lambda([WeakToolkit]() -> FText
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() ? Toolkit->GetPlayLabel() : FText::GetEmpty();
            })
            .IsEnabled_Lambda([WeakToolkit]() -> bool
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() && Toolkit->GetCanPressPlay();
            })
            .ToolTipText_Lambda([WeakToolkit]() -> FText
            {
                const auto Toolkit = WeakToolkit.Pin();
                if (NOT Toolkit.IsValid())
                { return FText::GetEmpty(); }
                const auto Reason = Toolkit->GetPlayBlockedReason();
                return Reason.IsEmpty()
                    ? LOCTEXT("PlayTip", "Play the production Solver preview: the settled motion the runtime would produce.")
                    : FText::FromString(Reason);
            })
            .OnClicked_Lambda([WeakToolkit]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { return Toolkit->OnPlayClicked(); }
                return FReply::Unhandled();
            }), TEXT("CkActionEditorPlay"))]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)
        [ck_procedural_action_editor_toolkit::Tagged(SNew(SCheckBox)
            .ToolTipText(LOCTEXT("RequestedTip", "Play the authored requested targets directly, without the Solver. Fast for blocking; it is not the motion the runtime settles."))
            .IsChecked_Lambda([WeakToolkit]() -> ECheckBoxState
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() && Toolkit->_RequestedPreview ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([WeakToolkit](ECheckBoxState InState)
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { Toolkit->OnRequestedPreviewChanged(InState == ECheckBoxState::Checked); }
            })
            [SNew(STextBlock).Text(LOCTEXT("RequestedPreview", "Requested preview (authored targets, not solved)"))],
            TEXT("CkActionEditorRequestedPreview"))]
        + SHorizontalBox::Slot().AutoWidth()
        [SNew(SButton).Text(LOCTEXT("ModeSolver", "Solver (restart)"))
            .ToolTipText(LOCTEXT("SolverRestartTip", "Rebuild the production preview for the current revision without playing it."))
            .OnClicked_Lambda([WeakToolkit]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { return Toolkit->OnSolverClicked(); }
                return FReply::Unhandled();
            })]
        + SHorizontalBox::Slot().AutoWidth()
        [SAssignNew(ResetButton, SButton).Text(LOCTEXT("Reset", "Reset"))
            .OnClicked_Lambda([WeakToolkit]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { return Toolkit->OnResetClicked(); }
                return FReply::Unhandled();
            })]
        + SHorizontalBox::Slot().AutoWidth()
        [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton).Text(LOCTEXT("Frame", "Frame"))
            .ToolTipText(LOCTEXT("FrameTip", "Step one 60 Hz frame: the pose time for a capture follows the transport."))
            .IsEnabled_Lambda([WeakToolkit]() -> bool
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() && Toolkit->_Modes.IsValid()
                    && Toolkit->_Modes->GetMode() != FCkProceduralActionEditorModes::EMode::Recorded;
            })
            .OnClicked_Lambda([WeakToolkit]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { return Toolkit->OnFrameClicked(); }
                return FReply::Unhandled();
            }), TEXT("CkActionEditorFrame"))]
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 0.0f)
        [SAssignNew(TransportSlider, SSlider)
            .OnMouseCaptureBegin_Lambda([WeakToolkit]()
            {
                if (const auto Toolkit = WeakToolkit.Pin(); Toolkit.IsValid() && Toolkit->_Modes.IsValid())
                { Toolkit->_Modes->BeginScrub(); }
            })
            .OnMouseCaptureEnd_Lambda([WeakToolkit]()
            {
                if (const auto Toolkit = WeakToolkit.Pin(); Toolkit.IsValid() && Toolkit->_Modes.IsValid())
                { Toolkit->_Modes->EndScrub(); }
            })
            .IsEnabled_Lambda([WeakToolkit]() -> bool
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() && Toolkit->_Session.IsValid() && Toolkit->_Modes.IsValid()
                    && (Toolkit->_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Authored
                        ? Toolkit->_Session->CanPlay()
                        : Toolkit->_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver);
            })
            .Value_Lambda([WeakToolkit]() -> float
            {
                const auto Toolkit = WeakToolkit.Pin();
                if (NOT Toolkit.IsValid() || NOT Toolkit->_Session.IsValid() || NOT Toolkit->_Modes.IsValid())
                { return 0.0f; }
                const auto Duration = Toolkit->_Modes->GetDuration();
                if (Duration <= FCk_Time{})
                { return 0.0f; }
                return static_cast<float>((Toolkit->_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Authored
                        ? Toolkit->_Session->GetTime().Get_Seconds()
                        : Toolkit->_Modes->GetTime().Get_Seconds())
                    / Duration.Get_Seconds());
            })
            .OnValueChanged_Lambda([WeakToolkit](float InValue)
            {
                const auto Toolkit = WeakToolkit.Pin();
                if (NOT Toolkit.IsValid() || NOT Toolkit->_Session.IsValid())
                { return; }
                if (Toolkit->_Modes.IsValid()
                    && Toolkit->_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver)
                { Toolkit->_Modes->Seek(Toolkit->_Modes->GetDuration() * static_cast<double>(InValue)); }
                else
                {
                    Toolkit->_Session->SetTime(Toolkit->_Session->GetDuration() * static_cast<double>(InValue));
                    Toolkit->SyncStagingTimeFromTransport();
                }
                Toolkit->RefreshModePose();
                for (const auto& Plot : Toolkit->_Plots)
                { if (auto Pinned = Plot.Pin()) { Pinned->Invalidate(EInvalidateWidgetReason::Paint); } }
            })];
    ResetButton->SetTag(FName{TEXT("CkActionEditorTransportReset")});
    TransportSlider->SetTag(FName{TEXT("CkActionEditorTransportTime")});
    return Transport;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildSubjectPanel()
    -> TSharedRef<SWidget>
{
    return ck_procedural_action_editor_toolkit::Tagged(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [SNew(STextBlock).Text(LOCTEXT("SubjectTree", "Body and limbs"))]
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("SubjectTreeHint",
            "Tick the channels a whole pose covers. Unticked limbs stay under ordinary control."))]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(_SubjectRows, SVerticalBox)]],
        TEXT("CkActionEditorSubjectTree"));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildViewportPanel()
    -> TSharedRef<SWidget>
{
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.0f)
        [SAssignNew(_Viewport, SCkProceduralAction_AuthoredViewport).Session(_Session)
            .OnChanged_Lambda([WeakToolkit]()
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { Toolkit->RefreshFromTimeline(); }
            })
            .OnTrackPicked_Lambda([WeakToolkit](FName InPhaseId, FName InSlotId)
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                {
                    if (Toolkit->_Timeline.IsValid())
                    { Toolkit->_Timeline->SelectTrack(InPhaseId, InSlotId); }
                }
            })]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
        [SNew(SHorizontalBox)
            .IsEnabled_Lambda([WeakToolkit]() -> bool
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() && Toolkit->_Modes.IsValid()
                    && Toolkit->_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Authored;
            })
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("EndpointGizmo", "Move endpoint key"))
                .OnClicked_Lambda([WeakToolkit]() -> FReply
                {
                    if (const auto Toolkit = WeakToolkit.Pin(); Toolkit.IsValid() && Toolkit->_Viewport.IsValid())
                    { Toolkit->_Viewport->SetGizmoTarget(SCkProceduralAction_AuthoredViewport::EGizmoTarget::Endpoint); }
                    return FReply::Handled();
                })]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).Text(LOCTEXT("BendGizmo", "Move bend guide"))
                .OnClicked_Lambda([WeakToolkit]() -> FReply
                {
                    if (const auto Toolkit = WeakToolkit.Pin(); Toolkit.IsValid() && Toolkit->_Viewport.IsValid())
                    { Toolkit->_Viewport->SetGizmoTarget(SCkProceduralAction_AuthoredViewport::EGizmoTarget::BendGuide); }
                    return FReply::Handled();
                })]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f)
            [SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakToolkit]() -> FText
            {
                const auto Toolkit = WeakToolkit.Pin();
                if (NOT Toolkit.IsValid() || NOT Toolkit->_Viewport.IsValid())
                { return FText::GetEmpty(); }
                const auto& Error = Toolkit->_Viewport->GetGizmoError();
                return Error.IsEmpty()
                    ? LOCTEXT("GizmoHint", "Select a phase and a limb, scrub into that phase, then drag the world widget. The key is edited on release.")
                    : FText::FromString(Error);
            })]];
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildInspectorPanel()
    -> TSharedRef<SWidget>
{
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    return ck_procedural_action_editor_toolkit::Tagged(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [SNew(STextBlock).Text(LOCTEXT("Inspector", "Selected pose"))]
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [ck_procedural_action_editor_toolkit::Tagged(SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakToolkit]() -> FText
        {
            const auto Toolkit = WeakToolkit.Pin();
            if (NOT Toolkit.IsValid() || NOT Toolkit->_Session.IsValid())
            { return FText::GetEmpty(); }
            const auto* View = Toolkit->_Session->FindPoseGroup(Toolkit->_SelectedPose);
            if (View == nullptr)
            { return LOCTEXT("NoPose", "No pose selected: the values below are staged for a new capture."); }
            const auto& Group = View->Get_Group();
            return FText::FromString(FString::Printf(TEXT("%s | %s%s%s"), *Group.Get_Label().ToString(),
                Group.Get_Provenance() == ECk_ProceduralAction_PoseProvenance::Solved ? TEXT("captured from the Solver") : TEXT("requested targets"),
                View->Get_Persisted() ? TEXT("") : TEXT(" | derived from matching key times"),
                View->Get_IsBroken() ? *FString::Printf(TEXT(" | BROKEN: %s"), *View->Get_BrokenReason()) : TEXT("")));
        }), TEXT("CkActionEditorSelectedPose"))]
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [SNew(STextBlock).Text_Lambda([WeakToolkit]() -> FText
        {
            const auto Toolkit = WeakToolkit.Pin();
            if (NOT Toolkit.IsValid() || NOT Toolkit->_Session.IsValid())
            { return FText::GetEmpty(); }
            const auto& Pose = Toolkit->_Session->GetStagingPose();
            return FText::FromString(FString::Printf(TEXT("Pose time: %s at %.3f of the phase"),
                *Toolkit->_Session->GetStagingPhaseId().ToString(), Pose.Get_Time().Get_Value()));
        })]
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(STextBlock).Text(LOCTEXT("PoseLabel", "New pose label"))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f)
            [SAssignNew(_PoseLabel, SEditableTextBox).HintText(LOCTEXT("PoseLabelHint", "optional"))]
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(_InspectorRows, SVerticalBox)]],
        TEXT("CkActionEditorInspector"));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildPoseActions()
    -> TSharedRef<SWidget>
{
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    const auto HasSelection = [WeakToolkit]() -> bool
    {
        const auto Toolkit = WeakToolkit.Pin();
        return Toolkit.IsValid() && Toolkit->_Session.IsValid()
            && Toolkit->_Session->FindPoseGroup(Toolkit->_SelectedPose) != nullptr;
    };
    const auto Action = [WeakToolkit](const FText& InLabel, const TCHAR* InTag, const FText& InTip,
        FReply (FCk_ProceduralAction_EditorToolkit::*InHandler)(), TFunction<bool()> InEnabled) -> TSharedRef<SWidget>
    {
        return ck_procedural_action_editor_toolkit::Tagged(SNew(SButton).Text(InLabel).ToolTipText(InTip)
            .IsEnabled_Lambda([InEnabled]() -> bool { return NOT InEnabled || InEnabled(); })
            .OnClicked_Lambda([WeakToolkit, InHandler]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { return (Toolkit.Get()->*InHandler)(); }
                return FReply::Unhandled();
            }), InTag);
    };
    const auto PlaneLabel = [](ECk_ProceduralAction_MirrorPlane InPlane) -> FText
    {
        switch (InPlane)
        {
            case ECk_ProceduralAction_MirrorPlane::XY: return LOCTEXT("PlaneXY", "XY plane (Z normal)");
            case ECk_ProceduralAction_MirrorPlane::YZ: return LOCTEXT("PlaneYZ", "YZ plane (X normal)");
            default: return LOCTEXT("PlaneXZ", "XZ plane (Y normal)");
        }
    };
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [Action(LOCTEXT("CapturePose", "Capture pose"), TEXT("CkActionEditorCapturePose"),
                LOCTEXT("CapturePoseTip", "Key every ticked channel from the staged values at the pose time. The time must be free on every channel."),
                &FCk_ProceduralAction_EditorToolkit::OnCapturePose, {})]
            + SHorizontalBox::Slot().AutoWidth()
            [Action(LOCTEXT("UpdatePose", "Update pose"), TEXT("CkActionEditorUpdatePose"),
                LOCTEXT("UpdatePoseTip", "Replace the selected pose's values with the staged values, at its own time."),
                &FCk_ProceduralAction_EditorToolkit::OnUpdatePose, HasSelection)]
            + SHorizontalBox::Slot().AutoWidth()
            [Action(LOCTEXT("DuplicatePose", "Duplicate pose"), TEXT("CkActionEditorDuplicatePose"),
                LOCTEXT("DuplicatePoseTip", "Copy the selected pose to the pose time."),
                &FCk_ProceduralAction_EditorToolkit::OnDuplicatePose, HasSelection)]
            + SHorizontalBox::Slot().AutoWidth()
            [Action(LOCTEXT("MovePose", "Move pose here"), TEXT("CkActionEditorMovePose"),
                LOCTEXT("MovePoseTip", "Move the selected pose's keys to the pose time."),
                &FCk_ProceduralAction_EditorToolkit::OnMovePose, HasSelection)]
            + SHorizontalBox::Slot().AutoWidth()
            [Action(LOCTEXT("DeletePose", "Delete pose"), TEXT("CkActionEditorDeletePose"),
                LOCTEXT("DeletePoseTip", "Remove the selected pose's keys and its name."),
                &FCk_ProceduralAction_EditorToolkit::OnDeletePose, HasSelection)]
            + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [Action(LOCTEXT("CaptureSolvedPose", "Capture solved pose"), TEXT("CkActionEditorCaptureSolved"),
                LOCTEXT("CaptureSolvedTip", "Pause the solved preview on a pose, then key the settled geometry into each ticked channel's track space."),
                &FCk_ProceduralAction_EditorToolkit::OnCaptureSolvedPose, {})]
            + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SComboButton)
                .OnGetMenuContent_Lambda([WeakToolkit]() -> TSharedRef<SWidget>
                {
                    auto Menu = FMenuBuilder{true, nullptr};
                    const auto Toolkit = WeakToolkit.Pin();
                    const auto* Clip = Toolkit.IsValid() && Toolkit->_Session.IsValid() ? Toolkit->_Session->GetClip() : nullptr;
                    if (Clip != nullptr)
                    {
                        for (const auto& Slot : Clip->Get_Slots())
                        {
                            const auto Id = Slot.Get_Id();
                            Menu.AddMenuEntry(FText::FromName(Id), FText::GetEmpty(), FSlateIcon{},
                                FUIAction{FExecuteAction::CreateLambda([WeakToolkit, Id]()
                                {
                                    if (const auto Pinned = WeakToolkit.Pin())
                                    { Pinned->_MirrorDestination = Id; }
                                })});
                        }
                    }
                    return Menu.MakeWidget();
                })
                .ButtonContent()
                [SNew(STextBlock).Text_Lambda([WeakToolkit]() -> FText
                {
                    const auto Toolkit = WeakToolkit.Pin();
                    return Toolkit.IsValid() && NOT Toolkit->_MirrorDestination.IsNone()
                        ? FText::Format(LOCTEXT("MirrorTo", "Mirror to {0}"), FText::FromName(Toolkit->_MirrorDestination))
                        : LOCTEXT("ChooseMirror", "Choose the partner limb");
                })]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SComboButton)
                .OnGetMenuContent_Lambda([WeakToolkit, PlaneLabel]() -> TSharedRef<SWidget>
                {
                    auto Menu = FMenuBuilder{true, nullptr};
                    for (const auto Plane : {ECk_ProceduralAction_MirrorPlane::XZ, ECk_ProceduralAction_MirrorPlane::YZ,
                        ECk_ProceduralAction_MirrorPlane::XY})
                    {
                        Menu.AddMenuEntry(PlaneLabel(Plane), FText::GetEmpty(), FSlateIcon{},
                            FUIAction{FExecuteAction::CreateLambda([WeakToolkit, Plane]()
                            {
                                if (const auto Pinned = WeakToolkit.Pin())
                                { Pinned->_MirrorPlane = Plane; }
                            })});
                    }
                    return Menu.MakeWidget();
                })
                .ButtonContent()
                [SNew(STextBlock).Text_Lambda([WeakToolkit, PlaneLabel]() -> FText
                {
                    const auto Toolkit = WeakToolkit.Pin();
                    return Toolkit.IsValid() ? PlaneLabel(Toolkit->_MirrorPlane) : FText::GetEmpty();
                })]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [Action(LOCTEXT("MirrorSelected", "Mirror to selected limb"), TEXT("CkActionEditorMirror"),
                LOCTEXT("MirrorSelectedTip", "Reflect the selected limb's tracks onto the chosen partner limb across the chosen plane."),
                &FCk_ProceduralAction_EditorToolkit::OnMirrorToSelectedLimb, {})]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [ck_procedural_action_editor_toolkit::Tagged(SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakToolkit]() -> FText
        {
            const auto Toolkit = WeakToolkit.Pin();
            return Toolkit.IsValid() ? FText::FromString(Toolkit->_PoseMessage) : FText::GetEmpty();
        }), TEXT("CkActionEditorPoseMessage"))];
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildTimingPanel()
    -> TSharedRef<SWidget>
{
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [SNew(STextBlock).Text_Lambda([WeakToolkit]() -> FText
        {
            const auto Toolkit = WeakToolkit.Pin();
            return Toolkit.IsValid()
                ? FText::Format(LOCTEXT("PhaseTiming", "Phase timing: {0}"), FText::FromName(Toolkit->_SelectedPhase))
                : FText::GetEmpty();
        })]
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [ck_procedural_action_editor_toolkit::Tagged(SAssignNew(_ProgressPlot, SCkProceduralAction_ProgressPlot),
            TEXT("CkActionEditorProgressPlot"))]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(_TimingRows, SVerticalBox)]]
        + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
        [SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakToolkit]() -> FText
        {
            const auto Toolkit = WeakToolkit.Pin();
            return Toolkit.IsValid() ? FText::FromString(Toolkit->_TimingMessage) : FText::GetEmpty();
        })];
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildIssuePanel()
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(STextBlock).Text(LOCTEXT("Issues", "Issues (click one to select its phase and limb)"))]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(_IssueRows, SVerticalBox)]];
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BuildAdvanced(
        const TSharedRef<SWidget>& InCurvePanel)
    -> TSharedRef<SWidget>
{
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    const auto AuthoredOnly = [WeakToolkit]() -> bool
    {
        const auto Toolkit = WeakToolkit.Pin();
        return Toolkit.IsValid() && Toolkit->_Modes.IsValid()
            && Toolkit->_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Authored;
    };
    const auto Diagnostics = SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).Text(LOCTEXT("ModeAuthored", "Authored"))
                    .ToolTipText(LOCTEXT("ModeAuthoredTip", "Leave the Solver and show the requested pose."))
                    .OnClicked_Lambda([WeakToolkit]() -> FReply
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { return Toolkit->OnAuthoredClicked(); }
                        return FReply::Unhandled();
                    })]
                + SHorizontalBox::Slot().AutoWidth()
                [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton).Text(LOCTEXT("ModeRecorded", "Recorded"))
                    .ToolTipText(LOCTEXT("ModeRecordedTip", "Freeze the last Solver run as copied receipts and step through them."))
                    .OnClicked_Lambda([WeakToolkit]() -> FReply
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { return Toolkit->OnRecordedClicked(); }
                        return FReply::Unhandled();
                    }), TEXT("CkActionEditorRecordedMode"))]
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).Text(LOCTEXT("PreviousRecord", "Previous receipt"))
                    .OnClicked_Lambda([WeakToolkit]() -> FReply
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { return Toolkit->OnRecordedStep(-1); }
                        return FReply::Unhandled();
                    })]
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).Text(LOCTEXT("NextRecord", "Next receipt"))
                    .OnClicked_Lambda([WeakToolkit]() -> FReply
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { return Toolkit->OnRecordedStep(1); }
                        return FReply::Unhandled();
                    })]]
            + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
            [SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakToolkit]() -> FText
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() ? Toolkit->GetRecordedDetailText() : FText::GetEmpty();
            })]
            + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
            [SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakToolkit]() -> FText
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() ? Toolkit->_ObservationText : FText::GetEmpty();
            })]
            + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
            [SNew(SHorizontalBox)
                .IsEnabled_Lambda(AuthoredOnly)
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).Text(LOCTEXT("CurveX", "X"))
                    .OnClicked_Lambda([WeakToolkit]()
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { Toolkit->_CurveAxis = 0; Toolkit->RefreshCurveSelection(); }
                        return FReply::Handled();
                    })]
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).Text(LOCTEXT("CurveY", "Y"))
                    .OnClicked_Lambda([WeakToolkit]()
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { Toolkit->_CurveAxis = 1; Toolkit->RefreshCurveSelection(); }
                        return FReply::Handled();
                    })]
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).Text(LOCTEXT("CurveZ", "Z"))
                    .OnClicked_Lambda([WeakToolkit]()
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { Toolkit->_CurveAxis = 2; Toolkit->RefreshCurveSelection(); }
                        return FReply::Handled();
                    })]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [SNew(STextBlock).Text_Lambda([WeakToolkit]() -> FText
                {
                    const auto Toolkit = WeakToolkit.Pin();
                    return Toolkit.IsValid() && Toolkit->_CurveAdapter.IsValid()
                        ? Toolkit->_CurveAdapter->GetStatusText() : FText::GetEmpty();
                })]]
            + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
            [SNew(SBox).HeightOverride(160.0f).IsEnabled_Lambda(AuthoredOnly)[InCurvePanel]]
            + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
            [SNew(SBox).IsEnabled_Lambda(AuthoredOnly)[_PlotContainer.ToSharedRef()]]
        ];

    return ck_procedural_action_editor_toolkit::Tagged(SAssignNew(_Advanced, SExpandableArea)
        .InitiallyCollapsed(true)
        .AllowAnimatedTransition(false)
        .HeaderContent()
        [ck_procedural_action_editor_toolkit::Tagged(SNew(STextBlock).Text(LOCTEXT("Advanced",
            "Advanced: raw Details, recorded receipts, support diagnostics and per-track tools")),
            TEXT("CkActionEditorAdvancedHeader"))]
        .BodyContent()
        [
            SNew(SBox).HeightOverride(340.0f)
            [
                SNew(SSplitter)
                + SSplitter::Slot().Value(0.25f)
                [
                    SNew(SVerticalBox)
                    .IsEnabled_Lambda(AuthoredOnly)
                    + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
                    [SNew(STextBlock).Text(LOCTEXT("ClipProperties", "Clip motion and events"))]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [_ClipDetails.ToSharedRef()]
                ]
                + SSplitter::Slot().Value(0.25f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
                    [SNew(STextBlock).Text(LOCTEXT("PreviewProperties", "Separate preview subject and appearance"))]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [_PreviewDetails.ToSharedRef()]
                ]
                + SSplitter::Slot().Value(0.25f)
                [SAssignNew(_Timeline, SCkProceduralAction_Timeline).Session(_Session)
                    .IsEnabled_Lambda(AuthoredOnly)
                    .OnChanged_Lambda([WeakToolkit]()
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { Toolkit->RefreshFromTimeline(); }
                    })
                    .OnSelectionChanged_Lambda([WeakToolkit]()
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { Toolkit->RefreshCurveSelection(); }
                    })]
                + SSplitter::Slot().Value(0.25f)
                [Diagnostics]
            ]
        ], TEXT("CkActionEditorAdvanced"));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RebuildWorkspaceRows()
    -> void
{
    if (_Closed || NOT _Session.IsValid())
    { return; }
    _BuiltRevision = _Session->GetRevision();
    if (_SelectedPose.IsValid() && _Session->FindPoseGroup(_SelectedPose) == nullptr)
    { _SelectedPose.Invalidate(); }
    RebuildSubjectRows();
    RebuildPoseRows();
    RebuildIssueRows();
    RebuildInspectorRows();
    RebuildTimingRows();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RebuildSubjectRows()
    -> void
{
    if (NOT _SubjectRows.IsValid() || NOT _Session.IsValid())
    { return; }
    _SubjectRows->ClearChildren();
    const auto* Clip = _Session->GetClip();
    const auto* Config = _Session->GetPreviewConfig();
    if (Clip == nullptr)
    { return; }
    if (Config == nullptr)
    {
        _SubjectRows->AddSlot().AutoHeight()
        [SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("NoSubject",
            "Choose a preview subject above, or create one next to the clip, to list its body and limbs."))];
    }
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    const auto AddChannel = [this, &WeakToolkit](FName InChannel, const FText& InLabel, bool InKeyable, const FText& InTip)
    {
        const auto TagName = InChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName()
            ? FString{TEXT("Body")} : InChannel.ToString();
        _SubjectRows->AddSlot().AutoHeight().Padding(1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ck_procedural_action_editor_toolkit::Tagged(SNew(SCheckBox)
                .IsEnabled(InKeyable)
                .ToolTipText(InTip)
                .IsChecked_Lambda([WeakToolkit, InChannel]() -> ECheckBoxState
                {
                    const auto Toolkit = WeakToolkit.Pin();
                    return Toolkit.IsValid() && Toolkit->_Session.IsValid()
                        && Toolkit->_Session->GetParticipants().Contains(InChannel)
                        ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([WeakToolkit, InChannel](ECheckBoxState InState)
                {
                    if (const auto Toolkit = WeakToolkit.Pin())
                    { Toolkit->SetParticipant(InChannel, InState == ECheckBoxState::Checked); }
                }), *(TEXT("CkActionEditorParticipant_") + TagName))]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton).ToolTipText(InTip)
                .Text_Lambda([WeakToolkit, InChannel, InLabel]() -> FText
                {
                    const auto Toolkit = WeakToolkit.Pin();
                    return Toolkit.IsValid() && Toolkit->_SelectedChannel == InChannel
                        ? FText::Format(LOCTEXT("SelectedChannel", "{0}  SELECTED"), InLabel) : InLabel;
                })
                .OnClicked_Lambda([WeakToolkit, InChannel]() -> FReply
                {
                    const auto Toolkit = WeakToolkit.Pin();
                    if (NOT Toolkit.IsValid())
                    { return FReply::Unhandled(); }
                    Toolkit->_SelectedChannel = InChannel;
                    if (Toolkit->_Timeline.IsValid())
                    {
                        Toolkit->_Timeline->SelectTrack(Toolkit->_SelectedPhase,
                            InChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName() ? NAME_None : InChannel);
                    }
                    return FReply::Handled();
                }), *(TEXT("CkActionEditorChannel_") + TagName))]
        ];
    };

    const auto BodyEnabled = Clip->Get_BodyChannel().Get_Enabled();
    AddChannel(FCk_ProceduralAction_PoseGroup::Get_BodyChannelName(), LOCTEXT("BodyChannel", "Body"), BodyEnabled,
        BodyEnabled ? LOCTEXT("BodyTip", "Body presentation offset.")
            : LOCTEXT("BodyDisabledTip", "The clip's body channel is disabled; enable it in Advanced to key the body."));
    auto BoundLimbs = TArray<FName>{};
    for (const auto& Slot : Clip->Get_Slots())
    {
        const auto Id = Slot.Get_Id();
        const auto* Binding = Config == nullptr ? nullptr : Config->Get_SlotBindings().FindByPredicate(
            [Id](const FCk_ProceduralAction_SlotBinding& InBinding) { return InBinding.Get_Slot() == Id; });
        if (Binding != nullptr)
        { BoundLimbs.Add(Binding->Get_LimbId()); }
        AddChannel(Id, FText::Format(LOCTEXT("SlotChannel", "{0} → {1}"), FText::FromName(Id),
                Binding != nullptr ? FText::FromName(Binding->Get_LimbId()) : LOCTEXT("Unbound", "unbound")),
            true, Binding != nullptr ? LOCTEXT("BoundTip", "A clip slot and the preview limb it drives.")
                : LOCTEXT("UnboundTip", "This slot has no preview limb; bind it in Advanced so the Solver can play it."));
    }
    if (Config == nullptr)
    { return; }
    auto UnusedLimbs = TArray<FName>{};
    if (Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
    {
        if (const auto* Rig = Config->Get_WalkingRig().Get())
        {
            for (const auto& Leg : Rig->Get_Legs())
            {
                if (NOT BoundLimbs.Contains(Leg.Get_Id()))
                { UnusedLimbs.Add(Leg.Get_Id()); }
            }
        }
    }
    else
    {
        for (const auto& Limb : Config->Get_Layout().Get_Limbs())
        {
            if (NOT BoundLimbs.Contains(Limb.Get_Id()))
            { UnusedLimbs.Add(Limb.Get_Id()); }
        }
    }
    for (const auto Limb : UnusedLimbs)
    {
        _SubjectRows->AddSlot().AutoHeight().Padding(18.0f, 1.0f, 1.0f, 1.0f)
        [SNew(STextBlock).Text(FText::Format(LOCTEXT("UnusedLimb", "{0} (no clip slot)"), FText::FromName(Limb)))];
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RebuildPoseRows()
    -> void
{
    if (NOT _PoseRows.IsValid() || NOT _Session.IsValid())
    { return; }
    _PoseRows->ClearChildren();
    const auto* Clip = _Session->GetClip();
    if (Clip == nullptr)
    { return; }
    if (Clip->Get_Phases().IsEmpty())
    {
        _PoseRows->AddSlot().AutoHeight()
        [SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("NoPhases",
            "This clip has no phases. Create clips from a recipe, or add a phase with the per-track tools in Advanced."))];
        return;
    }
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    for (const auto& Phase : Clip->Get_Phases())
    {
        const auto PhaseId = Phase.Get_Id();
        const auto Row = SNew(SHorizontalBox);
        Row->AddSlot().AutoWidth()
        [SNew(SButton)
            .Text(FText::Format(LOCTEXT("PhaseRow", "Phase {0} ({1}s)"), FText::FromName(PhaseId),
                FText::AsNumber(Phase.Get_Duration().Get_Seconds())))
            .ButtonColorAndOpacity_Lambda([WeakToolkit, PhaseId]() -> FSlateColor
            {
                const auto Toolkit = WeakToolkit.Pin();
                return Toolkit.IsValid() && Toolkit->_SelectedPhase == PhaseId
                    ? FSlateColor{FLinearColor{0.95f, 0.75f, 0.25f}} : FSlateColor{FLinearColor::White};
            })
            .OnClicked_Lambda([WeakToolkit, PhaseId]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { Toolkit->SelectPhase(PhaseId); }
                return FReply::Handled();
            })];
        auto Views = TArray<const FCk_ProceduralActionPoseGroupView*>{};
        for (const auto& View : _Session->GetPoseGroups())
        {
            if (View.Get_Group().Get_PhaseId() == PhaseId)
            { Views.Add(&View); }
        }
        Views.Sort([](const FCk_ProceduralActionPoseGroupView& InA, const FCk_ProceduralActionPoseGroupView& InB)
            { return InA.Get_Group().Get_Time().Get_Value() < InB.Get_Group().Get_Time().Get_Value(); });
        for (const auto* View : Views)
        {
            const auto& Group = View->Get_Group();
            const auto PoseId = Group.Get_Id();
            auto Label = Group.Get_Label().ToString();
            if (Group.Get_Provenance() == ECk_ProceduralAction_PoseProvenance::Solved)
            { Label += TEXT(" (solved)"); }
            if (View->Get_IsBroken())
            { Label += TEXT(" (broken)"); }
            Row->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
            [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton)
                .Text(FText::FromString(Label))
                .ToolTipText(FText::FromString(FString::Printf(TEXT("%s at %.3f of %s; channels: %s%s"),
                    *Group.Get_Label().ToString(), Group.Get_Time().Get_Value(), *PhaseId.ToString(),
                    *FString::JoinBy(Group.Get_Channels(), TEXT(", "), [](FName InName) { return InName.ToString(); }),
                    View->Get_IsBroken() ? *FString::Printf(TEXT("\n%s"), *View->Get_BrokenReason()) : TEXT(""))))
                .ButtonColorAndOpacity_Lambda([WeakToolkit, PoseId]() -> FSlateColor
                {
                    const auto Toolkit = WeakToolkit.Pin();
                    return Toolkit.IsValid() && Toolkit->_SelectedPose == PoseId
                        ? FSlateColor{FLinearColor{0.3f, 0.75f, 1.0f}} : FSlateColor{FLinearColor::White};
                })
                .OnClicked_Lambda([WeakToolkit, PoseId]() -> FReply
                {
                    if (const auto Toolkit = WeakToolkit.Pin())
                    { Toolkit->SelectPose(PoseId); }
                    return FReply::Handled();
                }), TEXT("CkActionEditorPose"))];
        }
        auto Events = FString{};
        for (const auto& Marker : Clip->Get_Markers())
        {
            if (Marker.Get_At().Get_PhaseId() == PhaseId)
            { Events += FString::Printf(TEXT("  marker %s @ %.2f"), *Marker.Get_Id().ToString(), Marker.Get_At().Get_Time().Get_Value()); }
        }
        for (const auto& Window : Clip->Get_Windows())
        {
            if (Window.Get_Start().Get_PhaseId() == PhaseId)
            { Events += FString::Printf(TEXT("  window %s opens @ %.2f"), *Window.Get_Id().ToString(), Window.Get_Start().Get_Time().Get_Value()); }
            if (Window.Get_End().Get_PhaseId() == PhaseId)
            { Events += FString::Printf(TEXT("  window %s closes @ %.2f"), *Window.Get_Id().ToString(), Window.Get_End().Get_Time().Get_Value()); }
        }
        if (NOT Events.IsEmpty())
        { Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)[SNew(STextBlock).Text(FText::FromString(Events))]; }
        _PoseRows->AddSlot().AutoHeight().Padding(0.0f, 1.0f)[Row];
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RebuildInspectorRows()
    -> void
{
    if (NOT _InspectorRows.IsValid() || NOT _Session.IsValid())
    { return; }
    _InspectorRows->ClearChildren();
    const auto& Participants = _Session->GetParticipants();
    if (Participants.IsEmpty())
    {
        _InspectorRows->AddSlot().AutoHeight()
        [SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("NoParticipants",
            "Select a pose in the timeline, or tick channels under Body and limbs, to stage their values."))];
        return;
    }
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    constexpr const TCHAR* AxisNames[] = {TEXT("X"), TEXT("Y"), TEXT("Z")};
    for (const auto Channel : Participants)
    {
        const auto IsBody = Channel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName();
        const auto TagName = IsBody ? FString{TEXT("Body")} : Channel.ToString();
        _InspectorRows->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [SNew(STextBlock).Text(IsBody ? LOCTEXT("BodyOffset", "Body offset (cm)")
            : FText::Format(LOCTEXT("LimbEndpoint", "{0} endpoint (track space)"), FText::FromName(Channel)))];
        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            const auto Nudge = [WeakToolkit, Channel, Axis](double InDelta) -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { Toolkit->SetStagedValue(Channel, Axis, Toolkit->GetStagedValue(Channel, Axis) + InDelta); }
                return FReply::Handled();
            };
            _InspectorRows->AddSlot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
                [SNew(STextBlock).Text(FText::FromString(AxisNames[Axis]))]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [SNew(SSpinBox<double>)
                    .Delta(0.1)
                    .Value_Lambda([WeakToolkit, Channel, Axis]() -> double
                    {
                        const auto Toolkit = WeakToolkit.Pin();
                        return Toolkit.IsValid() ? Toolkit->GetStagedValue(Channel, Axis) : 0.0;
                    })
                    .OnValueChanged_Lambda([WeakToolkit, Channel, Axis](double InValue)
                    {
                        if (const auto Toolkit = WeakToolkit.Pin())
                        { Toolkit->SetStagedValue(Channel, Axis, InValue); }
                    })]
                + SHorizontalBox::Slot().AutoWidth()
                [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton).Text(LOCTEXT("NudgeMinus", "-1"))
                    .OnClicked_Lambda([Nudge]() { return Nudge(-1.0); }),
                    *FString::Printf(TEXT("CkActionEditorNudge_%s_%sMinus"), *TagName, AxisNames[Axis]))]
                + SHorizontalBox::Slot().AutoWidth()
                [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton).Text(LOCTEXT("NudgePlus", "+1"))
                    .OnClicked_Lambda([Nudge]() { return Nudge(1.0); }),
                    *FString::Printf(TEXT("CkActionEditorNudge_%s_%sPlus"), *TagName, AxisNames[Axis]))]
            ];
        }
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RebuildTimingRows()
    -> void
{
    if (NOT _TimingRows.IsValid() || NOT _Session.IsValid() || NOT _ProgressPlot.IsValid())
    { return; }
    _TimingRows->ClearChildren();
    const auto* Clip = _Session->GetClip();
    if (Clip == nullptr || Clip->Get_Phases().IsEmpty())
    {
        _ProgressPlot->SetPhase(nullptr, {});
        return;
    }
    const auto* Phase = Clip->Get_Phases().FindByPredicate([this](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == _SelectedPhase; });
    if (Phase == nullptr)
    {
        _SelectedPhase = Clip->Get_Phases()[0].Get_Id();
        Phase = &Clip->Get_Phases()[0];
    }
    const auto PhaseId = Phase->Get_Id();
    const auto& Progress = Phase->Get_Progress();
    const auto Custom = Progress.Get_Mode() == ECk_ProceduralAction_ProgressMode::CustomCurve;
    auto KeyPoints = TArray<FVector2D>{};
    for (const auto& Key : Progress.Get_Keys())
    { KeyPoints.Add(FVector2D{Key.Get_Time().Get_Value(), Key.Get_Value().Get_Value()}); }
    const auto* Compiled = _Session->IsCompiled()
        ? _Session->GetCompiledClip().Get_Phases().FindByPredicate([PhaseId](const ck::procedural_action::FCompiledPhase& InPhase)
            { return InPhase.Get_Id() == PhaseId; })
        : nullptr;
    _ProgressPlot->SetPhase(Compiled, KeyPoints);
    if (Compiled == nullptr)
    { _TimingMessage = TEXT("The clip does not compile yet; the curve is drawn from the compiled phase once it does."); }

    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    _TimingRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [SNew(STextBlock).Text(LOCTEXT("Duration", "Duration (s)"))]
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f)
        [ck_procedural_action_editor_toolkit::Tagged(SNew(SSpinBox<double>)
            .MinValue(0.01).MaxValue(60.0).Delta(0.01)
            .Value(Phase->Get_Duration().Get_Seconds())
            .OnValueCommitted_Lambda([WeakToolkit](double InValue, ETextCommit::Type)
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { Toolkit->OnDurationCommitted(InValue); }
            }), TEXT("CkActionEditorPhaseDuration"))]
    ];
    const auto Easing = Phase->Get_Easing();
    _TimingRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SComboButton)
            .IsEnabled(NOT Custom)
            .OnGetMenuContent_Lambda([WeakToolkit]() -> TSharedRef<SWidget>
            {
                auto Menu = FMenuBuilder{true, nullptr};
                const auto* Enum = StaticEnum<ECk_ProceduralAction_Easing>();
                for (auto Index = 0; Index < Enum->NumEnums() - 1; ++Index)
                {
                    const auto Value = static_cast<ECk_ProceduralAction_Easing>(Enum->GetValueByIndex(Index));
                    Menu.AddMenuEntry(Enum->GetDisplayNameTextByIndex(Index), FText::GetEmpty(), FSlateIcon{},
                        FUIAction{FExecuteAction::CreateLambda([WeakToolkit, Value]()
                        {
                            if (const auto Toolkit = WeakToolkit.Pin())
                            { Toolkit->ApplyPhaseTiming(Value, FCk_ProceduralAction_ProgressCurve{}); }
                        })});
                }
                return Menu.MakeWidget();
            })
            .ButtonContent()
            [SNew(STextBlock).Text(FText::Format(LOCTEXT("EasingLabel", "Preset easing: {0}"),
                StaticEnum<ECk_ProceduralAction_Easing>()->GetDisplayNameTextByValue(static_cast<int64>(Easing))))]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
        [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton)
            .Text(Custom ? LOCTEXT("UsePreset", "Use preset easing") : LOCTEXT("UseCustom", "Custom curve"))
            .ToolTipText(LOCTEXT("CustomTip", "A custom curve remaps the phase's progress through monotone keys; it starts from the current curve."))
            .OnClicked_Lambda([WeakToolkit, Custom, PhaseId]() -> FReply
            {
                const auto Toolkit = WeakToolkit.Pin();
                if (NOT Toolkit.IsValid())
                { return FReply::Unhandled(); }
                if (Custom)
                {
                    Toolkit->ApplyPhaseTiming(ECk_ProceduralAction_Easing::Linear, FCk_ProceduralAction_ProgressCurve{});
                    return FReply::Handled();
                }
                const auto* Compiled = Toolkit->_Session->IsCompiled()
                    ? Toolkit->_Session->GetCompiledClip().Get_Phases().FindByPredicate(
                        [PhaseId](const ck::procedural_action::FCompiledPhase& InPhase) { return InPhase.Get_Id() == PhaseId; })
                    : nullptr;
                auto Keys = TArray<FCk_ProceduralAction_ProgressKey>{};
                for (const auto Time : {0.0, 0.25, 0.5, 0.75, 1.0})
                {
                    const auto Value = Time == 0.0 || Time == 1.0 || Compiled == nullptr
                        ? Time : ck::procedural_action::EvaluateProgress(*Compiled, Time);
                    Keys.Add(FCk_ProceduralAction_ProgressKey{FCk_FloatRange_0to1{Time}, FCk_FloatRange_0to1{Value}});
                }
                auto Curve = FCk_ProceduralAction_ProgressCurve{};
                Curve.Set_Mode(ECk_ProceduralAction_ProgressMode::CustomCurve);
                Curve.Set_Keys(Keys);
                Toolkit->ApplyPhaseTiming(ECk_ProceduralAction_Easing::Linear, Curve);
                return FReply::Handled();
            }), TEXT("CkActionEditorCustomCurve"))]
    ];
    if (NOT Custom)
    { return; }
    for (auto Index = 0; Index < Progress.Get_Keys().Num(); ++Index)
    {
        const auto& Key = Progress.Get_Keys()[Index];
        const auto Endpoint = Index == 0 || Index == Progress.Get_Keys().Num() - 1;
        _TimingRows->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
            .IsEnabled(NOT Endpoint)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(STextBlock).Text(FText::Format(LOCTEXT("KeyRow", "Key {0}: time"), FText::AsNumber(Index)))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f, 0.0f)
            [SNew(SSpinBox<double>).MinValue(0.0).MaxValue(1.0).Delta(0.01).Value(Key.Get_Time().Get_Value())
                .OnValueCommitted_Lambda([WeakToolkit, Index](double InValue, ETextCommit::Type)
                {
                    if (const auto Toolkit = WeakToolkit.Pin())
                    { Toolkit->OnProgressKeyCommitted(Index, true, InValue); }
                })]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(STextBlock).Text(LOCTEXT("KeyProgress", "progress"))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f, 0.0f)
            [SNew(SSpinBox<double>).MinValue(0.0).MaxValue(1.0).Delta(0.01).Value(Key.Get_Value().Get_Value())
                .OnValueCommitted_Lambda([WeakToolkit, Index](double InValue, ETextCommit::Type)
                {
                    if (const auto Toolkit = WeakToolkit.Pin())
                    { Toolkit->OnProgressKeyCommitted(Index, false, InValue); }
                })]
        ];
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RebuildIssueRows()
    -> void
{
    if (NOT _IssueRows.IsValid() || NOT _Session.IsValid())
    { return; }
    _IssueRows->ClearChildren();
    const auto& Diagnostics = _Session->GetDiagnostics();
    if (Diagnostics.IsEmpty())
    {
        _IssueRows->AddSlot().AutoHeight()
        [SNew(STextBlock).Text(_Session->IsCompiled()
            ? LOCTEXT("NoIssues", "No issues: the clip compiles.") : LOCTEXT("NoClipIssues", "The clip is unavailable."))];
        return;
    }
    const auto WeakToolkit = TWeakPtr<FCk_ProceduralAction_EditorToolkit>{
        StaticCastSharedRef<FCk_ProceduralAction_EditorToolkit>(AsShared())};
    for (const auto& Diagnostic : Diagnostics)
    {
        const auto PhaseId = Diagnostic.Get_PhaseId();
        const auto SlotId = Diagnostic.Get_Slot();
        _IssueRows->AddSlot().AutoHeight()
        [ck_procedural_action_editor_toolkit::Tagged(SNew(SButton)
            .Text(FText::FromString(FString::Printf(TEXT("%s%s [%s]: expected %s; got %s"),
                Diagnostic.Get_Severity() == ck::procedural_action::EDiagnosticSeverity::Error ? TEXT("") : TEXT("(warning) "),
                *Diagnostic.Get_FieldPath(), *Diagnostic.Get_Code().ToString(), *Diagnostic.Get_Expected(),
                *Diagnostic.Get_Actual())))
            .OnClicked_Lambda([WeakToolkit, PhaseId, SlotId]() -> FReply
            {
                if (const auto Toolkit = WeakToolkit.Pin())
                { return Toolkit->OnIssueClicked(PhaseId, SlotId); }
                return FReply::Unhandled();
            }), TEXT("CkActionEditorIssue"))];
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnIssueClicked(
        FName InPhaseId,
        FName InSlotId)
    -> FReply
{
    if (_Closed || NOT _Session.IsValid())
    { return FReply::Unhandled(); }
    if (NOT InSlotId.IsNone())
    { _SelectedChannel = InSlotId; }
    if (NOT InPhaseId.IsNone())
    { SelectPhase(InPhaseId); }
    else if (_Timeline.IsValid() && NOT InSlotId.IsNone())
    { _Timeline->SelectTrack(_Timeline->GetSelectedPhase(), InSlotId); }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    LocateTime(
        FCk_Time InTime,
        FName& OutPhaseId,
        double& OutNormalizedTime) const
    -> bool
{
    const auto* Clip = _Session.IsValid() ? _Session->GetClip() : nullptr;
    if (Clip == nullptr || Clip->Get_Phases().IsEmpty() || NOT FMath::IsFinite(InTime.Get_Seconds()))
    { return false; }
    const auto Seconds = InTime.Get_Seconds();
    auto Start = 0.0;
    const auto& Phases = Clip->Get_Phases();
    for (auto Index = 0; Index < Phases.Num(); ++Index)
    {
        const auto Duration = Phases[Index].Get_Duration().Get_Seconds();
        if (NOT FMath::IsFinite(Duration) || Duration <= 0.0)
        { return false; }
        if (Seconds < Start + Duration || Index == Phases.Num() - 1)
        {
            OutPhaseId = Phases[Index].Get_Id();
            OutNormalizedTime = FMath::Clamp((Seconds - Start) / Duration, 0.0, 1.0);
            return true;
        }
        Start += Duration;
    }
    return false;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SyncStagingTimeFromTransport()
    -> void
{
    auto PhaseId = FName{};
    auto Time = 0.0;
    if (NOT _Closed && _Session.IsValid() && LocateTime(_Session->GetTime(), PhaseId, Time))
    { _Session->SetStagingTime(PhaseId, FCk_FloatRange_0to1{Time}); }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SelectPose(
        const FGuid& InPoseId)
    -> void
{
    if (_Closed || NOT _Session.IsValid())
    { return; }
    const auto* View = _Session->FindPoseGroup(InPoseId);
    const auto* Clip = _Session->GetClip();
    if (View == nullptr || Clip == nullptr)
    { return; }
    const auto Group = View->Get_Group();
    _SelectedPose = InPoseId;
    auto Start = 0.0;
    for (const auto& Phase : Clip->Get_Phases())
    {
        if (Phase.Get_Id() == Group.Get_PhaseId())
        {
            _Session->SetTime(FCk_Time{Start + Phase.Get_Duration().Get_Seconds() * Group.Get_Time().Get_Value()});
            break;
        }
        Start += Phase.Get_Duration().Get_Seconds();
    }
    auto Error = FString{};
    _PoseMessage = _Session->LoadStagingFromPoseGroup(InPoseId, Error)
        ? FString::Printf(TEXT("Selected %s: its key values are staged."), *Group.Get_Label().ToString())
        : Error;
    SelectPhase(Group.Get_PhaseId());
    RebuildInspectorRows();
    RefreshModePose();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SelectPhase(
        FName InPhaseId)
    -> void
{
    if (_Closed)
    { return; }
    _SelectedPhase = InPhaseId;
    if (_Timeline.IsValid())
    {
        _Timeline->SelectTrack(InPhaseId,
            _SelectedChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName() ? NAME_None : _SelectedChannel);
    }
    _TimingMessage.Reset();
    RebuildTimingRows();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SetParticipant(
        FName InChannel,
        bool InParticipates)
    -> void
{
    if (_Closed || NOT _Session.IsValid())
    { return; }
    auto Participants = _Session->GetParticipants();
    if (InParticipates)
    {
        Participants.AddUnique(InChannel);
        _Session->SetParticipants(Participants);
        if (InChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
        {
            if (_Session->GetStagingPose().Get_BodyMode() != ECk_ProceduralAction_ValueMode::Set)
            { _Session->SetStagingBody(FTransform::Identity); }
        }
        else if (NOT _Session->GetStagingPose().Get_Limbs().ContainsByPredicate(
            [InChannel](const FCk_ProceduralAction_LimbPoseTarget& InTarget) { return InTarget.Get_Slot() == InChannel; }))
        { _Session->SetStagingLimb(FCk_ProceduralAction_LimbPoseTarget{InChannel, FVector::ZeroVector}); }
    }
    else
    {
        Participants.Remove(InChannel);
        _Session->SetParticipants(Participants);
    }
    RebuildInspectorRows();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetStagedValue(
        FName InChannel,
        int32 InAxis) const
    -> double
{
    if (NOT _Session.IsValid())
    { return 0.0; }
    const auto& Pose = _Session->GetStagingPose();
    if (InChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
    { return Pose.Get_Body().GetTranslation()[InAxis]; }
    const auto* Target = Pose.Get_Limbs().FindByPredicate([InChannel](const FCk_ProceduralAction_LimbPoseTarget& InTarget)
        { return InTarget.Get_Slot() == InChannel; });
    return Target != nullptr ? Target->Get_Position()[InAxis] : 0.0;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SetStagedValue(
        FName InChannel,
        int32 InAxis,
        double InValue)
    -> void
{
    if (_Closed || NOT _Session.IsValid() || NOT FMath::IsFinite(InValue))
    { return; }
    const auto& Pose = _Session->GetStagingPose();
    if (InChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
    {
        auto Body = Pose.Get_Body();
        auto Translation = Body.GetTranslation();
        Translation[InAxis] = InValue;
        Body.SetTranslation(Translation);
        _Session->SetStagingBody(Body);
        return;
    }
    const auto* Existing = Pose.Get_Limbs().FindByPredicate([InChannel](const FCk_ProceduralAction_LimbPoseTarget& InTarget)
        { return InTarget.Get_Slot() == InChannel; });
    auto Target = Existing != nullptr ? *Existing : FCk_ProceduralAction_LimbPoseTarget{InChannel, FVector::ZeroVector};
    auto Position = Target.Get_Position();
    Position[InAxis] = InValue;
    Target.Set_Position(Position);
    _Session->SetStagingLimb(Target);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetNewPoseLabel() const
    -> FName
{
    const auto Text = _PoseLabel.IsValid() ? _PoseLabel->GetText().ToString().TrimStartAndEnd() : FString{};
    return Text.IsEmpty() ? NAME_None : FName{*Text};
}

auto
    FCk_ProceduralAction_EditorToolkit::
    FinishPoseCommand(
        bool InSucceeded,
        const FString& InSuccessMessage,
        const FString& InError)
    -> void
{
    _PoseMessage = InSucceeded ? InSuccessMessage : InError;
    if (InSucceeded)
    {
        if (_PoseLabel.IsValid())
        { _PoseLabel->SetText(FText::GetEmpty()); }
        RefreshFromWorkspace();
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnCapturePose()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto PoseId = FGuid{};
    auto Error = FString{};
    const auto Captured = Commands.CapturePose(_Session->GetStagingPhaseId(), _Session->GetStagingPose(),
        _Session->GetParticipants(), GetNewPoseLabel(), PoseId, Error);
    FinishPoseCommand(Captured, TEXT("Captured a new pose from the staged values."), Error);
    if (Captured)
    { SelectPose(PoseId); }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnUpdatePose()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto Error = FString{};
    const auto PoseId = _SelectedPose;
    const auto Updated = Commands.UpdatePose(PoseId, _Session->GetStagingPose(), Error);
    FinishPoseCommand(Updated, TEXT("Updated the selected pose."), Error);
    if (Updated)
    { SelectPose(PoseId); }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnDuplicatePose()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto PoseId = FGuid{};
    auto Error = FString{};
    const auto Duplicated = Commands.DuplicatePose(_SelectedPose, _Session->GetStagingPhaseId(),
        _Session->GetStagingPose().Get_Time().Get_Value(), PoseId, Error);
    FinishPoseCommand(Duplicated, TEXT("Duplicated the selected pose at the pose time."), Error);
    if (Duplicated)
    { SelectPose(PoseId); }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnMovePose()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto Error = FString{};
    const auto PoseId = _SelectedPose;
    const auto Moved = Commands.MovePose(PoseId, _Session->GetStagingPhaseId(),
        _Session->GetStagingPose().Get_Time().Get_Value(), Error);
    FinishPoseCommand(Moved, TEXT("Moved the selected pose to the pose time."), Error);
    if (Moved)
    { SelectPose(PoseId); }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnDeletePose()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto Error = FString{};
    const auto Deleted = Commands.DeletePose(_SelectedPose, Error);
    if (Deleted)
    { _SelectedPose.Invalidate(); }
    FinishPoseCommand(Deleted, TEXT("Deleted the selected pose and its keys."), Error);
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnCaptureSolvedPose()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr || NOT _Modes.IsValid())
    { return FReply::Unhandled(); }
    const auto* Solver = _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver ? _Modes->GetSolver() : nullptr;
    if (Solver == nullptr)
    {
        _PoseMessage = _SolverRevision != 0 && _SolverRevision != _Session->GetRevision()
            ? FString::Printf(TEXT("The solved preview ran on revision %llu, but the clip and preview are now at revision %llu. Press Play solved preview to re-run it, then pause on the pose to capture."),
                _SolverRevision, _Session->GetRevision())
            : FString{TEXT("No solved preview is running. Press Play solved preview, then pause on the pose to capture.")};
        return FReply::Handled();
    }
    auto PhaseId = FName{};
    auto Time = 0.0;
    if (NOT LocateTime(_Modes->GetTime(), PhaseId, Time))
    {
        _PoseMessage = TEXT("The Solver time does not fall inside a phase of this clip.");
        return FReply::Handled();
    }
    auto Target = FCk_ProceduralActionSolvedCaptureTarget{_Session->GetPreviewConfig(), _Session->GetRevision(), PhaseId,
        FCk_FloatRange_0to1{Time}, _Session->GetParticipants()};
    Target.Set_Label(GetNewPoseLabel());
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto PoseId = FGuid{};
    auto Error = FString{};
    const auto Captured = Commands.CaptureSolvedPose(FCk_ProceduralActionSolvedSnapshot::Make(*Solver), Target, PoseId, Error);
    FinishPoseCommand(Captured, FString::Printf(TEXT("Captured the solved pose at %s %.3f."), *PhaseId.ToString(), Time), Error);
    if (Captured)
    { SelectPose(PoseId); }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnMirrorToSelectedLimb()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    if (_SelectedChannel.IsNone() || _SelectedChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
    {
        _PoseMessage = TEXT("Select the source limb under Body and limbs first; the body channel is never mirrored.");
        return FReply::Handled();
    }
    if (_MirrorDestination.IsNone())
    {
        _PoseMessage = TEXT("Choose the partner limb to mirror onto.");
        return FReply::Handled();
    }
    const auto Normal = _MirrorPlane == ECk_ProceduralAction_MirrorPlane::YZ ? EAxis::X
        : _MirrorPlane == ECk_ProceduralAction_MirrorPlane::XY ? EAxis::Z : EAxis::Y;
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto Error = FString{};
    const auto Mirrored = Commands.MirrorSlot(_SelectedChannel, _MirrorDestination, Normal, Error);
    FinishPoseCommand(Mirrored, FString::Printf(TEXT("Mirrored %s onto %s."), *_SelectedChannel.ToString(),
        *_MirrorDestination.ToString()), Error);
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnDurationCommitted(
        double InSeconds)
    -> void
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return; }
    const auto* Phase = _Session->GetClip()->Get_Phases().FindByPredicate([this](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == _SelectedPhase; });
    if (Phase == nullptr || FMath::IsNearlyEqual(Phase->Get_Duration().Get_Seconds(), InSeconds))
    { return; }
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto Error = FString{};
    if (Commands.SetPhaseDuration(_SelectedPhase, FCk_Time{InSeconds}, Error))
    {
        _TimingMessage.Reset();
        RefreshFromWorkspace();
        return;
    }
    _TimingMessage = Error;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    ApplyPhaseTiming(
        ECk_ProceduralAction_Easing InEasing,
        const FCk_ProceduralAction_ProgressCurve& InProgress)
    -> void
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return; }
    auto Commands = FCk_ProceduralActionAuthoringCommands{_Session->GetClip()};
    auto Error = FString{};
    if (Commands.SetPhaseTiming(_SelectedPhase, InEasing, InProgress, Error))
    {
        _TimingMessage.Reset();
        RefreshFromWorkspace();
        return;
    }
    _TimingMessage = Error;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnProgressKeyCommitted(
        int32 InIndex,
        bool InTimeAxis,
        double InValue)
    -> void
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return; }
    const auto* Phase = _Session->GetClip()->Get_Phases().FindByPredicate([this](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == _SelectedPhase; });
    if (Phase == nullptr || NOT Phase->Get_Progress().Get_Keys().IsValidIndex(InIndex))
    { return; }
    auto Curve = Phase->Get_Progress();
    auto Keys = Curve.Get_Keys();
    const auto Current = InTimeAxis ? Keys[InIndex].Get_Time().Get_Value() : Keys[InIndex].Get_Value().Get_Value();
    if (FMath::IsNearlyEqual(Current, InValue))
    { return; }
    if (InTimeAxis)
    { Keys[InIndex].Set_Time(FCk_FloatRange_0to1{InValue}); }
    else
    { Keys[InIndex].Set_Value(FCk_FloatRange_0to1{InValue}); }
    Curve.Set_Keys(Keys);
    ApplyPhaseTiming(ECk_ProceduralAction_Easing::Linear, Curve);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RebuildPlots(
        bool InRefreshSamples)
    -> void
{
    if (_Closed || NOT _PlotContainer.IsValid() || NOT _Session.IsValid())
    { return; }

    auto Wanted = TArray<FName>{NAME_None};
    for (const auto& Slot : _Session->GetCompiledClip().Get_Slots())
    { Wanted.Add(Slot.Get_Id()); }
    if (Wanted == _PlotSlots)
    {
        for (const auto& Plot : _Plots)
        {
            if (auto Pinned = Plot.Pin())
            {
                if (InRefreshSamples)
                { Pinned->RefreshSamples(); }
                Pinned->Invalidate(EInvalidateWidgetReason::Paint);
            }
        }
        return;
    }

    _PlotSlots = MoveTemp(Wanted);
    _Plots.Reset();
    _PlotContainer->ClearChildren();
    for (const auto& SlotId : _PlotSlots)
    {
        TSharedPtr<SCkProceduralAction_AuthoredPlot> Plot;
        _PlotContainer->AddSlot().AutoHeight().Padding(0.0f, 7.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Text(FText::FromName(SlotId.IsNone() ? FName(TEXT("Body")) : SlotId))]
            + SVerticalBox::Slot().AutoHeight()
            [SAssignNew(Plot, SCkProceduralAction_AuthoredPlot).Session(_Session).Slot(SlotId)]
        ];
        _Plots.Add(Plot);
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RefreshFromEdit()
    -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Ck_ActionEditor_RefreshFromEdit);
    if (_Closed || NOT _Session.IsValid())
    { return; }
    _Session->Refresh();
    if (_Modes.IsValid())
    { _Modes->SourceChanged(); }
    if (_Viewport.IsValid())
    { RefreshModePose(); }
    if (_Timeline.IsValid())
    { _Timeline->Refresh(); }
    if (_CurveAdapter.IsValid())
    { _CurveAdapter->Refresh(); }
    RebuildPlots(true);
    RebuildWorkspaceRows();
    if (NOT _AppearanceRefreshPending)
    { _SourceAssetRefreshPending = false; }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RefreshFromTimeline()
    -> void
{
    if (_Closed || NOT _Session.IsValid())
    { return; }
    _Session->Refresh();
    if (_Modes.IsValid())
    { _Modes->SourceChanged(); }
    if (_Viewport.IsValid())
    { RefreshModePose(); }
    if (_ClipDetails.IsValid())
    { _ClipDetails->ForceRefresh(); }
    if (_CurveAdapter.IsValid())
    { _CurveAdapter->Refresh(); }
    RebuildPlots(true);
    RebuildWorkspaceRows();
    if (NOT _AppearanceRefreshPending)
    { _SourceAssetRefreshPending = false; }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RefreshFromWorkspace()
    -> void
{
    if (_Closed || NOT _Session.IsValid())
    { return; }
    if (_Timeline.IsValid())
    { _Timeline->Refresh(); }
    RefreshFromTimeline();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RefreshFromCurve()
    -> void
{
    if (_Closed || NOT _Session.IsValid())
    { return; }
    _Session->Refresh();
    if (_Modes.IsValid())
    { _Modes->SourceChanged(); }
    if (_ClipDetails.IsValid())
    { _ClipDetails->ForceRefresh(); }
    if (_Timeline.IsValid())
    { _Timeline->Refresh(); }
    if (_Viewport.IsValid())
    { RefreshModePose(); }
    RebuildPlots(true);
    RebuildWorkspaceRows();
    if (NOT _AppearanceRefreshPending)
    { _SourceAssetRefreshPending = false; }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RefreshCurveSelection()
    -> void
{
    if (_Closed || NOT _CurveAdapter.IsValid() || NOT _Timeline.IsValid())
    { return; }
    _CurveAdapter->Select(_Timeline->GetSelectedPhase(), _Timeline->GetSelectedSlot(), _CurveAxis);
    if (_Viewport.IsValid())
    { _Viewport->SetSelectedTrack(_Timeline->GetSelectedPhase(), _Timeline->GetSelectedSlot()); }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnSourceAssetChanged(
        UObject* InObject,
        FPropertyChangedEvent&)
    -> void
{
    if (_Closed || NOT _Session.IsValid())
    { return; }
    if (InObject == _Session->GetClip())
    { _SourceAssetRefreshPending = true; return; }
    const auto* Config = _Session->GetPreviewConfig();
    if (Config == nullptr)
    { return; }
    const auto Walking = Config->Get_SubjectMode()
        == ECk_ProceduralActionPreview_SubjectMode::Walking;
    if (InObject != Config && (NOT Walking
        || (InObject != Config->Get_WalkingRig().Get()
            && InObject != Config->Get_WalkingGait().Get())))
    { return; }
    _SourceAssetRefreshPending = true;
    _AppearanceRefreshPending = true;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnPreviewPicked(
        const FAssetData& InAsset)
    -> void
{
    if (_Closed || NOT _Session.IsValid() || NOT _PreviewDetails.IsValid())
    { return; }
    auto* Config = Cast<UCk_ProceduralActionPreview_Config>(InAsset.GetAsset());
    _Session->SetPreviewConfig(Config);
    _PreviewSelectionError.Empty();
    if (const auto* Clip = _Session->GetClip())
    {
        auto* Settings = GetMutableDefault<UCk_ProceduralActionEditor_UserSettings>();
        if (Settings->SetPreviewPath(Clip->GetPathName(),
            Config != nullptr ? Config->GetPathName() : FString{}))
        { Settings->SaveConfig(); }
    }
    if (_Modes.IsValid())
    { _Modes->SourceChanged(); }
    _PreviewDetails->SetObject(Config, true);
    if (_Viewport.IsValid())
    {
        RefreshModePose();
        _Viewport->RefreshAppearance();
    }
    if (_Timeline.IsValid())
    { _Timeline->Refresh(); }
    RebuildPlots(true);
    RebuildWorkspaceRows();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnCreatePreviewClicked()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    const auto* Clip = _Session->GetClip();
    const auto PackagePath = FPackageName::GetLongPackagePath(Clip->GetOutermost()->GetName());
    if (NOT FPackageName::IsValidLongPackageName(PackagePath))
    {
        _PreviewSelectionError = TEXT("Save the clip asset before creating its preview config.");
        return FReply::Handled();
    }
    auto Name = Clip->GetName();
    if (Name.EndsWith(TEXT("_DA")))
    { Name.LeftChopInline(3); }
    Name += TEXT("_Preview_DA");
    const auto ObjectPath = FString::Printf(TEXT("%s/%s.%s"),
        *PackagePath, *Name, *Name);
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    const auto Existing = Registry.GetAssetByObjectPath(FSoftObjectPath{ObjectPath});
    if (Existing.IsValid())
    {
        if (Cast<UCk_ProceduralActionPreview_Config>(Existing.GetAsset()) == nullptr)
        {
            _PreviewSelectionError = FString::Printf(
                TEXT("Sibling asset already exists with another type: %s"), *ObjectPath);
            return FReply::Handled();
        }
        OnPreviewPicked(Existing);
        return FReply::Handled();
    }
    auto& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    auto* Factory = NewObject<UCk_ProceduralActionPreview_Factory>();
    auto* Created = Cast<UCk_ProceduralActionPreview_Config>(Tools.CreateAsset(
        Name, PackagePath, UCk_ProceduralActionPreview_Config::StaticClass(), Factory));
    if (Created == nullptr)
    {
        _PreviewSelectionError = FString::Printf(
            TEXT("Could not create preview config at %s"), *ObjectPath);
        return FReply::Handled();
    }
    OnPreviewPicked(FAssetData{Created});
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetPreviewPath() const
    -> FString
{
    if (_Closed || NOT _Session.IsValid())
    { return {}; }
    const auto* Config = _Session->GetPreviewConfig();
    return Config != nullptr ? Config->GetPathName() : FString{};
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnPlayClicked()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || NOT _Modes.IsValid())
    { return FReply::Unhandled(); }
    if (_RequestedPreview)
    {
        if (_Modes->GetMode() != FCkProceduralActionEditorModes::EMode::Authored)
        { _Modes->EnterAuthored(); }
        _Session->SetPlaying(NOT _Session->IsPlaying());
        RefreshModePose();
        return FReply::Handled();
    }
    const auto* Solver = _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver ? _Modes->GetSolver() : nullptr;
    const auto NeedsRun = Solver == nullptr
        || Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Finished
        || Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Error
        || Solver->GetStatus() == FCk_ProceduralActionSolverWorld::EStatus::Cancelled;
    if (NeedsRun)
    {
        _Session->SetPlaying(false);
        _SolverRevision = _Session->GetRevision();
        if (NOT _Modes->StartSolver())
        {
            RefreshModePose();
            return FReply::Handled();
        }
    }
    _Modes->PlayPause();
    RefreshModePose();
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetPlayLabel() const
    -> FText
{
    if (_Closed || NOT _Session.IsValid() || NOT _Modes.IsValid())
    { return FText::GetEmpty(); }
    const auto Playing = _RequestedPreview
        ? _Session->IsPlaying()
        : _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver && _Modes->GetIsPlaying();
    if (Playing)
    { return LOCTEXT("Pause", "Pause"); }
    return _RequestedPreview ? LOCTEXT("PlayRequested", "Play requested preview") : LOCTEXT("PlaySolved", "Play solved preview");
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetCanPressPlay() const
    -> bool
{
    return NOT _Closed && _Session.IsValid() && _Modes.IsValid() && _Session->IsCompiled()
        && _Session->GetPreviewConfig() != nullptr && (NOT _RequestedPreview || _Session->CanPlay());
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetPlayBlockedReason() const
    -> FString
{
    if (_Closed || NOT _Session.IsValid() || NOT _Modes.IsValid())
    { return TEXT("The editor is closing."); }
    if (NOT _Session->IsCompiled())
    {
        const auto& Diagnostics = _Session->GetDiagnostics();
        const auto* First = Diagnostics.FindByPredicate([](const ck::procedural_action::FDiagnostic& InDiagnostic)
            { return InDiagnostic.Get_Severity() == ck::procedural_action::EDiagnosticSeverity::Error; });
        if (First == nullptr && NOT Diagnostics.IsEmpty())
        { First = &Diagnostics[0]; }
        return First == nullptr
            ? FString{TEXT("Play is disabled until the clip compiles.")}
            : FString::Printf(TEXT("Play is disabled until the clip compiles. %s [%s]: expected %s; got %s."),
                *First->Get_FieldPath(), *First->Get_Code().ToString(), *First->Get_Expected(), *First->Get_Actual());
    }
    if (_Session->GetPreviewConfig() == nullptr)
    { return TEXT("Choose a preview subject to play this clip."); }
    if (_RequestedPreview && NOT _Session->CanPlay())
    { return _Session->GetStatusText().ToString(); }
    return {};
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnRequestedPreviewChanged(
        bool InRequested)
    -> void
{
    if (_Closed || NOT _Session.IsValid() || NOT _Modes.IsValid())
    { return; }
    _RequestedPreview = InRequested;
    _Session->SetPlaying(false);
    if (InRequested)
    { _Modes->EnterAuthored(); }
    RefreshModePose();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnSavePairClicked()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || _Session->GetClip() == nullptr)
    { return FReply::Unhandled(); }
    RefreshFromEdit();
    auto Assets = TArray<UObject*>{_Session->GetClip()};
    if (auto* Preview = _Session->GetPreviewConfig())
    { Assets.Add(Preview); }
    _SaveReport = ck::procedural_action_editor::DescribeSaveResults(ck::procedural_action_editor::SaveAssets(Assets));
    if (_Session->GetPreviewConfig() == nullptr)
    { _SaveReport += TEXT("\nNo preview config is selected, so only the clip was saved."); }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnResetClicked()
    -> FReply
{
    if (GIsAutomationTesting && _Session.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("Procedural Action Reset invoked: mode=%d time=%.6f"),
            _Modes.IsValid() ? static_cast<int32>(_Modes->GetMode()) : -1,
            _Session->GetTime().Get_Seconds());
    }
    if (_Closed || NOT _Session.IsValid())
    { return FReply::Unhandled(); }
    if (_Modes.IsValid() && _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver)
    { _Modes->ResetTime(); }
    else if (NOT _Modes.IsValid() || _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Authored)
    {
        _Session->SetTime(FCk_Time{});
        SyncStagingTimeFromTransport();
    }
    RefreshModePose();
    RebuildPlots();
    if (GIsAutomationTesting)
    { UE_LOG(LogTemp, Display, TEXT("Procedural Action Reset returned: time=%.6f"),
        _Session->GetTime().Get_Seconds()); }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnFrameClicked()
    -> FReply
{
    if (_Closed || NOT _Session.IsValid() || NOT _Modes.IsValid())
    { return FReply::Unhandled(); }
    if (_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver)
    { _Modes->Frame(); }
    else if (_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Authored)
    {
        _Session->SetTime(FCk_Time{_Session->GetTime().Get_Seconds() + 1.0 / 60.0});
        SyncStagingTimeFromTransport();
        RefreshModePose();
    }
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnAuthoredClicked()
    -> FReply
{
    if (_Closed || NOT _Modes.IsValid())
    { return FReply::Unhandled(); }
    _Modes->EnterAuthored();
    RefreshModePose();
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnSolverClicked()
    -> FReply
{
    if (_Closed || NOT _Modes.IsValid())
    { return FReply::Unhandled(); }
    _RequestedPreview = false;
    _Session->SetPlaying(false);
    _SolverRevision = _Session->GetRevision();
    _Modes->StartSolver();
    RefreshModePose();
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnRecordedClicked()
    -> FReply
{
    if (_Closed || NOT _Modes.IsValid())
    { return FReply::Unhandled(); }
    _Session->SetPlaying(false);
    _Modes->EnterRecorded();
    RefreshModePose();
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnRecordedStep(
        int32 InDelta)
    -> FReply
{
    if (_Closed || NOT _Modes.IsValid())
    { return FReply::Unhandled(); }
    _Modes->SelectRecord(_Modes->GetSelectedRecordIndex() + InDelta);
    RefreshModePose();
    return FReply::Handled();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    GetRecordedDetailText() const
    -> FText
{
    if (_Closed || NOT _Modes.IsValid()
        || _Modes->GetMode() != FCkProceduralActionEditorModes::EMode::Recorded)
    { return FText::GetEmpty(); }
    const auto* Recording = _Modes->GetRecorded();
    const auto* Record = _Modes->GetSelectedRecord();
    if (Recording == nullptr)
    { return FText::FromString(TEXT("No copied receipt evidence.")); }
    if (Record == nullptr)
    { return FText::FromString(TEXT("Recording contains no selectable records.")); }
    const auto& Receipt = Record->Get_Receipt();
    auto Detail = FString::Printf(
        TEXT("record %d/%d · stream %lld · kind %d · sample %lld · %.3f–%.3fs · content %lld · tuning %lld · evaluated %d · accepted %d · backlog %.3fs · gaps %d\nsource %s · fixture %s · fixed step %.4fs"),
        _Modes->GetSelectedRecordIndex() + 1, Recording->Get_Records().Num(),
        Record->Get_StreamSequence(), static_cast<int32>(Record->Get_Kind()),
        Receipt.Get_Stamp().Get_SampleSequence(),
        Receipt.Get_Interval().Get_FromTime().Get_Seconds(),
        Receipt.Get_Interval().Get_ToTime().Get_Seconds(),
        Receipt.Get_Stamp().Get_ContentRevision(), Receipt.Get_Stamp().Get_TuningRevision(),
        Receipt.Get_Sampling().Get_EvaluatedSamples(), Receipt.Get_Sampling().Get_AcceptedSamples(),
        Receipt.Get_Sampling().Get_Backlog().Get_Seconds(), Recording->Get_Gaps().Num(),
        *Recording->Get_Provenance().Get_SourceAsset(), *Recording->Get_Provenance().Get_Fixture(),
        Recording->Get_Provenance().Get_FixedStep().Get_Seconds());
    if (_Modes->GetRecordedGeometryInvalidated())
    { Detail = FString{TEXT("SOURCE CHANGED: copied receipt is retained for provenance; geometry is hidden.\n")} + Detail; }
    if (Record->Get_Kind() == ECk_ProceduralAction_RecordKind::Terminal)
    {
        Detail += FString::Printf(TEXT("\nterminal outcome %d · recovery %d · reason %s"),
            static_cast<int32>(Record->Get_Result().Get_Outcome()),
            static_cast<int32>(Record->Get_Result().Get_Recovery()),
            *Record->Get_Result().Get_Reason().ToString());
    }
    if (NOT Recording->Get_Gaps().IsEmpty())
    {
        const auto& Gap = Recording->Get_Gaps()[0];
        Detail += FString::Printf(TEXT("\nmissing stream sequence %lld–%lld (first gap)"), Gap.Get_First(), Gap.Get_Last());
    }
    for (const auto& Event : Receipt.Get_Interval().Get_Events())
    {
        Detail += FString::Printf(TEXT("\nevent %s · kind %d · %.3fs"),
            *Event.Get_Id().ToString(), static_cast<int32>(Event.Get_Kind()),
            Event.Get_Time().Get_Seconds());
    }
    return FText::FromString(Detail);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RefreshModePose()
    -> void
{
    if (_Closed || NOT _Viewport.IsValid())
    { return; }
    auto LiveAction = TOptional<FCk_ProceduralAction_DebugInstance>{};
    auto LiveSubject = TOptional<FCk_ProceduralAction_DebugSubject>{};
    auto ObservedFloorReady = TOptional<bool>{};
    if (_Modes.IsValid() && _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver)
    {
        if (const auto* Solver = _Modes->GetSolver())
        {
            ObservedFloorReady = Solver->GetIsFloorReady();
            LiveAction = UCk_Utils_ProceduralAnimation_Debug_UE::Get_ActionSnapshot(
                Solver->GetAction().ConvertToHandle());
            LiveSubject = UCk_Utils_ProceduralAnimation_Debug_UE::Get_ControlSnapshot(
                Solver->GetSubject().ConvertToHandle());
        }
    }
    _Viewport->SetObservedFloorReady(ObservedFloorReady);
    _Viewport->SetLiveSubjectSnapshot(LiveSubject.IsSet() ? &LiveSubject.GetValue() : nullptr);
    if (_Modes.IsValid() && _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Recorded)
    {
        const auto* Record = _Modes->GetDisplayedRecord();
        _Viewport->SetObservedReceipt(Record != nullptr
            && Record->Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose
            ? &Record->Get_Receipt() : nullptr);
    }
    else if (_Modes.IsValid() && _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver)
    { _Viewport->SetObservedReceipt(nullptr); }
    else
    { _Viewport->UseAuthoredPose(); }
    RefreshObservationText(LiveAction.IsSet() ? &LiveAction.GetValue() : nullptr,
        LiveSubject.IsSet() ? &LiveSubject.GetValue() : nullptr);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RefreshObservationText(
        const FCk_ProceduralAction_DebugInstance* InAction,
        const FCk_ProceduralAction_DebugSubject* InSubject)
    -> void
{
    if (_Closed || NOT _Modes.IsValid() || NOT _Session.IsValid())
    { return; }
    if (_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Authored)
    {
        _ObservationText = LOCTEXT("AuthoredEvidence", "Authored: requested motion only; no accepted-contact evidence.");
        return;
    }
    const auto* Record = _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Recorded
        ? _Modes->GetDisplayedRecord() : nullptr;
    if (_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Recorded
        && _Modes->GetRecordedGeometryInvalidated())
    {
        _ObservationText = LOCTEXT("StaleRecordedGeometry",
            "Recorded receipts are from a previous clip/preview revision. Geometry hidden; restart Solver to record this revision.");
        return;
    }
    const auto HasLivePose = InSubject != nullptr && InSubject->Get_Available()
        && InSubject->Get_HasSettledPose();
    auto Detail = _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver
        ? HasLivePose
            ? FString{TEXT("Solver: live observed subject geometry. Current endpoint marks are not receipt paths.")}
            : FString{TEXT("Solver: waiting for a healthy settled subject pose. No receipt path is shown in this live view.")}
        : FString{TEXT("No accepted pose selected. Select an accepted Recorded receipt.")};
    if (_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver)
    {
        if (const auto* Solver = _Modes->GetSolver())
        {
            Detail += Solver->GetIsFloorReady()
                ? TEXT("\nLive Jolt floor: ready.") : TEXT("\nLive Jolt floor: absent or preparing.");
            Detail += FString::Printf(TEXT("\nSimulation step %lld | action terminal %d"),
                static_cast<long long>(Solver->GetStep()),
                Solver->GetIsActionTerminal());
            if (Solver->GetObservationHorizonStep() > 0)
            {
                Detail += FString::Printf(TEXT(" | observation horizon step %lld"),
                    static_cast<long long>(Solver->GetObservationHorizonStep()));
            }
            if (Solver->GetScenarioNextEventStep() != INDEX_NONE)
            {
                Detail += FString::Printf(TEXT(" | next environment event step %lld"),
                    static_cast<long long>(Solver->GetScenarioNextEventStep()));
            }
        }
    }
    if (Record != nullptr && Record->Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose
        && Record->Get_Receipt().Get_HasAcceptedPose())
    {
        const auto& Receipt = Record->Get_Receipt();
        const auto& Pose = Receipt.Get_CurrentPose();
        Detail = FString::Printf(TEXT("Yellow dashed: requested endpoint chord | Cyan solid: settled endpoint chord (neither is a collision sweep)\nReceipt %lld | phase %s | %s | %d accepted / %d evaluated | backlog %.3fs"),
            static_cast<long long>(Record->Get_StreamSequence()),
            *Receipt.Get_Interval().Get_PhaseId().ToString(),
            *UEnum::GetValueAsString(Receipt.Get_Interval().Get_Continuity()),
            Receipt.Get_Sampling().Get_AcceptedSamples(),
            Receipt.Get_Sampling().Get_EvaluatedSamples(),
            Receipt.Get_Sampling().Get_Backlog().Get_Seconds());
        if (Pose.Get_HasBodyPresentation())
        { Detail += TEXT("\nBody: admitted staged presentation must match settlement tolerance or publication faults; receipt stores the observed presentation."); }
        const auto* Config = _Session->GetPreviewConfig();
        const auto* Solver = _Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver
            ? _Modes->GetSolver() : nullptr;
        for (const auto& Limb : Pose.Get_Limbs())
        {
            Detail += FString::Printf(TEXT("\n%s: endpoint error %.2f cm | chain %s | contact evidence %s"),
                *Limb.Get_Id().ToString(), Limb.Get_EndpointErrorCm(),
                *UEnum::GetValueAsString(Limb.Get_ChainState()),
                *UEnum::GetValueAsString(Limb.Get_ContactEvidence()));
            auto ReachCm = 0.0;
            auto HipWorld = FVector::ZeroVector;
            auto FirstLengthCm = 0.0;
            if (Config != nullptr && Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
            {
                if (const auto* Rig = Config->Get_WalkingRig().Get())
                {
                    if (const auto* Leg = Rig->Get_Legs().FindByPredicate([&Limb](const auto& InLeg)
                        { return InLeg.Get_Id() == Limb.Get_Id(); }))
                    {
                        for (const auto Length : Leg->Get_Chain().Get_SegmentLengths())
                        { ReachCm += Length; }
                        if (NOT Leg->Get_Chain().Get_SegmentLengths().IsEmpty())
                        { FirstLengthCm = Leg->Get_Chain().Get_SegmentLengths()[0]; }
                    }
                }
            }
            else if (Config != nullptr)
            {
                if (const auto* Layout = Config->Get_Layout().Get_Limbs().FindByPredicate([&Limb](const auto& InLayout)
                    { return InLayout.Get_Id() == Limb.Get_Id(); }))
                {
                    for (const auto Length : Layout->Get_SegmentLengths())
                    { ReachCm += Length; }
                    if (NOT Layout->Get_SegmentLengths().IsEmpty())
                    { FirstLengthCm = Layout->Get_SegmentLengths()[0]; }
                }
            }
            if (ReachCm > 0.0 && ck::procedural_action_editor::TryGetObservedHip(
                Limb, FirstLengthCm, HipWorld))
            {
                const auto RequestedDistance = FVector::Distance(HipWorld,
                    Limb.Get_RequestedEndpoint().GetTranslation());
                Detail += FString::Printf(TEXT(" | chain reach %.1f cm | requested distance %.1f cm"),
                    ReachCm, RequestedDistance);
                if (RequestedDistance >= ReachCm - FMath::Max(0.05, ReachCm * 0.02))
                { Detail += TEXT(" | near/full extension (geometric warning)"); }
            }
            if (Solver != nullptr)
            {
                const auto Control = UCk_Utils_ProceduralArticulation_UE::Get_LimbControl(
                    Solver->GetSubject(), Limb.Get_Id());
                Detail += FString::Printf(TEXT("\n  control %s | generation %lld | locomotor %d | participating %d | trusted %d | reason %s"),
                    *UEnum::GetValueAsString(Control.Get_State()),
                    static_cast<long long>(Control.Get_Generation()), Control.Get_Locomotor(),
                    Control.Get_Participating(), Control.Get_TrustedSupport(), *Control.Get_Reason().ToString());
            }
        }
    }
    if (InAction != nullptr && InAction->Get_Available())
    {
        Detail += FString::Printf(TEXT("\nLIVE action %s | accepted %.3fs | backlog %.3fs | content rev %lld | tuning rev %lld | next sample %lld | publication pending %d"),
            *UEnum::GetValueAsString(InAction->Get_State()),
            InAction->Get_AcceptedTime().Get_Seconds(),
            InAction->Get_PlaybackBacklog().Get_Seconds(),
            static_cast<long long>(InAction->Get_ContentRevision()),
            static_cast<long long>(InAction->Get_TuningRevision()),
            static_cast<long long>(InAction->Get_NextSampleSequence()),
            InAction->Get_PublicationPending());
        for (const auto& Deadline : InAction->Get_Watchdogs())
        { Detail += FString::Printf(TEXT("\n  %s: %.3fs remaining / %.3fs limit"),
            *Deadline.Get_Name().ToString(), Deadline.Get_Remaining().Get_Seconds(),
            Deadline.Get_Limit().Get_Seconds()); }
    }
    if (InSubject != nullptr && InSubject->Get_Available())
    {
        Detail += FString::Printf(TEXT("\nLIVE subject settled rev %lld | pose %s | publication pending %d | support %s | contacts %d"),
            static_cast<long long>(InSubject->Get_SettledRevision()),
            InSubject->Get_HasSettledPose() ? TEXT("observed") : TEXT("unavailable"),
            InSubject->Get_PublicationPending(),
            InSubject->Get_SupportFresh() ? TEXT("valid support frame") : TEXT("support frame unavailable"),
            InSubject->Get_Contacts().Num());
        if (_Modes->GetMode() == FCkProceduralActionEditorModes::EMode::Solver
            && InSubject->Get_HasSettledPose())
        {
            const auto& Pose = InSubject->Get_SettledPose();
            Detail += FString::Printf(TEXT("\nLive observed geometry: %d limb(s); endpoint marks are current, with no receipt continuity claim."),
                Pose.Get_Limbs().Num());
            for (const auto& Limb : Pose.Get_Limbs())
            {
                Detail += FString::Printf(TEXT("\n  %s settled %s | error %.2f cm | chain %s"),
                    *Limb.Get_Id().ToString(), *Limb.Get_AcceptedEndpoint().GetLocation().ToString(),
                    Limb.Get_EndpointErrorCm(), *UEnum::GetValueAsString(Limb.Get_ChainState()));
            }
        }
        for (const auto& Contact : InSubject->Get_Contacts())
        { Detail += FString::Printf(TEXT("\n  %s contact trusted %d reserved %d radius %.1f cm"),
            *Contact.Get_LimbId().ToString(), Contact.Get_TrustedSupport(),
            Contact.Get_Reserved(), Contact.Get_ReservationRadiusCm()); }
        for (const auto& Lease : InSubject->Get_Leases())
        {
            Detail += FString::Printf(TEXT("\n  lease %lld support %s | trusted %d | edge %.1f cm | interrupted %d | %s"),
                static_cast<long long>(Lease.Get_Identity()), *Lease.Get_SupportResult().ToString(),
                Lease.Get_TrustedContacts(), Lease.Get_MinimumEdgeDistanceCm(),
                Lease.Get_Interrupted(), *Lease.Get_Reason().ToString());
            for (const auto& Landing : Lease.Get_Landings())
            { Detail += FString::Printf(TEXT("\n    %s proposed landing %s | actual contact trusted %d"),
                *Landing.Get_LimbId().ToString(), *Landing.Get_Position().ToString(),
                Landing.Get_ActualContactTrusted()); }
        }
        for (const auto& Control : InSubject->Get_Controllers())
        {
            Detail += FString::Printf(TEXT("\n  controller %lld return %s | awaiting settlement %d | reason %s"),
                static_cast<long long>(Control.Get_Identity()),
                *UEnum::GetValueAsString(Control.Get_ReturnState()),
                Control.Get_AwaitingSettlement(), *Control.Get_Reason().ToString());
            for (const auto& Deadline : Control.Get_Watchdogs())
            { Detail += FString::Printf(TEXT("\n    %s: %.3fs remaining / %.3fs limit"),
                *Deadline.Get_Name().ToString(), Deadline.Get_Remaining().Get_Seconds(),
                Deadline.Get_Limit().Get_Seconds()); }
        }
    }
    _ObservationText = FText::FromString(MoveTemp(Detail));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnTicker(
        float InDeltaSeconds)
    -> bool
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Ck_ActionEditor_OnTicker);
    if (_Closed)
    { return false; }
    if (_SourceAssetRefreshPending)
    {
        _SourceAssetRefreshPending = false;
        const auto RefreshAppearance = _AppearanceRefreshPending;
        _AppearanceRefreshPending = false;
        RefreshFromEdit();
        if (RefreshAppearance && _Viewport.IsValid())
        { _Viewport->RefreshAppearance(); }
    }
    if (_Session.IsValid() && _Session->IsPlaying())
    {
        _Session->Advance(FCk_Time{FMath::Min(static_cast<double>(InDeltaSeconds), 0.1)});
        if (_Viewport.IsValid())
        { _Viewport->RefreshPose(); }
        RebuildPlots();
    }
    if (_Modes.IsValid() && _Modes->Tick())
    { RefreshModePose(); }
    if (_Session.IsValid() && _BuiltRevision != _Session->GetRevision())
    { RebuildWorkspaceRows(); }
    return true;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SaveAsset_Execute()
    -> void
{
    if (GIsAutomationTesting)
    {
        UE_LOG(LogTemp, Display, TEXT("Procedural Action SaveAsset invoked: clip=%s dirty=%d"),
            _Session.IsValid() && _Session->GetClip() != nullptr
                ? *_Session->GetClip()->GetPathName() : TEXT("None"),
            _Session.IsValid() && _Session->GetClip() != nullptr
                && _Session->GetClip()->GetOutermost()->IsDirty());
    }
    RefreshFromEdit();
    FAssetEditorToolkit::SaveAsset_Execute();
    if (GIsAutomationTesting && _Session.IsValid() && _Session->GetClip() != nullptr)
    {
        UE_LOG(LogTemp, Display, TEXT("Procedural Action SaveAsset returned: dirty=%d"),
            _Session->GetClip()->GetOutermost()->IsDirty());
    }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SaveAssetAs_Execute()
    -> void
{
    RefreshFromEdit();
    FAssetEditorToolkit::SaveAssetAs_Execute();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    PostUndo(
        bool)
    -> void
{
    _SourceAssetRefreshPending = true;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    PostRedo(
        bool)
    -> void
{
    _SourceAssetRefreshPending = true;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnClose()
    -> void
{
    if (_Closed)
    { return; }
    _Closed = true;
    if (_Modes.IsValid())
    { _Modes->Close(); }
    if (_TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_TickerHandle);
        _TickerHandle.Reset();
    }
    if (_UndoRegistered && GEditor != nullptr)
    {
        GEditor->UnregisterForUndo(this);
        _UndoRegistered = false;
    }
    if (_SourceAssetPropertyHandle.IsValid())
    {
        FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(_SourceAssetPropertyHandle);
        _SourceAssetPropertyHandle.Reset();
    }
    _PlotContainer.Reset();
    _SubjectRows.Reset();
    _PoseRows.Reset();
    _InspectorRows.Reset();
    _TimingRows.Reset();
    _IssueRows.Reset();
    _PoseLabel.Reset();
    _ProgressPlot.Reset();
    _Advanced.Reset();
    if (_Viewport.IsValid())
    { _Viewport->Shutdown(); }
    _Viewport.Reset();
    _CurveAdapter.Reset();
    _Timeline.Reset();
    _Plots.Reset();
    _PreviewDetails.Reset();
    _ClipDetails.Reset();
    _Session.Reset();
    _Modes.Reset();
}

#undef LOCTEXT_NAMESPACE
