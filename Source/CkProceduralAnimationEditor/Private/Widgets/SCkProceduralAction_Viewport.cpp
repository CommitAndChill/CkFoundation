#include "Widgets/SCkProceduralAction_Viewport.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModes.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorData.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreviewSpace.h"
#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "AdvancedPreviewScene.h"
#include "Components/StaticMeshComponent.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "HitProxies.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneManagement.h"
#include "Tools/DefaultEdMode.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

struct HCkProceduralActionChannelProxy : public HHitProxy
{
    DECLARE_HIT_PROXY();

    explicit HCkProceduralActionChannelProxy(
        FName InChannel)
        : HHitProxy(HPP_UI)
        , Channel(InChannel)
    {}

    auto
        GetMouseCursor()
        -> EMouseCursor::Type override
    { return EMouseCursor::Crosshairs; }

    FName Channel;
};

IMPLEMENT_HIT_PROXY(HCkProceduralActionChannelProxy, HHitProxy);

namespace ck_procedural_action_viewport
{
    constexpr auto BendGuideCm = 40.0;

    auto
        WithScale(
            const FTransform& InPose,
            const FVector& InScale)
        -> FTransform
    { return FTransform{InPose.GetRotation(), InPose.GetTranslation(), InScale}; }

    auto
        DrawDashed(
            FPrimitiveDrawInterface* InPdi,
            const FVector& InFrom,
            const FVector& InTo,
            const FLinearColor& InColor)
        -> void
    {
        constexpr auto Dashes = 10;
        for (auto Dash = 0; Dash < Dashes; Dash += 2)
        {
            InPdi->DrawLine(FMath::Lerp(InFrom, InTo, static_cast<double>(Dash) / Dashes),
                FMath::Lerp(InFrom, InTo, static_cast<double>(Dash + 1) / Dashes), InColor, SDPG_World, 1.0f);
        }
    }

    auto
        SegmentLengths(
            const UCk_ProceduralActionPreview_Config& InConfig,
            FName InLimbId)
        -> const TArray<float>*
    {
        if (InConfig.Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
        {
            const auto* Rig = InConfig.Get_WalkingRig().Get();
            const auto* Leg = Rig == nullptr ? nullptr : Rig->Get_Legs().FindByPredicate([InLimbId](const auto& InLeg)
                { return InLeg.Get_Id() == InLimbId; });
            return Leg == nullptr ? nullptr : &Leg->Get_Chain().Get_SegmentLengths();
        }
        const auto* Layout = InConfig.Get_Layout().Get_Limbs().FindByPredicate([InLimbId](const auto& InLayout)
            { return InLayout.Get_Id() == InLimbId; });
        return Layout == nullptr ? nullptr : &Layout->Get_SegmentLengths();
    }

    auto
        BoundLimb(
            const UCk_ProceduralActionPreview_Config& InConfig,
            FName InSlot)
        -> FName
    {
        const auto* Binding = InConfig.Get_SlotBindings().FindByPredicate([InSlot](const auto& InBinding)
            { return InBinding.Get_Slot() == InSlot; });
        return Binding == nullptr ? NAME_None : Binding->Get_LimbId();
    }
}

// --------------------------------------------------------------------------------------------------------------------

class FCk_ProceduralActionViewportClient final : public FEditorViewportClient
{
public:
    FCk_ProceduralActionViewportClient(
        FPreviewScene* InScene,
        const TSharedRef<SCkProceduralAction_Viewport>& InWidget)
        : FEditorViewportClient(nullptr, InScene, InWidget)
        , _Widget(InWidget)
    {
        ModeTools->SetDefaultMode(FAssetEdModes::EM_AssetDefault);
        ModeTools->ActivateDefaultMode();
    }

    auto GetWidgetMode() const -> UE::Widget::EWidgetMode override
    {
        const auto View = _Widget.Pin();
        if (NOT View.IsValid() || NOT View->Get_CanEdit())
        { return UE::Widget::WM_None; }
        return View->_Gizmo == SCkProceduralAction_Viewport::EGizmo::Rotate ? UE::Widget::WM_Rotate : UE::Widget::WM_Translate;
    }

