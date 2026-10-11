#pragma once

#include "CkPixelSprite/CkPixelSprite_Data.h"

#include "CkPixelSprite_Render.generated.h"

UENUM(BlueprintType)
enum class ECk_PixelSprite_RenderMode : uint8
{
    Nearest,
    Bilinear,
    SmoothSilhouette
};

USTRUCT(BlueprintType)
struct CKPIXELSPRITE_API FCk_PixelSprite_RenderSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Sprite")
    ECk_PixelSprite_RenderMode _Mode = ECk_PixelSprite_RenderMode::Nearest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Sprite", meta=(ClampMin="1", ClampMax="8"))
    int32 _Scale = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Sprite")
    bool _Outline = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Sprite")
    FColor _OutlineColor = FColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Sprite")
    FColor _Background = FColor::Transparent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Sprite")
    TMap<int32, FColor> _PaletteOverrides;
};

struct CKPIXELSPRITE_API FCk_PixelSprite_RenderImage
{
    int32 _Width = 0;
    int32 _Height = 0;
    TArray<FColor> _Pixels;
};

struct CKPIXELSPRITE_API FCk_PixelSprite_Difference
{
    int32 _ChangedPixels = 0;
    TOptional<FIntRect> _Bounds;
    FCk_PixelSprite_RenderImage _Image;
};

namespace ck::pixel_sprite
{
    CKPIXELSPRITE_API auto
    TryRender_Grid(const FCk_PixelSprite_Grid& InGrid,
        const FCk_PixelSprite_RenderSettings& InSettings, FCk_PixelSprite_RenderImage& OutImage,
        FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool;

    CKPIXELSPRITE_API auto
    TryCompare_Grids(const FCk_PixelSprite_Grid& InLeft,
        const FCk_PixelSprite_Grid& InRight, FCk_PixelSprite_Difference& OutDifference,
        FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool;
}
