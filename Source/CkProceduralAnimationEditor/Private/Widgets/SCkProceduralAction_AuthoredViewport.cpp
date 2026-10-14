#include "Widgets/SCkProceduralAction_AuthoredViewport.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreviewSpace.h"
#include "Authoring/CkProceduralActionObservedReach.h"
#include "CkTween/CkTween_Easing_Utils.h"

#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/StaticMesh.h"
#include "PreviewScene.h"
#include "PrimitiveDrawInterface.h"
#include "PrimitiveDrawingUtils.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UnrealWidget.h"

namespace ck_procedural_action_viewport
{
    auto
        WithAppearanceScale(
            const FTransform& InPose,
            const FVector& InScale)
        -> FTransform
    {
        return FTransform{InPose.GetRotation(), InPose.GetTranslation(), InScale};
    }

    auto
        ToTween(
            ECk_ProceduralAction_Easing InEasing)
        -> ECk_TweenEasing
    {
        switch (InEasing)
        {
            case ECk_ProceduralAction_Easing::InQuad: return ECk_TweenEasing::InQuad;
            case ECk_ProceduralAction_Easing::OutQuad: return ECk_TweenEasing::OutQuad;
            case ECk_ProceduralAction_Easing::InOutQuad: return ECk_TweenEasing::InOutQuad;
            case ECk_ProceduralAction_Easing::InCubic: return ECk_TweenEasing::InCubic;
            case ECk_ProceduralAction_Easing::OutCubic: return ECk_TweenEasing::OutCubic;
            case ECk_ProceduralAction_Easing::InOutCubic: return ECk_TweenEasing::InOutCubic;
            default: return ECk_TweenEasing::Linear;
        }
    }

    auto
        DrawEndpointCross(
            FPrimitiveDrawInterface* InPdi,
            const FVector& InCenter,
            const FLinearColor& InColor)
        -> void
    {
        constexpr auto RadiusCm = 3.0;
        for (const auto Axis : {FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector})
        { InPdi->DrawLine(InCenter - Axis * RadiusCm, InCenter + Axis * RadiusCm,
            InColor, SDPG_Foreground, 2.0f); }
    }

    auto
        DrawRequestedPath(
            FPrimitiveDrawInterface* InPdi,
            const FVector& InFrom,
            const FVector& InTo)
        -> void
    {
        constexpr auto DashCount = 12;
        for (auto Dash = 0; Dash < DashCount; Dash += 2)
        {
            const auto Start = FMath::Lerp(InFrom, InTo,
                static_cast<double>(Dash) / DashCount);
            const auto End = FMath::Lerp(InFrom, InTo,
                static_cast<double>(Dash + 1) / DashCount);
            InPdi->DrawLine(Start, End, FLinearColor::Yellow, SDPG_World, 2.0f);
        }
    }

    auto
        DrawReachSphere(
            FPrimitiveDrawInterface* InPdi,
            const FVector& InHip,
            double InRadiusCm,
            const FLinearColor& InColor)
        -> void
    {
        constexpr auto SegmentCount = 24;
        for (auto Plane = 0; Plane < 3; ++Plane)
        {
            const auto XAxis = Plane == 1 ? FVector::YAxisVector : FVector::XAxisVector;
            const auto YAxis = Plane == 0 ? FVector::YAxisVector : FVector::ZAxisVector;
            for (auto Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const auto FirstAngle = 2.0 * UE_PI * Segment / SegmentCount;
                const auto LastAngle = 2.0 * UE_PI * (Segment + 1) / SegmentCount;
                const auto First = InHip + InRadiusCm
                    * (XAxis * FMath::Cos(FirstAngle) + YAxis * FMath::Sin(FirstAngle));
                const auto Last = InHip + InRadiusCm
                    * (XAxis * FMath::Cos(LastAngle) + YAxis * FMath::Sin(LastAngle));
                InPdi->DrawLine(First, Last, InColor, SDPG_World, 0.5f);
            }
        }
    }
}

class FCkProceduralActionViewportClient final : public FEditorViewportClient
{
public:
    FCkProceduralActionViewportClient(
        FPreviewScene* InScene,
        const TSharedRef<SCkProceduralAction_AuthoredViewport>& InWidget)
        : FEditorViewportClient(nullptr, InScene, InWidget), _Widget(InWidget)
    {
        if (Widget != nullptr)
        { Widget->SetDefaultVisibility(false); }
    }

    auto GetWidgetMode() const -> UE::Widget::EWidgetMode override
    {
        const auto PreviewWidget = _Widget.Pin();
        return PreviewWidget.IsValid() && PreviewWidget->CanEditGizmo()
            ? UE::Widget::WM_Translate : UE::Widget::WM_None;
    }

    auto GetWidgetLocation() const -> FVector override
    {
        const auto PreviewWidget = _Widget.Pin();
        return PreviewWidget.IsValid() ? PreviewWidget->GetGizmoLocation() : FVector::ZeroVector;
    }

    auto TrackingStarted(const FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge) -> void override
    {
        FEditorViewportClient::TrackingStarted(InInputState, bIsDraggingWidget, bNudge);
        if (bIsDraggingWidget)
        {
            if (const auto PreviewWidget = _Widget.Pin())
            { PreviewWidget->BeginGizmo(); }
        }
    }

