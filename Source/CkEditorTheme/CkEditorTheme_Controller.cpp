#include "CkEditorTheme_Controller.h"

#include "CkEditorTheme_Log.h"
#include "CkEditorTheme/Resolver/CkEditorTheme_Resolver.h"
#include "CkEditorTheme/Settings/CkEditorTheme_Settings.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Settings/EditorStyleSettings.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/StarshipCoreStyle.h"
#include "Styling/StyleColors.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_editor_theme_controller
{
    auto Get_Json(
        const FString& InContents)
        -> TSharedPtr<FJsonObject>
    {
        auto Root = TSharedPtr<FJsonObject>{};
        if (NOT FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(InContents), Root))
        { return {}; }

        return Root;
    }

    auto Get_Revision(
        const TSharedPtr<FJsonObject>& InTheme)
        -> TOptional<int32>
    {
        auto Revision = int32{};
        if (NOT InTheme.IsValid() || NOT InTheme->TryGetNumberField(TEXT("CkRevision"), Revision))
        { return {}; }

        return Revision;
    }

    auto Get_Id(
        const TSharedPtr<FJsonObject>& InTheme)
        -> TOptional<FGuid>
    {
        auto IdString = FString{};
        auto Id = FGuid{};
        if (NOT InTheme.IsValid() || NOT InTheme->TryGetStringField(TEXT("Id"), IdString) || NOT FGuid::Parse(IdString, Id))
        { return {}; }

        return Id;
    }
}

// --------------------------------------------------------------------------------------------------------------------

#if PLATFORM_WINDOWS
auto
    FCk_EditorTheme_WindowsWatcher::
    ProcessMessage(
        HWND InHwnd,
        uint32 InMessage,
        WPARAM InWParam,
        LPARAM InLParam,
        int32& OutResult)
    -> bool
{
    if (InMessage != WM_SETTINGCHANGE || InLParam == 0)
    { return false; }

    if (FCString::Strcmp(reinterpret_cast<const TCHAR*>(InLParam), TEXT("ImmersiveColorSet")) == 0)
    { _Pending = true; }

    return false;
}

auto
    FCk_EditorTheme_WindowsWatcher::
    Consume_Pending()
    -> bool
{
    const auto WasPending = _Pending;
    _Pending = false;
    return WasPending;
}
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_EditorTheme_Controller::
    Start()
    -> void
{
    if (IsRunningCommandlet() || NOT FSlateApplication::IsInitialized())
    { return; }

    // Gate editors render deterministically regardless of the last interactive theme.
    if (FApp::IsUnattended())
    {
        DoForceDark_ForSession();
        // In memory only: undoes Light graph colors an interactive session may have saved to the shared ini.
        _GraphPatch.Request_Apply(ECk_EditorTheme_Brightness::Dark);
        return;
    }

    DoPatch_Styles();
    DoStage_Themes();
    DoReconcile();
    DoApply_ThemeDependentStyles();

    _LastThemeId = USlateThemeManager::Get().GetCurrentThemeID();
    _ThemeChangedHandle = USlateThemeManager::Get().OnThemeChanged().AddRaw(this, &FCk_EditorTheme_Controller::OnThemeChanged);
    _SettingChangedHandle = GetMutableDefault<UCk_EditorTheme_UserSettings_UE>()->OnSettingChanged().AddRaw(
        this, &FCk_EditorTheme_Controller::OnSettingChanged);

    DoRegister_Watcher();

    // Slate shuts down before modules unload, so the watcher and the icon textures are released here.
    _PreShutdownHandle = FSlateApplication::Get().OnPreShutdown().AddRaw(this, &FCk_EditorTheme_Controller::OnPreShutdown);

    _TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FCk_EditorTheme_Controller::OnTick), 0.25f);
}

