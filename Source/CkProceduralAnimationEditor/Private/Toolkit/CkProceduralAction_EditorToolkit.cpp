#include "CkProceduralAnimationEditor/CkProceduralAction_EditorToolkit.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModes.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"
#include "CkProceduralAnimationEditor/CkProceduralActionSelectionProxy.h"
#include "CkProceduralAnimationEditor/CkProceduralAction_CurveAdapter.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "Widgets/SCkProceduralAction_Channels.h"
#include "Widgets/SCkProceduralAction_Issues.h"
#include "Widgets/SCkProceduralAction_Timeline.h"
#include "Widgets/SCkProceduralAction_Transport.h"
#include "Widgets/SCkProceduralAction_Viewport.h"

#include "Editor.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionEditorToolkit"

class FCk_ProceduralActionEditorCommands final : public TCommands<FCk_ProceduralActionEditorCommands>
{
public:
    FCk_ProceduralActionEditorCommands()
        : TCommands(TEXT("CkProceduralActionEditor"), LOCTEXT("Context", "Procedural Action Editor"), NAME_None, FAppStyle::GetAppStyleSetName())
    {}

    auto RegisterCommands() -> void override
    {
        UI_COMMAND(Record, "Record pose", "Key the channels Record covers at the playhead.", EUserInterfaceActionType::Button, FInputChord{EKeys::S});
        UI_COMMAND(TogglePlay, "Play", "Play or pause.", EUserInterfaceActionType::Button, FInputChord{EKeys::SpaceBar});
        UI_COMMAND(GoToStart, "Start", "Move the playhead to the start.", EUserInterfaceActionType::Button, FInputChord{EKeys::Home});
        UI_COMMAND(GoToEnd, "End", "Move the playhead to the end.", EUserInterfaceActionType::Button, FInputChord{EKeys::End});
        UI_COMMAND(Delete, "Delete", "Delete the selected section, key, pose or event.", EUserInterfaceActionType::Button, FInputChord{EKeys::Delete});
        UI_COMMAND(AddSection, "Section", "Add a section at the end of the clip.", EUserInterfaceActionType::Button, FInputChord{});
        UI_COMMAND(AddSlot, "Slot", "Add a limb slot to the clip.", EUserInterfaceActionType::Button, FInputChord{});
    }

    TSharedPtr<FUICommandInfo> Record;
    TSharedPtr<FUICommandInfo> TogglePlay;
    TSharedPtr<FUICommandInfo> GoToStart;
    TSharedPtr<FUICommandInfo> GoToEnd;
    TSharedPtr<FUICommandInfo> Delete;
    TSharedPtr<FUICommandInfo> AddSection;
    TSharedPtr<FUICommandInfo> AddSlot;
};

const FName FCk_ProceduralAction_EditorToolkit::ToolkitName(TEXT("CkProceduralActionEditor"));
const FName FCk_ProceduralAction_EditorToolkit::ViewportTabId(TEXT("CkProceduralAction_Viewport"));
const FName FCk_ProceduralAction_EditorToolkit::ChannelsTabId(TEXT("CkProceduralAction_Channels"));
const FName FCk_ProceduralAction_EditorToolkit::DetailsTabId(TEXT("CkProceduralAction_Details"));
const FName FCk_ProceduralAction_EditorToolkit::TimelineTabId(TEXT("CkProceduralAction_Timeline"));
const FName FCk_ProceduralAction_EditorToolkit::CurvesTabId(TEXT("CkProceduralAction_Curves"));

FCk_ProceduralAction_EditorToolkit::FCk_ProceduralAction_EditorToolkit() = default;

FCk_ProceduralAction_EditorToolkit::~FCk_ProceduralAction_EditorToolkit()
{ OnClose(); }

