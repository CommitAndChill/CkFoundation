#pragma once

#include "CkEditorTheme/Styling/CkEditorTheme_GraphPatch.h"
#include "CkEditorTheme/Styling/CkEditorTheme_IconPatch.h"
#include "CkEditorTheme/Styling/CkEditorTheme_LightStylePatch.h"

#include "CkCore/Macros/CkMacros.h"

#include "Containers/Ticker.h"
#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsApplication.h"
#endif

class GenericApplication;
class UObject;
struct FPropertyChangedEvent;

// --------------------------------------------------------------------------------------------------------------------

#if PLATFORM_WINDOWS
class FCk_EditorTheme_WindowsWatcher : public IWindowsMessageHandler
{
public:
    auto ProcessMessage(
        HWND InHwnd,
        uint32 InMessage,
        WPARAM InWParam,
        LPARAM InLParam,
        int32& OutResult) -> bool override;

    auto Consume_Pending() -> bool;

private:
    bool _Pending = false;
};
#endif

// --------------------------------------------------------------------------------------------------------------------

class FCk_EditorTheme_Controller
{
public:
    auto Start() -> void;
    auto Stop() -> void;

private:
    auto DoPatch_Styles() -> void;
    auto DoApply_ThemeDependentStyles() -> void;
    auto DoStage_Themes() -> void;
    auto DoReconcile() -> void;
    auto DoForceDark_ForSession() -> void;
    auto DoRead_AppsUseLightTheme() const -> FString;
    auto DoRegister_Watcher() -> void;
    auto DoUnregister_Watcher() -> void;

    auto OnTick(float InDeltaTime) -> bool;
    auto OnThemeChanged(FGuid InThemeId) -> void;
    auto OnSettingChanged(UObject* InSettings, FPropertyChangedEvent& InEvent) -> void;
    auto OnPreShutdown() -> void;

private:
#if PLATFORM_WINDOWS
    FCk_EditorTheme_WindowsWatcher _Watcher;
#endif
    FCk_EditorTheme_LightStylePatch _LightPatch;
    FCk_EditorTheme_IconPatch _IconPatch;
    FCk_EditorTheme_GraphPatch _GraphPatch;
    TWeakPtr<GenericApplication> _WatchedApplication;
    FTSTicker::FDelegateHandle _TickerHandle;
    FDelegateHandle _ThemeChangedHandle;
    FDelegateHandle _SettingChangedHandle;
    FDelegateHandle _PreShutdownHandle;
    FGuid _LastThemeId;
    bool _HasLoggedMissingTheme = false;
};

// --------------------------------------------------------------------------------------------------------------------
