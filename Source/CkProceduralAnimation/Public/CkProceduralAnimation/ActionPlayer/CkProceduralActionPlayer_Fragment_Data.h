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

/** A player for one articulated subject. Tuning is the only tuning its actions play with. Locomotion is the policy for
 *  borrowing walking limbs: with its support mode Disabled (the default) an action that binds a walking limb is rejected
 *  at admission with reason SupportUnavailable, and dedicated limbs and the body channel play as usual. */
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
