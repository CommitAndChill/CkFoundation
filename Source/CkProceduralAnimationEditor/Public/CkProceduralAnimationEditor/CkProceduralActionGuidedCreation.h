#pragma once

#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"

#include "CkProceduralActionGuidedCreation.generated.h"

class UCk_ProceduralAction_Data;
class UCk_ProceduralActionPreview_Config;

// --------------------------------------------------------------------------------------------------------------------

UENUM()
enum class ECk_ProceduralActionCreation_Subject : uint8
{
    // Reuse a preview config that already describes the subject.
    UseExisting,
    // A new standalone config with no limbs: body presentation only.
    NewBodyOnly,
    // A new standalone config whose dedicated limbs are declared afterwards in its Details.
    NewDedicatedLimbs,
    // A new walking config whose rig, gait and support policy are chosen afterwards in its Details.
    NewWalker
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralActionCreation_Subject);

// What the guided creation dialog asks before anything is written: the subject and the starting motion.
USTRUCT()
struct CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionCreation_Choice
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralActionCreation_Choice);

private:
    UPROPERTY(EditAnywhere, Category = "Subject", meta = (AllowPrivateAccess = true))
    ECk_ProceduralActionCreation_Subject _Subject = ECk_ProceduralActionCreation_Subject::NewBodyOnly;

    UPROPERTY(EditAnywhere, Category = "Subject", meta = (AllowPrivateAccess = true,
        EditCondition = "_Subject == ECk_ProceduralActionCreation_Subject::UseExisting", EditConditionHides))
    TSoftObjectPtr<UCk_ProceduralActionPreview_Config> _ExistingPreview;

    UPROPERTY(EditAnywhere, Category = "Motion", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_RecipeSpec _Recipe;

public:
    CK_PROPERTY(_Subject);
    CK_PROPERTY(_ExistingPreview);
    CK_PROPERTY(_Recipe);
};

// --------------------------------------------------------------------------------------------------------------------

// One asset's outcome in a paired save; a failure never hides another asset's result.
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionAssetSaveResult
{
    CK_GENERATED_BODY(FCk_ProceduralActionAssetSaveResult);

private:
    FString _AssetPath;
    bool _Saved = false;
    FString _Reason;

public:
    CK_PROPERTY_GET(_AssetPath);
    CK_PROPERTY_GET(_Saved);
    CK_PROPERTY_GET(_Reason);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralActionAssetSaveResult, _AssetPath, _Saved, _Reason);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    // The preview config created next to a clip: "<Clip>_Preview_DA" for "<Clip>_DA", "<Clip>_Preview_DA" otherwise.
    CKPROCEDURALANIMATIONEDITOR_API auto
        Get_PreviewSiblingPackageName(
            const FString& InClipPackageName)
        -> FString;

    // Checks the choice without writing anything: the subject is chosen and the recipe builds a draft.
    CKPROCEDURALANIMATIONEDITOR_API auto
        ValidateCreationChoice(
            const FCk_ProceduralActionCreation_Choice& InChoice,
            FString& OutError)
        -> bool;

    // Creates the clip in InParent, fills it from the recipe and, for a new subject, creates the preview config next to
    // it; the pairing is remembered for the editor. Every check runs before the first object is created, so a refusal
    // leaves no clip and no preview. Nothing is saved: both assets stay dirty until the author saves them.
    CKPROCEDURALANIMATIONEDITOR_API auto
        CreateFromChoice(
            UObject* InParent,
            FName InName,
            EObjectFlags InFlags,
            const FCk_ProceduralActionCreation_Choice& InChoice,
            FString& OutError)
        -> UCk_ProceduralAction_Data*;

    // Saves each asset on its own and reports each one, in order.
    CKPROCEDURALANIMATIONEDITOR_API auto
        SaveAssets(
            const TArray<UObject*>& InAssets)
        -> TArray<FCk_ProceduralActionAssetSaveResult>;

    CKPROCEDURALANIMATIONEDITOR_API auto
        DescribeSaveResults(
            const TArray<FCk_ProceduralActionAssetSaveResult>& InResults)
        -> FString;

    // Shows the modal creation dialog; unset when the author cancels.
    CKPROCEDURALANIMATIONEDITOR_API auto
        AskCreationChoice()
        -> TOptional<FCk_ProceduralActionCreation_Choice>;
}

// --------------------------------------------------------------------------------------------------------------------