    auto SetWidgetMode(UE::Widget::EWidgetMode InMode) -> void override
    {
        if (const auto View = _Widget.Pin(); View.IsValid() && NOT View->_Drag.IsSet())
        {
            View->_Gizmo = InMode == UE::Widget::WM_Rotate ? SCkProceduralAction_Viewport::EGizmo::Rotate
                : InMode == UE::Widget::WM_Scale ? SCkProceduralAction_Viewport::EGizmo::Bend : SCkProceduralAction_Viewport::EGizmo::Move;
        }
        Invalidate();
    }

    auto CanSetWidgetMode(UE::Widget::EWidgetMode InMode) const -> bool override
    { return InMode == UE::Widget::WM_Translate || InMode == UE::Widget::WM_Rotate || InMode == UE::Widget::WM_Scale; }

    auto CanCycleWidgetMode() const -> bool override
    { return true; }

    auto GetWidgetLocation() const -> FVector override
    {
        const auto View = _Widget.Pin();
        return View.IsValid() ? View->Get_WidgetLocation() : FVector::ZeroVector;
    }

    auto GetWidgetCoordSystem() const -> FMatrix override
    { return FMatrix::Identity; }

    auto TrackingStarted(const FInputEventState& InInputState, bool InIsDraggingWidget, bool InNudge) -> void override
    {
        FEditorViewportClient::TrackingStarted(InInputState, InIsDraggingWidget, InNudge);
        if (const auto View = _Widget.Pin(); View.IsValid() && InIsDraggingWidget)
        { View->BeginDrag(); }
    }

    auto InputWidgetDelta(FViewport* InViewport, EAxisList::Type InAxis, FVector& InDrag, FRotator& InRotation, FVector& InScale) -> bool override
    {
        const auto View = _Widget.Pin();
        if (View.IsValid() && InAxis != EAxisList::None && View->_Drag.IsSet())
        {
            View->Drag(InDrag, InRotation);
            return true;
        }
        return FEditorViewportClient::InputWidgetDelta(InViewport, InAxis, InDrag, InRotation, InScale);
    }

    auto TrackingStopped() -> void override
    {
        if (const auto View = _Widget.Pin())
        { View->EndDrag(); }
        FEditorViewportClient::TrackingStopped();
    }

    auto Draw(const FSceneView* InView, FPrimitiveDrawInterface* InPdi) -> void override
    {
        FEditorViewportClient::Draw(InView, InPdi);
        if (const auto View = _Widget.Pin())
        { View->Draw(InPdi); }
    }

    auto ProcessClick(FSceneView& InView, HHitProxy* InHitProxy, FKey InKey, EInputEvent InEvent, uint32 InX, uint32 InY) -> void override
    {
        if (InHitProxy != nullptr && InHitProxy->IsA(HCkProceduralActionChannelProxy::StaticGetType()))
        {
            if (const auto View = _Widget.Pin())
            { View->Pick(static_cast<HCkProceduralActionChannelProxy*>(InHitProxy)->Channel); }
            return;
        }
        FEditorViewportClient::ProcessClick(InView, InHitProxy, InKey, InEvent, InX, InY);
    }

private:
    TWeakPtr<SCkProceduralAction_Viewport> _Widget;
};

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAction_Viewport::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Session = InArgs._Session;
    _Model = InArgs._Model;
    _Modes = InArgs._Modes;
    _Hud = InArgs._Hud;
    _Scene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues().SetCreatePhysicsScene(false));
    _Scene->SetFloorVisibility(false);
    SEditorViewport::Construct(SEditorViewport::FArguments());
    SetTag(FName{TEXT("CkActionEditor.Viewport")});
    RefreshAppearance();
}

SCkProceduralAction_Viewport::~SCkProceduralAction_Viewport()
{
    Shutdown();
    if (_Client.IsValid())
    { _Client->Viewport = nullptr; }
    Client.Reset();
    _Client.Reset();
    _Scene.Reset();
}

auto
    SCkProceduralAction_Viewport::
    MakeEditorViewportClient()
    -> TSharedRef<FEditorViewportClient>
{
    _Client = MakeShared<FCk_ProceduralActionViewportClient>(_Scene.Get(), SharedThis(this));
    _Client->SetViewMode(VMI_Lit);
    _Client->SetViewLocation(FVector{-300.0, -300.0, 180.0});
    _Client->SetViewRotation(FRotator{-12.0, 45.0, 0.0});
    _Client->SetRealtime(true);
    return _Client.ToSharedRef();
}

