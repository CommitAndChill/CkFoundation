#include "CkPixelSpriteEditor/CkPixelSpriteEditor_Factory.h"

#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkPixelSprite/CkPixelSprite_Data.h"
#include "CkCore/Macros/CkMacros.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogCkPixelSpriteImport, Log, All);

namespace ck_pixel_sprite_editor_factory
{
    auto
        Is_Xpm(const FString& InPath)
        -> bool
    {
        return FPaths::GetExtension(InPath).Equals(TEXT("xpm"), ESearchCase::IgnoreCase);
    }

    auto
        Is_Manifest(const FString& InPath)
        -> bool
    {
        return FPaths::GetExtension(InPath).Equals(TEXT("json"), ESearchCase::IgnoreCase);
    }

    auto
        Add_SourceFile(const FString& InPath, const FString& InLabel,
            TArray<FAssetImportInfo::FSourceFile>& OutFiles,
            FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
        auto Absolute = FPaths::ConvertRelativePathToFull(InPath);
        FPaths::NormalizeFilename(Absolute);
        const auto Size = IFileManager::Get().FileSize(*Absolute);
        if (Size < 0)
        {
            OutDiagnostic = {Absolute, TEXT("io"), TEXT("Source file disappeared during import")};
            return false;
        }
        if (Size > 8 * 1024 * 1024)
        {
            OutDiagnostic = {Absolute, TEXT("source_limit"), TEXT("Source exceeds 8 MiB")};
            return false;
        }
        const auto Hash = FMD5Hash::HashFile(*Absolute);
        if (NOT Hash.IsValid())
        {
            OutDiagnostic = {Absolute, TEXT("io"), TEXT("Could not hash source file")};
            return false;
        }
        OutFiles.Emplace(Absolute, IFileManager::Get().GetTimeStamp(*Absolute), Hash, InLabel);
        return true;
    }

    auto
        Stage_Import(const FString& InPath, FCk_PixelSprite_Manifest& OutManifest,
            TArray<FAssetImportInfo::FSourceFile>& OutFiles,
            FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
        auto Absolute = FPaths::ConvertRelativePathToFull(InPath);
        FPaths::NormalizeFilename(Absolute);
        auto Candidate = FCk_PixelSprite_Manifest{};
        auto Files = TArray<FAssetImportInfo::FSourceFile>{};

        if (Is_Xpm(Absolute))
        {
            auto Grid = FCk_PixelSprite_Grid{};
            if (NOT ck::pixel_sprite::TryLoad_Xpm(Absolute, Grid, OutDiagnostic))
            {
                return false;
            }

            auto Source = FCk_PixelSprite_Source{};
            Source._Path = Absolute;
            Source._Bytes = IFileManager::Get().FileSize(*Absolute);
            Source._Grid = MoveTemp(Grid);
            Candidate._Width = Source._Grid._Width;
            Candidate._Height = Source._Grid._Height;
            Candidate._Sources.Add(MoveTemp(Source));

            auto Frame = FCk_PixelSprite_Frame{};
            Frame._Source = Absolute;
            Frame._SourceIndex = 0;
            Frame._DurationMilliseconds = 100.0;
            auto Animation = FCk_PixelSprite_Animation{};
            Animation._Name = TEXT("default");
            Animation._Frames.Add(MoveTemp(Frame));
            Candidate._Animations.Add(MoveTemp(Animation));
        }
        else if (Is_Manifest(Absolute))
        {
            if (NOT ck::pixel_sprite::TryLoad_Manifest(Absolute, Candidate, OutDiagnostic))
            {
                return false;
            }
        }
        else
        {
            OutDiagnostic = {Absolute, TEXT("format"), TEXT("Expected .xpm or .json")};
            return false;
        }

        if (NOT Add_SourceFile(Absolute, Is_Xpm(Absolute) ? TEXT("XPM") : TEXT("Manifest"),
            Files, OutDiagnostic))
        {
            return false;
        }
        if (Is_Manifest(Absolute))
        {
            for (const auto& Source : Candidate._Sources)
            {
                if (NOT Add_SourceFile(Source._Path, Source._Path, Files, OutDiagnostic))
                {
                    return false;
                }
            }
        }

        // The reflected asset validates structural invariants before any existing asset changes.
        auto Probe = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
            NewObject<UCk_PixelSpriteAsset_UE>(GetTransientPackage())};
        if (NOT Probe->Request_ReplaceManifest(Candidate, OutDiagnostic))
        {
            return false;
        }

