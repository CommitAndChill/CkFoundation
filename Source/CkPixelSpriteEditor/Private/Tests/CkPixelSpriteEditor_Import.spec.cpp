#include "CkPixelSpriteEditor/CkPixelSpriteEditor_AssetDefinition.h"
#include "CkPixelSpriteEditor/CkPixelSpriteEditor_Factory.h"
#include "CkPixelSpriteEditor/CkPixelSpriteEditor_ViewerBridge.h"

#include "AssetRegistry/AssetData.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "EditorFramework/AssetImportData.h"
#include "Editor/EditorEngine.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_EditorImport,
    "Ck.PixelSprite.Import.FactoryReimport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_EditorImport::
    RunTest(const FString&)
    -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin is available"), Plugin.IsValid()))
    {
        return false;
    }

    const auto FixtureRoot = FPaths::Combine(Plugin->GetBaseDir(),
        TEXT("Tools/CkPixel/Fixtures/NativeFile/ordinary"));
    const auto XpmPath = FPaths::Combine(FixtureRoot, TEXT("red.xpm"));
    const auto ManifestPath = FPaths::Combine(FixtureRoot, TEXT("ordinary-valid.json"));
    const auto BadManifestPath = FPaths::Combine(FixtureRoot,
        TEXT("late-missing-second-source.json"));
    auto Factory = TStrongObjectPtr<UCk_PixelSpriteFactory_UE>{NewObject<UCk_PixelSpriteFactory_UE>()};
    TestTrue(TEXT("Factory recognizes XPM"), Factory->FactoryCanImport(XpmPath));
    TestTrue(TEXT("Factory recognizes manifest"), Factory->FactoryCanImport(ManifestPath));
    TestFalse(TEXT("Malformed JSON is not claimed as a sprite manifest"),
        Factory->FactoryCanImport(FPaths::Combine(Plugin->GetBaseDir(),
            TEXT("Tools/CkPixel/Fixtures/Manifest/invalid-malformed.json"))));
    TestFalse(TEXT("Schema-invalid JSON is not claimed as a sprite manifest"),
        Factory->FactoryCanImport(FPaths::Combine(Plugin->GetBaseDir(),
            TEXT("Tools/CkPixel/Fixtures/Manifest/invalid-duration-zero.json"))));
    TestFalse(TEXT("Non-JSON file is not claimed as a sprite manifest"),
        Factory->FactoryCanImport(FPaths::Combine(FixtureRoot, TEXT("ordinary-valid.txt"))));

    auto Canceled = false;
    auto Image = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        Cast<UCk_PixelSpriteAsset_UE>(Factory->FactoryCreateFile(
            UCk_PixelSpriteAsset_UE::StaticClass(), GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UCk_PixelSpriteAsset_UE::StaticClass()),
            RF_Transient, XpmPath, nullptr, GWarn, Canceled))};
    if (NOT TestTrue(TEXT("XPM imports"), Image.IsValid()))
    {
        return false;
    }
    const auto ImageSnapshot = Image->Get_Snapshot();
    TestTrue(TEXT("XPM stores owned source and one default animation"),
        ImageSnapshot.IsValid() && ImageSnapshot->_Sources.Num() == 1 &&
        ImageSnapshot->_Animations.Num() == 1 &&
        ImageSnapshot->_Animations[0]._Name == TEXT("default") &&
        ImageSnapshot->_Animations[0]._Frames.Num() == 1);
    TestEqual(TEXT("XPM source path retained"), Image->Get_SourceFilename(),
        FPaths::ConvertRelativePathToFull(XpmPath));

    // Each test owns a unique ordinary tree. Never alter checked-in authoring sources.
    const auto OwnedRoot = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("PixelSpriteEditorTests"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
    if (NOT TestTrue(TEXT("Create owned source directory"),
        IFileManager::Get().MakeDirectory(*OwnedRoot, true)))
    {
        return false;
    }
    const auto OwnedManifest = FPaths::Combine(OwnedRoot, TEXT("animation.json"));
    const auto OwnedRed = FPaths::Combine(OwnedRoot, TEXT("red.xpm"));
    const auto OwnedBlue = FPaths::Combine(OwnedRoot, TEXT("blue.xpm"));
    ON_SCOPE_EXIT
    {
        IFileManager::Get().Delete(*OwnedManifest);
        IFileManager::Get().Delete(*OwnedRed);
        IFileManager::Get().Delete(*OwnedBlue);
        IFileManager::Get().DeleteDirectory(*OwnedRoot, false, false);
    };
    for (const auto& Name : {TEXT("ordinary-valid.json"), TEXT("red.xpm"), TEXT("blue.xpm")})
    {
        const auto Original = FPaths::Combine(FixtureRoot, Name);
        const auto Target = FPaths::Combine(OwnedRoot,
            FString{Name}.Equals(TEXT("ordinary-valid.json")) ? TEXT("animation.json") : Name);
        auto Bytes = TArray<uint8>{};
        if (NOT TestTrue(FString::Printf(TEXT("Read %s"), Name),
            FFileHelper::LoadFileToArray(Bytes, *Original)) ||
            NOT TestTrue(FString::Printf(TEXT("Copy %s"), Name),
                FFileHelper::SaveArrayToFile(Bytes, *Target)))
        {
            return false;
        }
    }

    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        Cast<UCk_PixelSpriteAsset_UE>(Factory->FactoryCreateFile(
            UCk_PixelSpriteAsset_UE::StaticClass(), GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UCk_PixelSpriteAsset_UE::StaticClass()),
            RF_Transient, OwnedManifest, nullptr, GWarn, Canceled))};
    if (NOT TestTrue(TEXT("Manifest imports"), Asset.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("Manifest source table"), Asset->Get_Snapshot()->_Sources.Num(), 2);
    TestEqual(TEXT("ImportData tracks manifest and both frame files"),
        Asset->Get_ImportData()->GetSourceData().SourceFiles.Num(), 3);
    auto RegisteredFilenames = TArray<FString>{};
    TestTrue(TEXT("Reimport handler accepts asset"),
        Factory->CanReimport(Asset.Get(), RegisteredFilenames));
    TestEqual(TEXT("Reimport lists all source files"), RegisteredFilenames.Num(), 3);

    const auto RegistryData = FAssetData{Asset.Get()};
    auto SourceTag = FString{};
    TestTrue(TEXT("Asset registry exposes source dependencies"),
        RegistryData.GetTagValue(UObject::SourceFileTagName(), SourceTag));
    TestTrue(TEXT("Source tag names both frame files"),
        SourceTag.Contains(TEXT("red.xpm")) && SourceTag.Contains(TEXT("blue.xpm")));

    auto Settings = Asset->Get_RenderSettings();
    Settings._Scale = 3;
    Asset->Request_SetRenderSettings(Settings);
    const auto BeforeFailure = Asset->Get_Snapshot();
    const auto RevisionBeforeFailure = Asset->Get_Revision();
    const auto SourceBeforeFailure = Asset->Get_SourceFilename();
    const auto ImportBeforeFailure = Asset->Get_ImportData()->GetSourceData().ToJson();
    const auto DirtyBeforeFailure = Asset->GetOutermost()->IsDirty();
    AddExpectedError(TEXT("source path component is missing, linked, or cannot be inspected"),
        EAutomationExpectedErrorFlags::Contains, 2);
    Factory->SetReimportPaths(Asset.Get(), TArray<FString>{BadManifestPath});
    TestEqual(TEXT("Missing second source rejects reimport"),
        Factory->Reimport(Asset.Get()), EReimportResult::Failed);
    TestTrue(TEXT("Failed reimport retains snapshot"),
        Asset->Get_Snapshot() == BeforeFailure);
    TestEqual(TEXT("Failed reimport retains revision"), Asset->Get_Revision(),
        RevisionBeforeFailure);
    TestEqual(TEXT("Failed reimport retains source path"), Asset->Get_SourceFilename(),
        SourceBeforeFailure);
    TestEqual(TEXT("Failed reimport retains dependency metadata"),
        Asset->Get_ImportData()->GetSourceData().ToJson(), ImportBeforeFailure);
    TestEqual(TEXT("Failed direct reimport retains dirty state"),
        Asset->GetOutermost()->IsDirty(), DirtyBeforeFailure);
    TestEqual(TEXT("Failed reimport retains preview settings"),
        Asset->Get_RenderSettings()._Scale, 3);

    auto RedBytes = TArray<uint8>{};
    if (NOT TestTrue(TEXT("Load replacement frame"), FFileHelper::LoadFileToArray(RedBytes, *OwnedRed)) ||
        NOT TestTrue(TEXT("Change only owned second frame"),
            FFileHelper::SaveArrayToFile(RedBytes, *OwnedBlue)))
    {
        return false;
    }
    TestEqual(TEXT("Indexed frame reimport reloads manifest"),
        Factory->Reimport(Asset.Get(), 2), EReimportResult::Succeeded);
    TestTrue(TEXT("Changed frame creates new snapshot"),
        Asset->Get_Snapshot() != BeforeFailure);
    TestEqual(TEXT("Successful reimport retains preview settings"),
        Asset->Get_RenderSettings()._Scale, 3);
    TestEqual(TEXT("Reimport retains source table"), Asset->Get_Snapshot()->_Sources.Num(), 2);
    TestEqual(TEXT("Changed second frame now has red palette"),
        Asset->Get_Snapshot()->_Sources[1]._Grid._Palette[1]._Color, FColor::Red);

    const auto MetadataBeforeJsonChange = Asset->Get_ImportData()->GetSourceData().ToJson();
    const auto RevisionBeforeJsonChange = Asset->Get_Revision();
    const auto ChangedManifest = FString{
        TEXT("{\"version\":1,\"canvas\":{\"width\":2,\"height\":2},")
        TEXT("\"metadata\":{\"reimport_marker\":\"json_rewrite\"},")
        TEXT("\"animations\":{\"idle\":{\"playback\":\"once\",\"frames\":[")
        TEXT("{\"source\":\"blue.xpm\",\"duration_ms\":250,\"anchors\":{\"origin\":[1,1]}},")
        TEXT("{\"source\":\"red.xpm\",\"duration_ms\":75}]}}}")};
    if (NOT TestTrue(TEXT("Rewrite owned manifest at its original path"),
        FFileHelper::SaveStringToFile(ChangedManifest, *OwnedManifest)) ||
        NOT TestEqual(TEXT("Edited JSON reimports from source index zero"),
            Factory->Reimport(Asset.Get(), 0), EReimportResult::Succeeded))
    {
        return false;
    }
    const auto ChangedSnapshot = Asset->Get_Snapshot();
    if (NOT TestTrue(TEXT("Edited JSON publishes a complete animation"),
        ChangedSnapshot.IsValid() && ChangedSnapshot->_Sources.Num() == 2 &&
        ChangedSnapshot->_Animations.Num() == 1 &&
        ChangedSnapshot->_Animations[0]._Frames.Num() == 2))
    {
        return false;
    }
    const auto& ChangedAnimation = ChangedSnapshot->_Animations[0];
    TestTrue(TEXT("Edited JSON changes playback to once"),
        ChangedAnimation._Playback == ECk_PixelSprite_Playback::Once);
    TestEqual(TEXT("Edited JSON puts blue source first"),
        ChangedAnimation._Frames[0]._Source, FString{TEXT("blue.xpm")});
    TestEqual(TEXT("Edited JSON puts red source second"),
        ChangedAnimation._Frames[1]._Source, FString{TEXT("red.xpm")});
    TestTrue(TEXT("Edited JSON changes indexed source order"),
        ChangedAnimation._Frames[0]._SourceIndex == 0 &&
        ChangedAnimation._Frames[1]._SourceIndex == 1 &&
        ChangedSnapshot->_Sources[0]._Path.EndsWith(TEXT("/blue.xpm")) &&
        ChangedSnapshot->_Sources[1]._Path.EndsWith(TEXT("/red.xpm")));
    TestEqual(TEXT("Edited JSON changes first duration"),
        ChangedAnimation._Frames[0]._DurationMilliseconds, 250.0);
    TestEqual(TEXT("Edited JSON changes second duration"),
        ChangedAnimation._Frames[1]._DurationMilliseconds, 75.0);
    TestTrue(TEXT("Edited JSON adds origin anchor"),
        ChangedAnimation._Frames[0]._Anchors.Contains(TEXT("origin")));
    if (ChangedAnimation._Frames[0]._Anchors.Contains(TEXT("origin")))
    {
        TestTrue(TEXT("Edited JSON preserves origin position"),
            ChangedAnimation._Frames[0]._Anchors.FindChecked(TEXT("origin")) == FIntPoint{1, 1});
    }
    TestTrue(TEXT("Edited JSON retains root metadata marker"),
        ChangedSnapshot->_MetadataJson.Contains(TEXT("json_rewrite")));
    TestEqual(TEXT("Edited JSON retains root source path"), Asset->Get_SourceFilename(),
        FPaths::ConvertRelativePathToFull(OwnedManifest));
    TestEqual(TEXT("Edited JSON retains root and two frame dependencies"),
        Asset->Get_ImportData()->GetSourceData().SourceFiles.Num(), 3);
    TestTrue(TEXT("Edited JSON changes import hash metadata"),
        Asset->Get_ImportData()->GetSourceData().ToJson() != MetadataBeforeJsonChange);
    TestEqual(TEXT("Edited JSON retains preview scale"), Asset->Get_RenderSettings()._Scale, 3);
    TestTrue(TEXT("Edited JSON advances asset revision"), Asset->Get_Revision() > RevisionBeforeJsonChange);
    auto RedAfterJsonChange = TArray<uint8>{};
    auto BlueAfterJsonChange = TArray<uint8>{};
    if (TestTrue(TEXT("Read authored XPM sources after JSON reimport"),
        FFileHelper::LoadFileToArray(RedAfterJsonChange, *OwnedRed) &&
        FFileHelper::LoadFileToArray(BlueAfterJsonChange, *OwnedBlue)))
    {
        TestTrue(TEXT("JSON reimport leaves both authored XPM byte streams unchanged"),
            RedAfterJsonChange == RedBytes && BlueAfterJsonChange == RedBytes);
    }

    IFileManager::Get().Delete(*OwnedBlue);
    const auto OfflineSnapshot = Asset->Get_Snapshot();
    TestEqual(TEXT("Missing source rejects later reimport"),
        Factory->Reimport(Asset.Get()), EReimportResult::Failed);
    TestTrue(TEXT("Cookable payload remains available offline"),
        Asset->Get_Snapshot() == OfflineSnapshot &&
        Asset->Get_Snapshot()->_Sources[1]._Grid._Indices.Num() == 4);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_EditorReimportUndoRedo,
    "Ck.PixelSprite.Import.ReimportUndoRedo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_EditorReimportUndoRedo::
    RunTest(const FString&)
    -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin is available"), Plugin.IsValid()) ||
        NOT TestTrue(TEXT("Editor transactions are available"), GEditor && GEditor->CanTransact()))
    {
        return false;
    }

    const auto FixtureRoot = Plugin->GetBaseDir() /
        TEXT("Tools/CkPixel/Fixtures/NativeFile/ordinary");
    const auto OwnedRoot = FPaths::ProjectSavedDir() / TEXT("PixelSpriteEditorTests") /
        FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if (NOT TestTrue(TEXT("Create owned reimport source directory"),
        IFileManager::Get().MakeDirectory(*OwnedRoot, true)))
    {
        return false;
    }
    const auto OriginalManifest = OwnedRoot / TEXT("original.json");
    const auto ReplacementManifest = OwnedRoot / TEXT("replacement.json");
    const auto OwnedRed = OwnedRoot / TEXT("red.xpm");
    const auto OwnedBlue = OwnedRoot / TEXT("blue.xpm");
    ON_SCOPE_EXIT
    {
        IFileManager::Get().Delete(*OriginalManifest);
        IFileManager::Get().Delete(*ReplacementManifest);
        IFileManager::Get().Delete(*OwnedRed);
        IFileManager::Get().Delete(*OwnedBlue);
        IFileManager::Get().DeleteDirectory(*OwnedRoot, false, false);
    };
    for (const auto& Name : {TEXT("ordinary-valid.json"), TEXT("red.xpm"), TEXT("blue.xpm")})
    {
        auto Bytes = TArray<uint8>{};
        if (NOT TestTrue(FString::Printf(TEXT("Read %s"), Name),
            FFileHelper::LoadFileToArray(Bytes, *(FixtureRoot / Name))))
        {
            return false;
        }
        if (FString{Name}.Equals(TEXT("ordinary-valid.json")))
        {
            if (NOT TestTrue(TEXT("Write original manifest"),
                FFileHelper::SaveArrayToFile(Bytes, *OriginalManifest)) ||
                NOT TestTrue(TEXT("Write replacement manifest"),
                    FFileHelper::SaveArrayToFile(Bytes, *ReplacementManifest)))
            {
                return false;
            }
        }
        else if (NOT TestTrue(FString::Printf(TEXT("Write %s"), Name),
            FFileHelper::SaveArrayToFile(Bytes, *(OwnedRoot / Name))))
        {
            return false;
        }
    }

    auto Factory = TStrongObjectPtr<UCk_PixelSpriteFactory_UE>{NewObject<UCk_PixelSpriteFactory_UE>()};
    auto Canceled = false;
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        Cast<UCk_PixelSpriteAsset_UE>(Factory->FactoryCreateFile(
            UCk_PixelSpriteAsset_UE::StaticClass(), GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UCk_PixelSpriteAsset_UE::StaticClass()),
            RF_Transient | RF_Transactional, OriginalManifest, nullptr, GWarn, Canceled))};
    if (NOT TestTrue(TEXT("Original manifest imports transactionally"), Asset.IsValid()) ||
        NOT TestTrue(TEXT("Import data is transactional"),
            Asset->Get_ImportData() && Asset->Get_ImportData()->HasAnyFlags(RF_Transactional)))
    {
        return false;
    }
    auto Settings = Asset->Get_RenderSettings();
    Settings._Scale = 3;
    Asset->Request_SetRenderSettings(Settings);
    const auto OriginalSnapshot = Asset->Get_Snapshot();
    const auto OriginalMetadata = Asset->Get_ImportData()->GetSourceData().ToJson();
    const auto OriginalRevision = Asset->Get_Revision();
    if (NOT TestTrue(TEXT("Original second source is blue"),
        OriginalSnapshot.IsValid() && OriginalSnapshot->_Sources.Num() == 2 &&
        OriginalSnapshot->_Sources[1]._Grid._Palette.IsValidIndex(1) &&
        OriginalSnapshot->_Sources[1]._Grid._Palette[1]._Color == FColor::Blue))
    {
        return false;
    }

    auto RedBytes = TArray<uint8>{};
    if (NOT TestTrue(TEXT("Read replacement pixels"),
        FFileHelper::LoadFileToArray(RedBytes, *OwnedRed)) ||
        NOT TestTrue(TEXT("Replace only owned second source"),
            FFileHelper::SaveArrayToFile(RedBytes, *OwnedBlue)))
    {
        return false;
    }
    Factory->SetReimportPaths(Asset.Get(), TArray<FString>{ReplacementManifest});
    if (NOT TestEqual(TEXT("Reimport alternate manifest"), Factory->Reimport(Asset.Get()),
        EReimportResult::Succeeded))
    {
        return false;
    }
    if (NOT TestTrue(TEXT("Reimport retains import data"), Asset->Get_ImportData() != nullptr))
    {
        return false;
    }
    const auto ReplacementMetadata = Asset->Get_ImportData()->GetSourceData().ToJson();
    TestTrue(TEXT("Reimport updates dependency metadata"), ReplacementMetadata != OriginalMetadata);
    TestEqual(TEXT("Reimport publishes replacement source path"),
        Asset->Get_SourceFilename(), FPaths::ConvertRelativePathToFull(ReplacementManifest));
    const auto ReplacementSnapshot = Asset->Get_Snapshot();
    if (NOT TestTrue(TEXT("Reimport snapshot contains second-source pixels"),
        ReplacementSnapshot.IsValid() && ReplacementSnapshot->_Sources.Num() == 2 &&
        ReplacementSnapshot->_Sources[1]._Grid._Palette.IsValidIndex(1)))
    {
        return false;
    }
    TestEqual(TEXT("Reimport publishes red second source"),
        ReplacementSnapshot->_Sources[1]._Grid._Palette[1]._Color, FColor::Red);
    TestEqual(TEXT("Reimport retains preview settings"), Asset->Get_RenderSettings()._Scale, 3);
    const auto ReplacementRevision = Asset->Get_Revision();
    TestTrue(TEXT("Reimport advances revision"), ReplacementRevision > OriginalRevision);

    if (NOT TestTrue(TEXT("Reimport transaction undoes"), GEditor->UndoTransaction()))
    {
        return false;
    }
    if (NOT TestTrue(TEXT("Undo retains import data"), Asset->Get_ImportData() != nullptr))
    {
        return false;
    }
    TestEqual(TEXT("Undo restores original source path"),
        Asset->Get_SourceFilename(), FPaths::ConvertRelativePathToFull(OriginalManifest));
    TestEqual(TEXT("Undo restores original dependency metadata"),
        Asset->Get_ImportData()->GetSourceData().ToJson(), OriginalMetadata);
    const auto UndoSnapshot = Asset->Get_Snapshot();
    if (NOT TestTrue(TEXT("Undo snapshot contains second-source pixels"),
        UndoSnapshot.IsValid() && UndoSnapshot->_Sources.Num() == 2 &&
        UndoSnapshot->_Sources[1]._Grid._Palette.IsValidIndex(1)))
    {
        return false;
    }
    TestEqual(TEXT("Undo restores original blue pixels"),
        UndoSnapshot->_Sources[1]._Grid._Palette[1]._Color, FColor::Blue);
    TestEqual(TEXT("Undo retains preview settings"), Asset->Get_RenderSettings()._Scale, 3);
    TestTrue(TEXT("Undo invalidates cached snapshot"), UndoSnapshot != OriginalSnapshot);
    TestTrue(TEXT("Undo advances revision"), Asset->Get_Revision() > ReplacementRevision);
    const auto UndoRevision = Asset->Get_Revision();

    if (NOT TestTrue(TEXT("Reimport transaction redoes"), GEditor->RedoTransaction()))
    {
        return false;
    }
    if (NOT TestTrue(TEXT("Redo retains import data"), Asset->Get_ImportData() != nullptr))
    {
        return false;
    }
    TestEqual(TEXT("Redo restores replacement source path"),
        Asset->Get_SourceFilename(), FPaths::ConvertRelativePathToFull(ReplacementManifest));
    TestEqual(TEXT("Redo restores replacement dependency metadata"),
        Asset->Get_ImportData()->GetSourceData().ToJson(), ReplacementMetadata);
    const auto RedoSnapshot = Asset->Get_Snapshot();
    if (NOT TestTrue(TEXT("Redo snapshot contains second-source pixels"),
        RedoSnapshot.IsValid() && RedoSnapshot->_Sources.Num() == 2 &&
        RedoSnapshot->_Sources[1]._Grid._Palette.IsValidIndex(1)))
    {
        return false;
    }
    TestEqual(TEXT("Redo restores red pixels"),
        RedoSnapshot->_Sources[1]._Grid._Palette[1]._Color, FColor::Red);
    TestEqual(TEXT("Redo retains preview settings"), Asset->Get_RenderSettings()._Scale, 3);
    TestTrue(TEXT("Redo advances revision"), Asset->Get_Revision() > UndoRevision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_EditorOpen,
    "Ck.PixelSprite.Import.AssetOpening",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_EditorOpen::
    RunTest(const FString&)
    -> bool
{
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        NewObject<UCk_PixelSpriteAsset_UE>(GetTransientPackage())};
    auto Definition = TStrongObjectPtr<UCk_PixelSpriteAssetDefinition_UE>{
        NewObject<UCk_PixelSpriteAssetDefinition_UE>()};
    auto Assets = TArray<FAssetData>{FAssetData{Asset.Get()}};
    auto OpenArgs = FAssetOpenArgs{};
    OpenArgs.Assets = Assets;
    OpenArgs.OpenMethod = EAssetOpenMethod::Edit;

    auto& Delegate = ck::pixel_sprite_editor::Get_OpenViewer();
    auto Original = Delegate;
    ON_SCOPE_EXIT { Delegate = MoveTemp(Original); };
    Delegate.Unbind();
    TestEqual(TEXT("No viewer remains unhandled"), Definition->OpenAssets(OpenArgs),
        EAssetCommandResult::Unhandled);
    auto OpenCount = 0;
    Delegate.BindLambda([&OpenCount](UCk_PixelSpriteAsset_UE&, const FAssetOpenArgs&) -> bool
    {
        ++OpenCount;
        return true;
    });
    const auto Result = Definition->OpenAssets(OpenArgs);
    Delegate.Unbind();
    TestEqual(TEXT("Bound viewer handles opening"), Result, EAssetCommandResult::Handled);
    TestEqual(TEXT("Viewer receives one asset"), OpenCount, 1);
    return true;
}

#endif