auto
    SCkProceduralAction_Viewport::
    PopulateViewportOverlays(
        TSharedRef<SOverlay> InOverlay)
    -> void
{
    SEditorViewport::PopulateViewportOverlays(InOverlay);
    const auto WeakViewport = TWeakPtr<SCkProceduralAction_Viewport>{SharedThis(this)};
    InOverlay->AddSlot().VAlign(VAlign_Top).HAlign(HAlign_Left).Padding(8.0f)
    [
        SNew(STextBlock)
        .Tag(FName{TEXT("CkActionEditor.Status")})
        .ShadowOffset(FVector2D{1.0, 1.0})
        .Text_Lambda([WeakViewport]() -> FText
        {
            const auto Viewport = WeakViewport.Pin();
            if (NOT Viewport.IsValid())
            { return FText::GetEmpty(); }
            const auto Hud = Viewport->_Hud.Get();
            return Viewport->_Error.IsEmpty() ? Hud
                : FText::FromString(FString::Printf(TEXT("%s\n%s"), *Hud.ToString(), *Viewport->_Error));
        })
    ];
}

auto
    SCkProceduralAction_Viewport::
    OnFocusViewportToSelection()
    -> void
{ FrameSubject(); }

auto
    SCkProceduralAction_Viewport::
    Shutdown()
    -> void
{
    if (_Closed)
    { return; }
    _Closed = true;
    if (_LoadHandle.IsValid())
    {
        _LoadHandle->CancelHandle();
        _LoadHandle.Reset();
    }
    ClearComponents();
    _LiveSubject.Reset();
    _Drag.Reset();
}

auto
    SCkProceduralAction_Viewport::
    Tick(
        const FGeometry& InGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SEditorViewport::Tick(InGeometry, InCurrentTime, InDeltaTime);
    if (_Closed)
    { return; }
    _LiveSubject.Reset();
    if (const auto Modes = _Modes.Pin(); Modes.IsValid() && Get_IsShowingSolver())
    { _LiveSubject = Modes->Get_DisplayedFrame()->Subject; }
    if (NOT _Drag.IsSet())
    { RefreshPose(); }
    if (_NeedsFrame)
    { FrameSubject(); }
}

auto
    SCkProceduralAction_Viewport::
    Get_IsShowingSolver() const
    -> bool
{
    const auto Modes = _Modes.Pin();
    return Modes.IsValid() && Modes->Get_DisplayedFrame() != nullptr;
}

