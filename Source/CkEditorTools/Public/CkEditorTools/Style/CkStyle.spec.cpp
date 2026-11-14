#include "CkEditorTools/Style/CkStyle.h"

#include "CkEditorTools/Settings/CkStyleSettings.h"

#include "CkCore/Macros/CkMacros.h"

#include <Misc/AutomationTest.h>
#include <Misc/ScopeExit.h>
#include <UObject/UnrealType.h>

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_style_tests
{
    // The engine's default Dark Background (StyleColors.cpp) and the Ck Light theme's (CkEditorTheme "Ck Light.json").
    const auto EngineDarkBackground = FLinearColor::FromSRGBColor(FColor(0x15, 0x15, 0x15));
    const auto CkLightBackground = FLinearColor(0.822786f, 0.822786f, 0.822786f);
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Style_ResolvePalette,
    "Ck.CkEditorTools.Style.Palette.Resolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool
    FCkTest_Style_ResolvePalette::
    RunTest(
        const FString& Parameters)
{
    using namespace ck_style_tests;

    TestTrue(TEXT("Follow on the engine Dark background resolves Dark"),
        CkStyle::ResolvePalette(ECk_Style_PaletteMode::FollowEditorTheme, false, EngineDarkBackground) == ECk_Style_Palette::Dark);
    TestTrue(TEXT("Follow on the Ck Light background resolves Light"),
        CkStyle::ResolvePalette(ECk_Style_PaletteMode::FollowEditorTheme, false, CkLightBackground) == ECk_Style_Palette::Light);
    TestTrue(TEXT("Follow resolves Dark when unattended"),
        CkStyle::ResolvePalette(ECk_Style_PaletteMode::FollowEditorTheme, true, CkLightBackground) == ECk_Style_Palette::Dark);
    TestTrue(TEXT("Always Dark ignores a light background"),
        CkStyle::ResolvePalette(ECk_Style_PaletteMode::AlwaysDark, false, CkLightBackground) == ECk_Style_Palette::Dark);
    TestTrue(TEXT("Always Light ignores a dark background"),
        CkStyle::ResolvePalette(ECk_Style_PaletteMode::AlwaysLight, false, EngineDarkBackground) == ECk_Style_Palette::Light);
    TestTrue(TEXT("Always Light resolves Dark when unattended"),
        CkStyle::ResolvePalette(ECk_Style_PaletteMode::AlwaysLight, true, EngineDarkBackground) == ECk_Style_Palette::Dark);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Style_GetterPicksPalette,
    "Ck.CkEditorTools.Style.Palette.GetterPicksPalette",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool
    FCkTest_Style_GetterPicksPalette::
    RunTest(
        const FString& Parameters)
{
    const auto* Settings = GetDefault<UCk_Style_UserSettings_UE>();
    const auto ThemeLight = CkStyle::IsThemeLight();

    {
        const auto Scope = CkStyle::FScopedPalette{ECk_Style_Palette::Light};
        TestTrue(TEXT("A Light scope resolves Light"), CkStyle::IsLight());
        TestEqual(TEXT("The unscoped theme resolution ignores the scope"), CkStyle::IsThemeLight(), ThemeLight);
        TestEqual(TEXT("Bg1 reads the Light field"), CkStyle::Bg1(), Settings->Light.Bg1);
        TestEqual(TEXT("Text reads the Light field"), CkStyle::Text(), Settings->Light.Text);
        TestEqual(TEXT("Accent reads the Light field"), CkStyle::Accent(), Settings->Light.Accent);
        TestEqual(TEXT("NodeFill_GoalInactive reads the Light field"), CkStyle::NodeFill_GoalInactive(), Settings->Light.NodeFill_GoalInactive);
        TestEqual(TEXT("Non-colour slots stay shared"), CkStyle::FontSizeBody(), Settings->FontSizeBody);

        {
            const auto Inner = CkStyle::FScopedPalette{ECk_Style_Palette::Dark};
            TestFalse(TEXT("A nested Dark scope resolves Dark"), CkStyle::IsLight());
            TestEqual(TEXT("Bg1 reads the Dark field"), CkStyle::Bg1(), Settings->Bg1);
            TestEqual(TEXT("Text reads the Dark field"), CkStyle::Text(), Settings->Text);
        }

        TestTrue(TEXT("Leaving the nested scope restores Light"), CkStyle::IsLight());

        {
            const auto Unset = CkStyle::FScopedPalette{TOptional<ECk_Style_Palette>{}};
            TestTrue(TEXT("An unset scope changes nothing"), CkStyle::IsLight());
        }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Style_EverySlotHasLight,
    "Ck.CkEditorTools.Style.Palette.EverySlotHasLight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool
    FCkTest_Style_EverySlotHasLight::
    RunTest(
        const FString& Parameters)
{
    const auto* ColorStruct = TBaseStructure<FLinearColor>::Get();
    const auto* PaletteStruct = FCk_Style_Palette::StaticStruct();
    const auto* Settings = GetDefault<UCk_Style_UserSettings_UE>();

    auto IsColor = [ColorStruct](const FProperty* InProperty)
    {
        const auto* StructProperty = CastField<FStructProperty>(InProperty);
        return StructProperty != nullptr && StructProperty->Struct == ColorStruct;
    };

    auto DarkSlots = 0;
    for (TFieldIterator<FProperty> It{UCk_Style_UserSettings_UE::StaticClass(), EFieldIteratorFlags::ExcludeSuper}; It; ++It)
    {
        if (NOT IsColor(*It))
        { continue; }

        ++DarkSlots;
        const auto* LightProperty = PaletteStruct->FindPropertyByName(It->GetFName());
        if (NOT TestTrue(*FString::Printf(TEXT("[%s] has a Light counterpart"), *It->GetName()), IsColor(LightProperty)))
        { continue; }

        const auto& LightValue = *LightProperty->ContainerPtrToValuePtr<FLinearColor>(&Settings->Light);
        TestTrue(*FString::Printf(TEXT("[%s] has a visible Light value"), *It->GetName()), LightValue.A > 0.0f);
    }

    auto LightSlots = 0;
    for (TFieldIterator<FProperty> It{PaletteStruct}; It; ++It)
    {
        ++LightSlots;
        TestTrue(*FString::Printf(TEXT("Light [%s] is a colour slot of the Dark palette"), *It->GetName()),
            IsColor(*It) && IsColor(UCk_Style_UserSettings_UE::StaticClass()->FindPropertyByName(It->GetFName())));
    }

    TestTrue(TEXT("The Dark palette has colour slots"), DarkSlots > 0);
    TestEqual(TEXT("The Light palette has exactly the Dark colour slots"), LightSlots, DarkSlots);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Style_OverlayPalette,
    "Ck.CkEditorTools.Style.Palette.OverlayOptOut",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool
    FCkTest_Style_OverlayPalette::
    RunTest(
        const FString& Parameters)
{
    auto* Settings = GetMutableDefault<UCk_Style_UserSettings_UE>();
    const auto Previous = Settings->bOverlaysFollowEditorTheme;
    ON_SCOPE_EXIT
    {
        Settings->bOverlaysFollowEditorTheme = Previous;
    };

    Settings->bOverlaysFollowEditorTheme = true;
    TestFalse(TEXT("Following overlays leave the palette unforced"), CkStyle::GetOverlayPalette().IsSet());

    Settings->bOverlaysFollowEditorTheme = false;
    const auto Forced = CkStyle::GetOverlayPalette();
    TestTrue(TEXT("Opted-out overlays force a palette"), Forced.IsSet() && *Forced == ECk_Style_Palette::Dark);

    {
        const auto Light = CkStyle::FScopedPalette{ECk_Style_Palette::Light};
        const auto Overlay = CkStyle::FScopedPalette{CkStyle::GetOverlayPalette()};
        TestFalse(TEXT("An opted-out overlay scope wins over an enclosing Light scope"), CkStyle::IsLight());
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
