#include "CkHexGridObject_EntityScript.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Utils.h"
#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Fragment.h"
#include "CkResourceLoader/CkResourceLoader_Utils.h"

#include <Engine/World.h>
#include <TimerManager.h>

namespace ck
{
    // The target owns the root, including when its EntityScript is recycled or it retires during streaming.
    struct FFragment_HexGridObject_Loading
    {
        FCk_ResourceLoader_RootedAssetBatch Batch;
    };
}

namespace ck_hex_grid_object_authoring
{
    auto IsLive(const FCk_Handle& InHandle) -> bool
    {
        // The handle's default validity excludes Teardown and Destroyed. Also reject initiation and EndPlay.
        return ck::IsValid(InHandle) && !UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            InHandle, ECk_EntityLifetime_DestructionPhase::BeginDestroy);
    }
}

auto
    UCk_HexGridObject_EntityScript::
    Construct(FCk_Handle& InHandle, const FInstancedStruct& InSpawnParams)
    -> ECk_EntityScript_ConstructionFlow
{
    _ObjectFailure = ECk_HexGridObject_Failure::InvalidObject;
    if (!ck_hex_grid_object_authoring::IsLive(InHandle))
    { return ECk_EntityScript_ConstructionFlow::Continue; }
    if (_Spec.IsNull())
    {
        InHandle.AddOrGet<ck::FTag_HexGridObject_RecipeRejected>();
        return ECk_EntityScript_ConstructionFlow::Continue;
    }

    if (const auto* Asset = _Spec.Get(); IsValid(Asset))
    { return Compose(InHandle, Asset, InSpawnParams); }

    auto Batch = UCk_Utils_ResourceLoader_UE::RequestLoad_RootedBatch(
        TEXT("HexGridObject.Construct"), {_Spec.ToSoftObjectPath()});
    if (Batch.Get_IsReady())
    {
        return Compose(InHandle, Cast<UCk_HexGridObject_AuthoringSpec>(
            Batch.Get_ResolvedObject(_Spec.ToSoftObjectPath())), InSpawnParams);
    }

    InHandle.Add<ck::FFragment_HexGridObject_Loading>(MoveTemp(Batch));
    return ECk_EntityScript_ConstructionFlow::Continue;
}

auto
    UCk_HexGridObject_EntityScript::
    ContinueConstruction(FCk_Handle InHandle)
    -> void
{
    if (!ck_hex_grid_object_authoring::IsLive(InHandle) || Get_AssociatedEntity() != InHandle)
    { return; }
    if (InHandle.Has<ck::FFragment_HexGridObject_Loading>())
    { PollAsset(InHandle); }
    else if (_ObjectFailure == ECk_HexGridObject_Failure::None)
    { Super::ContinueConstruction(InHandle); }
    else
    { DoFinishConstruction(); }
}

auto
    UCk_HexGridObject_EntityScript::
    PollAsset(FCk_Handle InHandle)
    -> void
{
    if (!ck_hex_grid_object_authoring::IsLive(InHandle) || Get_AssociatedEntity() != InHandle
        || !InHandle.Has<ck::FFragment_HexGridObject_Loading>())
    { return; }

    const auto& Loading = InHandle.Get<ck::FFragment_HexGridObject_Loading>();
    if (!Loading.Batch.Get_IsReady())
    {
        if (auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle); IsValid(World))
        {
            World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
                this, &ThisType::PollAsset, InHandle));
            return;
        }
    }

    // Move the root out before entering Blueprint. Its callbacks may change the registry or retire this entity.
    auto Batch = MoveTemp(InHandle.Get<ck::FFragment_HexGridObject_Loading>().Batch);
    InHandle.Remove<ck::FFragment_HexGridObject_Loading>();
    // Spawn properties were injected by the spawner. GenericEntityScript::Construct only forwards InHandle to Blueprint.
    const auto Flow = Compose(InHandle, Cast<UCk_HexGridObject_AuthoringSpec>(
        Batch.Get_ResolvedObject(Batch.Get_RequestedPaths()[0])), FInstancedStruct{});
    if (!ck_hex_grid_object_authoring::IsLive(InHandle) || Get_AssociatedEntity() != InHandle)
    { return; }
    if (_ObjectFailure == ECk_HexGridObject_Failure::None && Flow == ECk_EntityScript_ConstructionFlow::Continue)
    { Super::ContinueConstruction(InHandle); }
    else
    { DoFinishConstruction(); }
}

auto
    UCk_HexGridObject_EntityScript::
    Compose(FCk_Handle& InHandle, const UCk_HexGridObject_AuthoringSpec* InAsset,
        const FInstancedStruct& InSpawnParams)
    -> ECk_EntityScript_ConstructionFlow
{
    auto Object = FCk_Handle_HexGridObject{};
    if (!IsValid(InAsset) ||
        !UCk_Utils_HexGridObject_UE::TryAddWithInitialization(InHandle, InAsset->Get_ObjectSpec(),
            _Initialization, Object, _ObjectFailure))
    {
        // The generic spawner has no failed flow. Finish privately in its continuation, without invoking user events.
        if (ck_hex_grid_object_authoring::IsLive(InHandle)) { InHandle.AddOrGet<ck::FTag_HexGridObject_RecipeRejected>(); }
        return ECk_EntityScript_ConstructionFlow::Continue;
    }
    return Super::Construct(InHandle, InSpawnParams);
}

auto
    UCk_HexGridObject_EntityScript::
    BeginPlay() -> void
{
    if (_ObjectFailure == ECk_HexGridObject_Failure::None)
    { Super::BeginPlay(); }
}

auto
    UCk_HexGridObject_EntityScript::
    EndPlay() -> void
{
    if (_ObjectFailure == ECk_HexGridObject_Failure::None)
    { Super::EndPlay(); }
}
