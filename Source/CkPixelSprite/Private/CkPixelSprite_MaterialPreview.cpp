#include "CkPixelSprite/CkPixelSprite_MaterialPreview.h"

#include "CkCore/Macros/CkMacros.h"
#include "Engine/Texture2D.h"

auto
    UCk_PixelSpriteMaterialPreview_UE::
    Request_Clear()
    -> void
{
    auto Targets = _BoundMaterials;
    for (const auto& Material : Materials)
    {
        Targets.AddUnique(Material);
    }
    for (const auto& Material : Targets)
    {
        if (Material)
        {
            Material->SetScalarParameterValue(TEXT("SpriteReady"), 0.0f);
        }
    }
    _BoundMaterials.Reset();
    _BoundTexture = nullptr;
}

auto
    UCk_PixelSpriteMaterialPreview_UE::
    Get_RequiresRefresh() const
    -> bool
{
    return NOT Player || NOT _BoundTexture || _BoundMaterials != Materials
        || Player->Get_CurrentTexture() != _BoundTexture;
}

auto
    UCk_PixelSpriteMaterialPreview_UE::
    Request_Refresh()
    -> bool
{
    if (NOT Player || Materials.IsEmpty())
    {
        Request_Clear();
        return false;
    }
    const auto Asset = Player->Get_SourceAsset();
    const auto Texture = Player->Get_CurrentTexture();
    if (NOT Asset || NOT Texture)
    {
        Request_Clear();
        return false;
    }
    const auto& Settings = Asset->Get_RenderSettings();
    const auto RawSource = Settings._Mode == ECk_PixelSprite_RenderMode::Nearest
        && Settings._Scale == 1 && NOT Settings._Outline
        && Settings._Background == FColor::Transparent && Settings._PaletteOverrides.IsEmpty();
    if (NOT RawSource || Texture->GetSizeX() != Asset->Get_Width()
        || Texture->GetSizeY() != Asset->Get_Height())
    {
        Request_Clear();
        return false;
    }
    for (const auto& Material : Materials)
    {
        if (NOT Material)
        {
            Request_Clear();
            return false;
        }
    }
    const auto Size = FLinearColor{float(Asset->Get_Width()), float(Asset->Get_Height()), 0, 0};
    if (_BoundMaterials != Materials)
    {
        Request_Clear();
    }
    for (const auto& Material : Materials)
    {
        Material->SetTextureParameterValue(TEXT("SpriteTexture"), Texture.Get());
        Material->SetVectorParameterValue(TEXT("CanvasSize"), Size);
        Material->SetScalarParameterValue(TEXT("SpriteReady"), 1.0f);
    }
    _BoundMaterials = Materials;
    _BoundTexture = Texture;
    return true;
}
