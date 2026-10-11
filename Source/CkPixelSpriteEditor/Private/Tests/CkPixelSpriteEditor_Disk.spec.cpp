#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkCore/Macros/CkMacros.h"

#include "EditorValidatorSubsystem.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_DiskReload,
    "Ck.PixelSprite.Import.DiskReloadWithoutSources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_DiskReload::
    RunTest(const FString&)
    -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin is available"), Plugin.IsValid()))
    {
        return false;
    }

    auto Manifest = FCk_PixelSprite_Manifest{};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT TestTrue(TEXT("Manifest loads before saving"), ck::pixel_sprite::TryLoad_Manifest(
        Plugin->GetBaseDir() / TEXT("Tools/CkPixel/Fixtures/Manifest/valid-loop.json"),
        Manifest, Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }

    const auto Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const auto Mount = FString::Printf(TEXT("/PixelSpriteDisk%s/"), *Id);
    const auto Directory = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectSavedDir() / TEXT("PixelSpriteDiskTests") / Id) + TEXT("/");
    if (NOT TestTrue(TEXT("Create owned package directory"),
        IFileManager::Get().MakeDirectory(*Directory, true)))
    {
        return false;
    }
    FPackageName::RegisterMountPoint(Mount, Directory);
    ON_SCOPE_EXIT { FPackageName::UnRegisterMountPoint(Mount, Directory); };

    // These source paths have never existed. Reload must consume the owned indexed payload.
    for (auto Index = int32{0}; Index < Manifest._Sources.Num(); ++Index)
    {
        Manifest._Sources[Index]._Path = Directory / FPaths::GetCleanFilename(Manifest._Sources[Index]._Path);
    }
    const auto PackageName = Mount + TEXT("Animation");
    const auto Filename = Directory / TEXT("Animation.uasset");
    auto Package = TStrongObjectPtr<UPackage>{CreatePackage(*PackageName)};
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>(
        Package.Get(), TEXT("Animation"), RF_Public | RF_Standalone)};
    if (NOT TestTrue(TEXT("Publish manifest for serialization"),
        Asset->Request_ReplaceManifest(Manifest, Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }
    auto Settings = FCk_PixelSprite_RenderSettings{};
    Settings._Scale = 3;
    Settings._Mode = ECk_PixelSprite_RenderMode::SmoothSilhouette;
    Settings._PaletteOverrides.Add(1, FColor::Green);
    Asset->Request_SetRenderSettings(Settings);
    Asset->Set_SourceFilename(Directory / TEXT("offline.json"));
    const auto SavedSnapshot = Asset->Get_Snapshot();
    auto SaveArgs = FSavePackageArgs{};
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    {
        // This serialization test owns a temporary mount, not a Content asset.
        // Disable only this save's deferred editor validation; otherwise its queued
        // localization validator runs after the test unregisters the mount.
        const auto DisableValidation = FScopedDisableValidateOnSave{};
        if (NOT TestTrue(TEXT("Save real asset package to disk"),
            UPackage::SavePackage(Package.Get(), Asset.Get(), *Filename, SaveArgs)))
        {
            return false;
        }
    }

    const auto WeakAsset = TWeakObjectPtr<UCk_PixelSpriteAsset_UE>{Asset.Get()};
    Asset->ClearFlags(RF_Standalone);
    ResetLoaders(Package.Get());
    Asset.Reset();
    Package.Reset();
    CollectGarbage(RF_NoFlags);
    if (NOT TestFalse(TEXT("Original UObject is collected before reload"), WeakAsset.IsValid()))
    {
        return false;
    }

    auto ReloadedPackage = TStrongObjectPtr<UPackage>{LoadPackage(nullptr, *PackageName, LOAD_None)};
    if (NOT TestTrue(TEXT("Reload package from disk"), ReloadedPackage.IsValid()))
    {
        return false;
    }
    auto Reloaded = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{FindObject<UCk_PixelSpriteAsset_UE>(
        ReloadedPackage.Get(), TEXT("Animation"))};
    if (NOT TestTrue(TEXT("Find reloaded asset"), Reloaded.IsValid()))
    {
        return false;
    }
    const auto Snapshot = Reloaded->Get_Snapshot();
    if (NOT TestTrue(TEXT("Reload reconstructs valid indexed snapshot without source files"), Snapshot.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("Source count survives disk reload"), Snapshot->_Sources.Num(), SavedSnapshot->_Sources.Num());
    TestEqual(TEXT("Animation count survives disk reload"), Snapshot->_Animations.Num(), SavedSnapshot->_Animations.Num());
    TestTrue(TEXT("Indexed pixels survive disk reload"),
        Snapshot->_Sources[0]._Grid._Indices == SavedSnapshot->_Sources[0]._Grid._Indices);
    TestEqual(TEXT("Authored duration survives disk reload"),
        Snapshot->_Animations[0]._Frames[0]._DurationMilliseconds,
        SavedSnapshot->_Animations[0]._Frames[0]._DurationMilliseconds);
    TestEqual(TEXT("Preview settings survive disk reload"), Reloaded->Get_RenderSettings()._Scale, 3);
    TestEqual(TEXT("Palette override survives disk reload"),
        Reloaded->Get_RenderSettings()._PaletteOverrides.FindRef(1), FColor::Green);
    TestFalse(TEXT("Source manifest remains unavailable"),
        IFileManager::Get().FileExists(*Reloaded->Get_SourceFilename()));
    return true;
}

#endif
