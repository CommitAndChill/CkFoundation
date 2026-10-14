#pragma once

#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"

#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Containers/Ticker.h"

class FCk_ProceduralActionAuthoringSession;
class FCkProceduralAction_CurveAdapter;
class FCkProceduralActionEditorModes;
class IDetailsView;
class SCkProceduralAction_AuthoredPlot;
class SCkProceduralAction_AuthoredViewport;
class SCkProceduralAction_ProgressPlot;
class SCkProceduralAction_Timeline;
class SDockTab;
class SEditableTextBox;
class SExpandableArea;
class SWidget;
class SVerticalBox;
class UCk_ProceduralAction_Data;
struct FAssetData;
class FSpawnTabArgs;
struct FPropertyChangedEvent;
struct FCk_ProceduralAction_DebugInstance;
struct FCk_ProceduralAction_DebugSubject;

// The pose-first workspace: subject, viewport and selected pose on top, grouped pose rows, phase timing and issues
// below; the raw Details, recorded evidence and the one-by-one track tools live in the collapsed Advanced area.
// Rows are rebuilt when the session revision or the selection changes, never per frame.
class FCk_ProceduralAction_EditorToolkit final : public FAssetEditorToolkit, public FEditorUndoClient
{
public:
    FCk_ProceduralAction_EditorToolkit();
    ~FCk_ProceduralAction_EditorToolkit() override;

    auto
        Init(
            EToolkitMode::Type InMode,
            const TSharedPtr<IToolkitHost>& InHost,
            UCk_ProceduralAction_Data* InClip)
        -> void;

    auto
        GetToolkitFName() const
        -> FName override;
    auto
        GetBaseToolkitName() const
        -> FText override;
    auto
        GetWorldCentricTabPrefix() const
        -> FString override;
    auto
        GetWorldCentricTabColorScale() const
        -> FLinearColor override;
    auto
        RegisterTabSpawners(
            const TSharedRef<FTabManager>& InTabManager)
        -> void override;
    auto
        UnregisterTabSpawners(
            const TSharedRef<FTabManager>& InTabManager)
        -> void override;
    auto
        SaveAsset_Execute()
        -> void override;
    auto
        SaveAssetAs_Execute()
        -> void override;
    auto
        OnClose()
        -> void override;
    auto
        PostUndo(
            bool InSuccess)
        -> void override;
    auto
        PostRedo(
            bool InSuccess)
        -> void override;

private:
    auto
        SpawnEditorTab(
            const FSpawnTabArgs& InArgs)
        -> TSharedRef<SDockTab>;
    auto
        BuildEditorContent()
        -> TSharedRef<SWidget>;
    auto
        BuildHeader()
        -> TSharedRef<SWidget>;
    auto
        BuildTransport()
        -> TSharedRef<SWidget>;
    auto
        BuildSubjectPanel()
        -> TSharedRef<SWidget>;
    auto
        BuildViewportPanel()
        -> TSharedRef<SWidget>;
    auto
        BuildInspectorPanel()
        -> TSharedRef<SWidget>;
    auto
        BuildPoseActions()
        -> TSharedRef<SWidget>;
    auto
        BuildTimingPanel()
        -> TSharedRef<SWidget>;
    auto
        BuildIssuePanel()
        -> TSharedRef<SWidget>;
    auto
        BuildAdvanced(
            const TSharedRef<SWidget>& InCurvePanel)
        -> TSharedRef<SWidget>;
    auto
        RebuildPlots(
            bool InRefreshSamples = false)
        -> void;
    auto
        RebuildWorkspaceRows()
        -> void;
    auto
        RebuildSubjectRows()
        -> void;
    auto
        RebuildPoseRows()
        -> void;
    auto
        RebuildInspectorRows()
        -> void;
    auto
        RebuildTimingRows()
        -> void;
    auto
        RebuildIssueRows()
        -> void;
    auto
        RefreshFromEdit()
        -> void;
    auto
        RefreshFromTimeline()
        -> void;
    auto
        RefreshFromCurve()
        -> void;
    auto
        RefreshFromWorkspace()
        -> void;
    auto
        RefreshCurveSelection()
        -> void;
    auto
        OnSourceAssetChanged(
            UObject* InObject,
            FPropertyChangedEvent& InEvent)
        -> void;
    auto
        OnPreviewPicked(
            const FAssetData& InAsset)
        -> void;
    auto
        OnCreatePreviewClicked()
        -> FReply;
    auto
        OnSavePairClicked()
        -> FReply;
    auto
        OnTicker(
            float InDeltaSeconds)
        -> bool;
    auto
        GetPreviewPath() const
        -> FString;
    auto
        OnPlayClicked()
        -> FReply;
    auto
        GetPlayLabel() const
        -> FText;
    auto
        GetCanPressPlay() const
        -> bool;
    auto
        GetPlayBlockedReason() const
        -> FString;
    auto
        OnRequestedPreviewChanged(
            bool InRequested)
        -> void;
    auto
        OnResetClicked()
        -> FReply;
    auto
        OnFrameClicked()
        -> FReply;
    auto
        OnAuthoredClicked()
        -> FReply;
    auto
        OnSolverClicked()
        -> FReply;
    auto
        OnRecordedClicked()
        -> FReply;
    auto
        OnRecordedStep(
            int32 InDelta)
        -> FReply;
    auto
        GetRecordedDetailText() const
        -> FText;
    auto
        RefreshModePose()
        -> void;
    auto
        RefreshObservationText(
            const FCk_ProceduralAction_DebugInstance* InAction,
            const FCk_ProceduralAction_DebugSubject* InSubject)
        -> void;

