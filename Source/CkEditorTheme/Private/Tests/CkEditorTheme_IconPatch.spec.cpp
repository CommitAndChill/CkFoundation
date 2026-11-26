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

    // A 4x3 variable-node body shaped like VarNode_body.png: an outline ring (209) more opaque than the interior (105),
    // an anti-aliased edge pixel (52) left of the middle row, caps of 1 pixel.
    auto Body = TArray64<uint8>{};
    for (const auto& Row : {TArray<uint8>{0, 209, 209, 209}, TArray<uint8>{52, 209, 105, 105}, TArray<uint8>{0, 209, 209, 209}})
    {
        for (const auto Alpha : Row)
        { Body.Append({uint8{20}, uint8{20}, uint8{20}, Alpha}); }
    }
    const auto Pill = ck::editor_theme::Get_VarPillFill(Body, 4, 3, 8, 5, 1, 1);
    if (TestTrue(TEXT("The variable pill fill is converted"), Pill.IsSet()))
    {
        const auto& Pixels = Pill.GetValue();
        const auto AlphaAt = [&Pixels](int32 InX, int32 InY) { return Pixels[(static_cast<int64>(InY) * 8 + InX) * 4 + 3]; };
        TestEqual(TEXT("Resampled to the target size"), Pixels.Num(), int64{8 * 5 * 4});
        TestEqual(TEXT("The top-left cap maps 1:1, outside the pill stays transparent"), AlphaAt(0, 0), uint8{0});
        TestEqual(TEXT("The interior is a 30% tint"), AlphaAt(7, 2), uint8{77});
        TestEqual(TEXT("The stretched middle samples the interior"), AlphaAt(4, 2), uint8{77});
        TestEqual(TEXT("The ring is the full colour"), AlphaAt(1, 2), uint8{255});
        TestEqual(TEXT("The anti-aliased edge fades the ring"), AlphaAt(0, 2), uint8{63});
        TestEqual(TEXT("The bottom cap maps 1:1 from the last source row"), AlphaAt(7, 4), uint8{255});
        TestEqual(TEXT("The bottom-left cap stays transparent"), AlphaAt(0, 4), uint8{0});
        TestEqual(TEXT("The fill is white so the tint is the hue"), Pixels[(2 * 8 + 7) * 4], uint8{255});
    }
    TestFalse(TEXT("Caps that do not fit are refused"), ck::editor_theme::Get_VarPillFill(Body, 4, 3, 8, 5, 2, 1).IsSet());
    TestFalse(TEXT("A body without a ring is refused"), ck::editor_theme::Get_VarPillFill(Make_Solid(20, 20, 20), 4, 4, 8, 8, 1, 1).IsSet());

    // A banner stripe row: stripe, anti-aliased edge, gap.
    auto Stripes = TArray64<uint8>{};
    for (const auto Alpha : {uint8{255}, uint8{0x7D}, uint8{0}})
    { Stripes.Append({uint8{255}, uint8{255}, uint8{255}, Alpha}); }
    const auto Tape = ck::editor_theme::Get_OpaqueStripes(Stripes, 3, 1);
    if (TestTrue(TEXT("The banner stripes are converted"), Tape.IsSet()))
    {
        const auto& Pixels = Tape.GetValue();
        TestEqual(TEXT("A stripe stays white for the tint"), Pixels[0], uint8{255});
        TestEqual(TEXT("A gap is dark"), Pixels[2 * 4], uint8{0x50});
        TestTrue(TEXT("The edge blends between them"), Pixels[4] > 0x50 && Pixels[4] < 255);
        TestTrue(TEXT("Every pixel is opaque"), Pixels[3] == 255 && Pixels[7] == 255 && Pixels[11] == 255);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