auto
    SCkProceduralAction_Viewport::
    Resolve(
        FName InChannel,
        FResolved& OutResolved) const
    -> bool
{
    const auto Session = _Session.Pin();
    const auto Model = _Model.Pin();
    const auto* Config = Session.IsValid() ? Session->Get_PreviewConfig() : nullptr;
    const auto* Clip = Session.IsValid() ? Session->Get_Clip() : nullptr;
    auto Key = FCk_ProceduralActionKeyTime{};
    if (_Closed || Config == nullptr || Clip == nullptr || InChannel.IsNone() || NOT Model.IsValid()
        || NOT Session->ElapsedToKey(Model->Get_Time(), Key))
    { return false; }
    const auto* Phase = Clip->Get_Phases().FindByPredicate([&Key](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == Key.Get_PhaseId(); });
    if (Phase == nullptr)
    { return false; }
    const auto KeyTime = Key.Get_KeyTime().Get_Value();
    const auto KeyedHere = [KeyTime](const auto& InKey)
        { return ck::procedural_action_editor::IsSamePoseTime(InKey.Get_Time().Get_Value(), KeyTime); };
    auto Pose = ck::procedural_action::FEvaluatedPose{};
    const auto Evaluated = Session->EvaluateAt(FMath::Clamp(Model->Get_Time(), FCk_Time{}, Session->Get_Duration()), Pose);
    const auto IsWalking = Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking;
    auto BodyWorld = IsWalking ? Config->Get_RootFrame() : Config->Get_PresentationFrame();
    if (Evaluated && NOT ck::procedural_action_editor::ResolveBodyWorld(*Config, Session->Get_CompiledClip(), Pose, BodyWorld))
    { return false; }
    OutResolved = FResolved{};
    OutResolved.PhaseId = Key.Get_PhaseId();
    OutResolved.Channel = InChannel;
    OutResolved.KeyTime = KeyTime;
    if (InChannel == ck::procedural_action_editor::Get_BodyChannelName())
    {
        if (NOT Clip->Get_BodyChannel().Get_Enabled())
        { return false; }
        OutResolved.IsBody = true;
        OutResolved.Frame = Config->Get_RootFrame();
        const auto* BodyKey = Phase->Get_BodyKeys().FindByPredicate(KeyedHere);
        OutResolved.Body = Evaluated ? Pose.Get_Body()
            : BodyKey != nullptr ? FTransform{BodyKey->Get_Rotation(), BodyKey->Get_Translation()} : FTransform::Identity;
        OutResolved.Endpoint = BodyWorld.GetLocation();
        OutResolved.Orientation = BodyWorld.GetRotation();
        return NOT OutResolved.Endpoint.ContainsNaN();
    }
    const auto* Slot = Clip->Get_Slots().FindByPredicate([InChannel](const FCk_ProceduralAction_Slot& InSlot) { return InSlot.Get_Id() == InChannel; });
    if (Slot == nullptr)
    { return false; }
    const auto Compiled = ck::procedural_action::FCompiledSlot{Slot->Get_Id(), Slot->Get_InitialSource(), Slot->Get_Space()};
    if (NOT ck::procedural_action_editor::ResolveSlotFrame(*Config, Compiled, BodyWorld, OutResolved.Frame, OutResolved.Scale))
    { return false; }
    const auto* Limb = Evaluated ? Pose.Get_Limbs().FindByPredicate([InChannel](const auto& InLimb)
        { return InLimb.Get_Slot() == InChannel; }) : nullptr;
    const auto* Track = Phase->Get_LimbTracks().FindByPredicate([InChannel](const FCk_ProceduralAction_LimbTrack& InTrack)
        { return InTrack.Get_Slot() == InChannel; });
    if (Limb != nullptr)
    {
        OutResolved.Position = Limb->Get_Position();
        OutResolved.Orientation = Limb->Get_HasOrientation() ? Limb->Get_Orientation() : FQuat::Identity;
        OutResolved.Bend = Limb->Get_HasBend() ? Limb->Get_BendDirection() : FVector::UpVector;
    }
    else if (Track != nullptr)
    {
        if (const auto* Position = Track->Get_PositionKeys().FindByPredicate(KeyedHere))
        { OutResolved.Position = Position->Get_Value(); }
        if (const auto* Orientation = Track->Get_OrientationKeys().FindByPredicate(KeyedHere))
        { OutResolved.Orientation = Orientation->Get_Value(); }
        if (const auto* Bend = Track->Get_BendKeys().FindByPredicate(KeyedHere))
        { OutResolved.Bend = Bend->Get_Value(); }
    }
    OutResolved.Endpoint = ck::procedural_action_editor::ToWorldPosition(OutResolved.Frame, OutResolved.Scale, OutResolved.Position);
    return NOT OutResolved.Endpoint.ContainsNaN() && OutResolved.Bend.SizeSquared() > 1.0e-8;
}

auto
    SCkProceduralAction_Viewport::
    Get_CanEdit() const
    -> bool
{
    if (_Drag.IsSet())
    { return true; }
    const auto Model = _Model.Pin();
    auto Resolved = FResolved{};
    return Model.IsValid() && NOT Get_IsShowingSolver() && Resolve(Model->Get_SelectedChannel(), Resolved)
        && (_Gizmo != EGizmo::Bend || NOT Resolved.IsBody);
}

auto
    SCkProceduralAction_Viewport::
    Get_WidgetLocation() const
    -> FVector
{
    if (_Drag.IsSet())
    { return _DragLocation; }
    const auto Model = _Model.Pin();
    auto Resolved = FResolved{};
    if (NOT Model.IsValid() || NOT Resolve(Model->Get_SelectedChannel(), Resolved))
    { return FVector::ZeroVector; }
    return _Gizmo == EGizmo::Bend
        ? Resolved.Endpoint + Resolved.Frame.TransformVectorNoScale(Resolved.Bend).GetSafeNormal() * ck_procedural_action_viewport::BendGuideCm
        : Resolved.Endpoint;
}

