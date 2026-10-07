#include "CkEntityScript_WithActor_Utils.h"

#include "CkEntityScript_WithActor.h"
#include "CkEntityScript_WithActor_Data.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Fragment.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <StructUtils/InstancedStruct.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_EntityScript_WithActor_UE::
    Request_SpawnEntityScript_OnActor(
        AActor* InActor,
        TSubclassOf<UCk_EntityScript_WithActor_UE> InEntityScriptClass)
    -> FCk_Handle_PendingEntityScript
{
    const auto ActorIsValid = ck::IsValid(InActor);
    CK_ENSURE_IF_NOT(ActorIsValid, TEXT("Request_SpawnEntityScript_OnActor called with an invalid Actor")) {}
    if (NOT ActorIsValid)
    { return {}; }
    const auto ActorIsLive = NOT InActor->IsActorBeingDestroyed();
    CK_ENSURE_IF_NOT(ActorIsLive, TEXT("Request_SpawnEntityScript_OnActor called on a retiring Actor")) {}
    if (NOT ActorIsLive)
    { return {}; }
    const auto ClassIsValid = ck::IsValid(InEntityScriptClass);
    CK_ENSURE_IF_NOT(ClassIsValid, TEXT("Request_SpawnEntityScript_OnActor called with an invalid EntityScript class")) {}
    if (NOT ClassIsValid)
    { return {}; }
    if (NOT InActor->HasAuthority())
    { return {}; }

    auto* World = InActor->GetWorld();
    const auto WorldIsValid = ck::IsValid(World);
    CK_ENSURE_IF_NOT(WorldIsValid, TEXT("WithActor admission has no live World")) {}
    if (NOT WorldIsValid)
    { return {}; }
    const auto WorldIsLive = NOT World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
    CK_ENSURE_IF_NOT(WorldIsLive, TEXT("WithActor admission World is retiring")) {}
    if (NOT WorldIsLive)
    { return {}; }
    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*World);
    const auto TransientIsValid = ck::IsValid(TransientEntity);
    CK_ENSURE_IF_NOT(TransientIsValid, TEXT("WithActor admission has no world transient entity")) {}
    if (NOT TransientIsValid)
    { return {}; }
    const auto TransientIsLive = NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
        TransientEntity, ECk_EntityLifetime_DestructionPhase::BeginDestroy);
    CK_ENSURE_IF_NOT(TransientIsLive, TEXT("WithActor admission transient entity is retiring")) {}
    if (NOT TransientIsLive)
    { return {}; }
    const auto TransientCanCreate = UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(TransientEntity);
    CK_ENSURE_IF_NOT(TransientCanCreate, TEXT("WithActor admission cannot allocate in a retiring World")) {}
    if (NOT TransientCanCreate)
    { return {}; }
    const auto HasWorldSettings = TransientEntity.Has<ck::FFragment_Net_Params>();
    CK_ENSURE_IF_NOT(HasWorldSettings, TEXT("WithActor admission has no world net settings")) {}
    if (NOT HasWorldSettings)
    { return {}; }
    const auto* ScriptDefault = InEntityScriptClass->GetDefaultObject<UCk_EntityScript_WithActor_UE>();
    const auto ScriptDefaultIsValid = ck::IsValid(ScriptDefault);
    CK_ENSURE_IF_NOT(ScriptDefaultIsValid, TEXT("WithActor admission has no valid script CDO")) {}
    if (NOT ScriptDefaultIsValid)
    { return {}; }
    const auto& WorldSettings = TransientEntity.Get<ck::FFragment_Net_Params>().Get_ConnectionSettings();
    const auto Replication = InActor->GetIsReplicated()
        ? ScriptDefault->Get_EffectiveReplication() : ECk_Replication::DoesNotReplicate;
    const auto ReplicationIsValid = Replication == ECk_Replication::Replicates ||
        Replication == ECk_Replication::DoesNotReplicate;
    CK_ENSURE_IF_NOT(ReplicationIsValid, TEXT("WithActor admission has invalid script replication policy")) {}
    if (NOT ReplicationIsValid)
    { return {}; }
    const auto SettingsAreUsable = Replication != ECk_Replication::Replicates ||
        WorldSettings.Get_NetMode() == ECk_Net_NetModeType::Client ||
        WorldSettings.Get_NetMode() == ECk_Net_NetModeType::Host ||
        WorldSettings.Get_NetMode() == ECk_Net_NetModeType::ClientAndHost;
    CK_ENSURE_IF_NOT(SettingsAreUsable, TEXT("WithActor admission has unusable replicated world settings")) {}
    if (NOT SettingsAreUsable)
    { return {}; }

    const auto SpawnParams = FInstancedStruct::Make<FCk_EntityScript_WithActor_SpawnParams>(InActor);
    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(TransientEntity);
    const auto NewEntityIsValid = ck::IsValid(NewEntity);
    CK_ENSURE_IF_NOT(NewEntityIsValid, TEXT("WithActor admission failed to allocate its owned target")) {}
    if (NOT NewEntityIsValid)
    { return {}; }
    // Keep runtime actor policy: local non-replicated actors on clients must not wait as replicated proxies.
    UCk_Utils_Net_UE::Add(NewEntity, FCk_Net_ConnectionSettings{
        Replication, WorldSettings.Get_NetMode(), WorldSettings.Get_NetRole()});
    return UCk_Utils_EntityScript_UE::Add(NewEntity, InEntityScriptClass, SpawnParams, nullptr, {}, true);

}

// --------------------------------------------------------------------------------------------------------------------
