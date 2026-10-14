#pragma once

#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"

#include "Containers/Ticker.h"
#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/StrongObjectPtr.h"

class FCk_ProceduralActionAuthoringSession;
class FCk_ProceduralActionEditorModel;
class FCkProceduralAction_CurveAdapter;
class FCkProceduralActionEditorModes;
class IDetailsView;
class SCkProceduralAction_Channels;
class SCkProceduralAction_Issues;
class SCkProceduralAction_Timeline;
class SCkProceduralAction_Viewport;
class FToolBarBuilder;
class UCk_ProceduralAction_Data;
class UCk_ProceduralActionPreview_Config;
class UCk_ProceduralActionSelectionProxy;

// --------------------------------------------------------------------------------------------------------------------

/** The clip editor: a host for its panels (Channels, Viewport, Details, Timeline with its transport, Curves) over one
 *  editor model, session and set of playback sources. The toolbar actions are public so the editor's tests drive the same
 *  calls a click does; every clip edit goes through FCk_ProceduralActionAuthoringCommands. */
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralAction_EditorToolkit final : public FAssetEditorToolkit, public FEditorUndoClient
{
public:
    static const FName ToolkitName;
    static const FName ViewportTabId;
    static const FName ChannelsTabId;
    static const FName DetailsTabId;
    static const FName TimelineTabId;
    static const FName CurvesTabId;

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

    auto
        Get_Model() const
        -> TSharedPtr<FCk_ProceduralActionEditorModel>
    { return _Model; }
    auto
        Get_Session() const
        -> TSharedPtr<FCk_ProceduralActionAuthoringSession>
    { return _Session; }
    auto
        Get_Modes() const
        -> const FCkProceduralActionEditorModes*
    { return _Modes.Get(); }
    auto
        Get_SelectionProxy() const
        -> UCk_ProceduralActionSelectionProxy*;
    // The playback source's state line: the session in Authored, the production Solver in Solved.
    auto
        Get_StatusText() const
        -> FText;
    // The outcome of the last toolbar action or edit: what it did, or why it was refused.
    auto
        Get_Message() const
        -> const FString&
    { return _Message; }

    // Records the playhead's pose for the channels Record covers: the authored pose in Authored, the paused production
    // pose in Solved. A pose already named at that time is replaced.
    auto
        RecordPose()
        -> bool;
    auto
        TogglePlay()
        -> void;
    // Rebuilds the production preview for the current revision, paused at its start.
    auto
        RestartSolver()
        -> bool;
    auto
        StepFrames(
            int32 InFrames)
        -> void;
    auto
        AddSection()
        -> bool;
    auto
        AddSlot()
        -> bool;
    auto
        AddMarkerAtPlayhead()
        -> bool;
    auto
        AddWindowAtPlayhead()
        -> bool;
    auto
        ApplyStartingShape(
            ECk_ProceduralAction_Recipe InRecipe)
        -> bool;
    // Deletes the selected pose, else the selected key.
    auto
        DeleteSelection()
        -> bool;
    auto
        SetScenario(
            UCk_ProceduralActionPreview_Config* InScenario)
        -> void;
    auto
        FrameSubject()
        -> void;

private:
    auto
        BindCommands()
        -> void;
    auto
        FillToolbar(
            FToolBarBuilder& InBuilder)
        -> void;
    auto
        MakeSubjectMenu()
        -> TSharedRef<SWidget>;
    auto
        MakeStartingShapeMenu()
        -> TSharedRef<SWidget>;
    auto
        SpawnTab(
            const FSpawnTabArgs& InArgs,
            FName InTabId)
        -> TSharedRef<SDockTab>;
    auto
        Finish(
            bool InSucceeded,
            const FString& InSuccess,
            const FString& InError)
        -> bool;
    auto
        Refresh()
        -> void;
    auto
        ApplyRequestedTime()
        -> void;
    auto
        RefreshDetails()
        -> void;
    auto
        OnProxyEdited(
            FName InProperty)
        -> void;
    auto
        OnObjectPropertyChanged(
            UObject* InObject,
            FPropertyChangedEvent& InEvent)
        -> void;
    auto
        OnTicker(
            float InDeltaSeconds)
        -> bool;
    auto
        TickPlayback(
            float InDeltaSeconds)
        -> void;
    auto
        Get_IsSolved() const
        -> bool;
    // The clip's duration, or the solved run's when it continues past the clip's end.
    auto
        Get_PlaybackLength() const
        -> double;
    auto
        Get_HudText() const
        -> FText;
    // The section a clip time falls in and the raw fraction there, for events (keys use the session's ElapsedToKey).
    auto
        Get_SectionAt(
            FCk_Time InTime,
            FName& OutPhaseId,
            double& OutFraction) const
        -> bool;

private:
    TSharedPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TSharedPtr<FCk_ProceduralActionEditorModel> _Model;
    TSharedPtr<FCkProceduralActionEditorModes> _Modes;
    TSharedPtr<FCkProceduralAction_CurveAdapter> _Curves;
    TSharedPtr<SWidget> _CurvesPanel;
    TSharedPtr<SCkProceduralAction_Viewport> _Viewport;
    TSharedPtr<SCkProceduralAction_Channels> _Channels;
    TSharedPtr<SCkProceduralAction_Timeline> _Timeline;
    TSharedPtr<SCkProceduralAction_Issues> _Issues;
    TSharedPtr<IDetailsView> _Details;
    TStrongObjectPtr<UCk_ProceduralActionSelectionProxy> _SelectionProxy;
    FTSTicker::FDelegateHandle _TickerHandle;
    FDelegateHandle _PropertyChangedHandle;
    FDelegateHandle _ModelChangedHandle;
    uint64 _BuiltContentRevision = 0;
    uint64 _BuiltSelectionRevision = 0;
    uint64 _AppliedTimeRequest = 0;
    bool _SourceRefreshPending = false;
    bool _AppearanceRefreshPending = false;
    bool _DetailsRefreshPending = false;
    bool _UndoRegistered = false;
    bool _Closed = false;
    FString _Message;
};

// --------------------------------------------------------------------------------------------------------------------
