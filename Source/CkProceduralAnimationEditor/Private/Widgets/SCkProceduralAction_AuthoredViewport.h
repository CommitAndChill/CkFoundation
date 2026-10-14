#pragma once

#include "SEditorViewport.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Receipt.h"
#include "CkProceduralAnimation/Debug/CkProceduralAction_Debug.h"

class FCk_ProceduralActionAuthoringSession;
class FPreviewScene;
struct FStreamableHandle;
class UStaticMeshComponent;
class FSceneView;
class FPrimitiveDrawInterface;

DECLARE_DELEGATE_TwoParams(FCkProceduralActionTrackPicked, FName, FName);

// Presentation-only EditorPreview scene. Authored draws pure Evaluate output, Solver draws
// a copied live settlement, and Recorded draws a selected receipt. This scene never runs ECS or Jolt.
class SCkProceduralAction_AuthoredViewport : public SEditorViewport
{
public:
    enum class EGizmoTarget : uint8 { Endpoint, BendGuide };

    SLATE_BEGIN_ARGS(SCkProceduralAction_AuthoredViewport) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionAuthoringSession>, Session)
        SLATE_EVENT(FSimpleDelegate, OnChanged)
        SLATE_EVENT(FCkProceduralActionTrackPicked, OnTrackPicked)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs)
        -> void;
    ~SCkProceduralAction_AuthoredViewport() override;

    auto
        RefreshAppearance()
        -> void;
    auto
        RefreshPose()
        -> void;
    auto
        SetObservedReceipt(
            const FCk_ProceduralAction_Receipt* InReceipt)
        -> void;
    auto
        SetLiveSubjectSnapshot(
            const FCk_ProceduralAction_DebugSubject* InSnapshot)
        -> void;
    auto
        SetObservedFloorReady(
            TOptional<bool> InFloorReady)
        -> void;
    auto
        UseAuthoredPose()
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
        SetSelectedTrack(
            FName InPhaseId,
            FName InSlotId)
        -> void;
    auto
        SetGizmoTarget(
            EGizmoTarget InTarget)
        -> void;
    auto
        GetGizmoError() const
        -> const FString&
    {
        return _GizmoError;
    }

private:
    friend class FCkProceduralActionViewportClient;
    auto
        MakeEditorViewportClient()
        -> TSharedRef<FEditorViewportClient> override;
    auto
        OnAssetsLoaded()
        -> void;
    auto
        ClearComponents()
        -> void;
    auto
        TryFrameAppearance()
        -> void;
    auto
        ResolveSelected(
            FTransform& OutFrame,
            float& OutScale,
            FVector& OutEndpoint,
            FVector& OutBendWorld,
            double& OutKeyTime) const
        -> bool;
    auto
        CanEditGizmo() const
        -> bool;
    auto
        GetGizmoLocation() const
        -> FVector;
    auto
        BeginGizmo()
        -> void;
    auto
        DragGizmo(
            const FVector& InWorldDelta)
        -> bool;
    auto
        EndGizmo()
        -> void;
    auto
        PickAtPixel(
            const FSceneView& InView,
            uint32 InX,
            uint32 InY)
        -> bool;
    auto
        DrawObservedPaths(
            FPrimitiveDrawInterface* InPdi) const
        -> void;
    auto
        DrawLiveSupport(
            FPrimitiveDrawInterface* InPdi) const
        -> void;
    auto
        DrawLiveSegments(
            FPrimitiveDrawInterface* InPdi) const
        -> void;
    auto
        DrawPreviewSurface(
            FPrimitiveDrawInterface* InPdi) const
        -> void;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    FSimpleDelegate _OnChanged;
    FCkProceduralActionTrackPicked _OnTrackPicked;
    TSharedPtr<FPreviewScene> _Scene;
    TSharedPtr<FEditorViewportClient> _Client;
    TSharedPtr<FStreamableHandle> _LoadHandle;
    UStaticMeshComponent* _BodyComponent = nullptr;
    TMap<FName, UStaticMeshComponent*> _TerminalComponents;
    TOptional<FCk_ProceduralArticulation_Pose> _ObservedPreviousPose;
    TOptional<FCk_ProceduralArticulation_Pose> _ObservedPose;
    TOptional<FCk_ProceduralAction_DebugSubject> _LiveSubject;
    TOptional<bool> _ObservedFloorReady;
    ECk_ProceduralAction_Continuity _ObservedContinuity = ECk_ProceduralAction_Continuity::Incomplete;
    bool _UseObservation = false;
    bool _Closed = false;
    bool _GizmoDragging = false;
    bool _NeedsAutoFrame = false;
    FName _SelectedPhase = NAME_None;
    FName _SelectedSlot = NAME_None;
    EGizmoTarget _GizmoTarget = EGizmoTarget::Endpoint;
    FVector _GizmoWorldPosition = FVector::ZeroVector;
    FVector _GizmoStartWorldPosition = FVector::ZeroVector;
    FString _GizmoError;
};
