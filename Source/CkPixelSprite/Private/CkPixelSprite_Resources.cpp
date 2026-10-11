#include "CkPixelSprite/CkPixelSprite_Resources.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkPixelSprite/CkPixelSprite_Render.h"
#include "Engine/Texture2D.h"
#include "RHIGlobals.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace ck_pixel_sprite_resources
{
    constexpr auto MaxGenerationBytes = int64{64} * 1024 * 1024;
    constexpr auto MaxOverlapBytes = int64{128} * 1024 * 1024;

    auto
        Fail(
            FCk_PixelSprite_Diagnostic& OutDiagnostic,
            const FString& InCode,
            const FString& InMessage)
        -> bool
    {
        OutDiagnostic = {TEXT("PixelSpriteResources"), InCode, InMessage};
        return false;
    }

    auto
        SameSettings(
            const FCk_PixelSprite_RenderSettings& InLeft,
            const FCk_PixelSprite_RenderSettings& InRight)
        -> bool
    {
        if (InLeft._Mode != InRight._Mode || InLeft._Scale != InRight._Scale ||
            InLeft._Outline != InRight._Outline ||
            InLeft._OutlineColor != InRight._OutlineColor ||
            InLeft._Background != InRight._Background ||
            InLeft._PaletteOverrides.Num() != InRight._PaletteOverrides.Num())
        {
            return false;
        }
        for (const auto& Override : InLeft._PaletteOverrides)
        {
            if (NOT InRight._PaletteOverrides.Contains(Override.Key) ||
                InRight._PaletteOverrides.FindChecked(Override.Key) != Override.Value)
            {
                return false;
            }
        }
        return true;
    }
}

auto
    UCk_PixelSpriteResources_UE::TryBuild(
        TObjectPtr<UCk_PixelSpriteAsset_UE> InAsset,
        const FCk_PixelSprite_RenderSettings& InSettings,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    if (NOT IsInGameThread())
    {
        return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("thread"), TEXT("Texture generation requires the game thread"));
    }
    if (NOT IsValid(InAsset.Get()))
    {
        return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("asset"), TEXT("Pixel sprite asset is invalid"));
    }
    const auto Snapshot = InAsset->Get_Snapshot();
    if (NOT Snapshot.IsValid() || Snapshot->_Sources.IsEmpty())
    {
        return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("asset"), TEXT("Pixel sprite asset has no valid source snapshot"));
    }
    const auto Revision = InAsset->Get_Revision();
    if (_HasGeneration && _Asset == InAsset && _Revision == Revision &&
        _Snapshot == Snapshot && ck_pixel_sprite_resources::SameSettings(_Settings, InSettings))
    {
        OutDiagnostic = {};
        return true;
    }

    const auto MaxDimension = GUsingNullRHI ? uint32{4096} : GetMax2DTextureDimension();
    if (MaxDimension == 0)
    {
        return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("rhi"), TEXT("RHI texture limit is unavailable"));
    }
    if (InSettings._Scale < 1 || InSettings._Scale > 8 ||
        (InSettings._Mode != ECk_PixelSprite_RenderMode::Nearest &&
         InSettings._Mode != ECk_PixelSprite_RenderMode::Bilinear &&
         InSettings._Mode != ECk_PixelSprite_RenderMode::SmoothSilhouette))
    {
        return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("settings"), TEXT("Rendering settings are invalid"));
    }

    auto GenerationBytes = int64{0};
    for (const auto& Source : Snapshot->_Sources)
    {
        const auto Width = int64{Source._Grid._Width} * InSettings._Scale;
        const auto Height = int64{Source._Grid._Height} * InSettings._Scale;
        if (Width <= 0 || Height <= 0 || Width > MaxDimension || Height > MaxDimension)
        {
            return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("dimensions"), TEXT("Rendered source exceeds active RHI texture dimensions"));
        }
        const auto SourceBytes = Width * Height * int64{sizeof(FColor)};
        if (SourceBytes > ck_pixel_sprite_resources::MaxGenerationBytes - GenerationBytes)
        {
            return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("budget"), TEXT("Texture generation exceeds 64 MiB"));
        }
        GenerationBytes += SourceBytes;
    }
    if (GenerationBytes > ck_pixel_sprite_resources::MaxOverlapBytes - _GenerationBytes)
    {
        return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("budget"), TEXT("Old and new texture generations exceed 128 MiB"));
    }
    if (Snapshot->_Sources.Num() > MAX_int32 - _UploadCount)
    {
        return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("budget"), TEXT("Upload counter is exhausted"));
    }

    auto StagedImages = TArray<FCk_PixelSprite_RenderImage>{};
    StagedImages.Reserve(Snapshot->_Sources.Num());
    for (const auto& Source : Snapshot->_Sources)
    {
        auto Image = FCk_PixelSprite_RenderImage{};
        if (NOT ck::pixel_sprite::TryRender_Grid(Source._Grid, InSettings, Image, OutDiagnostic))
        {
            return false;
        }
        StagedImages.Add(MoveTemp(Image));
    }

    auto StagedTextures = TArray<TStrongObjectPtr<UTexture2D>>{};
    StagedTextures.Reserve(StagedImages.Num());
    for (const auto& Image : StagedImages)
    {
        const auto ImageBytes = int64{Image._Pixels.Num()} * sizeof(FColor);
        auto Texture = TStrongObjectPtr<UTexture2D>{
            UTexture2D::CreateTransient(Image._Width, Image._Height, PF_B8G8R8A8)};
        if (NOT Texture.IsValid())
        {
            return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("texture"), TEXT("Transient texture allocation failed"));
        }
        Texture->SRGB = true;
        Texture->Filter = TF_Nearest;
        Texture->NeverStream = true;
        if (Texture->GetPlatformData() == nullptr || Texture->GetPlatformData()->Mips.Num() != 1 ||
            Texture->GetPlatformData()->Mips[0].BulkData.GetBulkDataSize() != ImageBytes)
        {
            return ck_pixel_sprite_resources::Fail(OutDiagnostic, TEXT("texture"), TEXT("Transient texture storage is invalid"));
        }
        auto& Mip = Texture->GetPlatformData()->Mips[0];
        FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Image._Pixels.GetData(), ImageBytes);
        Mip.BulkData.Unlock();
        Texture->UpdateResource();
        StagedTextures.Add(MoveTemp(Texture));
    }

    auto PublishedTextures = TArray<TObjectPtr<UTexture2D>>{};
    PublishedTextures.Reserve(StagedTextures.Num());
    for (const auto& Texture : StagedTextures)
    {
        PublishedTextures.Add(Texture.Get());
    }
    _Textures = MoveTemp(PublishedTextures);
    _Asset = InAsset;
    _Snapshot = Snapshot;
    _Settings = InSettings;
    _Revision = Revision;
    _GenerationBytes = GenerationBytes;
    _UploadCount += _Textures.Num();
    _HasGeneration = true;
    OutDiagnostic = {};
    return true;
}

auto
    UCk_PixelSpriteResources_UE::
    Get_Texture(
        int32 InSourceIndex) const
    -> TObjectPtr<UTexture2D>
{
    if (NOT _Textures.IsValidIndex(InSourceIndex))
    {
        return {};
    }
    return _Textures[InSourceIndex];
}

auto
    UCk_PixelSpriteResources_UE::
    Get_UploadCount() const
    -> int32
{
    return _UploadCount;
}
