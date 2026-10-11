#include "CkPixelSprite/CkPixelSprite_Preview.h"

#include "CkCore/Macros/CkMacros.h"
#include "Widgets/Layout/SBox.h"

auto
    UCk_PixelSpritePreview_UE::
    EnsurePlayer()
    -> void
{
    if (NOT _Player)
    {
        _Player = NewObject<UCk_PixelSpritePlayer_UE>(this);
    }
    if (NOT _Sprite)
    {
        _Sprite = NewObject<UCk_PixelSpriteWidget_UE>(this);
    }
    _Sprite->Request_SetPlayer(_Player);
}

auto
    UCk_PixelSpritePreview_UE::
    Request_LoadAsset(
        const FSoftObjectPath& InPath)
    -> bool
{
    EnsurePlayer();
    if (NOT _Player->Request_LoadAsset(InPath))
    {
        return false;
    }
    _Player->Request_Play();
    _Sprite->Request_RefreshImage();
    return true;
}

auto
    UCk_PixelSpritePreview_UE::
    Request_SelectAnimation(
        const FString& InName)
    -> bool
{
    EnsurePlayer();
    if (NOT _Player->Request_SelectAnimation(InName))
    {
        return false;
    }
    _Player->Request_Play();
    _Sprite->Request_RefreshImage();
    return true;
}

auto
    UCk_PixelSpritePreview_UE::
    Get_CurrentFrame() const
    -> int32
{
    return _Player ? _Player->Get_CurrentFrame() : INDEX_NONE;
}

auto
    UCk_PixelSpritePreview_UE::
    Get_IsReady() const
    -> bool
{
    return _Player && _Player->Get_CurrentFrame() != INDEX_NONE;
}

auto
    UCk_PixelSpritePreview_UE::
    RebuildWidget()
    -> TSharedRef<SWidget>
{
    EnsurePlayer();
    return SNew(SBox).WidthOverride(256.0f).HeightOverride(256.0f)[_Sprite->TakeWidget()];
}

auto
    UCk_PixelSpritePreview_UE::
    NativeTick(
        const FGeometry& InGeometry,
        float InDeltaTime)
    -> void
{
    Super::NativeTick(InGeometry, InDeltaTime);
    if (_Player && _Player->Request_AdvanceMilliseconds(double{InDeltaTime} * 1000.0))
    {
        _Sprite->Request_RefreshImage();
    }
}

auto
    UCk_PixelSpritePreview_UE::
    ReleaseSlateResources(
        bool InReleaseChildren)
    -> void
{
    Super::ReleaseSlateResources(InReleaseChildren);
    if (_Sprite)
    {
        _Sprite->ReleaseSlateResources(InReleaseChildren);
    }
}