    auto InputWidgetDelta(FViewport* InViewport, EAxisList::Type InAxis,
        FVector& InDrag, FRotator& InRot, FVector& InScale) -> bool override
    {
        if (InAxis != EAxisList::None)
        {
            if (const auto PreviewWidget = _Widget.Pin())
            {
                if (PreviewWidget->DragGizmo(InDrag))
                { return true; }
            }
        }
        return FEditorViewportClient::InputWidgetDelta(InViewport, InAxis, InDrag, InRot, InScale);
    }

    auto TrackingStopped() -> void override
    {
        if (const auto PreviewWidget = _Widget.Pin())
        { PreviewWidget->EndGizmo(); }
        FEditorViewportClient::TrackingStopped();
    }

    auto Draw(const FSceneView* InView, FPrimitiveDrawInterface* InPdi) -> void override
    {
        FEditorViewportClient::Draw(InView, InPdi);
        const auto PreviewWidget = _Widget.Pin();
        if (PreviewWidget.IsValid())
        {
            PreviewWidget->DrawPreviewSurface(InPdi);
            PreviewWidget->DrawObservedPaths(InPdi);
            PreviewWidget->DrawLiveSegments(InPdi);
            PreviewWidget->DrawLiveSupport(InPdi);
        }
        if (NOT PreviewWidget.IsValid() || NOT PreviewWidget->CanEditGizmo())
        { return; }
        const auto Origin = PreviewWidget->GetGizmoLocation();
        InPdi->SetHitProxy(new HWidgetAxis(EAxisList::X));
        InPdi->DrawLine(Origin + FVector::XAxisVector * 7.0,
            Origin + FVector::XAxisVector * 50.0, FLinearColor::Red, SDPG_Foreground, 5.0f);
        InPdi->SetHitProxy(new HWidgetAxis(EAxisList::Y));
        InPdi->DrawLine(Origin + FVector::YAxisVector * 7.0,
            Origin + FVector::YAxisVector * 50.0, FLinearColor::Green, SDPG_Foreground, 5.0f);
        InPdi->SetHitProxy(new HWidgetAxis(EAxisList::Z));
        InPdi->DrawLine(Origin + FVector::ZAxisVector * 7.0,
            Origin + FVector::ZAxisVector * 50.0, FLinearColor::Blue, SDPG_Foreground, 5.0f);
        InPdi->SetHitProxy(nullptr);
        if (PreviewWidget->_GizmoTarget != SCkProceduralAction_AuthoredViewport::EGizmoTarget::BendGuide)
        { return; }
        auto Frame = FTransform::Identity;
        auto Scale = 1.0f;
        auto Endpoint = FVector::ZeroVector;
        auto Bend = FVector::ZeroVector;
        auto KeyTime = 0.0;
        if (PreviewWidget->ResolveSelected(Frame, Scale, Endpoint, Bend, KeyTime))
        { InPdi->DrawLine(Endpoint, PreviewWidget->GetGizmoLocation(), FLinearColor::Yellow, SDPG_World, 2.0f); }
    }

    auto ProcessClick(FSceneView& InView, HHitProxy* InHitProxy, FKey InKey,
        EInputEvent InEvent, uint32 InX, uint32 InY) -> void override
    {
        const auto PreviewWidget = _Widget.Pin();
        if (PreviewWidget.IsValid() && PreviewWidget->PickAtPixel(InView, InX, InY))
        { return; }
        FEditorViewportClient::ProcessClick(InView, InHitProxy, InKey, InEvent, InX, InY);
    }

private:
    TWeakPtr<SCkProceduralAction_AuthoredViewport> _Widget;
};

auto
    SCkProceduralAction_AuthoredViewport::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Session = InArgs._Session;
    _OnChanged = InArgs._OnChanged;
    _OnTrackPicked = InArgs._OnTrackPicked;
    _Scene = MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues()
        .SetCreateDefaultLighting(true).SetCreatePhysicsScene(false));
    SEditorViewport::Construct(SEditorViewport::FArguments());
    RefreshAppearance();
}

auto
    SCkProceduralAction_AuthoredViewport::
    DrawPreviewSurface(
        FPrimitiveDrawInterface* InPdi) const
    -> void
{
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->GetPreviewConfig() : nullptr;
    auto Error = FString{};
    if (_Closed || Config == nullptr || InPdi == nullptr || NOT Config->ValidateSurface(Error))
    { return; }
    // A recorded pose has no environmental timeline. Only Authored shows the canonical
    // fixtures; Solver shows the floor after the production Jolt body is actually ready.
    if (_UseObservation && NOT _ObservedFloorReady.IsSet())
    { return; }
    if (Config->Get_HasStaticFloor()
        && (NOT _UseObservation || _ObservedFloorReady.GetValue()))
    {
        const auto Extents = Config->Get_StaticFloorHalfExtents();
        DrawWireBox(InPdi, Config->Get_StaticFloorFrame().ToMatrixWithScale(),
            FBox{-Extents, Extents}, FLinearColor{0.4f, 0.5f, 0.6f}, SDPG_World, 1.0f);
    }
    for (const auto& Obstacle : Config->Get_StaticObstacles())
    {
        const auto Extents = Obstacle.Get_HalfExtents();
        DrawWireBox(InPdi, Obstacle.Get_Frame().ToMatrixWithScale(),
            FBox{-Extents, Extents}, FLinearColor{1.0f, 0.4f, 0.1f}, SDPG_World, 2.0f);
    }
}

