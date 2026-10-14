#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"

#include "CkProceduralAction_Debug.generated.h"

// --------------------------------------------------------------------------------------------------------------------

// Only active watchdogs are included; times use the same serviced clock as the runtime.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_DebugDeadline
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralAction_DebugDeadline);

private:
    friend class UCk_Utils_ProceduralAnimation_Debug_UE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FName _Name = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_Time _Elapsed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_Time _Limit;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_Time _Remaining;

public:
    CK_PROPERTY_GET(_Name);
    CK_PROPERTY_GET(_Elapsed);
    CK_PROPERTY_GET(_Limit);
    CK_PROPERTY_GET(_Remaining);

    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_DebugDeadline, _Name, _Elapsed, _Limit, _Remaining);
};

// --------------------------------------------------------------------------------------------------------------------

// Copied on demand. This is diagnostic state, never permission to publish or apply gameplay.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_DebugInstance
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralAction_DebugInstance);

private:
    friend class UCk_Utils_ProceduralAnimation_Debug_UE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _Available = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Identity _Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _Generation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_State _State = ECk_ProceduralAction_State::PendingAdmission;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Result _Result;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_Time _AcceptedTime;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_Time _PlaybackBacklog;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _ContentRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _TuningRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _NextSampleSequence = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _PublicationPending = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_DebugDeadline> _Watchdogs;

public:
    CK_PROPERTY_GET(_Available);
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_State);
    CK_PROPERTY_GET(_Result);
    CK_PROPERTY_GET(_AcceptedTime);
    CK_PROPERTY_GET(_PlaybackBacklog);
    CK_PROPERTY_GET(_ContentRevision);
    CK_PROPERTY_GET(_TuningRevision);
    CK_PROPERTY_GET(_NextSampleSequence);
    CK_PROPERTY_GET(_PublicationPending);
    CK_PROPERTY_GET(_Watchdogs);
};

// --------------------------------------------------------------------------------------------------------------------

// World-space committed gait contact/reservation; no physics query is performed to refresh it.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_DebugContact
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralAction_DebugContact);

private:
    friend class UCk_Utils_ProceduralAnimation_Debug_UE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FName _LimbId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FVector _Position = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FVector _Normal = FVector::UpVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _TrustedSupport = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _Reserved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    float _ReservationRadiusCm = 0.0f;

public:
    CK_PROPERTY_GET(_LimbId);
    CK_PROPERTY_GET(_Position);
    CK_PROPERTY_GET(_Normal);
    CK_PROPERTY_GET(_TrustedSupport);
    CK_PROPERTY_GET(_Reserved);
    CK_PROPERTY_GET(_ReservationRadiusCm);
};

// --------------------------------------------------------------------------------------------------------------------

// Proposed return destination, separate from the actual accepted foot and its contact evidence.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_DebugLanding
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralAction_DebugLanding);

private:
    friend class UCk_Utils_ProceduralAnimation_Debug_UE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FName _LimbId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FVector _Position = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FVector _Normal = FVector::UpVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _ActualContactTrusted = false;

public:
    CK_PROPERTY_GET(_LimbId);
    CK_PROPERTY_GET(_Position);
    CK_PROPERTY_GET(_Normal);
    CK_PROPERTY_GET(_ActualContactTrusted);
};

// --------------------------------------------------------------------------------------------------------------------

// Current support evaluation of a pinned lease policy, without takeoff prediction or a new query.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_DebugLease
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralAction_DebugLease);

private:
    friend class UCk_Utils_ProceduralAnimation_Debug_UE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _Identity = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _Generation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_ProceduralLimbControl_Support _Policy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _ProtectionActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _Interrupted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FName _Reason = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FName _SupportResult = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int32 _TrustedContacts = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    double _MinimumEdgeDistanceCm = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    TArray<FVector> _ProjectedRegion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_DebugLanding> _Landings;

public:
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_Policy);
    CK_PROPERTY_GET(_ProtectionActive);
    CK_PROPERTY_GET(_Interrupted);
    CK_PROPERTY_GET(_Reason);
    CK_PROPERTY_GET(_SupportResult);
    CK_PROPERTY_GET(_TrustedContacts);
    CK_PROPERTY_GET(_MinimumEdgeDistanceCm);
    CK_PROPERTY_GET(_ProjectedRegion);
    CK_PROPERTY_GET(_Landings);
};

// --------------------------------------------------------------------------------------------------------------------

// Root-owned recovery remains inspectable after the action entity is destroyed.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_DebugController
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralAction_DebugController);

private:
    friend class UCk_Utils_ProceduralAnimation_Debug_UE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _Identity = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _Generation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FString _OwnerEntityId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    TArray<FName> _LimbIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _Body = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    ECk_ProceduralLimbControl_ReturnState _ReturnState = ECk_ProceduralLimbControl_ReturnState::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FName _Reason = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _AwaitingSettlement = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_DebugDeadline> _Watchdogs;

public:
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_OwnerEntityId);
    CK_PROPERTY_GET(_LimbIds);
    CK_PROPERTY_GET(_Body);
    CK_PROPERTY_GET(_ReturnState);
    CK_PROPERTY_GET(_Reason);
    CK_PROPERTY_GET(_AwaitingSettlement);
    CK_PROPERTY_GET(_Watchdogs);
};

// --------------------------------------------------------------------------------------------------------------------

// Game-thread value copy. SupportFresh validates the support/root frame; it does not date the last contact solve.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_DebugSubject
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_ProceduralAction_DebugSubject);

private:
    friend class UCk_Utils_ProceduralAnimation_Debug_UE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _Available = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FTransform _RootFrame = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    int64 _SettledRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _HasSettledPose = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_Pose _SettledPose;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _PublicationPending = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    bool _SupportFresh = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    FVector _SupportUp = FVector::UpVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_DebugContact> _Contacts;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_DebugLease> _Leases;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostics", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_DebugController> _Controllers;

public:
    CK_PROPERTY_GET(_Available);
    CK_PROPERTY_GET(_RootFrame);
    CK_PROPERTY_GET(_SettledRevision);
    CK_PROPERTY_GET(_HasSettledPose);
    CK_PROPERTY_GET(_SettledPose);
    CK_PROPERTY_GET(_PublicationPending);
    CK_PROPERTY_GET(_SupportFresh);
    CK_PROPERTY_GET(_SupportUp);
    CK_PROPERTY_GET(_Contacts);
    CK_PROPERTY_GET(_Leases);
    CK_PROPERTY_GET(_Controllers);
};

