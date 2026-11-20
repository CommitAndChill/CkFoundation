#pragma once

#include "CkGrid/HexGridSystem/Authoring/CkHexGrid_Spec.h"
#include "CkGrid/HexGridSystem/Authoring/CkHexGridInitialization_Data.h"
#include "CkEcs/EntityScript/CkGenericEntityScript.h"

#include "CkHexGrid_EntityScript.generated.h"

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGrid_SpawnParams
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGrid_SpawnParams);

    FCk_HexGrid_SpawnParams() = default;
    FCk_HexGrid_SpawnParams(UCk_HexGrid_AuthoringSpec* InSpec, const FTransform& InSpawnTransform)
        : _Spec(InSpec), SpawnTransform(InSpawnTransform)
    {}

private:
    // Keep the reconstruction path in the saved spawn recipe even when the asset is unloaded.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UCk_HexGrid_AuthoringSpec> _Spec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FTransform SpawnTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Identity", meta = (AllowPrivateAccess = true))
    FCk_HexGridInitialization _Initialization;

public:
    CK_PROPERTY(_Spec);
    CK_PROPERTY(_Initialization);
};

UCLASS(Blueprintable, BlueprintType)
class CKGRID_API UCk_HexGrid_EntityScript : public UCk_GenericEntityScript_UE
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_HexGrid_EntityScript);

public:
    auto Get_ResolvedAuthoringSpec() const -> UCk_HexGrid_AuthoringSpec* { return _Spec.Get(); }

    UFUNCTION(BlueprintPure, Category = "Ck|HexGrid")
    ECk_HexBoardFailure Get_BoardFailure() const { return _BoardFailure; }

protected:
    auto Construct(FCk_Handle& InHandle, const FInstancedStruct& InSpawnParams)
        -> ECk_EntityScript_ConstructionFlow override;
    auto ContinueConstruction(FCk_Handle InHandle) -> void override;
    auto BeginPlay() -> void override;
    auto EndPlay() -> void override;

private:
    auto FinishRecipe(FCk_Handle& InHandle, UCk_HexGrid_AuthoringSpec* InAsset,
        const FInstancedStruct& InSpawnParams) -> ECk_EntityScript_ConstructionFlow;
    auto PollRecipe(FCk_Handle InHandle) -> void;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid",
        meta = (AllowPrivateAccess = true, ExposeOnSpawn = true))
    TSoftObjectPtr<UCk_HexGrid_AuthoringSpec> _Spec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid",
        meta = (AllowPrivateAccess = true, ExposeOnSpawn = true))
    FTransform SpawnTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Identity",
        meta = (AllowPrivateAccess = true, ExposeOnSpawn = true))
    FCk_HexGridInitialization _Initialization;

    ECk_HexBoardFailure _BoardFailure = ECk_HexBoardFailure::InvalidSpec;
};
