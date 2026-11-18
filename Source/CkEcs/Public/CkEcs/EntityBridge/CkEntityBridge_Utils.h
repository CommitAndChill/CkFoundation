#pragma once

#include "CkEntityBridge_Fragment_Data.h"
#include "CkCore/Macros/CkMacros.h"

#include <CoreMinimal.h>
#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkEntityBridge_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class FProcessor_EntityScript_SpawnEntity_HandleRequests;
    class FProcessor_EntityScript_BeginPlay;
}

class UCk_Fragment_EntityReplicationDriver_Rep;
class UCk_Utils_EntityScript_WithActor_UE;

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "AActor FCk_Handle"))
class CKECS_API UCk_Utils_EntityBridge_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_EntityBridge_UE);

public:
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
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Try Get Bridge")
    static UCk_EntityBridge_ActorComponent_UE*
    TryGet_Bridge(
        const AActor* InActor);

    /** True once the Actor is linked to a valid Entity. The Entity may not be Ready yet. */
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Has Entity",
              meta = (DefaultToSelf = "InActor"))
    static bool
    Get_HasEntity(
        const AActor* InActor);

    /** True once the Actor's Entity is Ready (see UCk_EntityBridge_ActorComponent_UE::Promise_OnEntityReady). */
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Is Entity Ready",
              meta = (DefaultToSelf = "InActor"))
    static bool
    Get_IsEntityReady(
        const AActor* InActor);

    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Promise On Entity Constructed",
              meta = (DefaultToSelf = "InActor"))
    static void
    Promise_OnEntityConstructed(
        AActor* InActor,
        const FCk_Delegate_EntityScript_Constructed& InDelegate);

    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Promise On Entity Ready",
              meta = (DefaultToSelf = "InActor"))
    static void
    Promise_OnEntityReady(
        AActor* InActor,
        const FCk_Delegate_EntityBridge_OnEntityReady& InDelegate);

    UFUNCTION(BlueprintCallable,
              DisplayName = "[Ck][EntityBridge] Promise/Future On Actor Ecs Ready",
              Category = "Ck|Utils|EntityBridge",
              meta = (DefaultToSelf = "InActor", Keywords = "bind, wait, when, ready, ecs, future, replicated"))
    static void
    Promise_OnActorEcsReady(
        AActor* InActor,
        const FCk_Delegate_EntityBridge_OnEntityReady& InDelegate,
        ECk_ActorEcsReady_Policy InPolicy = ECk_ActorEcsReady_Policy::ValuesReplicated);

public:
    // C++ only.
    static auto
    Promise_OnEntityReady(
        AActor* InActor,
        TFunction<void(AActor*, FCk_Handle)> InCallback) -> void;

    // C++ only, framework-internal: fires the moment the Actor and Entity are linked, before the script's
    // Construct runs. For code that must stamp the Entity before anything can observe it.
    static auto
    Promise_OnEntityLinked(
        AActor* InActor,
        TFunction<void(AActor*, FCk_Handle)> InCallback) -> void;

    static void
    Promise_OnActorEcsReady(
        AActor* InActor,
        TFunction<void(AActor*, FCk_Handle)> InCallback,
        ECk_ActorEcsReady_Policy InPolicy = ECk_ActorEcsReady_Policy::ValuesReplicated);

public:
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
    friend class ck::FProcessor_EntityScript_SpawnEntity_HandleRequests;
    friend class ck::FProcessor_EntityScript_BeginPlay;
    friend class UCk_EntityBridge_ActorComponent_UE;
    friend class UCk_Fragment_EntityReplicationDriver_Rep;
    friend class UCk_Utils_EntityScript_WithActor_UE;

    static auto
    DoLink(
        FCk_Handle& InEntity,
        UCk_EntityBridge_ActorComponent_UE* InBridge) -> void;

    static auto
    DoNotify_EntityBegunPlay(
        const FCk_Handle& InEntity) -> void;

    static auto
    DoTryBecomeReady(
        UCk_EntityBridge_ActorComponent_UE* InBridge) -> void;

    static auto
    DoGet_ShouldDeferUntilReplicationComplete(
        const FCk_Handle& InEntity) -> bool;

    static auto
    DoBind_ReplicationCompleteTrampoline(
        UCk_EntityBridge_ActorComponent_UE* InBridge,
        const FCk_Handle& InEntity) -> void;

    // The bridge an older entry point adds for an Actor that predates it; requests are made by that entry point.
    static auto
    DoGetOrAdd_Bridge(
        AActor* InActor) -> UCk_EntityBridge_ActorComponent_UE*;
};

// --------------------------------------------------------------------------------------------------------------------