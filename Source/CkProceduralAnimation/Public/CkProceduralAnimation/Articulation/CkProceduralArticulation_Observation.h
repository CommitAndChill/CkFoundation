#pragma once

#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment_Data.h"

#include "CkProceduralArticulation_Observation.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralArticulation_ContactEvidence : uint8
{
    None,
    Untrusted,
    Trusted
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralArticulation_ContactEvidence);

// --------------------------------------------------------------------------------------------------------------------

// Copied world-space geometry. Segment transforms retain visual part scale; topology owns solver lengths.
// Endpoint frames are rigid even when the bound terminal visual has scale. Contact evidence is gait support
// provenance, not an effects or collision verdict.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralArticulation_LimbPose
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralArticulation_LimbPose);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FName _Id = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FTransform _RequestedEndpoint = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FTransform _AcceptedEndpoint = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    TArray<FTransform> _Segments;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    float _EndpointErrorCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    ECk_ProceduralRig_ChainState _ChainState = ECk_ProceduralRig_ChainState::Clear;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    ECk_ProceduralArticulation_ContactEvidence _ContactEvidence = ECk_ProceduralArticulation_ContactEvidence::None;

public:
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY_GET(_RequestedEndpoint);
    CK_PROPERTY_GET(_AcceptedEndpoint);
    CK_PROPERTY_GET(_Segments);
    CK_PROPERTY_GET(_EndpointErrorCm);
    CK_PROPERTY_GET(_ChainState);
    CK_PROPERTY_GET(_ContactEvidence);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralArticulation_LimbPose, _Id, _RequestedEndpoint, _AcceptedEndpoint, _Segments, _EndpointErrorCm, _ChainState, _ContactEvidence);
};

// --------------------------------------------------------------------------------------------------------------------

// A copied observation containing only the requested channels; it holds no live entity handles.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralArticulation_Pose
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralArticulation_Pose);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FTransform _RootFrame = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FTransform _BodyPresentation = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    bool _HasBodyPresentation = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralArticulation_LimbPose> _Limbs;

public:
    CK_PROPERTY_GET(_RootFrame);
    CK_PROPERTY_GET(_BodyPresentation);
    CK_PROPERTY_GET(_HasBodyPresentation);
    CK_PROPERTY_GET(_Limbs);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralArticulation_Pose, _RootFrame, _BodyPresentation, _HasBodyPresentation, _Limbs);
};

// --------------------------------------------------------------------------------------------------------------------


