#pragma once

#include "CkEcs/EntityScript/CkGenericEntityScript.h"
#include "CkGrid/HexGridSystem/Authoring/CkHexGridObject_Spec.h"
#include "CkGrid/HexGridSystem/Authoring/CkHexGridInitialization_Data.h"

#include "CkHexGridObject_EntityScript.generated.h"

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridObject_SpawnParams
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridObject_SpawnParams);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UCk_HexGridObject_AuthoringSpec> _Spec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_HexGridInitialization _Initialization;

public:
    CK_PROPERTY(_Spec);
    CK_PROPERTY(_Initialization);
    CK_DEFINE_CONSTRUCTORS(FCk_HexGridObject_SpawnParams, _Spec);
};

UCLASS(Blueprintable, BlueprintType)
class CKGRID_API UCk_HexGridObject_EntityScript : public UCk_GenericEntityScript_UE
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_HexGridObject_EntityScript);

public:
    UFUNCTION(BlueprintPure, Category = "Ck|HexGridObject")
    ECk_HexGridObject_Failure Get_ObjectFailure() const { return _ObjectFailure; }

protected:
    auto Construct(FCk_Handle& InHandle, const FInstancedStruct& InSpawnParams)
        -> ECk_EntityScript_ConstructionFlow override;
    auto ContinueConstruction(FCk_Handle InHandle) -> void override;
    auto BeginPlay() -> void override;
    auto EndPlay() -> void override;

private:
    auto PollAsset(FCk_Handle InHandle) -> void;
    auto Compose(FCk_Handle& InHandle, const UCk_HexGridObject_AuthoringSpec* InAsset,
        const FInstancedStruct& InSpawnParams) -> ECk_EntityScript_ConstructionFlow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid Object",
        meta = (AllowPrivateAccess = true, ExposeOnSpawn = true))
    TSoftObjectPtr<UCk_HexGridObject_AuthoringSpec> _Spec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid Object",
        meta = (AllowPrivateAccess = true, ExposeOnSpawn = true))
    FCk_HexGridInitialization _Initialization;

    ECk_HexGridObject_Failure _ObjectFailure = ECk_HexGridObject_Failure::InvalidObject;
};