auto
    FCk_EditorTheme_Controller::
    Stop()
    -> void
{
    if (_TickerHandle.IsValid())
    {
        FTSTicker::RemoveTicker(_TickerHandle);
        _TickerHandle.Reset();
    }

    DoUnregister_Watcher();

    if (FSlateApplication::IsInitialized())
    {
        _LightPatch.Request_Apply(ECk_EditorTheme_Brightness::Dark);
        // Slate is still alive (hot unload): style copies outside the registry may still draw our textures.
        _IconPatch.Request_Release(FCk_EditorTheme_IconPatch::ECk_TextureRelease::KeepAlive);
    }

    if (UObjectInitialized())
    {
        _LightPatch.Request_ApplyNodeTitleInk();
        _GraphPatch.Request_Apply(ECk_EditorTheme_Brightness::Dark);
    }

    if (_PreShutdownHandle.IsValid() && FSlateApplication::IsInitialized())
    { FSlateApplication::Get().OnPreShutdown().Remove(_PreShutdownHandle); }
    _PreShutdownHandle.Reset();

    if (NOT UObjectInitialized())
    { return; }

    if (_ThemeChangedHandle.IsValid())
    { USlateThemeManager::Get().OnThemeChanged().Remove(_ThemeChangedHandle); }
    _ThemeChangedHandle.Reset();

    if (_SettingChangedHandle.IsValid())
    { GetMutableDefault<UCk_EditorTheme_UserSettings_UE>()->OnSettingChanged().Remove(_SettingChangedHandle); }
    _SettingChangedHandle.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_EditorTheme_Controller::
    DoPatch_Styles()
    -> void
{
    // Both are FSlateStyleSet subclasses (FStarshipCoreStyle::FStyle, FStarshipEditorStyle::FStyle); anything else
    // registered under these names is skipped. Runs before the main frame exists and mutates styles in place.
    auto CapturedFields = int32{};
    for (const auto* Style : {&FStarshipCoreStyle::GetCoreStyle(), &FAppStyle::Get()})
    {
        const auto StyleSetName = Style->GetStyleSetName();
        CK_ENSURE_IF_NOT(StyleSetName == FName{TEXT("CoreStyle")} || StyleSetName == FName{TEXT("EditorStyle")},
            TEXT("Unexpected style set [{}]; it is not patched"), StyleSetName)
        { continue; }

        auto& StyleSet = const_cast<FSlateStyleSet&>(static_cast<const FSlateStyleSet&>(*Style));
        CapturedFields += _LightPatch.Request_Capture(StyleSet);
    }

    ck::editor_theme::Log(TEXT("[{}] style colors switch with Light/Dark"), CapturedFields);
}

auto
    FCk_EditorTheme_Controller::
    DoApply_ThemeDependentStyles()
    -> void
{
    const auto Brightness = ck::editor_theme::Get_Brightness();
    const auto StylesChanged = _LightPatch.Request_Apply(Brightness);
    const auto TitleInkChanged = _LightPatch.Request_ApplyNodeTitleInk();
    const auto IconsChanged = _IconPatch.Request_Apply(Brightness,
        GetDefault<UCk_EditorTheme_UserSettings_UE>()->Get_OutlineLegacyIconsInLight(), _LightPatch.Get_LightGraphNodes());
    const auto GraphChanged = _GraphPatch.Request_Apply(Brightness);

    if (StylesChanged || TitleInkChanged || IconsChanged || GraphChanged)
    { FSlateApplication::Get().InvalidateAllWidgets(false); }
}

auto
    FCk_EditorTheme_Controller::
    DoStage_Themes()
    -> void
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));

    CK_ENSURE_IF_NOT(ck::IsValid(Plugin), TEXT("CkFoundation plugin descriptor not found while staging the editor themes"))
    { return; }

    auto& ThemeManager = USlateThemeManager::Get();
    const auto SourceDir = Plugin->GetBaseDir() / TEXT("Resources/Themes");
    const auto TargetDir = ThemeManager.GetUserThemeDir();

    auto ShippedFiles = TArray<FString>{};
    IFileManager::Get().FindFiles(ShippedFiles, *SourceDir, TEXT(".json"));

    auto AnyStaged = false;

    for (const auto& File : ShippedFiles)
    {
        const auto SourceFile = SourceDir / File;

        auto ShippedContents = FString{};
        if (NOT FFileHelper::LoadFileToString(ShippedContents, *SourceFile))
        {
            ck::editor_theme::Warning(TEXT("Could not read shipped theme [{}]"), SourceFile);
            continue;
        }

        const auto ShippedTheme = ck_editor_theme_controller::Get_Json(ShippedContents);
        const auto ShippedId = ck_editor_theme_controller::Get_Id(ShippedTheme);
        const auto ShippedRevision = ck_editor_theme_controller::Get_Revision(ShippedTheme);

        CK_ENSURE_IF_NOT(ShippedId.IsSet() && ShippedRevision.IsSet(),
            TEXT("Shipped theme [{}] needs an Id and a CkRevision"), SourceFile)
        { continue; }

        // Found by id: the theme editor saves a modified theme as <DisplayName>.json and deletes the old path.
        auto TargetFile = TargetDir / File;
        if (const auto* LoadedTheme = ThemeManager.GetThemes().FindByKey(ShippedId.GetValue());
            LoadedTheme != nullptr && FPaths::IsUnderDirectory(LoadedTheme->Filename, TargetDir))
        { TargetFile = LoadedTheme->Filename; }

        if (IFileManager::Get().FileExists(*TargetFile))
        {
            auto StagedContents = FString{};
            if (NOT FFileHelper::LoadFileToString(StagedContents, *TargetFile))
            {
                ck::editor_theme::Warning(TEXT("Could not read staged theme [{}]; leaving it as is"), TargetFile);
                continue;
            }

            // No CkRevision means the theme editor saved over the staged copy: the user's edits win.
            const auto StagedRevision = ck_editor_theme_controller::Get_Revision(
                ck_editor_theme_controller::Get_Json(StagedContents));

            if (NOT StagedRevision.IsSet() || StagedRevision.GetValue() >= ShippedRevision.GetValue())
            { continue; }
        }

        IFileManager::Get().MakeDirectory(*TargetDir, true);

        if (IFileManager::Get().Copy(*TargetFile, *SourceFile) != COPY_OK)
        {
            ck::editor_theme::Warning(TEXT("Could not stage theme [{}] to [{}]"), SourceFile, TargetFile);
            continue;
        }

        AnyStaged = true;
    }

    if (AnyStaged)
    { ThemeManager.LoadThemes(); }
}

