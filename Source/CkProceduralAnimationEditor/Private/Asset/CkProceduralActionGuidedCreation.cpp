#include "CkProceduralAnimationEditor/CkProceduralActionGuidedCreation.h"

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

namespace ck::procedural_action_editor
{
    auto
        ValidateCreationChoice(
            const FCk_ProceduralActionCreation_Choice& InChoice,
            FString& OutError)
        -> bool
    {
        const auto& Subject = InChoice.Get_Subject();
        if (Subject.Get_WalkingRig().IsNull() != Subject.Get_WalkingGait().IsNull())
        {
            OutError = TEXT("A walking subject needs both its rig and its gait.");
            return false;
        }
        if (NOT Subject.Get_WalkingRig().IsNull() && NOT Subject.Get_Layout().Get_Limbs().IsEmpty())
        {
            OutError = TEXT("Choose a walking subject (rig and gait) or a dedicated layout, not both.");
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
        auto* Clip = NewObject<UCk_ProceduralAction_Data>(InParent, InName, InFlags | RF_Transactional);
        Clip->Set_EditorSubject(InChoice.Get_Subject());
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
                "Choose the subject the motion is authored on: a rig and a gait for a walking creature, a layout for dedicated limbs, or nothing to preview the body alone. Nothing is created until you press Create; the name and folder come next, and the starting motion is applied in the editor."))]
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
