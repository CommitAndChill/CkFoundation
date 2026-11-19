#include "CkEditorStyle_Module.h"

#include "CkEditorStyle/CkEditorStyle_Utils.h"
#include "CkEditorStyle/AssetThumbnail/CkAssetThumbnailRenderer.h"
#include "CkEditorStyle/Settings/CkEditorStyle_Settings.h"

#include <ThumbnailRendering/ThumbnailManager.h>
#include <PropertyEditorModule.h>
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#define LOCTEXT_NAMESPACE "FCkEditorStyleModule"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEditorStyleModule::
    StartupModule()
    -> void
{
    const double StartTime = FPlatformTime::Seconds();
    UCk_Utils_EditorStyle_UE::DoRegister_SlateStyle();
    // The style set starts empty. Custom brushes load when they are registered.
    const bool bLegacyReload = FParse::Param(FCommandLine::Get(), TEXT("CkLegacyEmptyStyleReload"));
    if (bLegacyReload && !IsRunningGame() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
    UE_LOG(LogTemp, Log, TEXT("Initial CK style registration completed in %.3f ms (global texture reload: %s)"),
        (FPlatformTime::Seconds() - StartTime) * 1000.0, bLegacyReload ? TEXT("yes") : TEXT("no"));

    auto& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout
    (
        UCk_EditorStyle_ProjectSettings_UE::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&ck::layout::FEditorStyle_ProjectSettings_Details::MakeInstance)
    );

    UThumbnailManager::Get().UnregisterCustomRenderer(UBlueprint::StaticClass());
    UThumbnailManager::Get().RegisterCustomRenderer(UBlueprint::StaticClass(), UCk_CustomBlueprintThumbnailRenderer_UE::StaticClass());

    UThumbnailManager::Get().UnregisterCustomRenderer(UDataAsset::StaticClass());
    UThumbnailManager::Get().RegisterCustomRenderer(UDataAsset::StaticClass(), UCk_CustomDataAssetThumbnailRenderer_UE::StaticClass());
}

auto
    FCkEditorStyleModule::
    ShutdownModule()
    -> void
{
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        auto& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyModule.UnregisterCustomPropertyTypeLayout(UCk_EditorStyle_ProjectSettings_UE::StaticClass()->GetFName());
    }

    UCk_Utils_EditorStyle_UE::DoUnregister_SlateStyle();
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkEditorStyleModule, CkEditorStyle)