        OutManifest = MoveTemp(Candidate);
        OutFiles = MoveTemp(Files);
        OutDiagnostic = {};
        return true;
    }

    auto
        Commit_Import(UCk_PixelSpriteAsset_UE& InAsset, const FString& InPath,
            const FCk_PixelSprite_Manifest& InManifest,
            TArray<FAssetImportInfo::FSourceFile>&& InFiles,
            FCk_PixelSprite_Diagnostic& OutDiagnostic,
            const bool InIsReimport)
        -> bool
    {
        FScopedTransaction Transaction{
            NSLOCTEXT("CkPixelSpriteEditor", "Reimport", "Reimport Pixel Sprite"), InIsReimport};
        InAsset.SetFlags(RF_Transactional);
        InAsset.Modify();
        if (NOT InAsset.Request_ReplaceManifest(InManifest, OutDiagnostic))
        {
            return false;
        }

        auto Absolute = FPaths::ConvertRelativePathToFull(InPath);
        FPaths::NormalizeFilename(Absolute);
        InAsset.Set_SourceFilename(Absolute);
        auto ImportData = InAsset.Get_ImportData();
        if (NOT ImportData)
        {
            ImportData = NewObject<UAssetImportData>(&InAsset, NAME_None, RF_Transactional);
            InAsset.Set_ImportData(ImportData);
        }
        ImportData->SetFlags(RF_Transactional);
        ImportData->Modify();
        ImportData->SetSourceFiles(MoveTemp(InFiles));
        InAsset.MarkPackageDirty();
        InAsset.PostEditChange();
        return true;
    }
}

UCk_PixelSpriteFactory_UE::
    UCk_PixelSpriteFactory_UE()
{
    SupportedClass = UCk_PixelSpriteAsset_UE::StaticClass();
    bCreateNew = false;
    bEditorImport = true;
    Formats.Add(TEXT("xpm;X PixMap pixel sprite"));
    Formats.Add(TEXT("json;Ck pixel sprite animation manifest"));
}

auto
    UCk_PixelSpriteFactory_UE::
    FactoryCanImport(const FString& InFilename)
    -> bool
{
    if (ck_pixel_sprite_editor_factory::Is_Xpm(InFilename))
    {
        return true;
    }
    if (NOT ck_pixel_sprite_editor_factory::Is_Manifest(InFilename))
    {
        return false;
    }

    // Recognition must have the same depth, UTF-8 and filesystem bounds as import.
    auto Manifest = FCk_PixelSprite_Manifest{};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    return ck::pixel_sprite::TryLoad_Manifest(InFilename, Manifest, Diagnostic);
}

auto
    UCk_PixelSpriteFactory_UE::
    FactoryCreateFile(UClass* InClass, UObject* InParent,
        FName InName, EObjectFlags InFlags, const FString& InFilename, const TCHAR* InParms,
        FFeedbackContext* InWarn, bool& OutOperationCanceled)
    -> UObject*
{
    OutOperationCanceled = false;
    if (NOT InClass || NOT InClass->IsChildOf(UCk_PixelSpriteAsset_UE::StaticClass()) || NOT InParent)
    {
        return nullptr;
    }

    auto Manifest = FCk_PixelSprite_Manifest{};
    auto Files = TArray<FAssetImportInfo::FSourceFile>{};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT ck_pixel_sprite_editor_factory::Stage_Import(InFilename, Manifest, Files, Diagnostic))
    {
        UE_LOG(LogCkPixelSpriteImport, Error, TEXT("%s"), *Diagnostic.Get_Text());
        return nullptr;
    }

    auto Asset = TObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>(
        InParent, InClass, InName, InFlags)};
    if (NOT ck_pixel_sprite_editor_factory::Commit_Import(
        *Asset, InFilename, Manifest, MoveTemp(Files), Diagnostic, false))
    {
        UE_LOG(LogCkPixelSpriteImport, Error, TEXT("%s"), *Diagnostic.Get_Text());
        return nullptr;
    }
    return Asset.Get();
}