SCkProceduralAction_AuthoredViewport::~SCkProceduralAction_AuthoredViewport()
{
    Shutdown();
    if (_Client.IsValid())
    { _Client->Viewport = nullptr; }
    Client.Reset();
    _Client.Reset();
    _Scene.Reset();
}

auto
    SCkProceduralAction_AuthoredViewport::
    Tick(
        const FGeometry& InGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SEditorViewport::Tick(InGeometry, InCurrentTime, InDeltaTime);
    if (_NeedsAutoFrame)
    { TryFrameAppearance(); }
}

auto
    SCkProceduralAction_AuthoredViewport::
    Shutdown()
    -> void
{
    if (_Closed)
    { return; }
    _Closed = true;
    if (_LoadHandle.IsValid())
    { _LoadHandle->CancelHandle(); _LoadHandle.Reset(); }
    ClearComponents();
    _Session.Reset();
    _ObservedPreviousPose.Reset();
    _ObservedPose.Reset();
    _LiveSubject.Reset();
    _ObservedFloorReady.Reset();
    _GizmoDragging = false;
    _NeedsAutoFrame = false;
}

auto
    SCkProceduralAction_AuthoredViewport::
    SetObservedReceipt(
        const FCk_ProceduralAction_Receipt* InReceipt)
    -> void
{
    if (_Closed)
    { return; }
    _UseObservation = true;
    _ObservedPreviousPose = InReceipt != nullptr && InReceipt->Get_HasAcceptedPose()
        ? TOptional<FCk_ProceduralArticulation_Pose>{InReceipt->Get_PreviousPose()}
        : TOptional<FCk_ProceduralArticulation_Pose>{};
    _ObservedPose = InReceipt != nullptr && InReceipt->Get_HasAcceptedPose()
        ? TOptional<FCk_ProceduralArticulation_Pose>{InReceipt->Get_CurrentPose()}
        : TOptional<FCk_ProceduralArticulation_Pose>{};
    _ObservedContinuity = InReceipt != nullptr
        ? InReceipt->Get_Interval().Get_Continuity() : ECk_ProceduralAction_Continuity::Incomplete;
    RefreshPose();
}

auto
    SCkProceduralAction_AuthoredViewport::
    SetObservedFloorReady(
        TOptional<bool> InFloorReady)
    -> void
{
    if (_Closed)
    { return; }
    _ObservedFloorReady = InFloorReady;
    Invalidate();
}

auto
    SCkProceduralAction_AuthoredViewport::
    SetLiveSubjectSnapshot(
        const FCk_ProceduralAction_DebugSubject* InSnapshot)
    -> void
{
    if (_Closed)
    { return; }
    _LiveSubject = InSnapshot != nullptr && InSnapshot->Get_Available()
        ? TOptional<FCk_ProceduralAction_DebugSubject>{*InSnapshot}
        : TOptional<FCk_ProceduralAction_DebugSubject>{};
    RefreshPose();
}

