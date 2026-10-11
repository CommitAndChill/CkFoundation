#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Styling/SlateBrush.h"

#include "CkPixelSprite_Widget.generated.h"

class SImage;
class UTexture2D;
class UCk_PixelSpritePlayer_UE;

/** Passive UMG view of one player. The owner advances the shared player exactly once per tick. */
UCLASS(BlueprintType, meta = (DisplayName = "Ck Pixel Sprite"))
class CKPIXELSPRITE_API UCk_PixelSpriteWidget_UE : public UWidget
{
    GENERATED_BODY()

public:
    auto
        Request_SetPlayer(
            TObjectPtr<UCk_PixelSpritePlayer_UE> InPlayer)
        -> void;

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    void
    Request_ApplyRequestedPlayer();

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    void
    Request_RefreshImage();

    auto
        Get_Player() const
        -> TObjectPtr<UCk_PixelSpritePlayer_UE>;
    auto
        Get_DisplayedTexture() const
        -> TObjectPtr<UTexture2D>;

    auto
        SynchronizeProperties()
        -> void override;
    auto
        ReleaseSlateResources(
            bool bReleaseChildren)
        -> void override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "Ck|Pixel Sprite")
    TObjectPtr<UCk_PixelSpritePlayer_UE> _RequestedPlayer;

protected:
    auto
        RebuildWidget()
        -> TSharedRef<SWidget> override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCk_PixelSpritePlayer_UE> _Player;

    UPROPERTY(Transient)
    FSlateBrush _Brush;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> _DisplayedTexture;

    TSharedPtr<SImage> _Image;
};
