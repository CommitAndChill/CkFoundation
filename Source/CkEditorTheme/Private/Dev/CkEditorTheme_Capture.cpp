#include "CkEditorTheme/CkEditorTheme_Log.h"
#include "CkEditorTheme/Settings/CkEditorTheme_Settings.h"

#include "CkCore/Macros/CkMacros.h"

#include "ContentBrowserModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "ISettingsModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/SWindow.h"

// --------------------------------------------------------------------------------------------------------------------

// Dev tool: renders editor windows to PNG without driving the desktop, for checking theme changes.
namespace ck_editor_theme_capture
{
    constexpr auto SettleSeconds = 2.5;

    struct FCapture
    {
        TArray<FString> Steps;
        double ResumeAt = 0.0;
        FTSTicker::FDelegateHandle Ticker;
    };

    auto Get_Capture()
        -> FCapture&
    {
        static auto Capture = FCapture{};
        return Capture;
    }

    auto Get_FileSafe(
        const FString& InText)
        -> FString
    {
        auto Safe = FString{};
        for (const auto Char : InText)
        { Safe.AppendChar(FChar::IsAlnum(Char) ? Char : TEXT('_')); }
        return Safe.Left(40);
    }

    auto DoShot(
        const FString& InLabel)
        -> void
    {
        const auto Dir = FPaths::ProjectSavedDir() / TEXT("CkEditorTheme/Capture");
        auto Index = 0;

        // Editor windows (preferences, asset editors) are children of the main frame, not top-level windows.
        auto Windows = TArray<TSharedRef<SWindow>>{FSlateApplication::Get().GetTopLevelWindows()};
        for (auto WindowIndex = 0; WindowIndex < Windows.Num(); ++WindowIndex)
        { Windows.Append(Windows[WindowIndex]->GetChildWindows()); }

        for (const auto& Window : Windows)
        {
            if (NOT Window->IsVisible() || Window->IsWindowMinimized() || Window->GetType() != EWindowType::Normal)
            { continue; }

            auto Colors = TArray<FColor>{};
            auto Size = FIntVector{};
            if (NOT FSlateApplication::Get().TakeScreenshot(Window, Colors, Size) || Size.X <= 0 || Size.Y <= 0)
            { continue; }

            for (auto& Color : Colors)
            { Color.A = 255; }

            auto Png = TArray64<uint8>{};
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>{Colors.GetData(), Colors.Num()}, Png);

            const auto File = Dir / FString::Printf(TEXT("%s_%d_%s.png"), *InLabel, Index++, *Get_FileSafe(Window->GetTitle().ToString()));
            FFileHelper::SaveArrayToFile(Png, *File);
            ck::editor_theme::Log(TEXT("Capture saved [{}]"), File);
        }
    }

    auto DoSetMode(
        ECk_EditorTheme_Mode InMode)
        -> void
    {
        auto* Settings = GetMutableDefault<UCk_EditorTheme_UserSettings_UE>();
        Settings->Set_Mode(InMode);

        auto Event = FPropertyChangedEvent{nullptr};
        Settings->OnSettingChanged().Broadcast(Settings, Event);
    }

    // Returns the seconds to wait before the next step.
    auto DoStep(
        const FString& InStep)
        -> double
    {
        auto Key = InStep;
        auto Value = FString{};
        InStep.Split(TEXT("="), &Key, &Value);

        if (Key == TEXT("light") || Key == TEXT("dark"))
        {
            DoSetMode(Key == TEXT("light") ? ECk_EditorTheme_Mode::Light : ECk_EditorTheme_Mode::Dark);
            return SettleSeconds;
        }

        if (Key == TEXT("prefs"))
        {
            FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings")).ShowViewer(TEXT("Editor"), TEXT("General"), TEXT("Appearance"));
            return SettleSeconds;
        }

        if (Key == TEXT("tab"))
        {
            FGlobalTabmanager::Get()->TryInvokeTab(FTabId{FName{*Value}});
            return SettleSeconds;
        }

        if (Key == TEXT("asset"))
        {
            if (auto* Asset = LoadObject<UObject>(nullptr, *Value); Asset != nullptr && GEditor != nullptr)
            { GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset); }
            else
            { ck::editor_theme::Warning(TEXT("Capture could not load [{}]"), Value); }
            return SettleSeconds * 2.0;
        }

        if (Key == TEXT("graph"))
        {
            auto Path = FString{};
            auto GraphName = FString{};
            Value.Split(TEXT(":"), &Path, &GraphName);
            // An optional third part centres the view on the first node whose title contains it (spaces ignored).
            auto NodeTitle = FString{};
            if (GraphName.Contains(TEXT(":")))
            { GraphName.Split(TEXT(":"), &GraphName, &NodeTitle); }

            auto* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
            if (Blueprint == nullptr)
            {
                ck::editor_theme::Warning(TEXT("Capture could not load blueprint [{}]"), Path);
                return 0.0;
            }

            auto Graphs = TArray<UEdGraph*>{};
            Blueprint->GetAllGraphs(Graphs);
            for (auto* Graph : Graphs)
            {
                if (Graph != nullptr && Graph->GetName().Replace(TEXT(" "), TEXT("")) == GraphName.Replace(TEXT(" "), TEXT("")))
                {
                    auto* Focus = static_cast<UObject*>(Graph);
                    for (const auto& Node : Graph->Nodes)
                    {
                        if (Node != nullptr && NOT NodeTitle.IsEmpty() && Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString()
                            .Replace(TEXT(" "), TEXT("")).Contains(NodeTitle.Replace(TEXT(" "), TEXT(""))))
                        {
                            Focus = Node.Get();
                            break;
                        }
                    }
                    FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Focus);
                    return SettleSeconds * 2.0;
                }
            }

            auto Names = FString{};
            for (const auto* Graph : Graphs)
            { Names += (Graph != nullptr ? Graph->GetName() : FString{}) + TEXT(", "); }
            ck::editor_theme::Warning(TEXT("Capture found no graph [{}] in [{}]; graphs: {}"), GraphName, Path, Names);
            return 0.0;
        }

        if (Key == TEXT("browse"))
        {
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToFolders({Value});
            return SettleSeconds;
        }

        if (Key == TEXT("wait"))
        { return FCString::Atod(*Value); }

        if (Key == TEXT("shot"))
        {
            DoShot(Value.IsEmpty() ? TEXT("shot") : Value);
            return 0.5;
        }

        if (Key == TEXT("quit") && GEngine != nullptr)
        {
            GEngine->Exec(nullptr, TEXT("QUIT_EDITOR"));
            return 0.0;
        }

        ck::editor_theme::Warning(TEXT("Capture step [{}] is not known"), InStep);
        return 0.0;
    }

    auto OnTick(
        float InDeltaTime)
        -> bool
    {
        auto& Capture = Get_Capture();
        if (FPlatformTime::Seconds() < Capture.ResumeAt)
        { return true; }

        if (Capture.Steps.IsEmpty())
        {
            ck::editor_theme::Log(TEXT("Capture done"));
            Capture.Ticker.Reset();
            return false;
        }

        const auto Step = Capture.Steps[0];
        Capture.Steps.RemoveAt(0);
        Capture.ResumeAt = FPlatformTime::Seconds() + DoStep(Step);
        return true;
    }

    auto OnCommand(
        const TArray<FString>& InArgs)
        -> void
    {
        auto& Capture = Get_Capture();
        Capture.Steps.Append(InArgs);

        if (NOT Capture.Ticker.IsValid())
        {
            // A first settle lets an -ExecCmds launch finish building its main frame.
            Capture.ResumeAt = FPlatformTime::Seconds() + SettleSeconds * 2.0;
            Capture.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&OnTick), 0.1f);
        }
    }

    FAutoConsoleCommand CaptureCommand(
        TEXT("Ck.EditorTheme.Capture"),
        TEXT("Dev: run capture steps in order. light | dark (saved like a Preferences change), prefs, tab=<TabId>, "
             "asset=<ObjectPath>, browse=<Folder>, graph=<BP>:<Graph>[:<NodeTitle>], wait=<Seconds>, "
             "shot=<Label> (Saved/CkEditorTheme/Capture/<Label>_<Window>.png), quit."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&OnCommand));
}

// --------------------------------------------------------------------------------------------------------------------
