#pragma once

#include "CkCore/Actor/CkActor.h"
#include "CkCore/Component/CkActorComponent.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/EntityScript/CkEntityScript_Fragment_Data.h"
#include "CkEcs/Handle/CkHandle.h"

#include <StructUtils/InstancedStruct.h>

#include "CkEntityBridge_Fragment_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKECS_API FCk_EntityBridge_BasicDetails
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_EntityBridge_BasicDetails);

public:
    FCk_EntityBridge_BasicDetails() = default;
    FCk_EntityBridge_BasicDetails(AActor* InActor, FCk_Handle InHandle);

public:
    auto operator==(const ThisType& InOther) const -> bool;
    CK_DECL_AND_DEF_OPERATOR_NOT_EQUAL(ThisType);

private:
    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TWeakObjectPtr<AActor> _Actor;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_Handle _Handle;

public:
    CK_PROPERTY_GET(_Actor)
    CK_PROPERTY_GET(_Handle)
};

auto CKECS_API GetTypeHash(const FCk_EntityBridge_BasicDetails& InBasicDetails) -> uint32;

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_EntityBridge_State : uint8
{
    // Request_SpawnEntity has not been called and no replica has arrived.
    NotRequested,

    // Client of a replicated Actor: the request was acknowledged and the authority's entity has not arrived yet.
    AwaitingReplica,

    // The Actor has an entity. Its script may still be constructing, beginning play, or (on a client)
    // waiting for replicated values.
    Linked,

    // The script has begun play and, on a client replica, replicated values have been applied.
    Ready,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_EntityBridge_State);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_EntityBridge_RequestTiming : uint8
{
    // The Actor calls Request_SpawnEntity from its BeginPlay. The bridge ensures on the next tick if it did not.
    DuringBeginPlay,

    // The Actor calls Request_SpawnEntity later, when it decides to. The bridge does not check.
    Deferred,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_EntityBridge_RequestTiming);

// --------------------------------------------------------------------------------------------------------------------

// Fired once per machine when the Actor's Entity is Ready: its script has begun play and, on a client
// replica, replicated values have been applied.
DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FCk_Delegate_EntityBridge_OnEntityReady,
    AActor*, InActor,
    FCk_Handle, InEntity);

// --------------------------------------------------------------------------------------------------------------------

// Controls when Promise_OnActorEcsReady fires.
UENUM(BlueprintType)
enum class ECk_ActorEcsReady_Policy : uint8
{
    // Fire once the Entity is Ready.
    ValuesReplicated,

    // Fire as soon as the Actor and Entity are linked, before the script's Construct runs.
    LinkEstablished,
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(BlueprintType, ClassGroup = "Ck",
       meta = (BlueprintSpawnableComponent, DisplayName = "Ck Entity Bridge"))
class CKECS_API UCk_EntityBridge_ActorComponent_UE
    : public UCk_ActorComponent_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_EntityBridge_ActorComponent_UE);

public:
    friend class UCk_Utils_EntityBridge_UE;
    friend class UCk_Utils_EntityScript_WithActor_UE;

public:
    UCk_EntityBridge_ActorComponent_UE();

protected:
    auto
    BeginPlay() -> void override;

    auto
    EndPlay(
        const EEndPlayReason::Type InEndPlayReason) -> void override;

public:
    /** Gives this Actor its Entity, built by InEntityScriptClass. Call once, from the Actor's BeginPlay,
     *  on every machine. On a client of a replicated Actor the call is acknowledged and the authority's
     *  Entity is awaited instead. The Entity arrives through Promise_OnEntityConstructed / Promise_OnEntityReady. */
    UFUNCTION(BlueprintCallable,
              Category = "Ck|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Request Spawn Entity")
    void
    Request_SpawnEntity(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass);

    /** Request_SpawnEntity with spawn params. On a client of a replicated Actor the params are discarded;
     *  the authority's params arrive with its Entity. */
    UFUNCTION(BlueprintCallable,
              Category = "Ck|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Request Spawn Entity (With Params)")
    void
    Request_SpawnEntity_WithParams(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        FInstancedStruct InSpawnParams);

    /** Fires once when the Entity's script has finished construction (the Entity's OnConstructed signal):
     *  the Entity is composed, replicated values may NOT be applied yet. Bind signals here. Fires
     *  immediately when bound after the fact. */
    UFUNCTION(BlueprintCallable,
              Category = "Ck|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Promise On Entity Constructed")
    void
    Promise_OnEntityConstructed(
        const FCk_Delegate_EntityScript_Constructed& InDelegate);

    /** Fires once when the Entity is Ready: its script has begun play and, on a client replica, replicated
     *  values have been applied. Read replicated state here. Fires immediately when bound after the fact. */
    UFUNCTION(BlueprintCallable,
              Category = "Ck|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Promise On Entity Ready")
    void
    Promise_OnEntityReady(
        const FCk_Delegate_EntityBridge_OnEntityReady& InDelegate);

    UFUNCTION(BlueprintPure,
              Category = "Ck|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get Entity")
    FCk_Handle
    Get_Entity() const;

    UFUNCTION(BlueprintPure,
              Category = "Ck|EntityBridge",
              DisplayName = "[Ck][EntityBridge] Get State")
    ECk_EntityBridge_State
    Get_State() const;

private:
    auto
    DoRequest_SpawnEntity(
        TSubclassOf<UCk_EntityScript_UE> InEntityScriptClass,
        const FInstancedStruct& InSpawnParams) -> FCk_Handle;

    UFUNCTION()
    void
    DoHandle_EntityReplicationComplete(
        FCk_Handle InEntity);

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
              Category = "Ck|EntityBridge",
              meta = (AllowPrivateAccess = true))
    ECk_EntityBridge_RequestTiming _RequestTiming = ECk_EntityBridge_RequestTiming::DuringBeginPlay;

private:
    FCk_Handle _Entity;
    ECk_EntityBridge_State _State = ECk_EntityBridge_State::NotRequested;
    TWeakObjectPtr<UClass> _RequestedEntityScriptClass;
    bool _HasRequested = false;
    bool _EntityHasBegunPlay = false;
    bool _ReplicationCompleteTrampolineBound = false;

    // Promises queued before their moment. Living on the component means they are discarded with the Actor.
    TArray<TFunction<void(AActor*, FCk_Handle)>> _PendingLinked_Callbacks;
    TArray<FCk_Delegate_EntityScript_Constructed> _PendingConstructed_Delegates;
    TArray<FCk_Delegate_EntityBridge_OnEntityReady> _PendingReady_Delegates;
    TArray<TFunction<void(AActor*, FCk_Handle)>> _PendingReady_Callbacks;

public:
    CK_PROPERTY(_RequestTiming);
};

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_CUSTOM_IS_VALID_INLINE(FCk_EntityBridge_BasicDetails, IsValid_Policy_Default,
[=](const FCk_EntityBridge_BasicDetails& InBasicDetails)
{
    return ck::IsValid(InBasicDetails.Get_Actor()) && ck::IsValid(InBasicDetails.Get_Handle());
});

CK_DEFINE_CUSTOM_FORMATTER_INLINE(FCk_EntityBridge_BasicDetails, [](const FCk_EntityBridge_BasicDetails& InObj)
{
    return ck::Format
    (
        TEXT("Actor: [{}], Handle: [{}]"),
        InObj.Get_Actor(),
        InObj.Get_Handle()
    );
});

// --------------------------------------------------------------------------------------------------------------------
