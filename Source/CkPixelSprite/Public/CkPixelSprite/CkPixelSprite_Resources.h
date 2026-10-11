#pragma once

#include "CoreMinimal.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "UObject/Object.h"

#include "CkPixelSprite_Resources.generated.h"

class UTexture2D;

/** One coherent transient texture generation for every unique source in an asset. */
UCLASS()
class CKPIXELSPRITE_API UCk_PixelSpriteResources_UE : public UObject
{
    GENERATED_BODY()

public:
    auto
        TryBuild(
            TObjectPtr<UCk_PixelSpriteAsset_UE> InAsset,
            const FCk_PixelSprite_RenderSettings& InSettings,
            FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool;

    auto
        Get_Texture(
            int32 InSourceIndex) const
        -> TObjectPtr<UTexture2D>;
    auto
        Get_UploadCount() const
        -> int32;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCk_PixelSpriteAsset_UE> _Asset;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> _Textures;

    TSharedPtr<const FCk_PixelSprite_Manifest> _Snapshot;
    FCk_PixelSprite_RenderSettings _Settings;
    uint64 _Revision = 0;
    int64 _GenerationBytes = 0;
    int32 _UploadCount = 0;
    bool _HasGeneration = false;
};
