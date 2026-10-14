#pragma once

#include "CkProceduralAnimation/Limb/CkProceduralLimb_Fragment_Data.h"

#include "CkEcsExt/CkEcsExt_Utils.h"

#include "CkProceduralLimb_Utils.generated.h"

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_ProceduralLimb"))
class CKPROCEDURALANIMATION_API UCk_Utils_ProceduralLimb_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_Utils_ProceduralLimb_UE);

public:
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_ProceduralLimb);

public:
    friend class UCk_Utils_Ecs_Base_UE;
    static auto
        Has(
            const FCk_Handle& InHandle)
        -> bool;

private:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralLimb",
        DisplayName = "[Ck][ProceduralLimb] Cast", meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_ProceduralLimb
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralLimb",
        DisplayName = "[Ck][ProceduralLimb] Handle -> ProceduralLimb Handle",
        meta = (CompactNodeTitle = "<AsProceduralLimb>", BlueprintAutocast))
    static FCk_Handle_ProceduralLimb
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralLimb",
        DisplayName = "[Ck] Get Invalid ProceduralLimb Handle",
        meta = (CompactNodeTitle = "INVALID_ProceduralLimbHandle", Keywords = "make"))
    static FCk_Handle_ProceduralLimb
    Get_InvalidHandle()
    {
        return {};
    }

public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralLimb",
        DisplayName = "[Ck][ProceduralLimb] Get Id")
    static FName
    Get_Id(
        const FCk_Handle_ProceduralLimb& InLimb);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralLimb",
        DisplayName = "[Ck][ProceduralLimb] Get Layout")
    static FCk_ProceduralLimb_Layout
    Get_Layout(
        const FCk_Handle_ProceduralLimb& InLimb);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralLimb",
        DisplayName = "[Ck][ProceduralLimb] Try Get Layout")
    static bool
    Try_GetLayout(
        const FCk_Handle_ProceduralLimb& InLimb,
        FCk_ProceduralLimb_Layout& OutLayout);
};
