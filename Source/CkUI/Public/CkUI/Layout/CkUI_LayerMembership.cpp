// Copyright 2025 CkFoundation. All Rights Reserved.

#include "CkUI/Layout/CkUI_LayerMembership.h"

#include "CkUI/Layout/CkUI_Layout_Subsystem.h"
#include "CkUI/Layout/CkUI_PrimaryGameLayout.h"
#include "CkUI/Layout/CkUI_LayerStack.h"
#include "CkCore/Validation/CkIsValid.h"

#include <CommonActivatableWidget.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Framework/Application/SlateApplication.h>
#include <GameFramework/PlayerController.h>
#include <UObject/StrongObjectPtr.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_ui_layer_membership
{
    auto Fail(TArray<FString>& OutErrors, const TCHAR* InMessage) -> bool
    {
        OutErrors.Add(InMessage);
        return false;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_UI_LayerMembership_UE::
    TryObserve(
        UCommonActivatableWidget* InOwner,
        FGameplayTag InLayerTag,
        TArray<FString>& OutErrors)
    -> bool
{
    using namespace ck_ui_layer_membership;
    OutErrors.Reset();
    if (NOT IsInGameThread() || NOT FSlateApplication::IsInitialized())
    { return Fail(OutErrors, TEXT("Layer membership requires the game thread and initialized Slate.")); }
    if (_Started)
    { return Fail(OutErrors, TEXT("Layer membership may observe one admission and cannot restart after cancellation.")); }
    if (ck::Is_NOT_Valid(InOwner) || NOT InLayerTag.IsValid())
    { return Fail(OutErrors, TEXT("Layer membership requires a valid owner and layer tag.")); }
    auto* Controller = InOwner->GetOwningPlayer();
    if (ck::Is_NOT_Valid(Controller) || NOT Controller->IsLocalController())
    { return Fail(OutErrors, TEXT("Layer membership requires the owner's local controller.")); }
    auto* Player = InOwner->GetOwningLocalPlayer();
    if (ck::Is_NOT_Valid(Player) || Controller->GetLocalPlayer() != Player)
    { return Fail(OutErrors, TEXT("Layer membership requires the owner's exact local player.")); }
    auto* World = InOwner->GetWorld();
    if (ck::Is_NOT_Valid(World) || World->bIsTearingDown || Controller->GetWorld() != World || Player->GetWorld() != World)
    { return Fail(OutErrors, TEXT("Layer membership requires a consistent live world.")); }
    if (Player->GetPlayerController(World) != Controller)
    { return Fail(OutErrors, TEXT("Layer membership requires the player's current controller.")); }
    auto* Subsystem = Player->GetSubsystem<UCk_UI_Layout_Subsystem_UE>();
    if (ck::Is_NOT_Valid(Subsystem))
    { return Fail(OutErrors, TEXT("Layer membership requires the player's layout subsystem.")); }
    auto* Layout = Subsystem->Get_Layout();
    if (ck::Is_NOT_Valid(Layout) || Layout->GetOwningLocalPlayer() != Player || Layout->GetWorld() != World)
    { return Fail(OutErrors, TEXT("Layer membership requires the player's current layout.")); }
    auto* Layer = Layout->Get_Layer(InLayerTag);
    if (ck::Is_NOT_Valid(Layer) || Layer->Get_LayerTag() != InLayerTag || Layer->GetWorld() != World)
    { return Fail(OutErrors, TEXT("Layer membership requires the exact configured layer.")); }
    const auto Generation = Layer->Get_WidgetAdmissionGeneration(InOwner);
    const auto OwnerRoot = InOwner->GetCachedWidget();
    const auto StackRoot = Layer->GetCachedWidget();
    if (Generation == 0 || NOT OwnerRoot.IsValid() || NOT StackRoot.IsValid())
    { return Fail(OutErrors, TEXT("Layer membership requires a realized widget already admitted to its stack.")); }

    _Owner = InOwner;
    _Controller = Controller;
    _Player = Player;
    _World = World;
    _Subsystem = Subsystem;
    _Layout = Layout;
    _Layer = Layer;
    _LayerTag = InLayerTag;
    _Generation = Generation;
    _OwnerRoot = OwnerRoot;
    _StackRoot = StackRoot;
    _Observing = true;
    _PoppedHandle = Layer->OnWidgetPopped.AddUObject(this, &ThisClass::HandleWidgetChanged);
    _ClearedHandle = Layer->OnAllWidgetsCleared.AddUObject(this, &ThisClass::HandleContextChanged);
    _DisplayedHandle = Layer->OnDisplayedWidgetChanged().AddUObject(this, &ThisClass::HandleWidgetChanged);
    _DestroyedHandle = Subsystem->OnLayoutDestroyed.AddUObject(this, &ThisClass::HandleContextChanged);
    _TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float InDeltaTime) { return DoTick(InDeltaTime); }));
    const auto Registered = _PoppedHandle.IsValid() && _ClearedHandle.IsValid()
        && _DisplayedHandle.IsValid() && _DestroyedHandle.IsValid() && _TickHandle.IsValid();
    if (NOT Registered || NOT Get_IsCurrent())
    {
        _Observing = false;
        DoUnbind();
        DoResetContext();
        if (_TickHandle.IsValid())
        { FTSTicker::RemoveTicker(_TickHandle); _TickHandle.Reset(); }
        return Fail(OutErrors, TEXT("Layer membership changed or could not register during observation."));
    }
    _Started = true;
    return true;
}

