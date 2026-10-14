#pragma once

#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"

#include "CkProceduralLimb_Fragment_Data.generated.h"

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKPROCEDURALANIMATION_API FCk_Handle_ProceduralLimb : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_ProceduralLimb);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_ProceduralLimb);

UENUM(BlueprintType)
enum class ECk_ProceduralLimb_Role : uint8
{
    Dedicated,
    Locomotor
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralLimb_Role);

/** Immutable, handle-free topology. Segment lengths are hip-first centimetres. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralLimb_Layout
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralLimb_Layout);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology", meta = (AllowPrivateAccess = true))
    FName _Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (AllowPrivateAccess = true))
    FTransform _AttachmentLocal = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry", meta = (AllowPrivateAccess = true))
    TArray<float> _SegmentLengths;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rest", meta = (AllowPrivateAccess = true))
    FTransform _RestEndpointLocal = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rest", meta = (AllowPrivateAccess = true))
    FVector _BendLocal = FVector::UpVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solver", meta = (AllowPrivateAccess = true))
    ECk_ProceduralRig_ChainSolver _Solver = ECk_ProceduralRig_ChainSolver::Auto;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (AllowPrivateAccess = true))
    ECk_ProceduralRig_Clearance _Clearance = ECk_ProceduralRig_Clearance::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearance", meta = (AllowPrivateAccess = true))
    TArray<float> _SegmentClearanceRadii;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topology", meta = (AllowPrivateAccess = true))
    ECk_ProceduralLimb_Role _Role = ECk_ProceduralLimb_Role::Dedicated;

public:
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY_GET(_AttachmentLocal);
    CK_PROPERTY_GET(_SegmentLengths);
    CK_PROPERTY_GET(_RestEndpointLocal);
    CK_PROPERTY_GET(_BendLocal);
    CK_PROPERTY(_Solver);
    CK_PROPERTY(_Clearance);
    CK_PROPERTY(_SegmentClearanceRadii);
    CK_PROPERTY(_Role);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralLimb_Layout, _Id, _AttachmentLocal, _SegmentLengths, _RestEndpointLocal, _BendLocal);
};

/** Live presentation bindings; geometry remains solely in the layout. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralLimb_Binding
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralLimb_Binding);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings", meta = (AllowPrivateAccess = true))
    FName _LimbId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_Handle_Transform> _Segments;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings", meta = (AllowPrivateAccess = true))
    FCk_Handle_Transform _Terminal;

public:
    CK_PROPERTY_GET(_LimbId);
    CK_PROPERTY_GET(_Segments);
    CK_PROPERTY(_Terminal);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralLimb_Binding, _LimbId, _Segments);
};
