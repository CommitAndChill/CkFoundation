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
    for (const auto& Name : {FString{TEXT("Character")}, FString{TEXT("Robot")}, FString{TEXT("Sproutling")}})
    {
        const auto ExpectedSize = Name == TEXT("Sproutling") ? 32 : 16;
        const auto PackageName = TEXT("/Game/PixelSpriteExamples/") + Name;
        const auto Filename = FPackageName::LongPackageNameToFilename(PackageName, TEXT(".uasset"));
        if (IFileManager::Get().FileExists(*Filename))
        {
            auto Existing = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
                LoadObject<UCk_PixelSpriteAsset_UE>(nullptr, *(PackageName + TEXT(".") + Name))};
            TestTrue(TEXT("Existing example is a valid pixel sprite; preserve it"),
                Existing.IsValid() && Existing->Get_Snapshot().IsValid() && Existing->Get_Width() == ExpectedSize);
            continue;
        }
        auto Package = TStrongObjectPtr<UPackage>{CreatePackage(*PackageName)};
        const auto Source = Root / (Name == TEXT("Character") ?
            TEXT("Character/character.json") : Name == TEXT("Sproutling") ?
            TEXT("Sproutling/sproutling.json") : TEXT("Robot.xpm"));
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
        TestEqual(TEXT("Example canvas width"), Asset->Get_Width(), ExpectedSize);
        TestEqual(TEXT("Example canvas height"), Asset->Get_Height(), ExpectedSize);
        TestEqual(TEXT("Example sequences"), Asset->Get_AnimationNames().Num(), Name == TEXT("Character") ? 3 : 1);
        if (Name == TEXT("Sproutling"))
        {
            const auto Snapshot = Asset->Get_Snapshot();
            if (NOT TestTrue(TEXT("Sproutling imports every native pose"), Snapshot.IsValid() &&
                Snapshot->_Sources.Num() == 8 && Snapshot->_Animations.Num() == 1 &&
                Snapshot->_Animations[0]._Frames.Num() == 8))
            {
                return false;
            }
            const auto& Rest = Snapshot->_Sources[0]._Grid;
            TestTrue(TEXT("Breathing changes source pixels"),
                Rest._Indices != Snapshot->_Sources[2]._Grid._Indices);
            TestTrue(TEXT("Final settled pose closes the loop"),
                Rest._Indices == Snapshot->_Sources[7]._Grid._Indices);
            for (const auto& Pose : Snapshot->_Sources)
            {
                auto GroundMatches = Rest._Indices.Num() == 32 * 32 &&
                    Pose._Grid._Indices.Num() == Rest._Indices.Num();
                if (GroundMatches)
                {
                    for (auto Index = int32{28 * 32}; Index < 32 * 32; ++Index)
                    {
                        GroundMatches &= Pose._Grid._Indices[Index] == Rest._Indices[Index];
                    }
                }
                TestTrue(TEXT("Every native pose keeps feet planted"),
                    GroundMatches);
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_SproutlingBorder,
    "Ck.PixelSprite.Examples.SproutlingBorder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_SproutlingBorder::
    RunTest(const FString&)
    -> bool
{
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{LoadObject<UCk_PixelSpriteAsset_UE>(nullptr,
        TEXT("/Game/PixelSpriteExamples/Sproutling.Sproutling"))};
    if (NOT TestTrue(TEXT("Sproutling example exists"), Asset.IsValid()))
    {
        return false;
    }
    auto Factory = TStrongObjectPtr<UCk_PixelSpriteFactory_UE>{NewObject<UCk_PixelSpriteFactory_UE>()};
    if (NOT TestTrue(TEXT("Corrected XPM reimports through production importer"),
        Factory->Reimport(Asset.Get()) == EReimportResult::Succeeded))
    {
        return false;
    }
    const auto Snapshot = Asset->Get_Snapshot();
    if (NOT TestTrue(TEXT("All eight frames survive reimport"), Snapshot.IsValid() && Snapshot->_Sources.Num() == 8))
    {
        return false;
    }
    for (const auto Source : {1, 5})
    {
        const auto& Grid = Snapshot->_Sources[Source]._Grid;
        if (NOT TestTrue(TEXT("Corrected frame has the expected canvas and border indices"),
            Grid._Width == 32 && Grid._Height == 32 && Grid._Indices.Num() == 1024))
        {
            return false;
        }
        if (NOT TestTrue(TEXT("Corrected frame references valid border colors"),
            Grid._Palette.IsValidIndex(Grid._Indices[13 * 32 + 6])
            && Grid._Palette.IsValidIndex(Grid._Indices[13 * 32 + 7])))
        {
            return false;
        }
        const auto Outside = Grid._Palette[Grid._Indices[13 * 32 + 6]]._Color;
        const auto Border = Grid._Palette[Grid._Indices[13 * 32 + 7]]._Color;
        TestTrue(TEXT("Shoulder outline follows the adjacent rows"), Outside.A == 0 && Border == FColor{56, 31, 46, 255});
    }
    const auto Filename = FPackageName::LongPackageNameToFilename(TEXT("/Game/PixelSpriteExamples/Sproutling"), TEXT(".uasset"));
    auto Args = FSavePackageArgs{};
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return TestTrue(TEXT("Save corrected native Sproutling example"), UPackage::SavePackage(Asset->GetPackage(), Asset.Get(), *Filename, Args));
}

#endif
