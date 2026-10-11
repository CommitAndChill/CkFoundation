#pragma once

#include "AssetDefinitionDefault.h"

#include "CkPixelSpriteEditor_AssetDefinition.generated.h"

UCLASS()
class CKPIXELSPRITEEDITOR_API UCk_PixelSpriteAssetDefinition_UE : public UAssetDefinitionDefault
{
    GENERATED_BODY()

public:
    virtual auto
    GetAssetDisplayName() const -> FText override;
    virtual auto
    GetAssetClass() const -> TSoftClassPtr<UObject> override;
    virtual auto
    GetAssetColor() const -> FLinearColor override;
    virtual auto
    OpenAssets(const FAssetOpenArgs& InArgs) const -> EAssetCommandResult override;
};