auto
    SCkProceduralAction_Viewport::
    BeginDrag()
    -> void
{
    const auto Model = _Model.Pin();
    auto Resolved = FResolved{};
    if (NOT Get_CanEdit() || NOT Model.IsValid() || NOT Resolve(Model->Get_SelectedChannel(), Resolved))
    { return; }
    _DragLocation = Get_WidgetLocation();
    _DragRotation = FQuat::Identity;
    _Drag = Resolved;
    _Error.Reset();
}

auto
    SCkProceduralAction_Viewport::
    Drag(
        const FVector& InDrag,
        const FRotator& InRotation)
    -> void
{
    if (InDrag.ContainsNaN() || InRotation.ContainsNaN())
    { return; }
    _DragLocation += InDrag;
    _DragRotation = InRotation.Quaternion() * _DragRotation;
    if (_Drag->IsBody)
    {
        if (_BodyComponent != nullptr)
        {
            _BodyComponent->SetWorldLocation(_DragLocation);
            _BodyComponent->SetWorldRotation(_DragRotation * _Drag->Orientation);
        }
    }
    else if (_Gizmo == EGizmo::Move)
    {
        const auto Session = _Session.Pin();
        const auto Limb = Session.IsValid() && Session->Get_PreviewConfig() != nullptr
            ? ck_procedural_action_viewport::BoundLimb(*Session->Get_PreviewConfig(), _Drag->Channel) : NAME_None;
        if (auto** Component = _LimbComponents.Find(Limb); Component != nullptr && *Component != nullptr)
        { (*Component)->SetWorldLocation(_DragLocation); }
    }
    Invalidate();
}

auto
    SCkProceduralAction_Viewport::
    EndDrag()
    -> void
{
    if (NOT _Drag.IsSet())
    { return; }
    const auto Start = _Drag.GetValue();
    _Drag.Reset();
    const auto Session = _Session.Pin();
    const auto Model = _Model.Pin();
    const auto Moved = NOT _DragLocation.Equals(Get_WidgetLocation(), 1.0e-4) || NOT _DragRotation.Equals(FQuat::Identity, 1.0e-6);
    if (NOT Moved || NOT Session.IsValid() || NOT Model.IsValid() || Session->Get_Clip() == nullptr)
    { return; }
    auto Pose = FCk_ProceduralAction_PoseKey{FCk_FloatRange_0to1{Start.KeyTime}};
    const auto RootRotation = Start.Frame.GetRotation();
    if (Start.IsBody)
    {
        const auto Delta = _DragLocation - Start.Endpoint;
        auto Body = Start.Body;
        Body.AddToTranslation(Start.Frame.InverseTransformVectorNoScale(Delta));
        Body.SetRotation((RootRotation.Inverse() * _DragRotation * RootRotation * Body.GetRotation()).GetNormalized());
        Pose.Set_BodyMode(ECk_ProceduralAction_ValueMode::Set);
        Pose.Set_Body(Body);
    }
    else
    {
        auto Target = FCk_ProceduralAction_LimbPoseTarget{Start.Channel, Start.Position};
        switch (_Gizmo)
        {
            case EGizmo::Move:
                Target.Set_Position(ck::procedural_action_editor::ToTrackPosition(Start.Frame, Start.Scale, _DragLocation));
                break;
            case EGizmo::Rotate:
                Target.Set_OrientationMode(ECk_ProceduralAction_ValueMode::Set);
                Target.Set_Orientation((RootRotation.Inverse() * _DragRotation * RootRotation * Start.Orientation).GetNormalized());
                break;
            case EGizmo::Bend:
                Target.Set_BendMode(ECk_ProceduralAction_ValueMode::Set);
                Target.Set_Bend(Start.Frame.InverseTransformVectorNoScale(_DragLocation - Start.Endpoint).GetSafeNormal());
                break;
        }
        Pose.Set_Limbs({Target});
    }
    if (FCk_ProceduralActionAuthoringCommands{Session->Get_Clip()}.SetPoseKeys(Start.PhaseId, Pose, _Error))
    {
        Model->SelectKey(Start.PhaseId, Start.Channel, FCk_FloatRange_0to1{Start.KeyTime});
    }
}