auto
    SCkProceduralAction_AuthoredViewport::
    DrawLiveSegments(
        FPrimitiveDrawInterface* InPdi) const
    -> void
{
    if (_Closed || InPdi == nullptr || NOT _LiveSubject.IsSet()
        || NOT _LiveSubject.GetValue().Get_HasSettledPose())
    { return; }
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->GetPreviewConfig() : nullptr;
    if (Config == nullptr)
    { return; }
    for (const auto& Limb : _LiveSubject.GetValue().Get_SettledPose().Get_Limbs())
    {
        const auto* Lengths = static_cast<const TArray<float>*>(nullptr);
        if (Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
        {
            if (const auto* Rig = Config->Get_WalkingRig().Get())
            {
                if (const auto* Leg = Rig->Get_Legs().FindByPredicate([&Limb](const auto& InLeg)
                    { return InLeg.Get_Id() == Limb.Get_Id(); }))
                { Lengths = &Leg->Get_Chain().Get_SegmentLengths(); }
            }
        }
        else if (const auto* Layout = Config->Get_Layout().Get_Limbs().FindByPredicate([&Limb](const auto& InLayout)
            { return InLayout.Get_Id() == Limb.Get_Id(); }))
        { Lengths = &Layout->Get_SegmentLengths(); }
        if (Lengths == nullptr)
        { continue; }
        const auto& Segments = Limb.Get_Segments();
        for (auto Index = 0; Index < FMath::Min(Lengths->Num(), Segments.Num()); ++Index)
        {
            const auto Axis = Segments[Index].GetRotation().GetAxisX();
            const auto Half = Axis * ((*Lengths)[Index] * 0.5);
            InPdi->DrawLine(Segments[Index].GetLocation() - Half,
                Segments[Index].GetLocation() + Half,
                FLinearColor{0.2f, 0.9f, 0.8f}, SDPG_World, 3.0f);
        }
    }
}

auto
    SCkProceduralAction_AuthoredViewport::
    DrawLiveSupport(
        FPrimitiveDrawInterface* InPdi) const
    -> void
{
    if (_Closed || NOT _LiveSubject.IsSet() || InPdi == nullptr)
    { return; }
    const auto& Snapshot = _LiveSubject.GetValue();
    for (const auto& Contact : Snapshot.Get_Contacts())
    {
        ck_procedural_action_viewport::DrawEndpointCross(InPdi, Contact.Get_Position(),
            Contact.Get_TrustedSupport() ? FLinearColor::Green : FLinearColor::Red);
        if (Contact.Get_Reserved())
        { InPdi->DrawLine(Contact.Get_Position(), Contact.Get_Position() + Snapshot.Get_SupportUp() * 12.0,
            FLinearColor::Yellow, SDPG_World, 1.5f); }
    }
    for (const auto& Lease : Snapshot.Get_Leases())
    {
        const auto& Region = Lease.Get_ProjectedRegion();
        if (Snapshot.Get_SupportFresh() && Region.Num() >= 2)
        {
            for (auto Index = 0; Index < Region.Num(); ++Index)
            { InPdi->DrawLine(Region[Index], Region[(Index + 1) % Region.Num()],
                FLinearColor::Green, SDPG_World, 1.5f); }
        }
        for (const auto& Landing : Lease.Get_Landings())
        {
            ck_procedural_action_viewport::DrawEndpointCross(InPdi, Landing.Get_Position(),
                FLinearColor::Yellow);
        }
    }
}

auto
    SCkProceduralAction_AuthoredViewport::
    DrawObservedPaths(
        FPrimitiveDrawInterface* InPdi) const
    -> void
{
    if (_Closed || NOT _UseObservation || InPdi == nullptr)
    { return; }
    const auto Live = _LiveSubject.IsSet() && _LiveSubject.GetValue().Get_HasSettledPose();
    const auto* Pose = Live ? &_LiveSubject.GetValue().Get_SettledPose()
        : _ObservedPose.IsSet() ? &_ObservedPose.GetValue() : nullptr;
    if (Pose == nullptr)
    { return; }
    for (const auto& Limb : Pose->Get_Limbs())
    {
        if (NOT _SelectedSlot.IsNone())
        {
            const auto Session = _Session.Pin();
            const auto* Config = Session.IsValid() ? Session->GetPreviewConfig() : nullptr;
            const auto* Binding = Config != nullptr
                ? Config->Get_SlotBindings().FindByPredicate([this](const auto& InBinding)
                    { return InBinding.Get_Slot() == _SelectedSlot; }) : nullptr;
            if (Binding == nullptr || Binding->Get_LimbId() != Limb.Get_Id())
            { continue; }
        }
        const auto Requested = Limb.Get_RequestedEndpoint().GetTranslation();
        const auto Accepted = Limb.Get_AcceptedEndpoint().GetTranslation();
        ck_procedural_action_viewport::DrawEndpointCross(InPdi, Requested, FLinearColor::Yellow);
        ck_procedural_action_viewport::DrawEndpointCross(InPdi, Accepted,
            FLinearColor{0.15f, 0.85f, 0.95f});
        if (NOT Live && _ObservedContinuity == ECk_ProceduralAction_Continuity::Continuous
            && _ObservedPreviousPose.IsSet())
        {
            const auto* PreviousLimb = _ObservedPreviousPose.GetValue().Get_Limbs().FindByPredicate(
                [&Limb](const auto& InLimb) { return InLimb.Get_Id() == Limb.Get_Id(); });
            if (PreviousLimb != nullptr)
            {
                const auto From = PreviousLimb->Get_AcceptedEndpoint().GetTranslation();
                ck_procedural_action_viewport::DrawRequestedPath(InPdi, From, Requested);
                InPdi->DrawLine(From, Accepted,
                    FLinearColor{0.15f, 0.85f, 0.95f}, SDPG_World, 2.0f);
            }
        }
        if (_SelectedSlot.IsNone())
        { continue; }
        const auto Session = _Session.Pin();
        const auto* Config = Session.IsValid() ? Session->GetPreviewConfig() : nullptr;
        if (Config == nullptr)
        { continue; }
        auto Hip = FVector::ZeroVector;
        auto Reach = 0.0;
        auto FirstLength = 0.0;
        if (Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
        {
            if (const auto* Rig = Config->Get_WalkingRig().Get())
            {
                if (const auto* Leg = Rig->Get_Legs().FindByPredicate([&Limb](const auto& InLeg)
                    { return InLeg.Get_Id() == Limb.Get_Id(); }))
                {
                    for (const auto Length : Leg->Get_Chain().Get_SegmentLengths())
                    { Reach += Length; }
                    if (NOT Leg->Get_Chain().Get_SegmentLengths().IsEmpty())
                    { FirstLength = Leg->Get_Chain().Get_SegmentLengths()[0]; }
                }
            }
        }
        else if (const auto* Layout = Config->Get_Layout().Get_Limbs().FindByPredicate(
            [&Limb](const auto& InLayout) { return InLayout.Get_Id() == Limb.Get_Id(); }))
        {
            for (const auto Length : Layout->Get_SegmentLengths())
            { Reach += Length; }
            if (NOT Layout->Get_SegmentLengths().IsEmpty())
            { FirstLength = Layout->Get_SegmentLengths()[0]; }
        }
        if (Reach > 0.0 && ck::procedural_action_editor::TryGetObservedHip(
            Limb, FirstLength, Hip))
        {
            ck_procedural_action_viewport::DrawReachSphere(InPdi, Hip, Reach,
                FVector::Distance(Hip, Requested) >= Reach - FMath::Max(0.05, Reach * 0.02)
                    ? FLinearColor::Yellow : FLinearColor{0.45f, 0.45f, 0.45f});
        }
    }
}

auto
    SCkProceduralAction_AuthoredViewport::
    UseAuthoredPose()
    -> void
{
    if (_Closed)
    { return; }
    _UseObservation = false;
    _ObservedPreviousPose.Reset();
    _ObservedPose.Reset();
    _LiveSubject.Reset();
    RefreshPose();
}

auto
    SCkProceduralAction_AuthoredViewport::
    MakeEditorViewportClient()
    -> TSharedRef<FEditorViewportClient>
{
    _Client = MakeShared<FCkProceduralActionViewportClient>(_Scene.Get(), SharedThis(this));
    _Client->SetViewMode(VMI_Lit);
    _Client->SetViewLocation(FVector{-300.0, -300.0, 180.0});
    _Client->SetViewRotation(FRotator{-12.0, 45.0, 0.0});
    _Client->SetRealtime(true);
    return _Client.ToSharedRef();
}

auto
    SCkProceduralAction_AuthoredViewport::
    SetSelectedTrack(
        FName InPhaseId,
        FName InSlotId)
    -> void
{
    _GizmoDragging = false;
    _SelectedPhase = InPhaseId;
    _SelectedSlot = InSlotId;
    _GizmoError.Reset();
    if (_BodyComponent == nullptr && _TerminalComponents.IsEmpty())
    { _NeedsAutoFrame = true; }
    Invalidate();
}

auto
    SCkProceduralAction_AuthoredViewport::
    SetGizmoTarget(
        EGizmoTarget InTarget)
    -> void
{
    _GizmoDragging = false;
    _GizmoTarget = InTarget;
    _GizmoError.Reset();
    if (_BodyComponent == nullptr && _TerminalComponents.IsEmpty())
    { _NeedsAutoFrame = true; }
    Invalidate();
}

auto
    SCkProceduralAction_AuthoredViewport::
    ResolveSelected(
        FTransform& OutFrame,
        float& OutScale,
        FVector& OutEndpoint,
        FVector& OutBendWorld,
        double& OutKeyTime) const
    -> bool
{
    const auto Session = _Session.Pin();
    if (_Closed || _UseObservation || NOT Session.IsValid() || NOT Session->IsCompiled()
        || NOT Session->HasPose() || Session->GetPreviewConfig() == nullptr
        || _SelectedPhase.IsNone() || _SelectedSlot.IsNone())
    { return false; }
    const auto& Compiled = Session->GetCompiledClip();
    const auto* Phase = Compiled.Get_Phases().FindByPredicate([this](const auto& InPhase)
        { return InPhase.Get_Id() == _SelectedPhase; });
    const auto* Slot = Compiled.Get_Slots().FindByPredicate([this](const auto& InSlot)
        { return InSlot.Get_Id() == _SelectedSlot; });
    const auto& Pose = Session->GetPose();
    const auto* Limb = Pose.Get_Limbs().FindByPredicate([this](const auto& InLimb)
        { return InLimb.Get_Slot() == _SelectedSlot; });
    if (Phase == nullptr || Slot == nullptr || Limb == nullptr
        || NOT Phase->Get_LimbTracks().ContainsByPredicate([this](const auto& InTrack)
            { return InTrack.Get_Slot() == _SelectedSlot; })
        || NOT Compiled.Get_Phases().IsValidIndex(Pose.Get_PhaseIndex())
        || Compiled.Get_Phases()[Pose.Get_PhaseIndex()].Get_Id() != _SelectedPhase)
    { return false; }
    const auto& Config = *Session->GetPreviewConfig();
    if (NOT Config.Get_SlotBindings().ContainsByPredicate([this](const auto& InBinding)
        { return InBinding.Get_Slot() == _SelectedSlot; }))
    { return false; }
    auto BodyWorld = FTransform::Identity;
    if (NOT ck::procedural_action_editor::ResolveBodyWorld(Config, Compiled, Pose, BodyWorld)
        || NOT ck::procedural_action_editor::ResolveSlotFrame(
            Config, *Slot, BodyWorld, OutFrame, OutScale))
    { return false; }
    OutEndpoint = ck::procedural_action_editor::ToWorldPosition(
        OutFrame, OutScale, Limb->Get_Position());
    const auto LocalBend = Limb->Get_HasBend() ? Limb->Get_BendDirection() : FVector::UpVector;
    OutBendWorld = OutFrame.TransformVectorNoScale(LocalBend).GetSafeNormal();
    const auto Raw = FMath::Clamp((Session->GetTime().Get_Seconds() - Phase->Get_Start().Get_Seconds())
        / Phase->Get_Duration().Get_Seconds(), 0.0, 1.0);
    OutKeyTime = UCk_Utils_TweenEasing_UE::Apply(
        ck_procedural_action_viewport::ToTween(Phase->Get_Easing()), FCk_FloatRange_0to1{Raw});
    return NOT OutEndpoint.ContainsNaN() && NOT OutBendWorld.ContainsNaN()
        && OutBendWorld.SizeSquared() > 1.0e-8;
}

auto
    SCkProceduralAction_AuthoredViewport::
    CanEditGizmo() const
    -> bool
{
    auto Frame = FTransform::Identity;
    auto Scale = 1.0f;
    auto Endpoint = FVector::ZeroVector;
    auto Bend = FVector::ZeroVector;
    auto Time = 0.0;
    return ResolveSelected(Frame, Scale, Endpoint, Bend, Time);
}

auto
    SCkProceduralAction_AuthoredViewport::
    GetGizmoLocation() const
    -> FVector
{
    if (_GizmoDragging)
    { return _GizmoWorldPosition; }
    auto Frame = FTransform::Identity;
    auto Scale = 1.0f;
    auto Endpoint = FVector::ZeroVector;
    auto Bend = FVector::ZeroVector;
    auto Time = 0.0;
    if (NOT ResolveSelected(Frame, Scale, Endpoint, Bend, Time))
    { return FVector::ZeroVector; }
    return _GizmoTarget == EGizmoTarget::Endpoint ? Endpoint : Endpoint + Bend * 40.0;
}

auto
    SCkProceduralAction_AuthoredViewport::
    BeginGizmo()
    -> void
{
    if (NOT CanEditGizmo())
    { return; }
    _GizmoWorldPosition = GetGizmoLocation();
    _GizmoStartWorldPosition = _GizmoWorldPosition;
    _GizmoDragging = true;
    _GizmoError.Reset();
}

auto
    SCkProceduralAction_AuthoredViewport::
    DragGizmo(
        const FVector& InWorldDelta)
    -> bool
{
    if (NOT _GizmoDragging || InWorldDelta.ContainsNaN())
    { return false; }
    _GizmoWorldPosition += InWorldDelta;
    if (_GizmoTarget == EGizmoTarget::Endpoint)
    {
        const auto Session = _Session.Pin();
        if (Session.IsValid() && Session->GetPreviewConfig() != nullptr)
        {
            const auto* Binding = Session->GetPreviewConfig()->Get_SlotBindings().FindByPredicate(
                [this](const auto& InBinding) { return InBinding.Get_Slot() == _SelectedSlot; });
            auto** Component = Binding != nullptr ? _TerminalComponents.Find(Binding->Get_LimbId()) : nullptr;
            if (Component != nullptr && *Component != nullptr)
            { (*Component)->SetWorldLocation(_GizmoWorldPosition); }
        }
    }
    Invalidate();
    return true;
}

auto
    SCkProceduralAction_AuthoredViewport::
    EndGizmo()
    -> void
{
    if (NOT _GizmoDragging)
    { return; }
    _GizmoDragging = false;
    if (_GizmoWorldPosition.Equals(_GizmoStartWorldPosition, 1.0e-6))
    { return; }
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetClip() == nullptr)
    { return; }
    auto Frame = FTransform::Identity;
    auto Scale = 1.0f;
    auto Endpoint = FVector::ZeroVector;
    auto Bend = FVector::ZeroVector;
    auto KeyTime = 0.0;
    if (NOT ResolveSelected(Frame, Scale, Endpoint, Bend, KeyTime))
    { _GizmoError = TEXT("Selected track changed during gizmo movement; no key was written."); RefreshPose(); return; }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->GetClip()};
    const auto Succeeded = _GizmoTarget == EGizmoTarget::Endpoint
        ? Commands.SetPositionKey(_SelectedPhase, _SelectedSlot, KeyTime,
            ck::procedural_action_editor::ToTrackPosition(Frame, Scale, _GizmoWorldPosition), _GizmoError)
        : Commands.SetBendKey(_SelectedPhase, _SelectedSlot, KeyTime,
            Frame.InverseTransformVectorNoScale(_GizmoWorldPosition - Endpoint).GetSafeNormal(), _GizmoError);
    if (Succeeded)
    { _OnChanged.ExecuteIfBound(); }
    RefreshPose();
}

