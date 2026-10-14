#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "CkProceduralActionGuidedCreation.generated.h"

// --------------------------------------------------------------------------------------------------------------------

// What the guided creation dialog asks before anything is written: the subject. A rig and a gait make a walking subject,
// a layout a dedicated one, and nothing previews the body alone. The starting motion is applied in the editor afterwards.
USTRUCT()
struct CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionCreation_Choice
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralActionCreation_Choice);

private:
    UPROPERTY(EditAnywhere, Category = "Subject", meta = (AllowPrivateAccess = true, ShowOnlyInnerProperties))
    FCk_ProceduralAction_EditorSubject _Subject;

public:
    CK_PROPERTY(_Subject);
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
    // Checks the choice without writing anything: a walking subject names both its rig and its gait, and no subject is
    // both walking and dedicated.
    CKPROCEDURALANIMATIONEDITOR_API auto
        ValidateCreationChoice(
            const FCk_ProceduralActionCreation_Choice& InChoice,
            FString& OutError)
        -> bool;

    // Creates one empty clip in InParent that remembers the subject. Every check runs before the clip is created, so a
    // refusal leaves nothing. Nothing is saved: the clip stays dirty until the author saves it.
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
