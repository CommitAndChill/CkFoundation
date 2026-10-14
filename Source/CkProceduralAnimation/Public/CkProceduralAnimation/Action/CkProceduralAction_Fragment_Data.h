#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"

#include "CkCore/Enums/CkEnums.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Time/CkTime.h"
#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkEcs/Request/CkRequest_Data.h"

#include "CkProceduralAction_Fragment_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_State : uint8
{
    PendingLoad,
    PendingAdmission,
    Acquiring,
    Playing,
    Returning,
    Cancelling,
    Finished
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_State);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_Outcome : uint8
{
    None,
    Completed,
    Rejected,
    Cancelled,
    OwnerLost,
    SubjectLost,
    PartLost,
    AssetFailed,
    InvalidContent,
    SupportLost,
    AcquisitionTimeout,
    PublicationTimeout,
    PublicationFailed,
    SubjectPoseRejected,
    SamplingBudgetExceeded,
    ReceiptOverflow,
    RecoveryFailed
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_Outcome);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_Recovery : uint8
{
    NotRequired,
    Pending,
    Succeeded,
    Failed
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_Recovery);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_Continuity : uint8
{
    Continuous,
    FirstSample,
    Teleported,
    ControllerChanged,
    SolverDiscontinuity,
    Incomplete
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_Continuity);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_RecordKind : uint8
{
    AcceptedPose,
    WindowsClosed,
    Terminal
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_RecordKind);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_ReadResult : uint8
{
    Succeeded,
    Pending,
    StreamExpired,
    InvalidCursor,
    InvalidPlayer,
    Gap
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_ReadResult);

// --------------------------------------------------------------------------------------------------------------------

// Who acknowledges an action's receipt stream. Explicit: the caller reads and consumes it. Auto: the player acknowledges
// every record as it is published, and OnFinished is the completion signal. Either way the action releases itself once
// its terminal record is acknowledged.
UENUM(BlueprintType)
enum class ECk_ProceduralAction_Delivery : uint8
{
    Explicit,
    Auto
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_Delivery);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKPROCEDURALANIMATION_API FCk_Handle_ProceduralAction : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_ProceduralAction);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_ProceduralAction);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Identity
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Identity);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    int64 _Value = 0;

public:
    CK_PROPERTY_GET(_Value);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Identity, _Value);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Playback
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Playback);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Playback", meta = (AllowPrivateAccess = true))
    float _PlayRate = 1.0f;

public:
    CK_PROPERTY(_PlayRate);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Transition
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Transition);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Transition", meta = (AllowPrivateAccess = true))
    FCk_Time _EntryBlend = FCk_Time{0.1};

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Transition", meta = (AllowPrivateAccess = true))
    FCk_Time _DedicatedReturn = FCk_Time{0.2};

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Transition", meta = (AllowPrivateAccess = true))
    FCk_Time _ReturnTimeout = FCk_Time{1.0};

