#pragma once

#include "CkEcs/EntityScript/CkEntityScript.h"
#include "CkEcs/EntityScript/CkGenericEntityScript.h"

#include "CkEntityScript_WithActor.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Blueprintable, BlueprintType)
class CKECSEXT_API UCk_EntityScript_WithActor_UE : public UCk_GenericEntityScript_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_EntityScript_WithActor_UE);

public:
    [[nodiscard]]
    auto
    Construct(
        FCk_Handle& InHandle,
        const FInstancedStruct& InSpawnParams) -> ECk_EntityScript_ConstructionFlow override;

    [[nodiscard]]
    auto
    Get_EffectiveReplication() const -> ECk_Replication override;

    [[nodiscard]]
    auto
    Get_IsActorBound() const -> bool override;

    auto
    EndPlay() -> void override;

protected:
    UFUNCTION(BlueprintPure,
        Category = "Ck|EntityScript|WithActor",
        DisplayName = "[Ck][EntityScript] Get Actor",
        meta = (CompactNodeTitle="Actor", HideSelfPin = true))
    AActor*
    DoGet_Actor() const;

private:
    UPROPERTY(Transient)
    TObjectPtr<AActor> _Actor;

public:
    CK_PROPERTY_GET(_Actor);
};

// --------------------------------------------------------------------------------------------------------------------