auto
    FCk_EditorTheme_Controller::
    DoReconcile()
    -> void
{
    const auto* Settings = GetDefault<UCk_EditorTheme_UserSettings_UE>();
    const auto DesiredThemeId = ck::editor_theme::Get_DesiredThemeId(Settings->Get_Mode(), DoRead_AppsUseLightTheme());

    if (NOT DesiredThemeId.IsSet())
    { return; }

    auto& ThemeManager = USlateThemeManager::Get();

    if (NOT ThemeManager.DoesThemeExist(DesiredThemeId.GetValue()))
    {
        if (NOT _HasLoggedMissingTheme)
        {
            ck::editor_theme::Warning(TEXT("Theme [{}] is not loaded; the editor theme is left unchanged"),
                DesiredThemeId.GetValue().ToString());
            _HasLoggedMissingTheme = true;
        }
        return;
    }

    if (ThemeManager.GetCurrentThemeID() == DesiredThemeId.GetValue())
    { return; }

    ThemeManager.ApplyTheme(DesiredThemeId.GetValue());

    auto* StyleSettings = GetMutableDefault<UEditorStyleSettings>();
    StyleSettings->CurrentAppliedTheme = DesiredThemeId.GetValue();

    // ApplyTheme already ran OnThemeChanged, so the CDO holds the graph patch's Light grid; SaveConfig writes every
    // config property, so the engine defaults are put back around it.
    const auto GraphWasLight = _GraphPatch.Request_Apply(ECk_EditorTheme_Brightness::Dark);
    StyleSettings->SaveConfig();
    if (GraphWasLight)
    { _GraphPatch.Request_Apply(ECk_EditorTheme_Brightness::Light); }

    // 5.7's ApplyTheme does not invalidate; 5.8's does, so this is redundant there.
    FSlateApplication::Get().InvalidateAllWidgets(false);

    ck::editor_theme::Log(TEXT("Applied editor theme [{}]"), ThemeManager.GetCurrentTheme().DisplayName.ToString());
}

