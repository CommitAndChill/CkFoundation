#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkGameSettings/CkGameSettings_Common.h"
#include "CkGameSettings/Storage/CkGameSettings_StorageProvider.h"
#include "CkGameSettings/Subsystem/CkGameSettings_Subsystem.h"

#include <UObject/StrongObjectPtr.h>

#include "CkGameSettings_SpecSupport.generated.h"

class UGameInstance;
class UCk_GameSettings_TimeoutSpecFixture_UE;

// --------------------------------------------------------------------------------------------------------------------

/** Reflected change-delegate receiver for the registry spec — dynamic delegates can only bind UFUNCTIONs. */
UCLASS(NotBlueprintable)
class UCk_GameSettings_SpecListener_UE : public UObject
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameSettings_SpecListener_UE);

public:
    UFUNCTION()
    void
    OnSettingChanged(
        FName InKey,
        const FString& InNewValue)
    {
        ++_FireCount;
        _LastKey = InKey;
        _LastValue = InNewValue;
    }

private:
    int32 _FireCount = 0;
    FName _LastKey;
    FString _LastValue;

public:
    CK_PROPERTY_GET(_FireCount);
    CK_PROPERTY_GET(_LastKey);
    CK_PROPERTY_GET(_LastValue);
};

// --------------------------------------------------------------------------------------------------------------------

/** Deterministic provider failures injected at the real subsystem/provider boundary. */
UCLASS(NotBlueprintable)
class UCk_GameSettings_SpecStorageProvider_UE : public UCk_GameSettings_StorageProvider_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameSettings_SpecStorageProvider_UE);

    auto
    Get_StoredValues_Implementation(
        ECk_GameSettings_Scope InScope,
        int32 InUserId,
        TArray<FCk_GameSettings_StoredValue>& OutValues) const
        -> ECk_GameSettings_StorageReadResult override;

    auto
    Request_StoreValues_Implementation(
        const TArray<FCk_GameSettings_StoreValue>& InValues)
        -> bool override;

    auto
    Request_Flush_Implementation()
        -> bool override;

private:
    bool _RejectWrites = false;
    bool _RejectFlush = false;
    bool _RejectPlayerRead = false;
    TArray<FCk_GameSettings_StoreValue> _Values;
    int32 _BatchCount = 0;

public:
    CK_PROPERTY(_RejectWrites);
    CK_PROPERTY(_RejectFlush);
    CK_PROPERTY(_RejectPlayerRead);
    CK_PROPERTY(_Values);
    CK_PROPERTY_GET(_BatchCount);
};

// --------------------------------------------------------------------------------------------------------------------

struct FCk_GameSettings_SpecAccess
{
    CK_GENERATED_BODY(FCk_GameSettings_SpecAccess);

    static auto
    Request_SetProvider(
        UCk_GameSettings_Subsystem_UE* InSubsystem,
        UCk_GameSettings_StorageProvider_UE* InProvider)
        -> void;

private:
    friend class UCk_GameSettings_TimeoutSpecFixture_UE;

    static auto Make_TickingSubsystem(UGameInstance* InGameInstance) -> UCk_GameSettings_Subsystem_UE*;
    static auto Get_IsReleased(const UCk_GameSettings_Subsystem_UE& InSubsystem) -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable)
class UCk_GameSettings_SpecCallback_UE : public UObject
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameSettings_SpecCallback_UE);

    UFUNCTION()
    bool
    Get_ExternalBool_ForDelegate();

    UFUNCTION()
    void
    Request_SetBool(
        bool InValue);

    UFUNCTION()
    void
    Request_ApplyAndReenter(
        const FString& InValue);

private:
    UPROPERTY(Transient)
    TObjectPtr<UCk_GameSettings_Subsystem_UE> _Subsystem;
    bool _NestedAccepted = false;
    bool _Reenter = false;
    FString _ObservedB;
    bool _ObservedPending = false;
    bool _ObservedUnapplied = false;
    bool _ExternalBool = false;
    int32 _ApplyCount = 0;

public:
    CK_PROPERTY(_Subsystem);
    CK_PROPERTY(_Reenter);
    CK_PROPERTY_GET(_NestedAccepted);
    CK_PROPERTY_GET(_ObservedB);
    CK_PROPERTY_GET(_ObservedPending);
    CK_PROPERTY_GET(_ObservedUnapplied);
    CK_PROPERTY_GET(_ExternalBool);
    CK_PROPERTY_GET(_ApplyCount);
};

// --------------------------------------------------------------------------------------------------------------------

/** Owns a registry, in-memory provider and real ticker for the deferred-timeout AutoTest. */
UCLASS(NotBlueprintable, BlueprintType, Transient)
class UCk_GameSettings_TimeoutSpecFixture_UE : public UObject
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameSettings_TimeoutSpecFixture_UE);

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Tests|GameSettings", meta = (DevelopmentOnly))
    UCk_GameSettings_Subsystem_UE*
    Request_Start();

    UFUNCTION(BlueprintCallable, Category = "Ck|Tests|GameSettings", meta = (DevelopmentOnly))
    bool
    Request_Release();

    auto BeginDestroy() -> void override;

private:
    // Strong references keep the children alive through this object's BeginDestroy fallback.
    // Their outer is the engine/private GameInstance, never this fixture (which would root a cycle).
    TStrongObjectPtr<UGameInstance> _GameInstance;
    TStrongObjectPtr<UCk_GameSettings_Subsystem_UE> _Subsystem;
    bool _HasStarted = false;
};