auto
    SCkProceduralAction_Viewport::
    Pick(
        FName InChannel)
    -> void
{
    const auto Model = _Model.Pin();
    const auto Session = _Session.Pin();
    auto Key = FCk_ProceduralActionKeyTime{};
    if (NOT Model.IsValid() || NOT Session.IsValid())
    { return; }
    if (Session->ElapsedToKey(Model->Get_Time(), Key))
    { Model->SelectKey(Key.Get_PhaseId(), InChannel, Key.Get_KeyTime()); }
    else
    { Model->SelectChannel(InChannel); }
}

auto
    SCkProceduralAction_Viewport::
    ClearComponents()
    -> void
{
    if (NOT _Scene.IsValid())
    { return; }
    if (_BodyComponent != nullptr)
    {
        _Scene->RemoveComponent(_BodyComponent);
        _BodyComponent = nullptr;
    }
    for (const auto& Entry : _LimbComponents)
    {
        if (Entry.Value != nullptr)
        { _Scene->RemoveComponent(Entry.Value); }
    }
    _LimbComponents.Reset();
}

auto
    SCkProceduralAction_Viewport::
    RefreshAppearance()
    -> void
{
    if (_Closed)
    { return; }
    if (_LoadHandle.IsValid())
    {
        _LoadHandle->CancelHandle();
        _LoadHandle.Reset();
    }
    ClearComponents();
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->Get_PreviewConfig() : nullptr;
    auto Error = FString{};
    if (Config == nullptr || NOT Config->ValidateAppearance(Error))
    { return; }
    auto Paths = TArray<FSoftObjectPath>{};
    if (NOT Config->Get_BodyPreviewMesh().IsNull())
    { Paths.AddUnique(Config->Get_BodyPreviewMesh().ToSoftObjectPath()); }
    for (const auto& Binding : Config->Get_LimbBindings())
    {
        if (NOT Binding.Get_PreviewMesh().IsNull())
        { Paths.AddUnique(Binding.Get_PreviewMesh().ToSoftObjectPath()); }
    }
    _NeedsFrame = true;
    if (Paths.IsEmpty())
    { return; }
    _LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths),
        FStreamableDelegate::CreateSP(this, &SCkProceduralAction_Viewport::OnAssetsLoaded));
}

auto
    SCkProceduralAction_Viewport::
    OnAssetsLoaded()
    -> void
{
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->Get_PreviewConfig() : nullptr;
    if (_Closed || Config == nullptr || NOT _Scene.IsValid())
    { return; }
    ClearComponents();
    const auto MakeComponent = [this](UStaticMesh* InMesh, const FTransform& InTransform) -> UStaticMeshComponent*
    {
        auto* Component = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
        Component->SetStaticMesh(InMesh);
        Component->SetMobility(EComponentMobility::Movable);
        _Scene->AddComponent(Component, InTransform);
        return Component;
    };
    if (auto* Mesh = Config->Get_BodyPreviewMesh().Get())
    { _BodyComponent = MakeComponent(Mesh, Config->Get_InitialBody()); }
    for (const auto& Binding : Config->Get_LimbBindings())
    {
        auto* Mesh = Binding.Get_PreviewMesh().Get();
        if (Mesh != nullptr && NOT Binding.Get_LimbId().IsNone() && NOT _LimbComponents.Contains(Binding.Get_LimbId()))
        { _LimbComponents.Add(Binding.Get_LimbId(), MakeComponent(Mesh, Binding.Get_TerminalFrame())); }
    }
    _NeedsFrame = true;
    RefreshPose();
}

