#include "CkPixelSpriteEditor/CkPixelSpriteEditor_AssetDefinition.h"

#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkPixelSpriteEditor/CkPixelSpriteEditor_ViewerBridge.h"

#define LOCTEXT_NAMESPACE "CkPixelSpriteAssetDefinition"

auto
    UCk_PixelSpriteAssetDefinition_UE::
    GetAssetDisplayName() const
    -> FText
{
    return LOCTEXT("PixelSprite", "Pixel Sprite");
}

auto
    UCk_PixelSpriteAssetDefinition_UE::
    GetAssetClass() const
    -> TSoftClassPtr<UObject>
{
    return UCk_PixelSpriteAsset_UE::StaticClass();
}

auto
    UCk_PixelSpriteAssetDefinition_UE::
    GetAssetColor() const
    -> FLinearColor
{
    return FLinearColor{0.29f, 0.75f, 0.93f};
}

auto
    UCk_PixelSpriteAssetDefinition_UE::
    OpenAssets(const FAssetOpenArgs& InArgs) const
    -> EAssetCommandResult
{
    auto& OpenViewer = ck::pixel_sprite_editor::Get_OpenViewer();
    if (NOT OpenViewer.IsBound())
    {
        return EAssetCommandResult::Unhandled;
    }

    auto bOpenedAny = false;
    for (const auto& AssetData : InArgs.Assets)
    {
        auto Asset = TObjectPtr<UCk_PixelSpriteAsset_UE>{Cast<UCk_PixelSpriteAsset_UE>(AssetData.GetAsset())};
        if (Asset && OpenViewer.Execute(*Asset, InArgs))
        {
            bOpenedAny = true;
        }
    }
    return bOpenedAny ? EAssetCommandResult::Handled : EAssetCommandResult::Unhandled;
}

#undef LOCTEXT_NAMESPACE