public:
    CK_PROPERTY(_EntryBlend);
    CK_PROPERTY(_DedicatedReturn);
    CK_PROPERTY(_ReturnTimeout);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Watchdogs
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Watchdogs);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Watchdogs", meta = (AllowPrivateAccess = true))
    FCk_Time _LoadTimeout = FCk_Time{5.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Watchdogs", meta = (AllowPrivateAccess = true))
    FCk_Time _AcquisitionTimeout = FCk_Time{1.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Watchdogs", meta = (AllowPrivateAccess = true))
    FCk_Time _PublicationTimeout = FCk_Time{1.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Watchdogs", meta = (AllowPrivateAccess = true))
    FCk_Time _MaxTimeBacklog = FCk_Time{0.5};

public:
    CK_PROPERTY(_LoadTimeout);
    CK_PROPERTY(_AcquisitionTimeout);
    CK_PROPERTY(_PublicationTimeout);
    CK_PROPERTY(_MaxTimeBacklog);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Tuning
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Tuning);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Playback", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Playback _Playback;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Transition", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Transition _Transition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Watchdogs", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Watchdogs _Watchdogs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Sampling", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Sampling _Sampling;

public:
    CK_PROPERTY(_Playback);
    CK_PROPERTY(_Transition);
    CK_PROPERTY(_Watchdogs);
    CK_PROPERTY(_Sampling);
};

// --------------------------------------------------------------------------------------------------------------------

/** A player's receipt budget. The rest is derived: 4 retained streams per concurrent action (at most 256), each with room
 *  for RecordsPerInstance accepted poses plus its closure and terminal records, 32 pose transforms per accepted pose, and
 *  a finished stream whose action is gone is kept 2 s for late readers. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Capacity
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Capacity);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true, ClampMin = "1", ClampMax = "64"))
    int32 _MaxConcurrentInstances = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true, ClampMin = "1", ClampMax = "4096"))
    int32 _RecordsPerInstance = 64;

public:
    CK_PROPERTY(_MaxConcurrentInstances);
    CK_PROPERTY(_RecordsPerInstance);
};

// --------------------------------------------------------------------------------------------------------------------

/** One play of a clip. A clip that is already loaded is compiled and bound inside Create_FromClip; an unloaded one is
 *  loaded asynchronously first. Every slot the clip declares needs one binding to a distinct limb of the subject. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_PlaySpec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_PlaySpec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Content", meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UCk_ProceduralAction_Data> _Clip;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_SlotBinding> _SlotBindings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Delivery", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Delivery _Delivery = ECk_ProceduralAction_Delivery::Auto;

public:
    CK_PROPERTY_GET(_Clip);
    CK_PROPERTY(_SlotBindings);
    CK_PROPERTY(_Delivery);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_PlaySpec, _Clip);
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_ReportStatus : uint8
{
    Valid,
    Invalid,
    ContentNotResident
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_ReportStatus);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_DiagnosticSeverity : uint8
{
    Error,
    Warning
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_DiagnosticSeverity);

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Diagnostic
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Diagnostic);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_DiagnosticSeverity _Severity = ECk_ProceduralAction_DiagnosticSeverity::Error;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    FName _Code;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    FString _AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    FString _FieldPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    FName _PhaseId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    FName _Slot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    int32 _KeyIndex = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    FString _Expected;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diagnostic", meta = (AllowPrivateAccess = true))
    FString _Actual;

public:
    CK_PROPERTY_GET(_Severity);
    CK_PROPERTY_GET(_Code);
    CK_PROPERTY_GET(_AssetPath);
    CK_PROPERTY_GET(_FieldPath);
    CK_PROPERTY_GET(_PhaseId);
    CK_PROPERTY_GET(_Slot);
    CK_PROPERTY_GET(_KeyIndex);
    CK_PROPERTY_GET(_Expected);
    CK_PROPERTY_GET(_Actual);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Diagnostic, _Severity, _Code, _AssetPath,
        _FieldPath, _PhaseId, _Slot, _KeyIndex, _Expected, _Actual);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ValidationReport
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_ValidationReport);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_ReportStatus _Status = ECk_ProceduralAction_ReportStatus::Invalid;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation", meta = (AllowPrivateAccess = true))
    bool _Advisory = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_Diagnostic> _Diagnostics;

public:
    CK_PROPERTY_GET(_Status);
    CK_PROPERTY_GET(_Advisory);
    CK_PROPERTY_GET(_Diagnostics);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_ValidationReport, _Status, _Advisory,
        _Diagnostics);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Result
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Result);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Outcome _Outcome = ECk_ProceduralAction_Outcome::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Recovery _Recovery = ECk_ProceduralAction_Recovery::NotRequired;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FName _Reason = NAME_None;

public:
    CK_PROPERTY_GET(_Outcome);
    CK_PROPERTY_GET(_Recovery);
    CK_PROPERTY_GET(_Reason);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Result, _Outcome, _Recovery, _Reason);
};

// --------------------------------------------------------------------------------------------------------------------

/** Where a failed action stood and what the failing check measured, captured at the failure source.
 *  Every field is optional: None, zero and empty mean "not measured", never a measured zero. _PhaseId is the clip phase
 *  at the accepted time; _LimbIds are the limbs the action claimed. _Elapsed against _Limit is the watchdog or
 *  return deadline that fired (AcquisitionTimeout, LoadTimeout, PublicationTimeout, and RecoveryFailed
 *  only when the return failed on its deadline). _Observed against _Budget is a count in _BudgetUnit: settled endpoint
 *  travel in micrometres or segment angle in millidegrees for SamplingBudgetExceeded, unacknowledged records for a
 *  ReceiptOverflow at the per-action limit. A RecoveryFailed that did not miss a deadline, a clip whose sampled
 *  interval was rejected before settlement and a ReceiptOverflow on a player-wide limit carry only phase and limbs. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_FailureContext
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_FailureContext);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FName _PhaseId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    TArray<FName> _LimbIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_Time _Elapsed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_Time _Limit;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FName _BudgetUnit = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    int64 _Observed = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              Category = "Action", meta = (AllowPrivateAccess = true))
    int64 _Budget = 0;

public:
    CK_PROPERTY(_PhaseId);
    CK_PROPERTY(_LimbIds);
    CK_PROPERTY(_Elapsed);
    CK_PROPERTY(_Limit);
    CK_PROPERTY(_BudgetUnit);
    CK_PROPERTY(_Observed);
    CK_PROPERTY(_Budget);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Terminal
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Terminal);
private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Identity _Identity;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    int64 _Generation = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Result _Result;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_FailureContext _Context;
public:
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_Result);
    CK_PROPERTY_GET(_Context);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Terminal, _Identity, _Generation, _Result, _Context);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_Request_ProceduralAction_Cancel : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_ProceduralAction_Cancel);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_ProceduralAction_Cancel);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FName _Reason = NAME_None;

public:
    CK_PROPERTY_GET(_Reason);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_ProceduralAction_Cancel, _Reason);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_Request_ProceduralAction_Release : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_ProceduralAction_Release);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_ProceduralAction_Release);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_Request_ProceduralActionPlayer_ConsumeReceipts : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_ProceduralActionPlayer_ConsumeReceipts);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_ProceduralActionPlayer_ConsumeReceipts);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Identity _ActionIdentity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    int64 _ThroughStreamSequence = 0;

public:
    CK_PROPERTY_GET(_ActionIdentity);
    CK_PROPERTY_GET(_ThroughStreamSequence);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_ProceduralActionPlayer_ConsumeReceipts, _ActionIdentity, _ThroughStreamSequence);
};

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FCk_Delegate_ProceduralAction_OnStateChanged,
    FCk_Handle_ProceduralAction, InAction,
    ECk_ProceduralAction_State, InState);

DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FCk_Delegate_ProceduralAction_OnFinished,
    FCk_Handle_ProceduralAction, InAction,
    FCk_ProceduralAction_Terminal, InTerminal);


