#include "CkProceduralAnimationEditor/CkProceduralAction_Factory.h"

#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

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
    const auto Configured = ConfigureFromChoice(ck::procedural_action_editor::AskCreationChoice(), Error);
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
    if (NOT ck::procedural_action_editor::ValidateCreationChoice(InChoice.GetValue(), OutError))
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
    auto Error = FString{};
    auto* Created = ck::procedural_action_editor::CreateFromChoice(InParent, InName, InFlags, Choice, Error);
    if (Created == nullptr && FSlateApplication::IsInitialized())
    {
        auto Info = FNotificationInfo{FText::FromString(FString::Printf(TEXT("The procedural action was not created: %s"), *Error))};
        Info.ExpireDuration = 8.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    return Created;
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
