#pragma once

#include "CkEntityBridge_Fragment_Data.h"
#include "CkCore/Macros/CkMacros.h"

#include <CoreMinimal.h>
#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkEntityBridge_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "AActor FCk_Handle"))
class CKECS_API UCk_Utils_EntityBridge_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_EntityBridge_UE);

public:
    static void
    Add(
        FCk_Handle& InHandle,
        AActor* InOwningActor);

    static void
    SetupActorEntityLink(
        FCk_Handle& InHandle,
        AActor* InActor);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Has Feature",
              meta = (ScriptName = "Has_EntityBridge"))
    static bool
    Has(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Ensure Has Feature",
              meta = (ScriptName = "Ensure_EntityBridge"))
    static bool
    Ensure(
        const FCk_Handle& InHandle);

public:
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName="[Ck][EntityBridge] Try Get Entity With OwningActor In Ownership Chain")
    static FCk_Handle
    TryGet_BridgedEntity_InOwnershipChain(
        const FCk_Handle& InHandle);

public:
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Entity To Actor",
              meta = (CompactNodeTitle="EntityToActor"))
    static AActor*
    Get_Actor(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Try Get Entity To Actor",
              meta = (CompactNodeTitle="TryEntityToActor"))
    static AActor*
    TryGet_Actor(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Entity To Actor (Recursive)",
              meta = (CompactNodeTitle="EntityToActor (Recursive)"))
    static AActor*
    TryGet_Actor_Recursive(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Basic Details (From Entity)",
              meta = (CompactNodeTitle="EntityBasicDetails"))
    static FCk_EntityBridge_BasicDetails
    Get_BasicDetails(
        const FCk_Handle& InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Basic Details (From Actor)",
              meta = (CompactNodeTitle="ActorBasicDetails"))
    static FCk_EntityBridge_BasicDetails
    Get_BasicDetails_FromActor(
        const AActor* InActor);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Actor To Entity",
              meta=(CompactNodeTitle="ActorToEntity"))
    static FCk_Handle
    Get_Entity(
        const AActor* InActor);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Try Get Actor To Entity",
              meta=(CompactNodeTitle="TryActorToEntity"))
    static FCk_Handle
    TryGet_Entity(
        const AActor* InActor);

    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][EntityBridge] Get Is Actor Ecs Ready",
              Category = "Ck|Utils|EntityBridge",
              meta = (DefaultToSelf = "InActor"))
    static bool
    Get_HasEntity(
        const AActor* InActor);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][EntityBridge] Promise/Future On Actor Ecs Ready",
              Category = "Ck|Utils|EntityBridge",
              meta = (DefaultToSelf = "InActor", Keywords = "bind, wait, when, ready, ecs, future, replicated"))
    static void
    Promise_OnActorEcsReady(
        AActor* InActor,
        const FCk_Delegate_EntityBridge_OnEntityReady& InDelegate,
        ECk_ActorEcsReady_Policy InPolicy = ECk_ActorEcsReady_Policy::ValuesReplicated);

    static void
    Promise_OnActorEcsReady(
        AActor* InActor,
        TFunction<void(AActor*, FCk_Handle)> InCallback,
        ECk_ActorEcsReady_Policy InPolicy = ECk_ActorEcsReady_Policy::ValuesReplicated);

    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][EntityBridge] OwningActorBasicDetails == OwningActorBasicDetails",
              Category = "Ck|Utils|EntityBridge",
              meta = (CompactNodeTitle = "==", KeyWords = "==,equal"))
    static bool
    IsEqual(
        const FCk_EntityBridge_BasicDetails& InBasicDetailsA,
        const FCk_EntityBridge_BasicDetails& InBasicDetailsB);

    UFUNCTION(BlueprintPure,
              DisplayName = "[Ck][EntityBridge] OwningActorBasicDetails != OwningActorBasicDetails",
              Category = "Ck|Utils|EntityBridge",
              meta = (CompactNodeTitle = "!=", KeyWords = "!=,not,equal"))
    static bool
    IsNotEqual(
        const FCk_EntityBridge_BasicDetails& InBasicDetailsA,
        const FCk_EntityBridge_BasicDetails& InBasicDetailsB);

private:
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Actor To Entity (From Self)",
              meta = (DefaultToSelf = "InActor", HidePin = "InActor", CompactNodeTitle="ActorToEntity_FromSelf"))
    static FCk_Handle
    Get_Entity_FromSelf(
        const AActor* InActor);

private:
    friend class UCk_EntityBridge_ActorComponent_UE;

    static auto
    DoGetOrAdd_EntityOwningActorComponent(
        AActor* InActor) -> UCk_EntityBridge_ActorComponent_UE*;

    static auto
    DoFlush_PendingEcsReady(
        UCk_EntityBridge_ActorComponent_UE* InComp,
        AActor* InActor,
        const FCk_Handle& InEntity) -> void;

    static auto
    DoFlush_PendingEcsReady_LinkEstablished(
        UCk_EntityBridge_ActorComponent_UE* InComp,
        AActor* InActor,
        const FCk_Handle& InEntity) -> void;

    static auto
    DoFlush_PendingEcsReady_ValuesReplicated(
        UCk_EntityBridge_ActorComponent_UE* InComp,
        AActor* InActor,
        const FCk_Handle& InEntity) -> void;

    static auto
    DoGet_ShouldDeferUntilReplicationComplete(
        const FCk_Handle& InEntity) -> bool;

    static auto
    DoBind_ReplicationCompleteTrampoline(
        UCk_EntityBridge_ActorComponent_UE* InComp,
        const FCk_Handle& InEntity) -> void;
};

// --------------------------------------------------------------------------------------------------------------------