auto
    SCkProceduralAction_AuthoredViewport::
    PickAtPixel(
        const FSceneView& InView,
        uint32 InX,
        uint32 InY)
    -> bool
{
    const auto Session = _Session.Pin();
    if (_Closed || _UseObservation || NOT Session.IsValid() || NOT Session->HasPose()
        || Session->GetPreviewConfig() == nullptr)
    { return false; }
    const auto& Phases = Session->GetCompiledClip().Get_Phases();
    if (NOT Phases.IsValidIndex(Session->GetPose().Get_PhaseIndex()))
    { return false; }
    auto BestSlot = FName{NAME_None};
    auto BestDistanceSquared = 24.0 * 24.0;
    for (const auto& Entry : _TerminalComponents)
    {
        if (Entry.Value == nullptr || NOT Entry.Value->IsVisible())
        { continue; }
        auto Pixel = FVector2D::ZeroVector;
        if (NOT InView.WorldToPixel(Entry.Value->GetComponentLocation(), Pixel))
        { continue; }
        const auto Delta = Pixel - FVector2D{static_cast<double>(InX), static_cast<double>(InY)};
        if (Delta.SizeSquared() >= BestDistanceSquared)
        { continue; }
        const auto* Binding = Session->GetPreviewConfig()->Get_SlotBindings().FindByPredicate(
            [&Entry](const auto& InBinding) { return InBinding.Get_LimbId() == Entry.Key; });
        if (Binding == nullptr)
        { continue; }
        BestDistanceSquared = Delta.SizeSquared();
        BestSlot = Binding->Get_Slot();
    }
    if (BestSlot.IsNone())
    { return false; }
    _OnTrackPicked.ExecuteIfBound(Phases[Session->GetPose().Get_PhaseIndex()].Get_Id(), BestSlot);
    return true;
}

