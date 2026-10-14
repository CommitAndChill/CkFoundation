#include "CkProceduralAnimationEditor/CkProceduralActionGuidedCreation.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditor_UserSettings.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Utils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IStructureDetailsView.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionGuidedCreation"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_action_guided_creation
{
    auto
        IsPackageFree(
            const FString& InPackageName)
        -> bool
    {
        const auto AssetName = FPackageName::GetLongPackageAssetName(InPackageName);
        const auto ObjectPath = FSoftObjectPath{InPackageName + TEXT(".") + AssetName};
        auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        auto Filename = FString{};
        const auto HasFile = FPackageName::TryConvertLongPackageNameToFilename(InPackageName, Filename,
                FPackageName::GetAssetPackageExtension())
            && IFileManager::Get().FileExists(*Filename);
        return ObjectPath.ResolveObject() == nullptr && NOT Registry.GetAssetByObjectPath(ObjectPath).IsValid() && NOT HasFile;
    }

    auto
        ToSubjectMode(
            ECk_ProceduralActionCreation_Subject InSubject)
        -> ECk_ProceduralActionPreview_SubjectMode
    {
        return InSubject == ECk_ProceduralActionCreation_Subject::NewWalker
            ? ECk_ProceduralActionPreview_SubjectMode::Walking : ECk_ProceduralActionPreview_SubjectMode::Standalone;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    auto
        Get_PreviewSiblingPackageName(
            const FString& InClipPackageName)
        -> FString
    {
        auto Name = FPackageName::GetLongPackageAssetName(InClipPackageName);
        if (Name.EndsWith(TEXT("_DA")))
        { Name.LeftChopInline(3); }
        return FPackageName::GetLongPackagePath(InClipPackageName) + TEXT("/") + Name + TEXT("_Preview_DA");
    }

    auto
        ValidateCreationChoice(
            const FCk_ProceduralActionCreation_Choice& InChoice,
            FString& OutError)
        -> bool
    {
        if (InChoice.Get_Subject() == ECk_ProceduralActionCreation_Subject::UseExisting
            && InChoice.Get_ExistingPreview().IsNull())
        {
            OutError = TEXT("Choose the existing preview config to reuse, or create a new subject.");
            return false;
        }
        auto Draft = FCk_ProceduralAction_ClipDraft{};
        const auto Report = UCk_Utils_ProceduralActionBuilder_UE::Make_FromRecipe(InChoice.Get_Recipe(), Draft);
        if (Report.Get_Status() != ECk_ProceduralAction_ReportStatus::Valid)
        {
            OutError = TEXT("The recipe cannot build a clip:");
            for (const auto& Diagnostic : Report.Get_Diagnostics())
            {
                OutError += FString::Printf(TEXT(" %s [%s]: expected %s; got %s."), *Diagnostic.Get_FieldPath(),
                    *Diagnostic.Get_Code().ToString(), *Diagnostic.Get_Expected(), *Diagnostic.Get_Actual());
            }
            return false;
        }
        return true;
    }

    auto
        CreateFromChoice(
            UObject* InParent,
            FName InName,
            EObjectFlags InFlags,
            const FCk_ProceduralActionCreation_Choice& InChoice,
            FString& OutError)
        -> UCk_ProceduralAction_Data*
    {
        if (InParent == nullptr || InName.IsNone())
        {
            OutError = TEXT("The clip needs a package and a name.");
            return nullptr;
        }
        if (NOT ValidateCreationChoice(InChoice, OutError))
        { return nullptr; }

        const auto CreatesSubject = InChoice.Get_Subject() != ECk_ProceduralActionCreation_Subject::UseExisting;
        const auto ClipPackageName = InParent->GetOutermost()->GetName();
        const auto PreviewPackageName = Get_PreviewSiblingPackageName(ClipPackageName);
        auto* ExistingPreview = CreatesSubject ? nullptr : InChoice.Get_ExistingPreview().LoadSynchronous();
        if (NOT CreatesSubject && ExistingPreview == nullptr)
        {
            OutError = FString::Printf(TEXT("The preview config %s could not be loaded."),
                *InChoice.Get_ExistingPreview().ToString());
            return nullptr;
        }
        if (CreatesSubject && NOT ck_procedural_action_guided_creation::IsPackageFree(PreviewPackageName))
        {
            OutError = FString::Printf(TEXT("An asset already exists at %s; reuse it as the subject or rename the clip."),
                *PreviewPackageName);
            return nullptr;
        }

        auto* Clip = NewObject<UCk_ProceduralAction_Data>(InParent, InName, InFlags | RF_Transactional);
        auto RecipeError = FString{};
        const auto RecipeApplied = FCk_ProceduralActionAuthoringCommands{Clip}.CreateFromRecipe(InChoice.Get_Recipe(), RecipeError);
        CK_ENSURE_IF_NOT(RecipeApplied, TEXT("A validated recipe was refused while creating clip [{}]: {}"), Clip->GetPathName(),
            RecipeError)
        {
            OutError = RecipeError;
            Clip->ClearFlags(RF_Public | RF_Standalone);
            Clip->MarkAsGarbage();
            return nullptr;
        }

        auto* Preview = ExistingPreview;
        if (CreatesSubject)
        {
            auto* Package = CreatePackage(*PreviewPackageName);
            Preview = NewObject<UCk_ProceduralActionPreview_Config>(Package,
                FName{*FPackageName::GetLongPackageAssetName(PreviewPackageName)}, RF_Public | RF_Standalone | RF_Transactional);
            Preview->Set_SubjectMode(ck_procedural_action_guided_creation::ToSubjectMode(InChoice.Get_Subject()));
            FAssetRegistryModule::AssetCreated(Preview);
            Package->MarkPackageDirty();
        }

        auto* Settings = GetMutableDefault<UCk_ProceduralActionEditor_UserSettings>();
        if (Settings->SetPreviewPath(Clip->GetPathName(), Preview->GetPathName()))
        { Settings->SaveConfig(); }
        return Clip;
    }

    auto
        SaveAssets(
            const TArray<UObject*>& InAssets)
        -> TArray<FCk_ProceduralActionAssetSaveResult>
    {
        auto Results = TArray<FCk_ProceduralActionAssetSaveResult>{};
        for (auto* Asset : InAssets)
        {
            if (NOT IsValid(Asset))
            {
                Results.Emplace(TEXT("<missing>"), false, TEXT("The asset is no longer loaded."));
                continue;
            }
            const auto Path = Asset->GetPathName();
            auto* Package = Asset->GetOutermost();
            if (Package == GetTransientPackage() || Package->HasAnyFlags(RF_Transient))
            {
                Results.Emplace(Path, false, TEXT("It lives in a transient package and has no file."));
                continue;
            }
            auto Filename = FString{};
            if (NOT FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), Filename,
                FPackageName::GetAssetPackageExtension()))
            {
                Results.Emplace(Path, false, TEXT("Its package path is not mounted, so it has no file to save to."));
                continue;
            }
            if (IFileManager::Get().FileExists(*Filename) && IFileManager::Get().IsReadOnly(*Filename))
            {
                Results.Emplace(Path, false, FString::Printf(TEXT("%s is read-only; check it out first."), *Filename));
                continue;
            }
            auto Args = FSavePackageArgs{};
            Args.TopLevelFlags = RF_Public | RF_Standalone;
            Args.SaveFlags = SAVE_NoError;
            Args.Error = GLog;
            if (NOT UPackage::SavePackage(Package, Asset, *Filename, Args))
            {
                Results.Emplace(Path, false, TEXT("The package save failed; the Output Log has the engine's reason."));
                continue;
            }
            Results.Emplace(Path, true, FString{});
        }
        return Results;
    }

    auto
        DescribeSaveResults(
            const TArray<FCk_ProceduralActionAssetSaveResult>& InResults)
        -> FString
    {
        auto Text = FString{};
        for (const auto& Result : InResults)
        {
            Text += Result.Get_Saved()
                ? FString::Printf(TEXT("Saved %s\n"), *Result.Get_AssetPath())
                : FString::Printf(TEXT("NOT saved %s: %s\n"), *Result.Get_AssetPath(), *Result.Get_Reason());
        }
        return Text.TrimEnd();
    }

    auto
        AskCreationChoice()
        -> TOptional<FCk_ProceduralActionCreation_Choice>
    {
        if (NOT FSlateApplication::IsInitialized())
        { return {}; }
        auto Choice = MakeShared<FStructOnScope>(FCk_ProceduralActionCreation_Choice::StaticStruct());
        auto Confirmed = MakeShared<bool>(false);
        auto& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        auto DetailArgs = FDetailsViewArgs{};
        DetailArgs.bAllowSearch = false;
        DetailArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
        const auto Details = PropertyEditor.CreateStructureDetailView(DetailArgs, FStructureDetailsViewArgs{}, Choice);

        const auto Window = SNew(SWindow)
            .Title(LOCTEXT("Title", "New Procedural Action"))
            .ClientSize(FVector2D{520.0, 560.0})
            .SupportsMinimize(false)
            .SupportsMaximize(false);
        const auto WeakWindow = TWeakPtr<SWindow>{Window};
        Window->SetContent(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(8.0f)
            [SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Explain",
                "Choose the subject the motion is authored on and the motion to start from. Nothing is created until you press Create; the name and folder come next."))]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
            [Details->GetWidget().ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(8.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
                [SNew(SButton).Text(LOCTEXT("Create", "Create"))
                    .OnClicked_Lambda([Confirmed, WeakWindow]() -> FReply
                    {
                        *Confirmed = true;
                        if (const auto Pinned = WeakWindow.Pin())
                        { Pinned->RequestDestroyWindow(); }
                        return FReply::Handled();
                    })]
                + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
                [SNew(SButton).Text(LOCTEXT("Cancel", "Cancel"))
                    .OnClicked_Lambda([WeakWindow]() -> FReply
                    {
                        if (const auto Pinned = WeakWindow.Pin())
                        { Pinned->RequestDestroyWindow(); }
                        return FReply::Handled();
                    })]
            ]);

        FSlateApplication::Get().AddModalWindow(Window, FGlobalTabmanager::Get()->GetRootWindow());
        if (NOT *Confirmed)
        { return {}; }
        return *reinterpret_cast<const FCk_ProceduralActionCreation_Choice*>(Choice->GetStructMemory());
    }
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
