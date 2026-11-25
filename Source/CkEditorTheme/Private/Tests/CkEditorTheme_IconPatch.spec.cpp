#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEditorTheme/Styling/CkEditorTheme_IconPatch.h"

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_editor_theme_icon_patch_tests
{
    // 4x4 BGRA: a dark ring (luma ~30) around a 2x2 white body, all opaque.
    auto Make_OutlinedIcon()
        -> TArray64<uint8>
    {
        auto Pixels = TArray64<uint8>{};
        for (auto Y = 0; Y < 4; ++Y)
        {
            for (auto X = 0; X < 4; ++X)
            {
                const auto IsBody = X >= 1 && X <= 2 && Y >= 1 && Y <= 2;
                const auto Value = IsBody ? uint8{255} : uint8{30};
                Pixels.Append({Value, Value, Value, uint8{255}});
            }
        }
        return Pixels;
    }

    auto Make_Solid(
        uint8 InB,
        uint8 InG,
        uint8 InR)
        -> TArray64<uint8>
    {
        auto Pixels = TArray64<uint8>{};
        for (auto Pixel = 0; Pixel < 16; ++Pixel)
        { Pixels.Append({InB, InG, InR, uint8{255}}); }
        return Pixels;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_IconPatch_OutlineOnly,
    "Ck.EditorTheme.IconPatch.OutlineOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_IconPatch_OutlineOnly::RunTest(const FString& Parameters)
{
    using namespace ck_editor_theme_icon_patch_tests;

    const auto Outline = ck::editor_theme::Get_OutlineOnlyIcon(Make_OutlinedIcon(), 4, 4);
    if (NOT TestTrue(TEXT("An outlined light glyph is converted"), Outline.IsSet()))
    { return false; }

    const auto& Pixels = Outline.GetValue();
    TestEqual(TEXT("Same size"), Pixels.Num(), int64{64});
    TestEqual(TEXT("The dark outline stays fully opaque"), Pixels[3], uint8{255});
    TestEqual(TEXT("The outline becomes the dark glyph color"), Pixels[0], uint8{0x3A});
    TestEqual(TEXT("The light body becomes transparent"), Pixels[(1 * 4 + 1) * 4 + 3], uint8{0});

    TestFalse(TEXT("A plain white glyph (no outline) is left alone"),
        ck::editor_theme::Get_OutlineOnlyIcon(Make_Solid(255, 255, 255), 4, 4).IsSet());
    TestFalse(TEXT("A colorful icon is left alone"),
        ck::editor_theme::Get_OutlineOnlyIcon(Make_Solid(20, 20, 220), 4, 4).IsSet());
    TestFalse(TEXT("A buffer that does not match the size is rejected"),
        ck::editor_theme::Get_OutlineOnlyIcon(Make_OutlinedIcon(), 5, 4).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_IconPatch_LightSurface,
    "Ck.EditorTheme.IconPatch.LightSurface",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_IconPatch_LightSurface::RunTest(const FString& Parameters)
{
    using namespace ck_editor_theme_icon_patch_tests;

    const auto Surface = ck::editor_theme::Get_LightSurface(Make_Solid(0x1B, 0x1B, 0x1B), 4, 4);
    if (NOT TestTrue(TEXT("A dark gray panel is converted"), Surface.IsSet()))
    { return false; }

    TestEqual(TEXT("The darkest panel gray becomes the Light panel"), Surface.GetValue()[0], uint8{0xF0});
    TestEqual(TEXT("Alpha is kept"), Surface.GetValue()[3], uint8{255});

    const auto Lighter = ck::editor_theme::Get_LightSurface(Make_Solid(0x60, 0x60, 0x60), 4, 4);
    TestTrue(TEXT("A lighter legacy gray maps darker, keeping the ramp's order"),
        Lighter.IsSet() && Lighter.GetValue()[0] < Surface.GetValue()[0]);

    TestFalse(TEXT("An icon with a light glyph is left alone"),
        ck::editor_theme::Get_LightSurface(Make_OutlinedIcon(), 4, 4).IsSet());
    TestFalse(TEXT("A colored surface is left alone"),
        ck::editor_theme::Get_LightSurface(Make_Solid(0x10, 0x10, 0x60), 4, 4).IsSet());

    auto Shadow = Make_Solid(0, 0, 0);
    for (auto Pixel = 0; Pixel < 16; ++Pixel)
    { Shadow[Pixel * 4 + 3] = 80; }
    TestFalse(TEXT("A translucent black shadow is left alone"), ck::editor_theme::Get_LightSurface(Shadow, 4, 4).IsSet());
    TestTrue(TEXT("Node bodies are converted without the opacity check"),
        ck::editor_theme::Get_LightSurface(Shadow, 4, 4, false).IsSet());

    // One row: a soft left edge (alpha 50), the peak (100), then the fade into the body (60, 0), with dark shading in
    // the RGB where the alpha peaks.
    constexpr uint8 Values[] = {240, 80, 150, 240};
    constexpr uint8 Alphas[] = {50, 100, 60, 0};
    auto Spill = TArray64<uint8>{};
    for (auto Pixel = 0; Pixel < 4; ++Pixel)
    { Spill.Append({Values[Pixel], Values[Pixel], Values[Pixel], Alphas[Pixel]}); }
    const auto Solid = ck::editor_theme::Get_SolidTitleSpill(Spill, 4, 1);
    if (TestTrue(TEXT("The title spill is converted"), Solid.IsSet()))
    {
        const auto& Pixels = Solid.GetValue();
        TestTrue(TEXT("The rounded left edge ramps up to 60% of the peak (50 / 60 of opaque)"), Pixels[3] >= 212 && Pixels[3] <= 213);
        TestEqual(TEXT("Opaque from the peak"), Pixels[7], uint8{255});
        TestEqual(TEXT("The fade into the body is gone"), Pixels[15], uint8{255});
        TestTrue(TEXT("No baked shading across the row"), Pixels[4] == Pixels[12] && Pixels[0] == Pixels[8]);
    }

    // A feather row: the stock art's top rows peak at alpha 5-8.
    auto Feather = TArray64<uint8>{};
    for (const auto Alpha : {uint8{2}, uint8{8}, uint8{5}, uint8{0}})
    { Feather.Append({uint8{240}, uint8{240}, uint8{240}, Alpha}); }
    const auto SolidFeather = ck::editor_theme::Get_SolidTitleSpill(Feather, 4, 1);
    TestTrue(TEXT("A near-transparent feather row stays transparent"),
        SolidFeather.IsSet() && SolidFeather.GetValue()[7] == 0 && SolidFeather.GetValue()[15] == 0);

    // Four rows of a variable-node glow: a strong title row, a weaker row, then the faint lower half.
    auto Glow = TArray64<uint8>{};
    for (const auto& Row : {TArray<uint8>{20, 156, 40, 0}, TArray<uint8>{6, 24, 8, 0},
                            TArray<uint8>{4, 30, 8, 0}, TArray<uint8>{2, 20, 4, 0}})
    {
        for (const auto Alpha : Row)
        { Glow.Append({uint8{255}, uint8{255}, uint8{255}, Alpha}); }
    }
    const auto Strip = ck::editor_theme::Get_SolidVarSpill(Glow, 4, 4);
    if (TestTrue(TEXT("The variable glow is converted"), Strip.IsSet()))
    {
        const auto& Pixels = Strip.GetValue();
        TestEqual(TEXT("The title row is solid at its peak"), Pixels[7], uint8{255});
        TestEqual(TEXT("The title row's ends stay transparent"), Pixels[15], uint8{0});
        TestEqual(TEXT("A weaker row is solid too"), Pixels[16 + 7], uint8{255});
        TestEqual(TEXT("The lower half is solid, so the hue covers the whole pill"), Pixels[32 + 7], uint8{255});
        TestEqual(TEXT("The last row is solid"), Pixels[48 + 7], uint8{255});
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
