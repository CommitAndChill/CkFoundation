#include "CkPixelSprite/CkPixelSprite_Widget.h"

#include "CkPixelSprite/CkPixelSprite_Player.h"
#include "CkCore/Macros/CkMacros.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"

auto
    UCk_PixelSpriteWidget_UE::
    Request_SetPlayer(
        TObjectPtr<UCk_PixelSpritePlayer_UE> InPlayer)
    -> void
{
    _Player = InPlayer;
    _RequestedPlayer = InPlayer;
    Request_RefreshImage();
}

auto
    UCk_PixelSpriteWidget_UE::
    Request_ApplyRequestedPlayer()
    -> void
{
    Request_SetPlayer(_RequestedPlayer);
}

auto
    UCk_PixelSpriteWidget_UE::
    Request_RefreshImage()
    -> void
{
    const auto Texture = IsValid(_Player.Get()) ? _Player->Get_CurrentTexture() : TObjectPtr<UTexture2D>{};
    const auto Size = IsValid(Texture.Get())
        ? FVector2D{static_cast<double>(Texture->GetSizeX()), static_cast<double>(Texture->GetSizeY())}
        : FVector2D::ZeroVector;
    if (_DisplayedTexture == Texture && _Brush.ImageSize == Size)
    {
        return;
    }
    _DisplayedTexture = Texture;
    _Brush.SetResourceObject(Texture.Get());
    _Brush.ImageSize = Size;
    if (_Image.IsValid())
    {
        _Image->SetImage(&_Brush);
        _Image->InvalidateImage();
    }
}

auto
    UCk_PixelSpriteWidget_UE::
    Get_Player() const
    -> TObjectPtr<UCk_PixelSpritePlayer_UE>
{
    return _Player;
}

auto
    UCk_PixelSpriteWidget_UE::
    Get_DisplayedTexture() const
    -> TObjectPtr<UTexture2D>
{
    return _DisplayedTexture;
}

auto
    UCk_PixelSpriteWidget_UE::
    SynchronizeProperties()
    -> void
{
    Super::SynchronizeProperties();
    Request_RefreshImage();
}

auto
    UCk_PixelSpriteWidget_UE::
    ReleaseSlateResources(
        bool bReleaseChildren)
    -> void
{
    Super::ReleaseSlateResources(bReleaseChildren);
    _Image.Reset();
    _Brush.SetResourceObject(nullptr);
    _Brush.ImageSize = FVector2D::ZeroVector;
    _DisplayedTexture = nullptr;
}

auto
    UCk_PixelSpriteWidget_UE::
    RebuildWidget()
    -> TSharedRef<SWidget>
{
    SAssignNew(_Image, SImage).Image(&_Brush);
    Request_RefreshImage();
    return _Image.ToSharedRef();
}
