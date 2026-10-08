#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CkMessaging/CkMessaging_Fragment.h"
#include "CkMessaging/CkMessaging_Fragment_Data.h"

#include "CkEcs/Net/CkNet_Utils.h"

#include "CkRecord/Record/CkRecord_Utils.h"

#include "CkEcs/Signal/CkSignal_Fragment_Data.h"

#include "CkMessaging_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Meta = (ScriptMixin = "FCk_Handle"))
class CKMESSAGING_API UCk_Utils_Messaging_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_Messaging_UE);

private:
    struct RecordOfMessengers_Utils : public ck::TUtils_RecordOfEntities<ck::FFragment_RecordOfMessengers> {};

public:
    // Pure lookup of an already-bound message endpoint; never creates a messenger or record.
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|Messaging",
              DisplayName = "[Ck][Messaging] Try Get Messenger Entity")
    static FCk_Handle
    TryGet_MessengerEntity(
        const FCk_Handle& InOwner,
        FGameplayTag InMessageName);

public:
    // Read existing endpoints only. Total includes conditional delegates and native
    // listeners; the exact query proves an unconditional dynamic callback.
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|Messaging")
    static int32
    Get_NumBoundTo_OnBroadcast(
        const FCk_Handle& InOwner,
        FGameplayTag InMessageName,
        ECk_Signal_PostFireBehavior InPostFireBehavior);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|Messaging")
    static int32
    Get_NumUnconditionalBindingsTo_OnBroadcast(
        const FCk_Handle& InOwner,
        FGameplayTag InMessageName,
        const FCk_Delegate_Messaging_OnBroadcast& InDelegate,
        ECk_Signal_PostFireBehavior InPostFireBehavior);

public:
    // Hidden in the editor through the DefaultCkFoundation.ini Config file (see: BlueprintEditor.Menu section)
    UFUNCTION(BlueprintCallable)
    static void
    Broadcast(
        UPARAM(ref) FCk_Handle& InHandle,
        FGameplayTag InMessageName,
        FInstancedStruct InPayload);

public:
    // Hidden in the editor through the DefaultCkFoundation.ini Config file (see: BlueprintEditor.Menu section)
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Messaging",
              DisplayName="[Ck][Messaging] Bind To OnBroadcast")
    static void
    BindTo_OnBroadcast(
        UPARAM(ref) FCk_Handle& InHandle,
        UPARAM(meta = (Categories = "Message")) FGameplayTag InMessageName,
        const FCk_Delegate_Messaging_OnBroadcast& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing);

    // Hidden in the editor through the DefaultCkFoundation.ini Config file (see: BlueprintEditor.Menu section)
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Messaging",
              DisplayName="[Ck][Messaging] Unbind From OnBroadcast")
    static void
    UnbindFrom_OnBroadcast(
        UPARAM(ref) FCk_Handle& InHandle,
        UPARAM(meta = (Categories = "Message")) FGameplayTag InMessageName,
        const FCk_Delegate_Messaging_OnBroadcast& InDelegate);

private:
    static auto
    DoGet_MessengerEntity(
        FCk_Handle& InHandle,
        FGameplayTag InMessageName) -> FCk_Handle;
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKMESSAGING_API UCk_Utils_MessageListener_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_MessageListener_UE);

public:
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Messaging",
              DisplayName = "[Ck][Messaging] Stop Listening For Messages")
    static void
    Stop(
        UPARAM(ref) FCk_Handle_MessageListener& InMessageListener);
};

// --------------------------------------------------------------------------------------------------------------------
