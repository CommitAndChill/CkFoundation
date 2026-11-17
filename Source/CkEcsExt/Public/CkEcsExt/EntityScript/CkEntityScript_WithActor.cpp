#include "CkEntityScript_WithActor.h"
#include "CkEntityScript_WithActor_Data.h"

#include "CkEcs/CkEcsLog.h"

#include <Engine/World.h>
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"
#include "CkEcs/Snapshot/CkSaveKey_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkEcsExt/EntityBridge/CkActorSpawnIntent_Fragment.h"
#include "CkLabel/CkLabel_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_EntityScript_WithActor_UE::
    Construct(
        FCk_Handle& InHandle,
        const FInstancedStruct& InSpawnParams)
    -> ECk_EntityScript_ConstructionFlow
{
    auto* BoundActor = UCk_Utils_EntityBridge_UE::TryGet_Actor(InHandle);

    CK_ENSURE_IF_NOT(ck::IsValid(BoundActor),
        TEXT("EntityScript_WithActor [{}] is constructing on Entity [{}] which is not linked to an Actor. "
             "Was it spawned through UCk_EntityBridge_ActorComponent_UE::Request_SpawnEntity?"),
        GetClass()->GetName(), InHandle)
    { return ECk_EntityScript_ConstructionFlow::Finished; }

    _OwningActor = BoundActor;

    if (ck::IsValid(_OwningActor->GetRootComponent()))
    {
        UCk_Utils_Transform_UE::Add(InHandle, _OwningActor->GetActorTransform(), Get_EffectiveReplication());
    }

    UCk_Utils_GameplayLabel_UE::Add(InHandle, {});
    UCk_Utils_Handle_UE::Set_DebugName(InHandle, *_OwningActor->GetName());

    // The two halves of one decision: who re-creates this actor after a load. A level-placed actor is re-created by
    // the LEVEL, so the save must rendezvous its state onto that fresh copy — stamping the respawn intent instead
    // would have the loader build a second actor beside the one the level already made (and the intent outranks the
    // key in capture's classification, so the two cannot both be stamped). A runtime-spawned actor has no such
    // re-creator, so the loader is the only thing that can bring it back and the respawn opt-in still governs.
    if (const auto PlacedIdentity = ck::save_key::Get_LevelPlacedIdentity(_OwningActor.Get());
        NOT PlacedIdentity.IsEmpty())
    {
        ck::save_key::Assign(InHandle, PlacedIdentity);
    }
    else if (Get_IsSnapshotRespawnable())
    {
        InHandle.AddOrGet<FFragment_ActorSpawnIntent>(
            FFragment_ActorSpawnIntent{_OwningActor->GetClass()->GetPathName()});
    }

    return Super::Construct(InHandle, InSpawnParams);
}

auto
    UCk_EntityScript_WithActor_UE::
    EndPlay()
    -> void
{
    Super::EndPlay();

    _OwningActor = nullptr;
}

auto
    UCk_EntityScript_WithActor_UE::
    DoGet_OwningActor() const
    -> AActor*
{
    return _OwningActor;
}

auto
    UCk_EntityScript_WithActor_UE::
    Get_EffectiveReplication() const
    -> ECk_Replication
{
    if (ck::IsValid(_AssociatedEntity) && UCk_Utils_Net_UE::Has(_AssociatedEntity))
    { return UCk_Utils_Net_UE::Get_EntityReplication(_AssociatedEntity); }

    return Super::Get_EffectiveReplication();
}

auto
    UCk_EntityScript_WithActor_UE::
    Get_IsActorBound() const
    -> bool
{
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
