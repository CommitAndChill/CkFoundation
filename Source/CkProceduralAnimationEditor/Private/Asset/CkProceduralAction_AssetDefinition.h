#pragma once

#include "AssetDefinitionDefault.h"

#include "CkProceduralAction_AssetDefinition.generated.h"

UCLASS()
class UCk_ProceduralAction_AssetDefinition : public UAssetDefinitionDefault
{
    GENERATED_BODY()

public:
    auto
        GetAssetDisplayName() const
        -> FText override;
    auto
        GetAssetColor() const
        -> FLinearColor override;
    auto
        GetAssetClass() const
        -> TSoftClassPtr<UObject> override;
    auto
        GetAssetCategories() const
        -> TConstArrayView<FAssetCategoryPath> override;
    auto
        OpenAssets(
            const FAssetOpenArgs& InOpenArgs) const
        -> EAssetCommandResult override;
};