auto
    SCkProceduralAction_Viewport::
    RefreshPose()
    -> void
{
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->Get_PreviewConfig() : nullptr;
    if (_Closed || Config == nullptr)
    { return; }
    const auto LimbScale = [Config](FName InLimb) -> FVector
    {
        const auto* Binding = Config->Get_LimbBindings().FindByPredicate([InLimb](const auto& InBinding) { return InBinding.Get_LimbId() == InLimb; });
        return Binding != nullptr ? Binding->Get_PreviewMeshScale() : FVector::OneVector;
    };
    if (_LiveSubject.IsSet() && _LiveSubject->Get_HasSettledPose())
    {
        const auto& Pose = _LiveSubject->Get_SettledPose();
        if (_BodyComponent != nullptr && Pose.Get_HasBodyPresentation())
        { _BodyComponent->SetWorldTransform(ck_procedural_action_viewport::WithScale(Pose.Get_BodyPresentation(), Config->Get_BodyPreviewMeshScale())); }
        for (const auto& Limb : Pose.Get_Limbs())
        {
            if (auto** Component = _LimbComponents.Find(Limb.Get_Id()); Component != nullptr && *Component != nullptr)
            { (*Component)->SetWorldTransform(ck_procedural_action_viewport::WithScale(Limb.Get_AcceptedEndpoint(), LimbScale(Limb.Get_Id()))); }
        }
        return;
    }
    auto Body = FResolved{};
    if (_BodyComponent != nullptr && Resolve(ck::procedural_action_editor::Get_BodyChannelName(), Body))
    { _BodyComponent->SetWorldTransform(ck_procedural_action_viewport::WithScale(FTransform{Body.Orientation, Body.Endpoint}, Config->Get_BodyPreviewMeshScale())); }
    if (const auto* Clip = Session->Get_Clip())
    {
        for (const auto& Slot : Clip->Get_Slots())
        {
            const auto Limb = ck_procedural_action_viewport::BoundLimb(*Config, Slot.Get_Id());
            auto Resolved = FResolved{};
            auto** Component = _LimbComponents.Find(Limb);
            if (Component != nullptr && *Component != nullptr && Resolve(Slot.Get_Id(), Resolved))
            {
                (*Component)->SetWorldTransform(ck_procedural_action_viewport::WithScale(
                    FTransform{Resolved.Frame.GetRotation() * Resolved.Orientation, Resolved.Endpoint}, LimbScale(Limb)));
            }
        }
    }
}

