#include "CkPixelSprite/CkPixelSprite_Data.h"
#include "CkPixelSprite_File.h"
#include "CkCore/Macros/CkMacros.h"

auto
    ck::pixel_sprite::TryLoad_Xpm(const FString& InSourcePath,
        FCk_PixelSprite_Grid& OutGrid, FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool
{
    auto Text = FString{};
    auto ByteCount = int64{0};
    if (NOT file_private::Load_NativeSource(InSourcePath, Text, ByteCount, OutDiagnostic))
    {
        return false;
    }
    return TryParse_Xpm(Text, InSourcePath, OutGrid, OutDiagnostic);
}
