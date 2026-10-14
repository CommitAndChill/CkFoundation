#include "CkProceduralAnimationEditor/CkProceduralAction_Factory.h"

#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionFactory"

namespace ck_procedural_action_factory
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
        UCk_Utils_ProceduralActionEditor_UE::Get_EditorData(Clip)->Set_Subject(InChoice.Get_Subject());
        return Clip;
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

UCk_ProceduralAction_Factory::UCk_ProceduralAction_Factory()
{
    SupportedClass = UCk_ProceduralAction_Data::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

auto
    UCk_ProceduralAction_Factory::
    ConfigureProperties()
    -> bool
{
    auto Error = FString{};
    const auto Configured = ConfigureFromChoice(ck_procedural_action_factory::AskCreationChoice(), Error);
    if (NOT Configured && NOT Error.IsEmpty())
    {
        auto Info = FNotificationInfo{FText::FromString(Error)};
        Info.ExpireDuration = 8.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    return Configured;
}

auto
    UCk_ProceduralAction_Factory::
    ConfigureFromChoice(
        const TOptional<FCk_ProceduralActionCreation_Choice>& InChoice,
        FString& OutError)
    -> bool
{
    _ConfirmedChoice.Reset();
    OutError.Reset();
    if (NOT InChoice.IsSet())
    { return false; }
    if (NOT ck_procedural_action_factory::ValidateCreationChoice(InChoice.GetValue(), OutError))
    { return false; }
    _ConfirmedChoice = InChoice;
    return true;
}

auto
    UCk_ProceduralAction_Factory::
    FactoryCreateNew(
        UClass* InClass,
        UObject* InParent,
        FName InName,
        EObjectFlags InFlags,
        UObject*,
        FFeedbackContext*)
    -> UObject*
{
    if (InClass != SupportedClass)
    { return nullptr; }
    if (NOT _ConfirmedChoice.IsSet())
    { return NewObject<UCk_ProceduralAction_Data>(InParent, InClass, InName, InFlags); }
    const auto Choice = _ConfirmedChoice.GetValue();
    _ConfirmedChoice.Reset();
    auto* Clip = NewObject<UCk_ProceduralAction_Data>(InParent, InClass, InName, InFlags | RF_Transactional);
    UCk_Utils_ProceduralActionEditor_UE::Get_EditorData(Clip)->Set_Subject(Choice.Get_Subject());
    return Clip;
}

UCk_ProceduralActionPreview_Factory::UCk_ProceduralActionPreview_Factory()
{
    SupportedClass = UCk_ProceduralActionPreview_Config::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

auto
    UCk_ProceduralActionPreview_Factory::
    FactoryCreateNew(
        UClass* InClass,
        UObject* InParent,
        FName InName,
        EObjectFlags InFlags,
        UObject*,
        FFeedbackContext*)
    -> UObject*
{
    return InClass == SupportedClass
        ? NewObject<UCk_ProceduralActionPreview_Config>(InParent, InClass, InName, InFlags)
        : nullptr;
}

#undef LOCTEXT_NAMESPACE
