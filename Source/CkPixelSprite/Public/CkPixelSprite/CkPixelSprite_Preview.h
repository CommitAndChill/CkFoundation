#pragma once

#include "Blueprint/UserWidget.h"
#include "CkPixelSprite/CkPixelSprite_Player.h"
#include "CkPixelSprite/CkPixelSprite_Widget.h"

#include "CkPixelSprite_Preview.generated.h"

/** Ready-to-use viewport preview; it owns and advances one private player. */
UCLASS(BlueprintType)
class CKPIXELSPRITE_API UCk_PixelSpritePreview_UE : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_LoadAsset(
        const FSoftObjectPath& InPath);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_SelectAnimation(
        const FString& InName);

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    int32
    Get_CurrentFrame() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    bool
    Get_IsReady() const;

protected:
    auto
        RebuildWidget()
        -> TSharedRef<SWidget> override;
    auto
        NativeTick(
            const FGeometry& InGeometry,
            float InDeltaTime)
        -> void override;
    auto
        ReleaseSlateResources(
            bool InReleaseChildren)
        -> void override;

private:
    auto
        EnsurePlayer()
        -> void;

    UPROPERTY(Transient)
    TObjectPtr<UCk_PixelSpritePlayer_UE> _Player;

    UPROPERTY(Transient)
    TObjectPtr<UCk_PixelSpriteWidget_UE> _Sprite;
};
