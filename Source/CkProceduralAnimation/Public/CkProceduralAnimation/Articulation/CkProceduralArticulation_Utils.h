#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment_Data.h"
#include "CkProceduralAnimation/CkProceduralAnimation_Fragment_Data.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"

#include "CkEcsExt/CkEcsExt_Utils.h"

#include "CkProceduralArticulation_Utils.generated.h"

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_ProceduralArticulation"))
class CKPROCEDURALANIMATION_API UCk_Utils_ProceduralArticulation_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_Utils_ProceduralArticulation_UE);

public:
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_ProceduralArticulation);

public:
    friend class UCk_Utils_Ecs_Base_UE;

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Add")
    static FCk_Handle_ProceduralArticulation
    Add(
        UPARAM(ref) FCk_Handle_Transform& InRoot,
        const FCk_ProceduralArticulation_Spec& InSpec);

    static auto
        Has(
            const FCk_Handle& InHandle)
        -> bool;

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Cast", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_ProceduralArticulation
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Handle -> ProceduralArticulation Handle",
        meta = (CompactNodeTitle = "<AsProceduralArticulation>", BlueprintAutocast))
    static FCk_Handle_ProceduralArticulation
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck] Get Invalid ProceduralArticulation Handle",
        meta = (CompactNodeTitle = "INVALID_ProceduralArticulationHandle", Keywords = "make"))
    static FCk_Handle_ProceduralArticulation
    Get_InvalidHandle()
    {
        return {};
    }

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Status")
    static ECk_ProceduralAnimation_Status
    Get_Status(
        const FCk_Handle_ProceduralArticulation& InArticulation);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Health")
    static ECk_ProceduralArticulation_Health
    Get_Health(
        const FCk_Handle_ProceduralArticulation& InArticulation);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Fault")
    static ECk_ProceduralArticulation_Fault
    Get_Fault(
        const FCk_Handle_ProceduralArticulation& InArticulation);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Settled Revision")
    static int64
    Get_SettledRevision(
        const FCk_Handle_ProceduralArticulation& InArticulation);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Limb")
    static FCk_Handle_ProceduralLimb
    Get_Limb(
        const FCk_Handle_ProceduralArticulation& InArticulation,
        FName InId);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Limbs")
    static TArray<FCk_Handle_ProceduralLimb>
    Get_Limbs(
        const FCk_Handle_ProceduralArticulation& InArticulation);

    // Copied control and locomotion observation. This query does not acquire a limb or guarantee later admission.
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Limb Control")
    static FCk_ProceduralLimbControl_Observation
    Get_LimbControl(
        const FCk_Handle_ProceduralArticulation& InArticulation,
        FName InLimbId);

    // Each query copies one observation per limb, in Get_Limbs order. Controlled means held by an action right now.
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Is Any Limb Controlled")
    static bool
    Get_IsAnyLimbControlled(
        const FCk_Handle_ProceduralArticulation& InArticulation);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Is Any Limb Returning")
    static bool
    Get_IsAnyLimbReturning(
        const FCk_Handle_ProceduralArticulation& InArticulation);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Get Unplanted Limb Ids")
    static TArray<FName>
    Get_UnplantedLimbIds(
        const FCk_Handle_ProceduralArticulation& InArticulation);

    // Retries every unplanted walking limb as one set, under the player's authored support and return policy.
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralArticulation",
        DisplayName = "[Ck][ProceduralArticulation] Request Recover Unplanted Limbs",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_ProceduralArticulation
    Request_RecoverUnplantedLimbs(
        UPARAM(ref) FCk_Handle_ProceduralArticulation& InArticulation,
        const FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);
};
