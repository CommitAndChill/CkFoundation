#pragma once

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Time/CkTime.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Request/CkRequest_Data.h"

#include "CkProceduralLimbControl_Fragment_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralLimbControl_State : uint8
{
    Default,
    Controlled,
    Returning,
    Unplanted,
    Unavailable
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralLimbControl_State);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralLimbControl_ReturnState : uint8
{
    None,
    Returning,
    Returned,
    Failed
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralLimbControl_ReturnState);

// --------------------------------------------------------------------------------------------------------------------

// Walking-limb recovery captures this policy once. Repeated cancellation cannot extend the deadline. Each recovery
// interval is held to the player's sampling limits (FCk_ProceduralAction_Sampling).
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralLimbControl_Return
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralLimbControl_Return);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (AllowPrivateAccess = true))
    FCk_Time _Duration = FCk_Time{0.2};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Watchdog", meta = (AllowPrivateAccess = true))
    FCk_Time _Timeout = FCk_Time{1.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search", meta = (AllowPrivateAccess = true, ClampMin = "1", ClampMax = "16"))
    int32 _MaxProbeAttempts = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search", meta = (AllowPrivateAccess = true))
    FCk_Time _RetryInterval = FCk_Time{0.1};

    // Maximum distance from the actually settled terminal endpoint to the freshly confirmed contact.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact", meta = (AllowPrivateAccess = true, Units = "cm", ClampMin = "0"))
    float _ContactToleranceCm = 2.0f;

public:
    CK_PROPERTY(_Duration);
    CK_PROPERTY(_Timeout);
    CK_PROPERTY(_MaxProbeAttempts);
    CK_PROPERTY(_RetryInterval);
    CK_PROPERTY(_ContactToleranceCm);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralLimbControl_Observation
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralLimbControl_Observation);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control", meta = (AllowPrivateAccess = true))
    ECk_ProceduralLimbControl_State _State = ECk_ProceduralLimbControl_State::Unavailable;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control", meta = (AllowPrivateAccess = true))
    FCk_Handle _Owner;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Control", meta = (AllowPrivateAccess = true))
    int64 _Generation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = true))
    bool _Locomotor = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = true))
    bool _Participating = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = true))
    bool _TrustedSupport = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (AllowPrivateAccess = true))
    FName _Reason;

public:
    CK_PROPERTY_GET(_State);
    CK_PROPERTY_GET(_Owner);
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_Locomotor);
    CK_PROPERTY_GET(_Participating);
    CK_PROPERTY_GET(_TrustedSupport);
    CK_PROPERTY_GET(_Reason);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralLimbControl_Observation, _State, _Owner, _Generation, _Locomotor,
        _Participating, _TrustedSupport, _Reason);
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralLimbControl_SupportMode : uint8
{
    Disabled,
    CountOnly,
    ProjectedSupportRegion
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralLimbControl_SupportMode);

// --------------------------------------------------------------------------------------------------------------------

// Borrowing walking limbs requires an explicit policy. The projected root is a kinematic support proxy, not a COM.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralLimbControl_Support
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralLimbControl_Support);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirement", meta = (AllowPrivateAccess = true))
    ECk_ProceduralLimbControl_SupportMode _Mode = ECk_ProceduralLimbControl_SupportMode::Disabled;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirement", meta = (AllowPrivateAccess = true))
    int32 _MinimumTrustedContacts = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (AllowPrivateAccess = true, Units = "cm"))
    float _ProjectedRegionInsetCm = 0.0f;

    // Every trusted normal must agree with the support frame up and with every other contact within this cone.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (AllowPrivateAccess = true, Units = "deg"))
    float _MaxContactNormalSpreadDegrees = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Watchdog", meta = (AllowPrivateAccess = true))
    FCk_Time _SupportHoldTimeout = FCk_Time{0.25};

public:
    CK_PROPERTY(_Mode);
    CK_PROPERTY(_MinimumTrustedContacts);
    CK_PROPERTY(_ProjectedRegionInsetCm);
    CK_PROPERTY(_MaxContactNormalSpreadDegrees);
    CK_PROPERTY(_SupportHoldTimeout);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralLimbControl_Support, _Mode, _MinimumTrustedContacts);
};

// --------------------------------------------------------------------------------------------------------------------

// Construction-time locomotion adapter settings. Active leases and recovery pin their admitted values.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralLimbControl_Locomotion
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralLimbControl_Locomotion);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support", meta = (AllowPrivateAccess = true))
    FCk_ProceduralLimbControl_Support _Support;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery", meta = (AllowPrivateAccess = true))
    FCk_ProceduralLimbControl_Return _Return;

public:
    CK_PROPERTY(_Support);
    CK_PROPERTY(_Return);
};

// --------------------------------------------------------------------------------------------------------------------

/** Recovers every limb that is Unplanted when the request drains, not when it was enqueued, as one complete set.
 *  An empty set succeeds without claims; a set the support policy cannot admit fails and claims nothing.
 *  Completion reports admission. Observe Get_LimbControl until the limbs return to Default or Unplanted. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs);
};