auto
    UCk_PixelSpriteFactory_UE::
    CanReimport(UObject* InObject, TArray<FString>& OutFilenames)
    -> bool
{
    const auto Asset = TObjectPtr<UCk_PixelSpriteAsset_UE>{Cast<UCk_PixelSpriteAsset_UE>(InObject)};
    if (NOT Asset || NOT Asset->Get_ImportData() || Asset->Get_SourceFilename().IsEmpty())
    {
        return false;
    }
    Asset->Get_ImportData()->ExtractFilenames(OutFilenames);
    return OutFilenames.Num() > 0;
}

auto
    UCk_PixelSpriteFactory_UE::
    SetReimportPaths(UObject* InObject, const TArray<FString>& InPaths)
    -> void
{
    if (NOT Cast<UCk_PixelSpriteAsset_UE>(InObject) || InPaths.IsEmpty() || InPaths[0].IsEmpty())
    {
        return;
    }
    _PendingRootPaths.Add(TWeakObjectPtr<UObject>{InObject}, InPaths[0]);
}

auto
    UCk_PixelSpriteFactory_UE::
    SetReimportPaths(UObject* InObject, const FString& InPath, const int32 InSourceIndex)
    -> void
{
    if (InSourceIndex != 0 && InSourceIndex != INDEX_NONE)
    {
        UE_LOG(LogCkPixelSpriteImport, Warning,
            TEXT("A manifest's frame source path is authored in JSON; relocate the manifest instead"));
        return;
    }
    SetReimportPaths(InObject, TArray<FString>{InPath});
}

auto
    UCk_PixelSpriteFactory_UE::
    Reimport(UObject* InObject)
    -> EReimportResult::Type
{
    auto Asset = TObjectPtr<UCk_PixelSpriteAsset_UE>{Cast<UCk_PixelSpriteAsset_UE>(InObject)};
    if (NOT Asset)
    {
        return EReimportResult::Failed;
    }

    const auto Key = TWeakObjectPtr<UObject>{InObject};
    const auto Path = _PendingRootPaths.Contains(Key)
        ? _PendingRootPaths.FindRef(Key) : Asset->Get_SourceFilename();
    _PendingRootPaths.Remove(Key);

    auto Manifest = FCk_PixelSprite_Manifest{};
    auto Files = TArray<FAssetImportInfo::FSourceFile>{};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT ck_pixel_sprite_editor_factory::Stage_Import(Path, Manifest, Files, Diagnostic))
    {
        UE_LOG(LogCkPixelSpriteImport, Error, TEXT("%s"), *Diagnostic.Get_Text());
        return EReimportResult::Failed;
    }
    if (NOT ck_pixel_sprite_editor_factory::Commit_Import(
        *Asset, Path, Manifest, MoveTemp(Files), Diagnostic, true))
    {
        UE_LOG(LogCkPixelSpriteImport, Error, TEXT("%s"), *Diagnostic.Get_Text());
        return EReimportResult::Failed;
    }
    return EReimportResult::Succeeded;
}

auto
    UCk_PixelSpriteFactory_UE::
    Reimport(UObject* InObject, int32 InSourceIndex)
    -> EReimportResult::Type
{
    return Reimport(InObject);
}