auto
    SCkProceduralAction_Viewport::
    Draw(
        FPrimitiveDrawInterface* InPdi) const
    -> void
{
    using namespace ck_procedural_action_viewport;
    const auto Session = _Session.Pin();
    const auto Model = _Model.Pin();
    const auto* Config = Session.IsValid() ? Session->Get_PreviewConfig() : nullptr;
    if (_Closed || InPdi == nullptr || Config == nullptr || NOT Model.IsValid())
    { return; }
    auto SurfaceError = FString{};
    if (Config->ValidateSurface(SurfaceError))
    {
        if (Config->Get_HasStaticFloor())
        {
            const auto Extents = Config->Get_StaticFloorHalfExtents();
            DrawWireBox(InPdi, Config->Get_StaticFloorFrame().ToMatrixWithScale(), FBox{-Extents, Extents}, CkStyle::Border(), SDPG_World, 1.0f);
        }
        for (const auto& Obstacle : Config->Get_StaticObstacles())
        {
            const auto Extents = Obstacle.Get_HalfExtents();
            DrawWireBox(InPdi, Obstacle.Get_Frame().ToMatrixWithScale(), FBox{-Extents, Extents}, CkStyle::Warn(), SDPG_World, 2.0f);
        }
    }
    for (const auto& Binding : Config->Get_LimbBindings())
    {
        auto Previous = TOptional<FVector>{};
        for (const auto& Frame : Binding.Get_SegmentFrames())
        {
            if (Previous.IsSet())
            { DrawDashed(InPdi, Previous.GetValue(), Frame.GetLocation(), CkStyle::TextMute()); }
            Previous = Frame.GetLocation();
        }
        if (Previous.IsSet())
        { DrawDashed(InPdi, Previous.GetValue(), Binding.Get_TerminalFrame().GetLocation(), CkStyle::TextMute()); }
    }
    if (_LiveSubject.IsSet() && _LiveSubject->Get_HasSettledPose())
    {
        for (const auto& Limb : _LiveSubject->Get_SettledPose().Get_Limbs())
        {
            const auto* Lengths = SegmentLengths(*Config, Limb.Get_Id());
            const auto& Segments = Limb.Get_Segments();
            for (auto Index = 0; Lengths != nullptr && Index < FMath::Min(Lengths->Num(), Segments.Num()); ++Index)
            {
                const auto Half = Segments[Index].GetRotation().GetAxisX() * ((*Lengths)[Index] * 0.5);
                InPdi->DrawLine(Segments[Index].GetLocation() - Half, Segments[Index].GetLocation() + Half, CkStyle::Info(), SDPG_World, 3.0f);
            }
        }
        return;
    }
    const auto Selected = Model->Get_SelectedChannel();
    const auto DrawHandle = [InPdi, Selected](FName InChannel, const FVector& InLocation)
    {
        InPdi->SetHitProxy(new HCkProceduralActionChannelProxy(InChannel));
        InPdi->DrawPoint(InLocation, InChannel == Selected ? CkStyle::Selection() : CkStyle::Accent(), 12.0f, SDPG_Foreground);
        InPdi->SetHitProxy(nullptr);
    };
    auto Body = FResolved{};
    if (Resolve(ck::procedural_action_editor::Get_BodyChannelName(), Body))
    { DrawHandle(Body.Channel, Body.Endpoint); }
    const auto* Clip = Session->Get_Clip();
    if (Clip == nullptr)
    { return; }
    for (const auto& Slot : Clip->Get_Slots())
    {
        auto Resolved = FResolved{};
        if (NOT Resolve(Slot.Get_Id(), Resolved))
        { continue; }
        const auto Limb = BoundLimb(*Config, Slot.Get_Id());
        if (const auto* Binding = Config->Get_LimbBindings().FindByPredicate([Limb](const auto& InBinding) { return InBinding.Get_LimbId() == Limb; });
            Binding != nullptr && NOT Binding->Get_SegmentFrames().IsEmpty())
        {
            const auto Hip = Binding->Get_SegmentFrames()[0].GetLocation()
                + (Body.IsBody ? Body.Endpoint - Config->Get_PresentationFrame().GetLocation() : FVector::ZeroVector);
            InPdi->DrawLine(Hip, Resolved.Endpoint, CkStyle::Accent(), SDPG_World, 2.0f);
        }
        DrawHandle(Slot.Get_Id(), Resolved.Endpoint);
        if (Slot.Get_Id() == Selected && _Gizmo == EGizmo::Bend)
        {
            InPdi->DrawLine(Resolved.Endpoint, Resolved.Endpoint + Resolved.Frame.TransformVectorNoScale(Resolved.Bend).GetSafeNormal() * BendGuideCm,
                CkStyle::Warn(), SDPG_Foreground, 1.5f);
        }
    }
}

auto
    SCkProceduralAction_Viewport::
    FrameSubject()
    -> void
{
    if (_Closed || NOT _Client.IsValid() || _Client->Viewport == nullptr)
    { return; }
    const auto Size = _Client->Viewport->GetSizeXY();
    if (Size.X < 64 || Size.Y < 64)
    { return; }
    auto Bounds = FBox{ForceInit};
    if (_BodyComponent != nullptr)
    { Bounds += _BodyComponent->Bounds.GetBox(); }
    for (const auto& Entry : _LimbComponents)
    {
        if (Entry.Value != nullptr)
        { Bounds += Entry.Value->Bounds.GetBox(); }
    }
    if (const auto Session = _Session.Pin(); Session.IsValid() && Session->Get_PreviewConfig() != nullptr)
    {
        for (const auto& Binding : Session->Get_PreviewConfig()->Get_LimbBindings())
        {
            for (const auto& Frame : Binding.Get_SegmentFrames())
            { Bounds += Frame.GetLocation(); }
            Bounds += Binding.Get_TerminalFrame().GetLocation();
        }
        Bounds += Session->Get_PreviewConfig()->Get_PresentationFrame().GetLocation();
    }
    if (NOT Bounds.IsValid)
    { return; }
    _Client->FocusViewportOnBox(Bounds.ExpandBy(30.0), true);
    _NeedsFrame = false;
}

// --------------------------------------------------------------------------------------------------------------------