auto
    SCkProceduralAction_AuthoredViewport::
    ClearComponents()
    -> void
{
    if (NOT _Scene.IsValid())
    { return; }
    if (_BodyComponent != nullptr)
    { _Scene->RemoveComponent(_BodyComponent); _BodyComponent = nullptr; }
    for (const auto& Entry : _TerminalComponents)
    {
        if (Entry.Value != nullptr)
        { _Scene->RemoveComponent(Entry.Value); }
    }
    _TerminalComponents.Reset();
}

auto
    SCkProceduralAction_AuthoredViewport::
    RefreshAppearance()
    -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Ck_ActionEditor_RefreshAppearance);
    if (_Closed)
    { return; }
    if (_LoadHandle.IsValid())
    { _LoadHandle->CancelHandle(); _LoadHandle.Reset(); }
    ClearComponents();
    _NeedsAutoFrame = false;
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetPreviewConfig() == nullptr)
    { Invalidate(); return; }
    const auto* Config = Session->GetPreviewConfig();
    auto AppearanceError = FString{};
    if (NOT Config->ValidateAppearance(AppearanceError))
    { Invalidate(); return; }
    auto Paths = TArray<FSoftObjectPath>{};
    if (NOT Config->Get_BodyPreviewMesh().IsNull())
    { Paths.AddUnique(Config->Get_BodyPreviewMesh().ToSoftObjectPath()); }
    for (const auto& Binding : Config->Get_LimbBindings())
    {
        if (NOT Binding.Get_PreviewMesh().IsNull())
        { Paths.AddUnique(Binding.Get_PreviewMesh().ToSoftObjectPath()); }
    }
    if (Paths.IsEmpty())
    { _NeedsAutoFrame = true; Invalidate(); return; }
    _LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        MoveTemp(Paths), FStreamableDelegate::CreateSP(this,
            &SCkProceduralAction_AuthoredViewport::OnAssetsLoaded));
}

