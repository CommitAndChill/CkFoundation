#pragma once

#include "CoreMinimal.h"
#include "CkPixelSprite/CkPixelSprite_Player.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "CkPixelSprite_MaterialPreview.generated.h"

/** Passive world-material presentation of one explicitly advanced player. */
UCLASS(BlueprintType)
class CKPIXELSPRITE_API UCk_PixelSpriteMaterialPreview_UE : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Sprite")
    TObjectPtr<UCk_PixelSpritePlayer_UE> Player;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pixel Sprite")
    TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool Request_Refresh();

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    bool Get_RequiresRefresh() const;

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    void Request_Clear();

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> _BoundMaterials;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> _BoundTexture;
};
