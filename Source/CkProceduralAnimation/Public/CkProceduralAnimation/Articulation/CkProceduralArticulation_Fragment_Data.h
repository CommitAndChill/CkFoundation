#pragma once

#include "CkProceduralAnimation/Limb/CkProceduralLimb_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkJolt/Query/CkJoltQuery_Data.h"

#include "CkProceduralArticulation_Fragment_Data.generated.h"

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKPROCEDURALANIMATION_API FCk_Handle_ProceduralArticulation : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_ProceduralArticulation);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_ProceduralArticulation);

UENUM(BlueprintType)
enum class ECk_ProceduralArticulation_Health : uint8
{
    Operational,
    Faulted
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralArticulation_Health);

UENUM(BlueprintType)
enum class ECk_ProceduralArticulation_Fault : uint8
{
    None,
    InvalidBinding,
    SolveFailed,
    SettlementMismatch,
    SettlementTimeout
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralArticulation_Fault);

UENUM(BlueprintType)
enum class ECk_ProceduralArticulation_ClearanceQuery : uint8
{
    None,
    JoltFilter
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralArticulation_ClearanceQuery);

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralArticulation_ClearanceSettings
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralArticulation_ClearanceSettings);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (AllowPrivateAccess = true))
    ECk_ProceduralArticulation_ClearanceQuery _Query = ECk_ProceduralArticulation_ClearanceQuery::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (AllowPrivateAccess = true))
    FCk_Jolt_QueryFilter _Filter;

public:
    CK_PROPERTY(_Query);
    CK_PROPERTY(_Filter);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralArticulation_Layout
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralArticulation_Layout);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralLimb_Layout> _Limbs;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (AllowPrivateAccess = true))
    FVector _BodyEnvelopeMin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (AllowPrivateAccess = true))
    FVector _BodyEnvelopeMax = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology", meta = (AllowPrivateAccess = true))
    int32 _CompatibilityVersion = 1;

public:
    CK_PROPERTY_GET(_Limbs);
    CK_PROPERTY(_BodyEnvelopeMin);
    CK_PROPERTY(_BodyEnvelopeMax);
    CK_PROPERTY_GET(_CompatibilityVersion);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralArticulation_Layout, _Limbs);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralArticulation_Spec
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralArticulation_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_Layout _Layout;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralLimb_Binding> _Bindings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation", meta = (AllowPrivateAccess = true))
    FCk_Handle_Transform _Presentation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_ClearanceSettings _ClearanceSettings;

public:
    CK_PROPERTY_GET(_Layout);
    CK_PROPERTY_GET(_Bindings);
    CK_PROPERTY(_Presentation);
    CK_PROPERTY(_ClearanceSettings);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralArticulation_Spec, _Layout, _Bindings);
};
