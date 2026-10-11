#include "CkPixelSprite/CkPixelSprite_Render.h"
#include "CkCore/Macros/CkMacros.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Render,
    "Ck.PixelSprite.Render.Deterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_Render::RunTest(const FString&) -> bool
{
    auto Grid = FCk_PixelSprite_Grid{};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    TestTrue(TEXT("Pixel fixture parses"), ck::pixel_sprite::TryParse_Xpm(
        TEXT("char *sprite[]={\"3 3 3 1\",\". c None\",\"R c #ff0000\",\"B c #0000ff\",\"...\",\".RB\",\"...\"};"),
        TEXT("render"), Grid, Diagnostic));
    auto Settings = FCk_PixelSprite_RenderSettings{};
    auto Image = FCk_PixelSprite_RenderImage{};
    TestTrue(TEXT("Nearest renders"), ck::pixel_sprite::TryRender_Grid(Grid, Settings, Image, Diagnostic));
    TestEqual(TEXT("Source resolution"), Image._Pixels.Num(), 9);
    TestEqual(TEXT("Authored red exact"), Image._Pixels[4], FColor::Red);
    TestEqual(TEXT("Transparency preserved"), Image._Pixels[0], FColor::Transparent);
    Settings._Scale = 4;
    TestTrue(TEXT("Nearest scales"), ck::pixel_sprite::TryRender_Grid(Grid, Settings, Image, Diagnostic));
    TestEqual(TEXT("Scaled width"), Image._Width, 12);
    TestEqual(TEXT("Square pixel stays sharp"), Image._Pixels[4 * 12 + 4], FColor::Red);
    const auto Nearest = Image._Pixels;
    Settings._Mode = ECk_PixelSprite_RenderMode::Bilinear;
    TestTrue(TEXT("Bilinear renders"), ck::pixel_sprite::TryRender_Grid(Grid, Settings, Image, Diagnostic));
    const auto Bilinear = Image._Pixels;
    Settings._Mode = ECk_PixelSprite_RenderMode::SmoothSilhouette;
    TestTrue(TEXT("Silhouette renders"), ck::pixel_sprite::TryRender_Grid(Grid, Settings, Image, Diagnostic));
    TestTrue(TEXT("Silhouette reconstructs edge coverage"), Image._Pixels != Nearest);
    TestTrue(TEXT("Silhouette is distinct from RGB bilinear interpolation"), Image._Pixels != Bilinear);
    TestEqual(TEXT("Silhouette retains nearest interior RGB"), Image._Pixels[5 * 12 + 7].R, uint8{255});
    TestEqual(TEXT("Silhouette creates no interior blue mixture"), Image._Pixels[5 * 12 + 7].B, uint8{0});
    Settings._Mode = ECk_PixelSprite_RenderMode::Nearest;
    Settings._PaletteOverrides.Add(1, FColor::Green);
    TestTrue(TEXT("Temporary palette override renders"), ck::pixel_sprite::TryRender_Grid(Grid, Settings, Image, Diagnostic));
    TestEqual(TEXT("Derived preview is green"), Image._Pixels[4 * 12 + 4], FColor::Green);
    TestEqual(TEXT("Source palette unchanged"), Grid._Palette[1]._Color, FColor::Red);
    Settings._PaletteOverrides.Reset();
    Settings._Outline = true;
    TestTrue(TEXT("Outline renders"), ck::pixel_sprite::TryRender_Grid(Grid, Settings, Image, Diagnostic));
    TestEqual(TEXT("Outline fills adjacent transparent pixel"), Image._Pixels[0], FColor::Black);
    auto Difference = FCk_PixelSprite_Difference{};
    TestTrue(TEXT("Identical comparison succeeds"), ck::pixel_sprite::TryCompare_Grids(Grid, Grid, Difference, Diagnostic));
    TestEqual(TEXT("Identical changed count"), Difference._ChangedPixels, 0);
    TestFalse(TEXT("Identical has no bounds"), Difference._Bounds.IsSet());
    auto Changed = Grid;
    Changed._Indices[0] = 1;
    TestTrue(TEXT("Changed comparison succeeds"), ck::pixel_sprite::TryCompare_Grids(Grid, Changed, Difference, Diagnostic));
    TestEqual(TEXT("One changed pixel"), Difference._ChangedPixels, 1);
    TestEqual(TEXT("Exclusive difference bounds"), Difference._Bounds.GetValue(), FIntRect(0, 0, 1, 1));
    Image._Width = 99;
    Settings._Scale = 9;
    TestFalse(TEXT("Overscale rejects"), ck::pixel_sprite::TryRender_Grid(Grid, Settings, Image, Diagnostic));
    TestEqual(TEXT("Rejected output unchanged"), Image._Width, 99);
    Settings._Scale = 1;
    Changed._Indices[0] = 100;
    TestFalse(TEXT("Invalid index rejects"), ck::pixel_sprite::TryRender_Grid(Changed, Settings, Image, Diagnostic));
    TestEqual(TEXT("Invalid grid leaves output unchanged"), Image._Width, 99);
    return true;
}
#endif