    auto
        LocateTime(
            FCk_Time InTime,
            FName& OutPhaseId,
            double& OutNormalizedTime) const
        -> bool;
    auto
        SyncStagingTimeFromTransport()
        -> void;
    auto
        SelectPose(
            const FGuid& InPoseId)
        -> void;
    auto
        SelectPhase(
            FName InPhaseId)
        -> void;
    auto
        SetParticipant(
            FName InChannel,
            bool InParticipates)
        -> void;
    auto
        GetStagedValue(
            FName InChannel,
            int32 InAxis) const
        -> double;
    auto
        SetStagedValue(
            FName InChannel,
            int32 InAxis,
            double InValue)
        -> void;
    auto
        GetNewPoseLabel() const
        -> FName;
    auto
        FinishPoseCommand(
            bool InSucceeded,
            const FString& InSuccessMessage,
            const FString& InError)
        -> void;
    auto
        OnCapturePose()
        -> FReply;
    auto
        OnUpdatePose()
        -> FReply;
    auto
        OnDuplicatePose()
        -> FReply;
    auto
        OnMovePose()
        -> FReply;
    auto
        OnDeletePose()
        -> FReply;
    auto
        OnCaptureSolvedPose()
        -> FReply;
    auto
        OnMirrorToSelectedLimb()
        -> FReply;
    auto
        OnDurationCommitted(
            double InSeconds)
        -> void;
    auto
        ApplyPhaseTiming(
            ECk_ProceduralAction_Easing InEasing,
            const FCk_ProceduralAction_ProgressCurve& InProgress)
        -> void;
    auto
        OnProgressKeyCommitted(
            int32 InIndex,
            bool InTimeAxis,
            double InValue)
        -> void;
    auto
        OnIssueClicked(
            FName InPhaseId,
            FName InSlotId)
        -> FReply;

private:
    static const FName EditorTabId;
    TSharedPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TUniquePtr<FCkProceduralActionEditorModes> _Modes;
    TSharedPtr<IDetailsView> _ClipDetails;
    TSharedPtr<IDetailsView> _PreviewDetails;
    TSharedPtr<SVerticalBox> _PlotContainer;
    TSharedPtr<SCkProceduralAction_AuthoredViewport> _Viewport;
    TSharedPtr<FCkProceduralAction_CurveAdapter> _CurveAdapter;
    TSharedPtr<SCkProceduralAction_Timeline> _Timeline;
    TSharedPtr<SVerticalBox> _SubjectRows;
    TSharedPtr<SVerticalBox> _PoseRows;
    TSharedPtr<SVerticalBox> _InspectorRows;
    TSharedPtr<SVerticalBox> _TimingRows;
    TSharedPtr<SVerticalBox> _IssueRows;
    TSharedPtr<SEditableTextBox> _PoseLabel;
    TSharedPtr<SCkProceduralAction_ProgressPlot> _ProgressPlot;
    TSharedPtr<SExpandableArea> _Advanced;
    TArray<FName> _PlotSlots;
    TArray<TWeakPtr<SCkProceduralAction_AuthoredPlot>> _Plots;
    FTSTicker::FDelegateHandle _TickerHandle;
    FDelegateHandle _SourceAssetPropertyHandle;
    FGuid _SelectedPose;
    FName _SelectedPhase = NAME_None;
    FName _SelectedChannel = NAME_None;
    FName _MirrorDestination = NAME_None;
    ECk_ProceduralAction_MirrorPlane _MirrorPlane = ECk_ProceduralAction_MirrorPlane::XZ;
    uint64 _BuiltRevision = 0;
    uint64 _SolverRevision = 0;
    bool _RequestedPreview = false;
    bool _SourceAssetRefreshPending = false;
    bool _AppearanceRefreshPending = false;
    bool _UndoRegistered = false;
    bool _Closed = false;
    int32 _CurveAxis = 0;
    FString _PreviewSelectionError;
    FString _PoseMessage;
    FString _TimingMessage;
    FString _SaveReport;
    FText _ObservationText;
};
