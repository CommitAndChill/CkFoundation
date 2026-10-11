#pragma once

#include "CkPixelSprite/CkPixelSprite_Asset.h"

namespace ck::pixel_sprite_editor
{
    /** Writes one source frame as a new PNG; existing destinations are never replaced. */
    CKPIXELSPRITEEDITOR_API auto
    TryExport_Frame(
        const UCk_PixelSpriteAsset_UE& InAsset,
        const FString& InAnimationName,
        int32 InFrameIndex,
        const FCk_PixelSprite_RenderSettings& InSettings,
        const FString& InOutputPng,
        FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool;

    /** Writes a horizontal frame sheet and its authored timing/metadata as a new PNG+JSON pair. */
    CKPIXELSPRITEEDITOR_API auto
    TryExport_Sheet(
        const UCk_PixelSpriteAsset_UE& InAsset,
        const FString& InAnimationName,
        const FCk_PixelSprite_RenderSettings& InSettings,
        const FString& InOutputPng,
        const FString& InOutputJson,
        FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool;
}
