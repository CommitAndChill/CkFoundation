#pragma once

#include "CkProceduralAnimation/Debug/CkProceduralAction_Debug.h"

#include "SEditorViewport.h"

class FAdvancedPreviewScene;
class FCk_ProceduralActionViewportClient;
class FCk_ProceduralActionAuthoringSession;
class FCk_ProceduralActionEditorModel;
class FCk_ProceduralActionEditorModes;
class FPrimitiveDrawInterface;
class UStaticMeshComponent;
struct FStreamableHandle;

// --------------------------------------------------------------------------------------------------------------------

/** The clip's subject on the engine preview scene: its meshes, its chains, the rest pose dashed, and a pick handle per
 *  channel. The engine transform widget moves the selected channel at the playhead (W move, E rotate, R the bend guide);
 *  the drag is staged here and the release writes one command. While the production Solver runs it shows the Solver's
 *  settled pose instead and the widget is off. */
class SCkProceduralAction_Viewport : public SEditorViewport
{
public:
    enum class EGizmo : uint8 { Move, Rotate, Bend };

    SLATE_BEGIN_ARGS(SCkProceduralAction_Viewport) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionAuthoringSession>, Session)
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionEditorModel>, Model)
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionEditorModes>, Modes)
        SLATE_ATTRIBUTE(FText, Hud)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs)
        -> void;
    ~SCkProceduralAction_Viewport() override;

    auto
        RefreshAppearance()
        -> void;
    auto
        FrameSubject()
        -> void;
    auto
        Shutdown()
        -> void;
    auto
        Tick(
            const FGeometry& InGeometry,
            double InCurrentTime,
            float InDeltaTime)
        -> void override;
    auto
        Get_Error() const
        -> const FString&
    { return _Error; }

private:
    friend class FCk_ProceduralActionViewportClient;

    struct FResolved
    {
        FName PhaseId;
        FName Channel;
        double KeyTime = 0.0;
        FTransform Frame = FTransform::Identity;
        float Scale = 1.0f;
        FVector Position = FVector::ZeroVector;
        FVector Endpoint = FVector::ZeroVector;
        FQuat Orientation = FQuat::Identity;
        FVector Bend = FVector::UpVector;
        FTransform Body = FTransform::Identity;
        bool IsBody = false;
    };

    auto
        MakeEditorViewportClient()
        -> TSharedRef<FEditorViewportClient> override;
    auto
        PopulateViewportOverlays(
            TSharedRef<SOverlay> InOverlay)
        -> void override;
    auto
        OnFocusViewportToSelection()
        -> void override;
    auto
        Get_IsShowingSolver() const
        -> bool;
    auto
        Resolve(
            FName InChannel,
            FResolved& OutResolved) const
        -> bool;
    auto
        Get_WidgetLocation() const
        -> FVector;
    auto
        Get_CanEdit() const
        -> bool;
    auto
        BeginDrag()
        -> void;
    auto
        Drag(
            const FVector& InDrag,
            const FRotator& InRotation)
        -> void;
    auto
        EndDrag()
        -> void;
    auto
        Pick(
            FName InChannel)
        -> void;
    auto
        OnAssetsLoaded()
        -> void;
    auto
        ClearComponents()
        -> void;
    auto
        RefreshPose()
        -> void;
    auto
        Draw(
            FPrimitiveDrawInterface* InPdi) const
        -> void;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TWeakPtr<FCk_ProceduralActionEditorModel> _Model;
    TWeakPtr<FCk_ProceduralActionEditorModes> _Modes;
    TAttribute<FText> _Hud;
    TSharedPtr<FAdvancedPreviewScene> _Scene;
    TSharedPtr<FCk_ProceduralActionViewportClient> _Client;
    TSharedPtr<FStreamableHandle> _LoadHandle;
    UStaticMeshComponent* _BodyComponent = nullptr;
    TMap<FName, UStaticMeshComponent*> _LimbComponents;
    TOptional<FCk_ProceduralAction_DebugSubject> _LiveSubject;
    TOptional<FResolved> _Drag;
    FVector _DragLocation = FVector::ZeroVector;
    FQuat _DragRotation = FQuat::Identity;
    EGizmo _Gizmo = EGizmo::Move;
    FString _Error;
    bool _NeedsFrame = false;
    bool _Closed = false;
};

// --------------------------------------------------------------------------------------------------------------------
