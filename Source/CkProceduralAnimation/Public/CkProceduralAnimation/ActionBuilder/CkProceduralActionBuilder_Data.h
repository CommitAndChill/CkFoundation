#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "CkProceduralActionBuilder_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_ValueMode : uint8
{
    Keep,
    Set
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_ValueMode);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_Recipe : uint8
{
    Blank,
    BodyLean,
    Reach,
    Sweep
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_Recipe);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_MirrorPlane : uint8
{
    XY,
    XZ,
    YZ
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_MirrorPlane);

// --------------------------------------------------------------------------------------------------------------------

/** Value-only copy of a clip's five authored fields. Edit it with UCk_Utils_ProceduralActionBuilder_UE, then Build_Into a
 *  caller-owned UCk_ProceduralAction_Data. It holds no live entity or object references. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ClipDraft
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_ClipDraft);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_BodyChannel _BodyChannel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slots", meta = (AllowPrivateAccess = true, TitleProperty = "_Id"))
    TArray<FCk_ProceduralAction_Slot> _Slots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phases", meta = (AllowPrivateAccess = true, TitleProperty = "_Id"))
    TArray<FCk_ProceduralAction_Phase> _Phases;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers", meta = (AllowPrivateAccess = true, TitleProperty = "_Id"))
    TArray<FCk_ProceduralAction_Marker> _Markers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Windows", meta = (AllowPrivateAccess = true, TitleProperty = "_Id"))
    TArray<FCk_ProceduralAction_Window> _Windows;
public:
    CK_PROPERTY(_BodyChannel);
    CK_PROPERTY(_Slots);
    CK_PROPERTY(_Phases);
    CK_PROPERTY(_Markers);
    CK_PROPERTY(_Windows);
};

// --------------------------------------------------------------------------------------------------------------------

/** One limb's requested target in a pose, in the slot's frame (see Set_SlotFrame). Orientation and bend are written only
 *  when their mode is Set; Keep leaves those key arrays untouched. A Set bend must already be a unit direction. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_LimbPoseTarget
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_LimbPoseTarget);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _Slot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FVector _Position = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_ValueMode _OrientationMode = ECk_ProceduralAction_ValueMode::Keep;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true,
        EditCondition = "_OrientationMode == ECk_ProceduralAction_ValueMode::Set"))
    FQuat _Orientation = FQuat::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_ValueMode _BendMode = ECk_ProceduralAction_ValueMode::Keep;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true,
        EditCondition = "_BendMode == ECk_ProceduralAction_ValueMode::Set"))
    FVector _Bend = FVector::UpVector;
public:
    CK_PROPERTY(_Slot);
    CK_PROPERTY(_Position);
    CK_PROPERTY(_OrientationMode);
    CK_PROPERTY(_Orientation);
    CK_PROPERTY(_BendMode);
    CK_PROPERTY(_Bend);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_LimbPoseTarget, _Slot, _Position);
};

/** Coordinated targets at one normalized phase time: the body (when _BodyMode is Set) and every listed limb. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_PoseKey
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_PoseKey);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_FloatRange_0to1 _Time;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_ValueMode _BodyMode = ECk_ProceduralAction_ValueMode::Keep;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true,
        EditCondition = "_BodyMode == ECk_ProceduralAction_ValueMode::Set"))
    FTransform _Body = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true, TitleProperty = "_Slot"))
    TArray<FCk_ProceduralAction_LimbPoseTarget> _Limbs;
public:
    CK_PROPERTY(_Time);
    CK_PROPERTY(_BodyMode);
    CK_PROPERTY(_Body);
    CK_PROPERTY(_Limbs);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_PoseKey, _Time);
};

// --------------------------------------------------------------------------------------------------------------------

/** Starting motion for a new clip; the result is ordinary, editable draft data with no damage or support semantics.
 *  Blank: one Motion phase of _Execute, no keys (the listed slots, or the body when none are listed).
 *  BodyLean: Prepare/Execute/Recover body keys from identity to -0.25 x amplitude along _Direction, to +amplitude, back.
 *  Reach: one slot, rest to rest + amplitude along _Direction and back over Prepare/Execute/Recover.
 *  Sweep: one slot, one Execute phase through _Direction, _Direction turned 90 degrees about body up, and turned 180. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_RecipeSpec
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_RecipeSpec);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Recipe _Recipe = ECk_ProceduralAction_Recipe::Blank;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    TArray<FName> _Slots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (AllowPrivateAccess = true))
    FVector _Direction = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (AllowPrivateAccess = true, Units = "cm"))
    float _AmplitudeCm = 20.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (AllowPrivateAccess = true))
    FCk_Time _Prepare = FCk_Time{0.3};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (AllowPrivateAccess = true))
    FCk_Time _Execute = FCk_Time{0.2};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (AllowPrivateAccess = true))
    FCk_Time _Recover = FCk_Time{0.4};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Easing _Easing = ECk_ProceduralAction_Easing::InOutCubic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_TrackSpace _Space = ECk_ProceduralAction_TrackSpace::BodyLocal;
public:
    CK_PROPERTY(_Recipe);
    CK_PROPERTY(_Slots);
    CK_PROPERTY(_Direction);
    CK_PROPERTY(_AmplitudeCm);
    CK_PROPERTY(_Prepare);
    CK_PROPERTY(_Execute);
    CK_PROPERTY(_Recover);
    CK_PROPERTY(_Easing);
    CK_PROPERTY(_Space);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_RecipeSpec, _Recipe, _Slots);
};

/** Copies a slot's tracks onto another slot reflected across a plane of the slot frame (XZ reflects Y). */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_MirrorSpec
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_MirrorSpec);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _SourceSlot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _DestinationSlot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_MirrorPlane _Plane = ECk_ProceduralAction_MirrorPlane::XZ;
public:
    CK_PROPERTY(_SourceSlot);
    CK_PROPERTY(_DestinationSlot);
    CK_PROPERTY(_Plane);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_MirrorSpec, _SourceSlot, _DestinationSlot, _Plane);
};

// --------------------------------------------------------------------------------------------------------------------
