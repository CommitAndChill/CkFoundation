#include "CkHexGrid_EntityScript.h"

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkResourceLoader/CkResourceLoader_Utils.h"
#include <Engine/World.h>
#include <TimerManager.h>

namespace ck
{
    struct FFragment_HexGridRecipeLoading
    {
        FCk_ResourceLoader_RootedAssetBatch Batch;
    };
}

auto
    UCk_HexGrid_EntityScript::
    Construct(
        FCk_Handle& InHandle,
        const FInstancedStruct& InSpawnParams)
    -> ECk_EntityScript_ConstructionFlow
{
    _BoardFailure = ECk_HexBoardFailure::InvalidSpec;
    if (ck::Is_NOT_Valid(InHandle) || UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
        InHandle, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
    { return ECk_EntityScript_ConstructionFlow::Continue; }
    if (_Spec.IsNull() || _Spec.IsValid())
    { return FinishRecipe(InHandle, _Spec.Get(), InSpawnParams); }
    auto Batch = UCk_Utils_ResourceLoader_UE::RequestLoad_RootedBatch(TEXT("HexGrid.Construct"), {_Spec.ToSoftObjectPath()});
    if (Batch.Get_IsReady())
    {
        return FinishRecipe(InHandle, Cast<UCk_HexGrid_AuthoringSpec>(Batch.Get_ResolvedObject(_Spec.ToSoftObjectPath())),
            InSpawnParams);
    }
    InHandle.Add<ck::FFragment_HexGridRecipeLoading>(MoveTemp(Batch));
    return ECk_EntityScript_ConstructionFlow::Continue;
}

auto UCk_HexGrid_EntityScript::FinishRecipe(FCk_Handle& InHandle, UCk_HexGrid_AuthoringSpec* InAsset,
    const FInstancedStruct& InSpawnParams) -> ECk_EntityScript_ConstructionFlow
{
    auto Spec = FCk_HexGrid_Spec{};
    auto Recipe = FCk_HexGridRecipeIdentity{};
    if (IsValid(InAsset) && InAsset->TryResolveWithIdentity(Spec, Recipe, _BoardFailure))
    {
        auto Grid = FCk_Handle_HexGrid{};
        const auto Durable = !Recipe.Get_AssetPath().IsEmpty();
        UCk_Utils_HexGrid_UE::TryAddForAuthoring(InHandle, SpawnTransform, Spec, Grid, _BoardFailure,
            Durable ? &_Initialization : nullptr, Durable ? &Recipe : nullptr);
    }
    if (_BoardFailure != ECk_HexBoardFailure::None)
    {
        if (ck::IsValid(InHandle)) { InHandle.AddOrGet<ck::FTag_HexGrid_RecipeRejected>(); }
        // The spawner has no failed flow. Complete privately without exposing an absent feature to user events.
        return ECk_EntityScript_ConstructionFlow::Continue;
    }
    // Blueprint Construct can use the public feature immediately and must see its load disposition.
    return Super::Construct(InHandle, InSpawnParams);
}

void UCk_HexGrid_EntityScript::ContinueConstruction(FCk_Handle InHandle)
{
    if (ck::Is_NOT_Valid(InHandle) || InHandle != Get_AssociatedEntity() ||
        UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InHandle, ECk_EntityLifetime_DestructionPhase::BeginDestroy)) { return; }
    if (InHandle.Has<ck::FFragment_HexGridRecipeLoading>()) { PollRecipe(InHandle); }
    else if (_BoardFailure == ECk_HexBoardFailure::None) { Super::ContinueConstruction(InHandle); }
    else { DoFinishConstruction(); }
}

void UCk_HexGrid_EntityScript::PollRecipe(FCk_Handle InHandle)
{
    if (ck::Is_NOT_Valid(InHandle) || InHandle != Get_AssociatedEntity() ||
        UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InHandle, ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
        !InHandle.Has<ck::FFragment_HexGridRecipeLoading>()) { return; }
    auto& Loading = InHandle.Get<ck::FFragment_HexGridRecipeLoading>();
    if (!Loading.Batch.Get_IsReady())
    {
        if (auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle); IsValid(World))
        {
            World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisType::PollRecipe, InHandle));
            return;
        }
    }
    auto Batch = MoveTemp(Loading.Batch);
    InHandle.Remove<ck::FFragment_HexGridRecipeLoading>();
    const auto Flow = FinishRecipe(InHandle, Cast<UCk_HexGrid_AuthoringSpec>(
        Batch.Get_ResolvedObject(Batch.Get_RequestedPaths()[0])), {});
    if (ck::Is_NOT_Valid(InHandle) || InHandle != Get_AssociatedEntity() ||
        UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InHandle, ECk_EntityLifetime_DestructionPhase::BeginDestroy)) { return; }
    if (_BoardFailure == ECk_HexBoardFailure::None && Flow == ECk_EntityScript_ConstructionFlow::Continue) { Super::ContinueConstruction(InHandle); }
    else { DoFinishConstruction(); }
}

void UCk_HexGrid_EntityScript::BeginPlay()
{
    if (_BoardFailure == ECk_HexBoardFailure::None) { Super::BeginPlay(); }
}

void UCk_HexGrid_EntityScript::EndPlay()
{
    if (_BoardFailure == ECk_HexBoardFailure::None) { Super::EndPlay(); }
}
