#pragma once

#include "CkProceduralAnimationEditor/CkProceduralActionGuidedCreation.h"

#include "Factories/Factory.h"

#include "CkProceduralAction_Factory.generated.h"

// The Content Browser asks for the subject before it asks for a name, and creates nothing when the author cancels. A
// factory used from code without that dialog keeps creating a bare clip.
UCLASS()
class CKPROCEDURALANIMATIONEDITOR_API UCk_ProceduralAction_Factory : public UFactory
{
    GENERATED_BODY()

public:
    UCk_ProceduralAction_Factory();

    auto
        ConfigureProperties()
        -> bool override;
    auto
        FactoryCreateNew(
            UClass* InClass,
            UObject* InParent,
            FName InName,
            EObjectFlags InFlags,
            UObject* InContext,
            FFeedbackContext* InWarn)
        -> UObject* override;

    // The dialog's answer: unset (cancel) or an invalid choice leaves the factory unconfigured and returns false.
    auto
        ConfigureFromChoice(
            const TOptional<FCk_ProceduralActionCreation_Choice>& InChoice,
            FString& OutError)
        -> bool;
    auto
        Get_HasConfirmedChoice() const
        -> bool
    { return _ConfirmedChoice.IsSet(); }

private:
    TOptional<FCk_ProceduralActionCreation_Choice> _ConfirmedChoice;
};

UCLASS()
class CKPROCEDURALANIMATIONEDITOR_API UCk_ProceduralActionPreview_Factory : public UFactory
{
    GENERATED_BODY()

public:
    UCk_ProceduralActionPreview_Factory();

    auto
        FactoryCreateNew(
            UClass* InClass,
            UObject* InParent,
            FName InName,
            EObjectFlags InFlags,
            UObject* InContext,
            FFeedbackContext* InWarn)
        -> UObject* override;
};
