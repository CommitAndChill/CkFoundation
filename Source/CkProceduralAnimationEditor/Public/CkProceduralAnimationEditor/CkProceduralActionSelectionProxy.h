#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorData.h"

#include "UObject/Object.h"

#include "CkProceduralActionSelectionProxy.generated.h"

class FCk_ProceduralActionAuthoringSession;
class FCk_ProceduralActionEditorModel;

// --------------------------------------------------------------------------------------------------------------------

UENUM()
enum class ECk_ProceduralActionSelectionProxy_Shows : uint8
{
    Nothing,
    Clip,
    Section,
    Key,
    Event
};

/** What the Details panel edits: the editor model's selection as plain properties. Load reads the clip; an edit calls
 *  PostEditChangeProperty, which hands the property's name to OnEdited, and the toolkit writes it through a command. The
 *  proxy never writes the clip. */
UCLASS(Transient)
class CKPROCEDURALANIMATIONEDITOR_API UCk_ProceduralActionSelectionProxy : public UObject
{
    GENERATED_BODY()

public:
    auto
        Load(
            const FCk_ProceduralActionAuthoringSession& InSession,
            const FCk_ProceduralActionEditorModel& InModel)
        -> void;
    // The category the current selection fills; the Details panel hides the others.
    auto
        Get_IsCategoryShown(
            FName InCategory) const
        -> bool;

    auto
        PostEditChangeProperty(
            FPropertyChangedEvent& InEvent)
        -> void override;

    TFunction<void(FName)> OnEdited;

public:
    UPROPERTY(VisibleAnywhere, Category = "Selection")
    ECk_ProceduralActionSelectionProxy_Shows _Shows = ECk_ProceduralActionSelectionProxy_Shows::Nothing;

    UPROPERTY(VisibleAnywhere, Category = "Key")
    FName _Channel;
    UPROPERTY(VisibleAnywhere, Category = "Key")
    FCk_Time _KeyTime;
    UPROPERTY(EditAnywhere, Category = "Key", meta = (Units = "cm"))
    FVector _Position = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Category = "Key")
    ECk_ProceduralAction_ValueMode _OrientationMode = ECk_ProceduralAction_ValueMode::Keep;
    UPROPERTY(EditAnywhere, Category = "Key", meta = (EditCondition = "_OrientationMode == ECk_ProceduralAction_ValueMode::Set"))
    FRotator _Orientation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, Category = "Key")
    ECk_ProceduralAction_ValueMode _BendMode = ECk_ProceduralAction_ValueMode::Keep;
    UPROPERTY(EditAnywhere, Category = "Key", meta = (EditCondition = "_BendMode == ECk_ProceduralAction_ValueMode::Set"))
    FVector _BendDirection = FVector::UpVector;
    UPROPERTY(EditAnywhere, Category = "Key")
    ECk_ProceduralAction_ReachPolicy _ReachPolicy = ECk_ProceduralAction_ReachPolicy::RejectUnreachable;
    UPROPERTY(EditAnywhere, Category = "Key")
    FName _PoseName;
    UPROPERTY(VisibleAnywhere, Category = "Key")
    ECk_ProceduralAction_TrackSpace _Space = ECk_ProceduralAction_TrackSpace::BodyLocal;

    UPROPERTY(EditAnywhere, Category = "Section")
    FName _SectionName;
    UPROPERTY(EditAnywhere, Category = "Section")
    FCk_Time _Duration;
    UPROPERTY(EditAnywhere, Category = "Section")
    ECk_ProceduralAction_Easing _Easing = ECk_ProceduralAction_Easing::Linear;
    UPROPERTY(VisibleAnywhere, Category = "Section")
    bool _StartsFromCurrentPose = true;

    UPROPERTY(EditAnywhere, Category = "Clip")
    FCk_ProceduralActionEditor_Subject _Subject;
    UPROPERTY(VisibleAnywhere, Category = "Clip")
    FCk_Time _Length;
    UPROPERTY(VisibleAnywhere, Category = "Clip")
    TArray<FCk_ProceduralAction_Marker> _Markers;
    UPROPERTY(VisibleAnywhere, Category = "Clip")
    TArray<FCk_ProceduralAction_Window> _Windows;

    UPROPERTY(VisibleAnywhere, Category = "Event")
    FName _EventName;
    UPROPERTY(VisibleAnywhere, Category = "Event")
    FName _EventSection;
    UPROPERTY(VisibleAnywhere, Category = "Event")
    FCk_Time _EventTime;
};

// --------------------------------------------------------------------------------------------------------------------