auto
    SCkProceduralAction_AuthoredViewport::
    OnAssetsLoaded()
    -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Ck_ActionEditor_RebuildAppearance);
    if (_Closed)
    { return; }
    ClearComponents();
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->GetPreviewConfig() == nullptr || NOT _Scene.IsValid())
    { return; }
    const auto* Config = Session->GetPreviewConfig();
    auto AppearanceError = FString{};
    if (NOT Config->ValidateAppearance(AppearanceError))
    { return; }
    if (auto* Mesh = Config->Get_BodyPreviewMesh().Get())
    {
        _BodyComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
        _BodyComponent->SetStaticMesh(Mesh);
        _BodyComponent->SetMobility(EComponentMobility::Movable);
        _Scene->AddComponent(_BodyComponent, Config->Get_InitialBody());
    }
    for (const auto& Binding : Config->Get_LimbBindings())
    {
        if (Binding.Get_LimbId().IsNone() || _TerminalComponents.Contains(Binding.Get_LimbId()))
        { continue; }
        auto* Mesh = Binding.Get_PreviewMesh().Get();
        if (Mesh == nullptr)
        { continue; }
        auto* Component = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
        Component->SetStaticMesh(Mesh);
        Component->SetMobility(EComponentMobility::Movable);
        _Scene->AddComponent(Component, Binding.Get_TerminalFrame());
        _TerminalComponents.Add(Binding.Get_LimbId(), Component);
    }
    RefreshPose();
    _NeedsAutoFrame = true;
    TryFrameAppearance();
}

auto
    SCkProceduralAction_AuthoredViewport::
    TryFrameAppearance()
    -> void
{
    if (_Closed || NOT _Client.IsValid() || _Client->Viewport == nullptr)
    { return; }
    const auto Size = _Client->Viewport->GetSizeXY();
    if (Size.X < 200 || Size.Y < 150)
    { return; }
    auto Bounds = FBox{ForceInit};
    if (_BodyComponent != nullptr && _BodyComponent->IsVisible())
    { Bounds += _BodyComponent->Bounds.GetBox(); }
    for (const auto& Entry : _TerminalComponents)
    {
        if (Entry.Value != nullptr && Entry.Value->IsVisible())
        { Bounds += Entry.Value->Bounds.GetBox(); }
    }
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->GetPreviewConfig() : nullptr;
    auto SurfaceError = FString{};
    if (Config != nullptr && Config->ValidateSurface(SurfaceError))
    {
        if (Config->Get_HasStaticFloor())
        {
            const auto Extents = Config->Get_StaticFloorHalfExtents();
            Bounds += FBox{-Extents, Extents}.TransformBy(Config->Get_StaticFloorFrame());
        }
        for (const auto& Obstacle : Config->Get_StaticObstacles())
        {
            const auto Extents = Obstacle.Get_HalfExtents();
            Bounds += FBox{-Extents, Extents}.TransformBy(Obstacle.Get_Frame());
        }
    }
    if (NOT Bounds.IsValid && CanEditGizmo())
    {
        const auto Origin = GetGizmoLocation();
        Bounds = FBox{Origin - FVector{25.0}, Origin + FVector{25.0}};
    }
    if (NOT Bounds.IsValid)
    { return; }
    _Client->FocusViewportOnBox(Bounds.ExpandBy(30.0), true);
    UE_LOG(LogTemp, Display,
        TEXT("Procedural Action authored auto-frame: viewport=%d x %d bounds=%s camera=%s look-at=%s"),
        Size.X, Size.Y, *Bounds.ToString(), *_Client->GetViewLocation().ToString(),
        *_Client->GetLookAtLocation().ToString());
    _NeedsAutoFrame = false;
    Invalidate();
}

