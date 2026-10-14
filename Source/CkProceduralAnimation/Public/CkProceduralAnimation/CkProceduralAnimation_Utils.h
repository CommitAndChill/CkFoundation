#pragma once

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment_Data.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment_Data.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment_Data.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment_Data.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"

#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkProceduralAnimation_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralWalker_LegChain
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralWalker_LegChain);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FName _LegId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_ProceduralRig_Spec _Rig;

public:
    CK_PROPERTY_GET(_LegId);
    CK_PROPERTY_GET(_Rig);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralWalker_LegChain, _LegId, _Rig);
};

// --------------------------------------------------------------------------------------------------------------------

/** Everything Add_Walker composes. The rig layout and gait preset are construction inputs consumed inside Add_Walker
 *  and not retained; a Spec kept on a data asset holds them as soft references on its own side and resolves them before
 *  calling. The body pose and the action player are optional compositions, present when enabled. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralWalker_Spec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralWalker_Spec);

private:
    UPROPERTY(BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    TObjectPtr<UCk_ProceduralRig_Data> _Rig;

    UPROPERTY(BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    TObjectPtr<UCk_ProceduralGait_Data> _Gait;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralWalker_LegChain> _Chains;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _BodyPose = ECk_EnableDisable::Disable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, EditCondition = "_BodyPose == ECk_EnableDisable::Enable"))
    FCk_ProceduralBodyPose_Spec _BodyPoseSpec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _ActionPlayer = ECk_EnableDisable::Disable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, EditCondition = "_ActionPlayer == ECk_EnableDisable::Enable"))
    FCk_ProceduralActionPlayer_Spec _ActionPlayerSpec;

public:
    CK_PROPERTY_GET(_Rig);
    CK_PROPERTY_GET(_Gait);
    CK_PROPERTY(_Chains);
    CK_PROPERTY(_BodyPose);
    CK_PROPERTY(_BodyPoseSpec);
    CK_PROPERTY(_ActionPlayer);
    CK_PROPERTY(_ActionPlayerSpec);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralWalker_Spec, _Rig, _Gait);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralWalker
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralWalker);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_Handle_ProceduralGait _Gait;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_Handle_ProceduralLeg> _Legs;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_Handle_ProceduralArticulation _Articulation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_Handle_ProceduralRig> _Rigs;

    // Invalid unless the Spec enabled the body pose.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_Handle_ProceduralBodyPose _BodyPose;

    // Invalid unless the Spec enabled the action player.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_Handle_ProceduralActionPlayer _ActionPlayer;

public:
    CK_PROPERTY_GET(_Gait);
    CK_PROPERTY_GET(_Legs);
    CK_PROPERTY_GET(_Articulation);
    CK_PROPERTY(_Rigs);
    CK_PROPERTY(_BodyPose);
    CK_PROPERTY(_ActionPlayer);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralWalker, _Gait, _Legs, _Articulation);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable)
class CKPROCEDURALANIMATION_API UCk_Utils_ProceduralAnimation_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_ProceduralAnimation_UE);

public:
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|ProceduralAnimation",
              DisplayName="[Ck][ProceduralAnimation] Add Walker")
    static FCk_ProceduralWalker
    Add_Walker(
        UPARAM(ref) FCk_Handle_Transform& InBody,
        const FCk_ProceduralWalker_Spec& InSpec);
};

// --------------------------------------------------------------------------------------------------------------------
