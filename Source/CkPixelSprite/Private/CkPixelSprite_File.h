#pragma once

#include "CkPixelSprite/CkPixelSprite_Data.h"

namespace ck::pixel_sprite::file_private
{
    auto
        Validate_SourcePath(
            const FString& InRelativePath,
            FString& OutDiagnostic)
        -> bool;
    auto
        Load_NativeSource(
            const FString& InAbsolutePath, FString& OutText,
            int64& OutByteCount, FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool;
}