auto
    SCkProceduralAction_AuthoredViewport::
    RefreshPose()
    -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Ck_ActionEditor_RefreshPose);
    if (_Closed)
    { return; }
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->GetPreviewConfig() : nullptr;
    if (Config == nullptr)
    {
        if (_BodyComponent != nullptr)
        { _BodyComponent->SetVisibility(false); }
        for (const auto& Entry : _TerminalComponents)
        { if (Entry.Value != nullptr) { Entry.Value->SetVisibility(false); } }
        Invalidate();
        return;
    }
    if (_UseObservation)
    {
        const auto Live = _LiveSubject.IsSet() && _LiveSubject.GetValue().Get_HasSettledPose();
        const auto* Observed = Live ? &_LiveSubject.GetValue().Get_SettledPose()
            : _ObservedPose.IsSet() ? &_ObservedPose.GetValue() : nullptr;
        if (Observed == nullptr)
        {
            if (_BodyComponent != nullptr)
            { _BodyComponent->SetVisibility(false); }
            for (const auto& Entry : _TerminalComponents)
            { if (Entry.Value != nullptr) { Entry.Value->SetVisibility(false); } }
            Invalidate();
            return;
        }
        const auto& Pose = *Observed;
        if (_BodyComponent != nullptr)
        {
            _BodyComponent->SetVisibility(Pose.Get_HasBodyPresentation());
            if (Pose.Get_HasBodyPresentation())
            { _BodyComponent->SetWorldTransform(ck_procedural_action_viewport::WithAppearanceScale(
                Pose.Get_BodyPresentation(), Config->Get_BodyPreviewMeshScale())); }
        }
        for (const auto& Entry : _TerminalComponents)
        { if (Entry.Value != nullptr) { Entry.Value->SetVisibility(false); } }
        for (const auto& Limb : Pose.Get_Limbs())
        {
            auto** Component = _TerminalComponents.Find(Limb.Get_Id());
            if (Component != nullptr && *Component != nullptr)
            {
                (*Component)->SetVisibility(true);
                const auto* VisualBinding = Config->Get_LimbBindings().FindByPredicate([&Limb](const auto& InBinding)
                    { return InBinding.Get_LimbId() == Limb.Get_Id(); });
                if (VisualBinding != nullptr)
                {
                    (*Component)->SetWorldTransform(ck_procedural_action_viewport::WithAppearanceScale(
                        Limb.Get_AcceptedEndpoint(), VisualBinding->Get_PreviewMeshScale()));
                }
            }
        }
        Invalidate();
        return;
    }
    if (NOT Session.IsValid() || NOT Session->HasPose())
    {
        if (_BodyComponent != nullptr)
        { _BodyComponent->SetVisibility(false); }
        for (const auto& Entry : _TerminalComponents)
        { if (Entry.Value != nullptr) { Entry.Value->SetVisibility(false); } }
        Invalidate();
        return;
    }
    const auto& Pose = Session->GetPose();
    if (NOT Session->IsCompiled())
    { Invalidate(); return; }
    auto BodyWorld = FTransform::Identity;
    if (NOT ck::procedural_action_editor::ResolveBodyWorld(
        *Config, Session->GetCompiledClip(), Pose, BodyWorld))
    { Invalidate(); return; }
    if (_BodyComponent != nullptr)
    {
        _BodyComponent->SetVisibility(
            Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Standalone);
        _BodyComponent->SetWorldTransform(ck_procedural_action_viewport::WithAppearanceScale(
            BodyWorld, Config->Get_BodyPreviewMeshScale()));
    }
    for (const auto& Entry : _TerminalComponents)
    { if (Entry.Value != nullptr) { Entry.Value->SetVisibility(false); } }
    for (const auto& Limb : Pose.Get_Limbs())
    {
        const auto* Binding = Config->Get_SlotBindings().FindByPredicate([&Limb](const auto& InBinding)
            { return InBinding.Get_Slot() == Limb.Get_Slot(); });
        if (Binding == nullptr)
        { continue; }
        auto** Component = _TerminalComponents.Find(Binding->Get_LimbId());
        if (Component == nullptr || *Component == nullptr)
        { continue; }
        const auto* Slot = Session->GetCompiledClip().Get_Slots().FindByPredicate([&Limb](const auto& InSlot)
            { return InSlot.Get_Id() == Limb.Get_Slot(); });
        if (Slot == nullptr)
        { continue; }
        auto Frame = FTransform::Identity;
        auto PositionScale = 1.0f;
        if (NOT ck::procedural_action_editor::ResolveSlotFrame(
            *Config, *Slot, BodyWorld, Frame, PositionScale))
        { continue; }
        (*Component)->SetVisibility(true);
        const auto Rotation = Limb.Get_HasOrientation() ? Limb.Get_Orientation() : FQuat::Identity;
        const auto* VisualBinding = Config->Get_LimbBindings().FindByPredicate([&Binding](const auto& InBinding)
            { return InBinding.Get_LimbId() == Binding->Get_LimbId(); });
        if (VisualBinding == nullptr)
        { continue; }
        (*Component)->SetWorldTransform(ck_procedural_action_viewport::WithAppearanceScale(
            FTransform{Rotation, Limb.Get_Position() * PositionScale} * Frame,
            VisualBinding->Get_PreviewMeshScale()));
    }
    Invalidate();
}