auto
    UCk_UI_LayerMembership_UE::
    Get_IsCurrent() const
    -> bool
{
    if (NOT IsInGameThread() || NOT FSlateApplication::IsInitialized() || NOT _Observing)
    { return false; }
    const auto* Owner = _Owner.Get();
    const auto* Player = _Player.Get();
    const auto* World = _World.Get();
    const auto* Subsystem = _Subsystem.Get();
    const auto* Layout = _Layout.Get();
    const auto* Layer = _Layer.Get();
    if (Owner == nullptr || Player == nullptr || World == nullptr || Subsystem == nullptr
        || Layout == nullptr || Layer == nullptr || World->bIsTearingDown)
    { return false; }
    const auto* Controller = _Controller.Get();
    if (ck::Is_NOT_Valid(Controller) || NOT Controller->IsLocalController()
        || Owner->GetOwningPlayer() != Controller || Player->GetPlayerController(World) != Controller
        || Controller->GetLocalPlayer() != Player || Controller->GetWorld() != World)
    { return false; }
    if (Owner->GetOwningLocalPlayer() != Player || Owner->GetWorld() != World || Player->GetWorld() != World
        || Player->GetSubsystem<UCk_UI_Layout_Subsystem_UE>() != Subsystem || Subsystem->Get_Layout() != Layout
        || Layout->GetOwningLocalPlayer() != Player || Layout->GetWorld() != World
        || Layout->Get_Layer(_LayerTag) != Layer || Layer->Get_LayerTag() != _LayerTag || Layer->GetWorld() != World)
    { return false; }
    const auto OwnerRoot = _OwnerRoot.Pin();
    const auto StackRoot = _StackRoot.Pin();
    return OwnerRoot.IsValid() && StackRoot.IsValid() && Owner->GetCachedWidget() == OwnerRoot
        && Layer->GetCachedWidget() == StackRoot && _Generation != 0
        && Layer->Get_WidgetAdmissionGeneration(_Owner.Get()) == _Generation;
}

auto
    UCk_UI_LayerMembership_UE::
    DoUnbind()
    -> void
{
    const auto Popped = MoveTemp(_PoppedHandle);
    const auto Cleared = MoveTemp(_ClearedHandle);
    const auto Displayed = MoveTemp(_DisplayedHandle);
    const auto Destroyed = MoveTemp(_DestroyedHandle);
    _PoppedHandle.Reset();
    _ClearedHandle.Reset();
    _DisplayedHandle.Reset();
    _DestroyedHandle.Reset();
    if (auto* Layer = _Layer.Get())
    {
        Layer->OnWidgetPopped.Remove(Popped);
        Layer->OnAllWidgetsCleared.Remove(Cleared);
        Layer->OnDisplayedWidgetChanged().Remove(Displayed);
    }
    if (auto* Subsystem = _Subsystem.Get())
    { Subsystem->OnLayoutDestroyed.Remove(Destroyed); }
}

auto
    UCk_UI_LayerMembership_UE::
    DoResetContext()
    -> void
{
    _Owner.Reset();
    _Controller.Reset();
    _Player.Reset();
    _World.Reset();
    _Subsystem.Reset();
    _Layout.Reset();
    _Layer.Reset();
    _OwnerRoot.Reset();
    _StackRoot.Reset();
    _LayerTag = {};
    _Generation = 0;
}

auto
    UCk_UI_LayerMembership_UE::
    DoReconcile()
    -> void
{
    if (NOT _Observing || Get_IsCurrent())
    { return; }
    _Observing = false;
    _RetirementPending = true;
    DoUnbind();
    DoResetContext();
}

auto
    UCk_UI_LayerMembership_UE::
    HandleWidgetChanged(
        UCommonActivatableWidget*)
    -> void
{ DoReconcile(); }

auto
    UCk_UI_LayerMembership_UE::
    HandleContextChanged()
    -> void
{ DoReconcile(); }

auto
    UCk_UI_LayerMembership_UE::
    DoTick(
        float)
    -> bool
{
    if (_Observing)
    {
        DoReconcile();
        return _Observing || _RetirementPending;
    }
    if (_RetirementPending)
    {
        _RetirementPending = false;
        _TickHandle.Reset();
        const TStrongObjectPtr<ThisClass> KeepAlive{this};
        OnRetired.Broadcast(this);
        return false;
    }
    _TickHandle.Reset();
    return false;
}

auto
    UCk_UI_LayerMembership_UE::
    Request_Stop()
    -> void
{
    if (NOT IsInGameThread())
    { return; }
    _Started = true;
    _Observing = false;
    _RetirementPending = false;
    DoUnbind();
    DoResetContext();
    const auto Tick = MoveTemp(_TickHandle);
    _TickHandle.Reset();
    if (Tick.IsValid())
    { FTSTicker::RemoveTicker(Tick); }
}

auto
    UCk_UI_LayerMembership_UE::
    BeginDestroy()
    -> void
{
    Request_Stop();
    Super::BeginDestroy();
}

// --------------------------------------------------------------------------------------------------------------------
