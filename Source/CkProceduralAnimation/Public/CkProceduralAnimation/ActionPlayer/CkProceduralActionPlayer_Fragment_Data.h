#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Receipt.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"

#include "CkProceduralActionPlayer_Fragment_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKPROCEDURALANIMATION_API FCk_Handle_ProceduralActionPlayer : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_ProceduralActionPlayer);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_ProceduralActionPlayer);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralActionPlayer_Spec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralActionPlayer_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Tuning _Tuning;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capacity", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Capacity _Capacity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (AllowPrivateAccess = true))
    FCk_ProceduralLimbControl_Locomotion _Locomotion;

public:
    CK_PROPERTY(_Tuning);
    CK_PROPERTY(_Capacity);
    CK_PROPERTY(_Locomotion);
};

// --------------------------------------------------------------------------------------------------------------------

/** Inputs of a player that borrows walking limbs. Support and return are explicit because no preset can know the
 *  subject's stance or terrain; both are validated when the resulting spec is added. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralActionPlayer_BorrowingSpec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralActionPlayer_BorrowingSpec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (AllowPrivateAccess = true))
    FCk_ProceduralLimbControl_Support _Support;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (AllowPrivateAccess = true))
    FCk_ProceduralLimbControl_Return _Return;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Tuning _Tuning;

public:
    CK_PROPERTY(_Support);
    CK_PROPERTY(_Return);
    CK_PROPERTY(_Tuning);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralActionPlayer_BorrowingSpec, _Support, _Return);
};

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FCk_Delegate_ProceduralAction_OnManagedCompleted,
    FCk_Handle_ProceduralActionPlayer, InPlayer,
    FCk_ProceduralAction_ManagedCompletion, InCompletion);

DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FCk_Delegate_ProceduralAction_OnManagedRecords,
    FCk_Handle_ProceduralActionPlayer, InPlayer,
    FCk_ProceduralAction_ManagedPage, InPage);

DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FCk_Delegate_ProceduralAction_OnManagedAcceptedPose,
    FCk_Handle_ProceduralActionPlayer, InPlayer,
    FCk_ProceduralAction_Receipt, InReceipt);

// --------------------------------------------------------------------------------------------------------------------