auto
    FCk_ProceduralAction_EditorToolkit::
    Init(
        EToolkitMode::Type InMode,
        const TSharedPtr<IToolkitHost>& InHost,
        UCk_ProceduralAction_Data* InClip)
    -> void
{
    if (NOT FCk_ProceduralActionEditorCommands::IsRegistered())
    { FCk_ProceduralActionEditorCommands::Register(); }
    _Session = MakeShared<FCk_ProceduralActionAuthoringSession>(InClip);
    _Model = MakeShared<FCk_ProceduralActionEditorModel>();
    _Modes = MakeShared<FCk_ProceduralActionEditorModes>(_Session);
    _SelectionProxy.Reset(NewObject<UCk_ProceduralActionSelectionProxy>(GetTransientPackage(), NAME_None, RF_Transient));
    _SelectionProxy->OnEdited = [this](FName InProperty) { OnProxyEdited(InProperty); };

    _Session->UseDerivedPreview();
    _ShownSubject = ck::procedural_action_editor::Get_Subject(*InClip);
    auto Recorded = TArray<FName>{};
    if (InClip->Get_BodyChannel().Get_Enabled())
    { Recorded.Add(ck::procedural_action_editor::Get_BodyChannelName()); }
    for (const auto& Slot : InClip->Get_Slots())
    { Recorded.Add(Slot.Get_Id()); }
    _Model->SetRecordChannels(Recorded);
    if (NOT InClip->Get_Phases().IsEmpty())
    { _Model->SelectSection(InClip->Get_Phases()[0].Get_Id()); }

    auto& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    auto DetailArgs = FDetailsViewArgs{};
    DetailArgs.bAllowSearch = false;
    DetailArgs.bHideSelectionTip = true;
    DetailArgs.bUpdatesFromSelection = false;
    DetailArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    _Details = PropertyEditor.CreateDetailView(DetailArgs);
    _Details->SetTag(FName{TEXT("CkActionEditor.Details")});
    const auto* Proxy = _SelectionProxy.Get();
    _Details->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([Proxy](const FPropertyAndParent& InProperty) -> bool
    {
        const auto* Top = &InProperty.Property;
        for (const auto* Parent : InProperty.ParentProperties)
        { Top = Parent; }
        return Proxy->Get_IsCategoryShown(FName{*Top->GetMetaData(TEXT("Category"))});
    }));
    _Curves = MakeShared<FCk_ProceduralAction_CurveAdapter>();
    _CurvesPanel = _Curves->Init(_Session, _Model);

    BindCommands();
    const auto Extender = MakeShared<FExtender>();
    Extender->AddToolBarExtension("Asset", EExtensionHook::After, GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateSP(this, &FCk_ProceduralAction_EditorToolkit::FillToolbar));
    AddToolbarExtender(Extender);

    const auto Layout = FTabManager::NewLayout("CkProceduralAction_v2")
        ->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
            ->Split(FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.65f)
                ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.2f)->AddTab(ChannelsTabId, ETabState::OpenedTab))
                ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.5f)->AddTab(ViewportTabId, ETabState::OpenedTab)->SetHideTabWell(true))
                ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.3f)->AddTab(DetailsTabId, ETabState::OpenedTab)))
            ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.35f)
                ->AddTab(TimelineTabId, ETabState::OpenedTab)->AddTab(CurvesTabId, ETabState::OpenedTab)->SetForegroundTab(TimelineTabId)));
    InitAssetEditor(InMode, InHost, ToolkitName, Layout, true, true, InClip);
    RegenerateMenusAndToolbars();

    _ModelChangedHandle = _Model->OnChanged.AddRaw(this, &FCk_ProceduralAction_EditorToolkit::Refresh);
    _DetailsRefreshPending = true;
    if (GEditor != nullptr)
    {
        GEditor->RegisterForUndo(this);
        _UndoRegistered = true;
    }
    _TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCk_ProceduralAction_EditorToolkit::OnTicker));
    _PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FCk_ProceduralAction_EditorToolkit::OnObjectPropertyChanged);
}

