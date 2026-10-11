#include "CkPixelSpriteEditor/CkPixelSpriteEditor_Factory.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkCore/Macros/CkMacros.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Examples,
    "Ck.PixelSprite.Examples.ImportNativeAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_Examples::
    RunTest(const FString&)
    -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin is available"), Plugin.IsValid()))
    {
        return false;
    }
    const auto Root = Plugin->GetBaseDir() / TEXT("Tools/CkPixel/Examples");
    auto Factory = TStrongObjectPtr<UCk_PixelSpriteFactory_UE>{NewObject<UCk_PixelSpriteFactory_UE>()};
    for (const auto& Name : {FString{TEXT("Character")}, FString{TEXT("Robot")}})
    {
        const auto PackageName = TEXT("/Game/PixelSpriteExamples/") + Name;
        const auto Filename = FPackageName::LongPackageNameToFilename(PackageName, TEXT(".uasset"));
        if (IFileManager::Get().FileExists(*Filename))
        {
            auto Existing = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
                LoadObject<UCk_PixelSpriteAsset_UE>(nullptr, *(PackageName + TEXT(".") + Name))};
            TestTrue(TEXT("Existing example is a valid pixel sprite; preserve it"),
                Existing.IsValid() && Existing->Get_Snapshot().IsValid() && Existing->Get_Width() == 16);
            continue;
        }
        auto Package = TStrongObjectPtr<UPackage>{CreatePackage(*PackageName)};
        const auto Source = Root / (Name == TEXT("Character") ?
            TEXT("Character/character.json") : TEXT("Robot.xpm"));
        auto Canceled = false;
        auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
            Cast<UCk_PixelSpriteAsset_UE>(Factory->FactoryCreateFile(
                UCk_PixelSpriteAsset_UE::StaticClass(), Package.Get(), FName{Name},
                RF_Public | RF_Standalone, Source, nullptr, GWarn, Canceled))};
        if (NOT TestTrue(TEXT("Example imports through the production factory"), Asset.IsValid()))
        {
            return false;
        }
        if (NOT TestTrue(TEXT("Create example content directory"),
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true)))
        {
            return false;
        }
        auto SaveArgs = FSavePackageArgs{};
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (NOT TestTrue(TEXT("Save native example asset"),
            UPackage::SavePackage(Package.Get(), Asset.Get(), *Filename, SaveArgs)))
        {
            return false;
        }
        TestEqual(TEXT("Example canvas width"), Asset->Get_Width(), 16);
        TestEqual(TEXT("Example canvas height"), Asset->Get_Height(), 16);
        TestEqual(TEXT("Example sequences"), Asset->Get_AnimationNames().Num(), Name == TEXT("Character") ? 3 : 1);
    }
    return true;
}

#endif
