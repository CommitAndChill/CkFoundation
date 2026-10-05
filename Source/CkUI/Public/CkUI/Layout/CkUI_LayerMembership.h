// Copyright 2025 CkFoundation. All Rights Reserved.

#pragma once

#include "CkCore/Macros/CkMacros.h"

#include <Containers/Ticker.h>
#include <GameplayTagContainer.h>
#include <UObject/Object.h>

#include "CkUI_LayerMembership.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class UCommonActivatableWidget;
class APlayerController;
class ULocalPlayer;
class UWorld;
class SWidget;
class UCk_UI_Layout_Subsystem_UE;
class UCk_UI_PrimaryGameLayout_UE;
class UCk_UI_LayerStack_UE;
class UCk_UI_LayerMembership_UE;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCk_Delegate_UI_LayerMembership_Retired, UCk_UI_LayerMembership_UE*, InMembership);

// --------------------------------------------------------------------------------------------------------------------

/** Observes one accepted player-layer admission without owning its widget, input or pause state. */
UCLASS(BlueprintType)
class CKUI_API UCk_UI_LayerMembership_UE : public UObject
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_UI_LayerMembership_UE);

    /** Call after pushing a realized widget. Rejection publishes no observation and may be retried. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|LayerMembership")
    bool
    TryObserve(
        UCommonActivatableWidget* InOwner,
        FGameplayTag InLayerTag,
        TArray<FString>& OutErrors);

    /** Pure live identity/membership query. Covered widgets remain current; no callbacks run here. */
    UFUNCTION(BlueprintPure, Category = "Ck|UI|LayerMembership")
    bool
    Get_IsCurrent() const;

    /** Terminal, idempotent cancellation. Also cancels a queued retirement notification. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|LayerMembership")
    void
    Request_Stop();

    /** At most once, from CoreTicker after invalidation/unsubscription. Retain this object to receive it. */
    UPROPERTY(BlueprintAssignable, Category = "Ck|UI|LayerMembership")
    FCk_Delegate_UI_LayerMembership_Retired OnRetired;

protected:
    auto BeginDestroy() -> void override;

private:
    auto DoReconcile() -> void;
    auto DoUnbind() -> void;
    auto DoResetContext() -> void;
    auto DoTick(float InDeltaTime) -> bool;
    auto HandleWidgetChanged(UCommonActivatableWidget* InWidget) -> void;
    auto HandleContextChanged() -> void;

private:
    bool _Started = false;
    bool _Observing = false;
    bool _RetirementPending = false;
    uint64 _Generation = 0;
    FGameplayTag _LayerTag;
    TWeakObjectPtr<UCommonActivatableWidget> _Owner;
    TWeakObjectPtr<APlayerController> _Controller;
    TWeakObjectPtr<ULocalPlayer> _Player;
    TWeakObjectPtr<UWorld> _World;
    TWeakObjectPtr<UCk_UI_Layout_Subsystem_UE> _Subsystem;
    TWeakObjectPtr<UCk_UI_PrimaryGameLayout_UE> _Layout;
    TWeakObjectPtr<UCk_UI_LayerStack_UE> _Layer;
    TWeakPtr<SWidget> _OwnerRoot;
    TWeakPtr<SWidget> _StackRoot;
    FDelegateHandle _PoppedHandle;
    FDelegateHandle _ClearedHandle;
    FDelegateHandle _DisplayedHandle;
    FDelegateHandle _DestroyedHandle;
    FTSTicker::FDelegateHandle _TickHandle;
};

// --------------------------------------------------------------------------------------------------------------------