auto FCk_ProceduralAction_EditorToolkit::GetToolkitFName() const -> FName { return ToolkitName; }
auto FCk_ProceduralAction_EditorToolkit::GetBaseToolkitName() const -> FText { return LOCTEXT("ToolkitName", "Procedural Action"); }
auto FCk_ProceduralAction_EditorToolkit::GetWorldCentricTabPrefix() const -> FString { return TEXT("Procedural Action "); }
auto FCk_ProceduralAction_EditorToolkit::GetWorldCentricTabColorScale() const -> FLinearColor { return FLinearColor{0.12f, 0.67f, 0.78f}; }
auto FCk_ProceduralAction_EditorToolkit::Get_SelectionProxy() const -> UCk_ProceduralActionSelectionProxy* { return _SelectionProxy.Get(); }

auto
    FCk_ProceduralAction_EditorToolkit::
    RegisterTabSpawners(
        const TSharedRef<FTabManager>& InTabManager)
    -> void
{
    WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("Workspace", "Procedural Action"));
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
    const auto Register = [this, &InTabManager](FName InTabId, const FText& InLabel)
    {
        InTabManager->RegisterTabSpawner(InTabId, FOnSpawnTab::CreateSP(this, &FCk_ProceduralAction_EditorToolkit::SpawnTab, InTabId))
            .SetDisplayName(InLabel).SetGroup(WorkspaceMenuCategory.ToSharedRef());
    };
    Register(ViewportTabId, LOCTEXT("ViewportTab", "Viewport"));
    Register(ChannelsTabId, LOCTEXT("ChannelsTab", "Channels"));
    Register(DetailsTabId, LOCTEXT("DetailsTab", "Details"));
    Register(TimelineTabId, LOCTEXT("TimelineTab", "Timeline"));
    Register(CurvesTabId, LOCTEXT("CurvesTab", "Curves"));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    UnregisterTabSpawners(
        const TSharedRef<FTabManager>& InTabManager)
    -> void
{
    for (const auto TabId : {ViewportTabId, ChannelsTabId, DetailsTabId, TimelineTabId, CurvesTabId})
    { InTabManager->UnregisterTabSpawner(TabId); }
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    SpawnTab(
        const FSpawnTabArgs&,
        FName InTabId)
    -> TSharedRef<SDockTab>
{
    const auto OnResult = [this](bool InSucceeded, const FString& InText) { Finish(InSucceeded, InText, InText); };
    auto Content = TSharedPtr<SWidget>{};
    if (InTabId == ViewportTabId)
    {
        Content = SAssignNew(_Viewport, SCkProceduralAction_Viewport).Session(_Session).Model(_Model).Modes(_Modes)
            .Hud_Lambda([this]() { return Get_HudText(); });
    }
    else if (InTabId == ChannelsTabId)
    { Content = SAssignNew(_Channels, SCkProceduralAction_Channels).Session(_Session).Model(_Model).OnResult(OnResult); }
    else if (InTabId == DetailsTabId)
    {
        Content = SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(0.7f)[_Details.ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight().Padding(4.0f)[SNew(STextBlock).Text(LOCTEXT("Issues", "Issues"))]
            + SVerticalBox::Slot().FillHeight(0.3f)[SAssignNew(_Issues, SCkProceduralAction_Issues).Session(_Session).Model(_Model)];
    }
    else if (InTabId == TimelineTabId)
    {
        Content = SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SAssignNew(_Timeline, SCkProceduralAction_Timeline).Session(_Session).Model(_Model).OnResult(OnResult)
                .OnAddEvent([this](bool InWindow) { InWindow ? AddWindowAtPlayhead() : AddMarkerAtPlayhead(); })
                .OnDelete([this]() { DeleteSelection(); })
                .Length([this]() { return Get_PlaybackLength(); })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(2.0f)
            [
                SNew(SCkProceduralAction_Transport).Session(_Session).Model(_Model)
                .OnTogglePlay([this]() { TogglePlay(); })
                .OnStep([this](int32 InFrames) { StepFrames(InFrames); })
                .Length([this]() { return Get_PlaybackLength(); })
            ];
    }
    else
    { Content = _CurvesPanel; }
    return SNew(SDockTab).TabRole(ETabRole::PanelTab)[Content.ToSharedRef()];
}

auto
    FCk_ProceduralAction_EditorToolkit::
    BindCommands()
    -> void
{
    const auto& Commands = FCk_ProceduralActionEditorCommands::Get();
    const auto& List = GetToolkitCommands();
    List->MapAction(Commands.Record, FExecuteAction::CreateLambda([this]() { RecordPose(); }));
    List->MapAction(Commands.TogglePlay, FExecuteAction::CreateSP(this, &FCk_ProceduralAction_EditorToolkit::TogglePlay));
    List->MapAction(Commands.GoToStart, FExecuteAction::CreateLambda([this]() { _Model->SetTime(FCk_Time{}); }));
    List->MapAction(Commands.GoToEnd, FExecuteAction::CreateLambda([this]() { _Model->SetTime(_Session->Get_Duration()); }));
    List->MapAction(Commands.Delete, FExecuteAction::CreateLambda([this]() { DeleteSelection(); }));
    List->MapAction(Commands.AddSection, FExecuteAction::CreateLambda([this]() { AddSection(); }));
    List->MapAction(Commands.AddSlot, FExecuteAction::CreateLambda([this]() { AddSlot(); }));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    FillToolbar(
        FToolBarBuilder& InBuilder)
    -> void
{
    const auto& Commands = FCk_ProceduralActionEditorCommands::Get();
    using EMode = ECk_ProceduralActionEditor_PreviewMode;
    InBuilder.BeginSection("CkProceduralAction");
    InBuilder.AddComboButton(FUIAction{}, FOnGetContent::CreateSP(this, &FCk_ProceduralAction_EditorToolkit::MakeSubjectMenu),
        LOCTEXT("Subject", "Subject"), LOCTEXT("SubjectTip", "The creature the clip previews on, and an optional authored scenario."));
    InBuilder.AddToolBarButton(Commands.Record, NAME_None, TAttribute<FText>{}, TAttribute<FText>{},
        FSlateIcon{FCkIconStyle::GetStyleSetName(), FCkIconStyle::Get_StyleKey(ECk_Icon::Recording, ECk_Icon_BrushSize::Size_24x24)});
    InBuilder.AddWidget(SNew(SBox).VAlign(VAlign_Center).Padding(FMargin{4.0f, 0.0f})
    [
        SNew(SSegmentedControl<EMode>)
        .Tag(FName{TEXT("CkActionEditor.Mode")})
        .Value_Lambda([this]() { return _Model->Get_PreviewMode(); })
        .OnValueChanged_Lambda([this](EMode InMode) { _Model->SetPreviewMode(InMode); })
        + SSegmentedControl<EMode>::Slot(EMode::Solved).Text(LOCTEXT("Solved", "Solved"))
        + SSegmentedControl<EMode>::Slot(EMode::Authored).Text(LOCTEXT("Authored", "Authored"))
    ]);
    const auto AddIcon = FSlateIcon{FAppStyle::GetAppStyleSetName(), "Icons.Plus"};
    InBuilder.AddToolBarButton(Commands.AddSection, NAME_None, TAttribute<FText>{}, TAttribute<FText>{}, AddIcon);
    InBuilder.AddToolBarButton(Commands.AddSlot, NAME_None, TAttribute<FText>{}, TAttribute<FText>{}, AddIcon);
    InBuilder.AddComboButton(FUIAction{}, FOnGetContent::CreateSP(this, &FCk_ProceduralAction_EditorToolkit::MakeStartingShapeMenu),
        LOCTEXT("StartingShape", "Apply starting shape"), LOCTEXT("StartingShapeTip", "Fill an empty clip with a starting motion."));
    InBuilder.AddWidget(SNew(SBox).VAlign(VAlign_Center).Padding(FMargin{8.0f, 0.0f})
    [
        SNew(STextBlock)
        .Tag(FName{TEXT("CkActionEditor.IssueCount")})
        .Text_Lambda([this]() { return FText::Format(LOCTEXT("IssueCount", "Issues: {0}"), _Session.IsValid() ? _Session->Get_Diagnostics().Num() : 0); })
    ]);
    InBuilder.EndSection();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    MakeSubjectMenu()
    -> TSharedRef<SWidget>
{
    return SNew(SBox).Padding(8.0f).WidthOverride(320.0f)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("SubjectHint",
            "The clip's subject (a rig and gait, or a layout) is under Clip in Details. A scenario config replaces the derived preview for obstacles, floor removal or tuning."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            SNew(SObjectPropertyEntryBox)
            .AllowedClass(UCk_ProceduralActionPreview_Config::StaticClass())
            .ObjectPath_Lambda([this]() { return _Session.IsValid() && _Session->Get_Clip() != nullptr
                ? ck::procedural_action_editor::Get_Subject(*_Session->Get_Clip()).Get_Scenario().ToString() : FString{}; })
            .OnObjectChanged_Lambda([this](const FAssetData& InAsset) { SetScenario(Cast<UCk_ProceduralActionPreview_Config>(InAsset.GetAsset())); })
        ]
    ];
}

auto
    FCk_ProceduralAction_EditorToolkit::
    MakeStartingShapeMenu()
    -> TSharedRef<SWidget>
{
    auto Menu = FMenuBuilder{true, nullptr};
    const auto* Enum = StaticEnum<ECk_ProceduralAction_Recipe>();
    for (auto Index = 0; Index < Enum->NumEnums() - 1; ++Index)
    {
        const auto Recipe = static_cast<ECk_ProceduralAction_Recipe>(Enum->GetValueByIndex(Index));
        Menu.AddMenuEntry(Enum->GetDisplayNameTextByIndex(Index), FText::GetEmpty(), FSlateIcon{},
            FUIAction{FExecuteAction::CreateLambda([this, Recipe]() { ApplyStartingShape(Recipe); })});
    }
    return Menu.MakeWidget();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    Finish(
        bool InSucceeded,
        const FString& InSuccess,
        const FString& InError)
    -> bool
{
    _Message = InSucceeded ? InSuccess : InError;
    return InSucceeded;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    Get_IsSolved() const
    -> bool
{ return _Model.IsValid() && _Model->Get_PreviewMode() == ECk_ProceduralActionEditor_PreviewMode::Solved; }

auto
    FCk_ProceduralAction_EditorToolkit::
    Get_PlaybackLength() const
    -> double
{
    if (_Closed || NOT _Session.IsValid())
    { return 0.0; }
    return (Get_IsSolved() && _Modes.IsValid() ? _Modes->Get_Duration() : _Session->Get_Duration()).Get_Seconds();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    Get_StatusText() const
    -> FText
{
    if (_Closed || NOT _Modes.IsValid() || NOT _Session.IsValid())
    { return FText::GetEmpty(); }
    return _Modes->Get_Mode() == FCk_ProceduralActionEditorModes::EMode::Solver ? _Modes->Get_StatusText()
        : _Session->Get_StatusText(_Model->Get_Time());
}

auto
    FCk_ProceduralAction_EditorToolkit::
    Get_HudText() const
    -> FText
{
    if (_Closed || NOT _Model.IsValid())
    { return FText::GetEmpty(); }
    auto Key = FCk_ProceduralActionKeyTime{};
    const auto Section = _Model->Get_Time() > _Session->Get_Duration() ? FString{TEXT("after the clip")}
        : _Session->ElapsedToKey(_Model->Get_Time(), Key) ? Key.Get_PhaseId().ToString() : FString{};
    return FText::FromString(FString::Printf(TEXT("%s | %.2f s | %s\n%s%s%s"), Get_IsSolved() ? TEXT("SOLVED") : TEXT("AUTHORED"),
        _Model->Get_Time().Get_Seconds(), *Section, *Get_StatusText().ToString(), _Message.IsEmpty() ? TEXT("") : TEXT("\n"), *_Message));
}

auto
    FCk_ProceduralAction_EditorToolkit::
    Refresh()
    -> void
{
    if (_Closed || NOT _Session.IsValid() || _Session->Get_Clip() == nullptr)
    { return; }
    const auto* Clip = _Session->Get_Clip();
    const auto Phase = _Model->Get_SelectedPhase();
    const auto PhaseIndex = Clip->Get_Phases().IndexOfByPredicate([Phase](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == Phase; });
    if (Phase.IsNone() && NOT Clip->Get_Phases().IsEmpty())
    {
        _Model->SelectSection(Clip->Get_Phases()[0].Get_Id());
        return;
    }
    // A selected section that vanished was renamed, by an edit or its undo: select what now sits where it was.
    if (PhaseIndex == INDEX_NONE && NOT Clip->Get_Phases().IsEmpty())
    {
        const auto Index = FMath::Clamp(_SelectedPhaseIndex, 0, Clip->Get_Phases().Num() - 1);
        _Model->RenameSection(Phase, Clip->Get_Phases()[Index].Get_Id());
        return;
    }
    _SelectedPhaseIndex = PhaseIndex;
    const auto ContentChanged = _BuiltContentRevision != _Model->Get_ContentRevision();
    const auto SelectionChanged = _BuiltSelectionRevision != _Model->Get_SelectionRevision();
    _BuiltContentRevision = _Model->Get_ContentRevision();
    _BuiltSelectionRevision = _Model->Get_SelectionRevision();
    if (ContentChanged)
    {
        _Session->Refresh();
        _Modes->SourceChanged();
        if (_Channels.IsValid())
        { _Channels->Refresh(); }
        if (_Timeline.IsValid())
        { _Timeline->Refresh(); }
        if (_Issues.IsValid())
        { _Issues->Refresh(); }
    }
    const auto& Selection = _Model->Get_Selection();
    auto KeyElapsed = FCk_Time{};
    if (SelectionChanged && Selection.Get_Kind() == ECk_ProceduralActionEditor_SelectionKind::Key
        && _Session->KeyToElapsed(Selection.Get_PhaseId(), Selection.Get_KeyTime(), KeyElapsed)
        && NOT FMath::IsNearlyEqual(KeyElapsed.Get_Seconds(), _Model->Get_Time().Get_Seconds(), 1.0e-6))
    { _Model->SetTime(KeyElapsed); }
    if (ContentChanged || SelectionChanged)
    {
        _Curves->Refresh();
        if (_Channels.IsValid())
        { _Channels->SyncSelection(); }
        _DetailsRefreshPending = true;
    }
    ApplyRequestedTime();
}

auto
    FCk_ProceduralAction_EditorToolkit::
    ApplyRequestedTime()
    -> void
{
    if (_AppliedTimeRequest == _Model->Get_TimeRequestRevision())
    { return; }
    _AppliedTimeRequest = _Model->Get_TimeRequestRevision();
    if (Get_IsSolved())
    { _Modes->Show(_Model->Get_Time()); }
}

auto
    FCk_ProceduralAction_EditorToolkit::
    RefreshDetails()
    -> void
{
    _DetailsRefreshPending = false;
    _SelectionProxy->Load(*_Session, *_Model);
    _Details->SetObject(_SelectionProxy.Get(), true);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    TickPlayback(
        float InDeltaSeconds)
    -> void
{
    const auto Playing = _Model->Get_IsPlaying();
    const auto Step = FCk_Time{FMath::Min(static_cast<double>(InDeltaSeconds), 0.1)};
    if (Get_IsSolved())
    {
        if (_Modes->Get_Mode() != FCk_ProceduralActionEditorModes::EMode::Solver)
        { RestartSolver(); }
        if (Playing)
        {
            // The playhead may lead the solved frames by one step, so the Solver keeps simulating toward it.
            const auto Next = (_Model->Get_Time() + Step).Get_Seconds();
            const auto SolvedEnd = _Modes->Get_SolvedTime().Get_Seconds();
            auto Target = FMath::Min(Next, SolvedEnd + 1.0 / FCk_ProceduralActionAuthoringSession::FramesPerSecond);
            if (_Modes->Get_IsComplete() && Next >= SolvedEnd)
            {
                Target = _Model->Get_IsLooping() ? 0.0 : SolvedEnd;
                if (NOT _Model->Get_IsLooping())
                { _Model->SetPlaying(false); }
            }
            _Model->FollowSourceTime(FCk_Time{Target});
            _Modes->Show(FCk_Time{Target});
        }
        _Modes->Tick();
        return;
    }
    if (_Modes->Get_Mode() != FCk_ProceduralActionEditorModes::EMode::Authored)
    { _Modes->EnterAuthored(); }
    if (NOT Playing)
    { return; }
    if (NOT _Session->Get_CanPlay())
    {
        _Model->SetPlaying(false);
        Finish(false, {}, _Session->Get_StatusText(_Model->Get_Time()).ToString());
        return;
    }
    const auto Duration = _Session->Get_Duration();
    auto Next = _Model->Get_Time() + Step;
    if (Next >= Duration)
    {
        Next = _Model->Get_IsLooping() ? FCk_Time{} : Duration;
        if (NOT _Model->Get_IsLooping())
        { _Model->SetPlaying(false); }
    }
    _Model->FollowSourceTime(Next);
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnTicker(
        float InDeltaSeconds)
    -> bool
{
    if (_Closed)
    { return false; }
    if (_SourceRefreshPending)
    {
        _SourceRefreshPending = false;
        const auto Subject = ck::procedural_action_editor::Get_Subject(*_Session->Get_Clip());
        _AppearanceRefreshPending |= NOT FCk_ProceduralActionEditor_Subject::StaticStruct()->CompareScriptStruct(
            &_ShownSubject, &Subject, PPF_None);
        if (_AppearanceRefreshPending)
        {
            _ShownSubject = Subject;
            _AppearanceRefreshPending = false;
            _Session->UseDerivedPreview();
            if (_Viewport.IsValid())
            { _Viewport->RefreshAppearance(); }
        }
        _Model->NotifyContentChanged();
    }
    TickPlayback(InDeltaSeconds);
    if (_DetailsRefreshPending)
    { RefreshDetails(); }
    return true;
}

auto
    FCk_ProceduralAction_EditorToolkit::
    OnObjectPropertyChanged(
        UObject* InObject,
        FPropertyChangedEvent&)
    -> void
{
    const auto* Clip = _Closed || NOT _Session.IsValid() ? nullptr : _Session->Get_Clip();
    if (Clip == nullptr || InObject == nullptr || InObject == _SelectionProxy.Get())
    { return; }
    // The clip's editor data is its subobject; a Details edit there changes the preview subject.
    if (InObject == Clip || InObject->GetOuter() == Clip)
    {
        _SourceRefreshPending = true;
        _AppearanceRefreshPending |= InObject != Clip;
        return;
    }
    const auto& Subject = ck::procedural_action_editor::Get_Subject(*Clip);
    if (InObject == Subject.Get_Scenario().Get() || InObject == Subject.Get_WalkingRig().Get() || InObject == Subject.Get_WalkingGait().Get())
    {
        _SourceRefreshPending = true;
        _AppearanceRefreshPending = true;
    }
}

auto FCk_ProceduralAction_EditorToolkit::PostUndo(bool) -> void { _SourceRefreshPending = true; }
auto FCk_ProceduralAction_EditorToolkit::PostRedo(bool) -> void { _SourceRefreshPending = true; }

auto
    FCk_ProceduralAction_EditorToolkit::
    OnClose()
    -> void
{
    if (_Closed)
    { return; }
    _Closed = true;
    if (_Model.IsValid())
    { _Model->OnChanged.Remove(_ModelChangedHandle); }
    if (_Modes.IsValid())
    { _Modes->Close(); }
    FTSTicker::GetCoreTicker().RemoveTicker(_TickerHandle);
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(_PropertyChangedHandle);
    if (_UndoRegistered && GEditor != nullptr)
    { GEditor->UnregisterForUndo(this); }
    if (_Viewport.IsValid())
    { _Viewport->Shutdown(); }
    if (_SelectionProxy.IsValid())
    { _SelectionProxy->OnEdited = nullptr; }
    _Viewport.Reset();
    _Channels.Reset();
    _Timeline.Reset();
    _Issues.Reset();
    _Details.Reset();
    _CurvesPanel.Reset();
    _Curves.Reset();
    _SelectionProxy.Reset();
    _Session.Reset();
    _Modes.Reset();
    _Model.Reset();
}

#undef LOCTEXT_NAMESPACE