auto
    FCk_EditorTheme_Controller::
    DoForceDark_ForSession()
    -> void
{
    auto& ThemeManager = USlateThemeManager::Get();
    const auto DarkThemeId = ck::editor_theme::Get_DarkThemeId();

    if (ThemeManager.GetCurrentThemeID() == DarkThemeId || NOT ThemeManager.DoesThemeExist(DarkThemeId))
    { return; }

    ThemeManager.ApplyTheme(DarkThemeId);
    FSlateApplication::Get().InvalidateAllWidgets(false);
}

auto
    FCk_EditorTheme_Controller::
    DoRead_AppsUseLightTheme() const
    -> FString
{
    auto Value = FString{};
#if PLATFORM_WINDOWS
    FWindowsPlatformMisc::QueryRegKey(HKEY_CURRENT_USER,
        TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"), TEXT("AppsUseLightTheme"), Value);
#endif
    return Value;
}

auto
    FCk_EditorTheme_Controller::
    DoRegister_Watcher()
    -> void
{
#if PLATFORM_WINDOWS
    // -RenderOffScreen swaps in FNullApplication; the cast below would be undefined there.
    if (FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")))
    { return; }

    const auto Application = FSlateApplication::Get().GetPlatformApplication();

    if (NOT Application.IsValid())
    { return; }

    static_cast<FWindowsApplication*>(Application.Get())->AddMessageHandler(_Watcher);
    _WatchedApplication = Application;
#endif
}

auto
    FCk_EditorTheme_Controller::
    DoUnregister_Watcher()
    -> void
{
#if PLATFORM_WINDOWS
    if (const auto Application = _WatchedApplication.Pin(); Application.IsValid())
    { static_cast<FWindowsApplication*>(Application.Get())->RemoveMessageHandler(_Watcher); }
#endif
    _WatchedApplication.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_EditorTheme_Controller::
    OnTick(
        float InDeltaTime)
    -> bool
{
#if PLATFORM_WINDOWS
    if (_Watcher.Consume_Pending() &&
        GetDefault<UCk_EditorTheme_UserSettings_UE>()->Get_Mode() == ECk_EditorTheme_Mode::FollowWindows)
    { DoReconcile(); }
#endif

    if (_IconPatch.Request_RefreshStyleSets() || _LightPatch.Request_ApplyNodeTitleInk())
    { FSlateApplication::Get().InvalidateAllWidgets(false); }

    return true;
}

auto
    FCk_EditorTheme_Controller::
    OnThemeChanged(
        FGuid InThemeId)
    -> void
{
    DoApply_ThemeDependentStyles();

    auto* Settings = GetMutableDefault<UCk_EditorTheme_UserSettings_UE>();
    const auto DesiredThemeId = ck::editor_theme::Get_DesiredThemeId(Settings->Get_Mode(), DoRead_AppsUseLightTheme());
    const auto PreviousThemeId = _LastThemeId;
    _LastThemeId = InThemeId;

    if (NOT ck::editor_theme::Get_ShouldDemoteToManual(Settings->Get_Mode(), InThemeId, PreviousThemeId, DesiredThemeId))
    { return; }

    Settings->Set_Mode(ECk_EditorTheme_Mode::Manual);
    Settings->SaveConfig();

    ck::editor_theme::Log(TEXT("A theme was picked in Appearance; Ck Editor Theme mode is now Manual"));
}

auto
    FCk_EditorTheme_Controller::
    OnSettingChanged(
        UObject* InSettings,
        FPropertyChangedEvent& InEvent)
    -> void
{
    DoReconcile();
    DoApply_ThemeDependentStyles();
}

auto
    FCk_EditorTheme_Controller::
    OnPreShutdown()
    -> void
{
    DoUnregister_Watcher();
    _IconPatch.Request_Release(FCk_EditorTheme_IconPatch::ECk_TextureRelease::Free);
}

// --------------------------------------------------------------------------------------------------------------------
