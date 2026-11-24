#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEditorTheme/Resolver/CkEditorTheme_Resolver.h"

#include "CkCore/Macros/CkMacros.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/StyleColors.h"

// --------------------------------------------------------------------------------------------------------------------
// Pure-function coverage only: ApplyTheme persists the theme id engine-wide, so no test here applies a theme.
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_Resolver_WindowsValue,
    "Ck.EditorTheme.Resolver.WindowsValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_Resolver_WindowsValue::RunTest(const FString& Parameters)
{
    using namespace ck::editor_theme;

    TestTrue(TEXT("1 is light"), Get_IsWindowsLight(TEXT("1")));
    TestTrue(TEXT("Padded 1 is light"), Get_IsWindowsLight(TEXT(" 1 ")));
    TestFalse(TEXT("0 is dark"), Get_IsWindowsLight(TEXT("0")));
    TestFalse(TEXT("Missing value is dark"), Get_IsWindowsLight(TEXT("")));
    TestFalse(TEXT("Garbage is dark"), Get_IsWindowsLight(TEXT("yes")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_Resolver_DesiredThemeId,
    "Ck.EditorTheme.Resolver.DesiredThemeId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_Resolver_DesiredThemeId::RunTest(const FString& Parameters)
{
    using namespace ck::editor_theme;

    TestFalse(TEXT("Manual wants nothing"), Get_DesiredThemeId(ECk_EditorTheme_Mode::Manual, TEXT("1")).IsSet());
    TestEqual(TEXT("Dark"), Get_DesiredThemeId(ECk_EditorTheme_Mode::Dark, TEXT("1")).Get(FGuid{}), Get_DarkThemeId());
    TestEqual(TEXT("Light"), Get_DesiredThemeId(ECk_EditorTheme_Mode::Light, TEXT("0")).Get(FGuid{}), Get_LightThemeId());
    TestEqual(TEXT("Follow, Windows light"),
        Get_DesiredThemeId(ECk_EditorTheme_Mode::FollowWindows, TEXT("1")).Get(FGuid{}), Get_LightThemeId());
    TestEqual(TEXT("Follow, Windows dark"),
        Get_DesiredThemeId(ECk_EditorTheme_Mode::FollowWindows, TEXT("0")).Get(FGuid{}), Get_DarkThemeId());
    TestEqual(TEXT("Follow, no registry value"),
        Get_DesiredThemeId(ECk_EditorTheme_Mode::FollowWindows, TEXT("")).Get(FGuid{}), Get_DarkThemeId());
    TestNotEqual(TEXT("Light and Dark ids differ"), Get_LightThemeId(), Get_DarkThemeId());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_Resolver_DemoteToManual,
    "Ck.EditorTheme.Resolver.DemoteToManual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_Resolver_DemoteToManual::RunTest(const FString& Parameters)
{
    using namespace ck::editor_theme;

    const auto OtherThemeId = FGuid{1, 2, 3, 4};

    const auto Light = Get_LightThemeId();
    const auto Dark = Get_DarkThemeId();

    TestFalse(TEXT("Manual never demotes"),
        Get_ShouldDemoteToManual(ECk_EditorTheme_Mode::Manual, OtherThemeId, Light, Light));
    TestFalse(TEXT("Nothing desired never demotes"),
        Get_ShouldDemoteToManual(ECk_EditorTheme_Mode::Light, OtherThemeId, Light, TOptional<FGuid>{}));
    TestFalse(TEXT("Re-broadcast of the desired theme keeps the mode"),
        Get_ShouldDemoteToManual(ECk_EditorTheme_Mode::Light, Light, Light, Light));
    TestFalse(TEXT("Re-broadcast of the previous theme keeps the mode even when it is not the desired one"),
        Get_ShouldDemoteToManual(ECk_EditorTheme_Mode::Light, Dark, Dark, Light));
    TestFalse(TEXT("The plugin applying its own pick keeps the mode"),
        Get_ShouldDemoteToManual(ECk_EditorTheme_Mode::FollowWindows, Light, Dark, Light));
    TestTrue(TEXT("Picking another theme in Appearance demotes"),
        Get_ShouldDemoteToManual(ECk_EditorTheme_Mode::Light, Dark, Light, Light));
    TestTrue(TEXT("Picking a user theme while following Windows demotes"),
        Get_ShouldDemoteToManual(ECk_EditorTheme_Mode::FollowWindows, OtherThemeId, Dark, Dark));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_Resolver_DarkIdIsEngineDefault,
    "Ck.EditorTheme.Resolver.DarkIdIsEngineDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_Resolver_DarkIdIsEngineDefault::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("The engine registers its default Dark theme under our Dark id"),
        USlateThemeManager::Get().DoesThemeExist(ck::editor_theme::Get_DarkThemeId()));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorTheme_ThemeFile_Lint,
    "Ck.EditorTheme.ThemeFile.Lint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEditorTheme_ThemeFile_Lint::RunTest(const FString& Parameters)
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));

    if (NOT TestTrue(TEXT("Plugin descriptor found"), Plugin.IsValid()))
    { return false; }

    const auto ThemeFile = Plugin->GetBaseDir() / TEXT("Resources/Themes/Ck Light.json");

    auto Contents = FString{};
    if (NOT TestTrue(TEXT("Ck Light.json loads"), FFileHelper::LoadFileToString(Contents, *ThemeFile)))
    { return false; }

    auto Root = TSharedPtr<FJsonObject>{};
    if (NOT TestTrue(TEXT("Ck Light.json parses"),
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Contents), Root) && Root.IsValid()))
    { return false; }

    // USlateThemeManager::ReadTheme rejects a file without a numeric Version or a DisplayName.
    auto Version = int32{};
    TestTrue(TEXT("Version present"), Root->TryGetNumberField(TEXT("Version"), Version));

    // The theme editor saves edits as <DisplayName>.json; matching names keep edits on the staged file.
    auto DisplayName = FString{};
    TestTrue(TEXT("DisplayName present"), Root->TryGetStringField(TEXT("DisplayName"), DisplayName));
    TestEqual(TEXT("File name is DisplayName.json"), FPaths::GetCleanFilename(ThemeFile), DisplayName + TEXT(".json"));

    auto IdString = FString{};
    auto ParsedId = FGuid{};
    TestTrue(TEXT("Id parses as a GUID"), Root->TryGetStringField(TEXT("Id"), IdString) && FGuid::Parse(IdString, ParsedId));
    TestEqual(TEXT("Id is the Light id the resolver applies"), ParsedId, ck::editor_theme::Get_LightThemeId());

    auto Revision = int32{};
    TestTrue(TEXT("CkRevision present"), Root->TryGetNumberField(TEXT("CkRevision"), Revision));

    const TSharedPtr<FJsonObject>* Colors = nullptr;
    if (NOT TestTrue(TEXT("Colors object present"), Root->TryGetObjectField(TEXT("Colors"), Colors)))
    { return false; }

    // Keys must be the enum-qualified names LoadThemeColors looks up; a bare "Panel" silently falls back to Dark.
    const auto* StyleColorEnum = StaticEnum<EStyleColor>();
    for (auto Index = 0; Index < static_cast<int32>(EStyleColor::User1); ++Index)
    {
        const auto Key = StyleColorEnum->GetNameByIndex(Index).ToString();
        TestTrue(FString::Printf(TEXT("Slot [%s] present"), *Key), (*Colors)->HasField(Key));
    }

    // PropertyEditorConstants::GetRowBackgroundColor adds up to +10 per uint8 channel to Panel/Header; above
    // #F5 nested Details rows wrap to near-black. #F0 leaves margin for the linear/sRGB round trip.
    for (const auto* Slot : {TEXT("EStyleColor::Panel"), TEXT("EStyleColor::Header")})
    {
        auto ColorString = FString{};
        auto Color = FLinearColor{};
        if (NOT TestTrue(FString::Printf(TEXT("[%s] parses"), Slot),
            (*Colors)->TryGetStringField(Slot, ColorString) && Color.InitFromString(ColorString)))
        { continue; }

        const auto SRGB = Color.ToFColor(true);
        TestTrue(FString::Printf(TEXT("[%s] stays at or below #F0"), Slot), FMath::Max3(SRGB.R, SRGB.G, SRGB.B) <= 0xF0);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
