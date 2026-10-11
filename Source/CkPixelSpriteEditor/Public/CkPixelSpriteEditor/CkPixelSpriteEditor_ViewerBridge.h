#pragma once

#include "CoreMinimal.h"

class UCk_PixelSpriteAsset_UE;
struct FAssetOpenArgs;

namespace ck::pixel_sprite_editor
{
    /** Viewer modules bind on startup and unbind on shutdown. True means an editor opened. */
    DECLARE_DELEGATE_RetVal_TwoParams(bool, FOpenViewer, UCk_PixelSpriteAsset_UE&, const FAssetOpenArgs&);

    CKPIXELSPRITEEDITOR_API auto
    Get_OpenViewer() -> FOpenViewer&;
}
