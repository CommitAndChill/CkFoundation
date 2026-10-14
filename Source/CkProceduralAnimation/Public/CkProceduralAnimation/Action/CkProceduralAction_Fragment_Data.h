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
    Ready,
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
    InputTimeout,
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
enum class ECk_ProceduralAction_TuningMode : uint8
{
    PlayerDefault,
    Override
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_TuningMode);

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
    Gap,
    // Names the refusal of a public consume on an identity whose stream managed delivery acknowledges. Reads never
    // return it: reading is non-consuming for every identity.
    ManagedDelivery
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_ReadResult);

// --------------------------------------------------------------------------------------------------------------------

// How a managed stream's delivery ended, independently of the producer's outcome.
UENUM(BlueprintType)
enum class ECk_ProceduralAction_DeliveryStatus : uint8
{
    Complete,
    Expired,
    PlayerLost
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_DeliveryStatus);

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

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Capacity
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Capacity);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true))
    int32 _MaxConcurrentInstances = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true))
    int32 _MaxRetainedInstances = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true))
    int32 _RecordsPerInstance = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true, ClampMin = "2"))
    int64 _MaxRetainedRecords = 1056;

    // Counts retained FTransforms, including diagnostic latest receipts after acknowledgement.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true, ClampMin = "1"))
    int64 _MaxRetainedPoseTransforms = 32768;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true, ClampMin = "1", ClampMax = "64"))
    int32 _MaxSamplesPerReceipt = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true, ClampMin = "1", ClampMax = "64"))
    int32 _MaxEventsPerReceipt = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true, ClampMin = "1", ClampMax = "32"))
    int32 _MaxWindowsPerReceipt = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true))
    FCk_Time _OrphanRetentionTimeout = FCk_Time{2.0};

    // Records delivered to managed listeners per player per tick, shared fairly across managed identities.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Capacity", meta = (AllowPrivateAccess = true, ClampMin = "1"))
    int32 _ManagedRecordsPerTick = 32;

public:
    CK_PROPERTY(_MaxConcurrentInstances);
    CK_PROPERTY(_MaxRetainedInstances);
    CK_PROPERTY(_RecordsPerInstance);
    CK_PROPERTY(_MaxRetainedRecords);
    CK_PROPERTY(_MaxRetainedPoseTransforms);
    CK_PROPERTY(_MaxSamplesPerReceipt);
    CK_PROPERTY(_MaxEventsPerReceipt);
    CK_PROPERTY(_MaxWindowsPerReceipt);
    CK_PROPERTY(_OrphanRetentionTimeout);
    CK_PROPERTY(_ManagedRecordsPerTick);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_SlotBinding
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_SlotBinding);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    FName _Slot = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    FName _LimbId = NAME_None;

public:
    CK_PROPERTY_GET(_Slot);
    CK_PROPERTY_GET(_LimbId);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_SlotBinding, _Slot, _LimbId);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_AnchorBinding
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_AnchorBinding);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    FName _Id = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    FTransform _Frame = FTransform::Identity;

public:
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY_GET(_Frame);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_AnchorBinding, _Id, _Frame);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_PlayOptions
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_PlayOptions);

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_SlotBinding> _SlotBindings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_AnchorBinding> _AnchorBindings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Tuning", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_TuningMode _TuningMode = ECk_ProceduralAction_TuningMode::PlayerDefault;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Tuning", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Tuning _Tuning;

    /** Applied to this play's compiled copy of the clip before its slots are bound: each mirror adds its destination
     *  slot, reflected from the source slot's tracks across the plane in the body frame (rest-normalized tracks are
     *  converted through both limbs' rest frames). The destination needs its own slot binding to a different limb.
     *  Body motion, markers and windows are not mirrored, and the clip asset is never changed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_MirrorSpec> _Mirrors;

public:
    CK_PROPERTY(_SlotBindings);
    CK_PROPERTY(_AnchorBindings);
    CK_PROPERTY(_Mirrors);
    CK_PROPERTY_GET(_TuningMode);
    CK_PROPERTY_GET(_Tuning);

    auto
        Set_TuningMode(
            const ECk_ProceduralAction_TuningMode& InValue)
        -> FCk_ProceduralAction_PlayOptions&
    {
        _TuningMode = InValue;
        if (InValue == ECk_ProceduralAction_TuningMode::PlayerDefault)
        { _Tuning = FCk_ProceduralAction_Tuning{}; }
        return *this;
    }
    CK_ANGELSCRIPT_PROPERTY_REGISTRATION_SETTER_ONLY(_TuningMode);

    auto
        Set_Tuning(
            const FCk_ProceduralAction_Tuning& InValue)
        -> FCk_ProceduralAction_PlayOptions&
    {
        _Tuning = InValue;
        _TuningMode = ECk_ProceduralAction_TuningMode::Override;
        return *this;
    }
    CK_ANGELSCRIPT_PROPERTY_REGISTRATION_SETTER_ONLY(_Tuning);

    auto
        Get_HasConsistentTuningSelection() const
        -> bool;

public:
    FCk_ProceduralAction_PlayOptions() = default;
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_PlaySpec : public FCk_ProceduralAction_PlayOptions
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_PlaySpec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Content", meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UCk_ProceduralAction_Data> _Clip;

public:
    CK_PROPERTY_GET(_Clip);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_PlaySpec, _Clip);

    CK_PROPERTY(_SlotBindings);
    CK_PROPERTY(_AnchorBindings);
    CK_PROPERTY(_Mirrors);
    CK_PROPERTY_GET(_TuningMode);
    CK_PROPERTY_GET(_Tuning);

    auto
        Set_TuningMode(
            const ECk_ProceduralAction_TuningMode& InValue)
        -> FCk_ProceduralAction_PlaySpec&
    {
        FCk_ProceduralAction_PlayOptions::Set_TuningMode(InValue);
        return *this;
    }
    CK_ANGELSCRIPT_PROPERTY_REGISTRATION_SETTER_ONLY(_TuningMode);

    auto
        Set_Tuning(
            const FCk_ProceduralAction_Tuning& InValue)
        -> FCk_ProceduralAction_PlaySpec&
    {
        FCk_ProceduralAction_PlayOptions::Set_Tuning(InValue);
        return *this;
    }
    CK_ANGELSCRIPT_PROPERTY_REGISTRATION_SETTER_ONLY(_Tuning);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_LoadedPlaySpec : public FCk_ProceduralAction_PlayOptions
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_LoadedPlaySpec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Content", meta = (AllowPrivateAccess = true))
    UCk_ProceduralAction_Data* _Clip = nullptr;

public:
    CK_PROPERTY_GET(_Clip);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_LoadedPlaySpec, _Clip);

    CK_PROPERTY(_SlotBindings);
    CK_PROPERTY(_AnchorBindings);
    CK_PROPERTY(_Mirrors);
    CK_PROPERTY_GET(_TuningMode);
    CK_PROPERTY_GET(_Tuning);

    auto
        Set_TuningMode(
            const ECk_ProceduralAction_TuningMode& InValue)
        -> FCk_ProceduralAction_LoadedPlaySpec&
    {
        FCk_ProceduralAction_PlayOptions::Set_TuningMode(InValue);
        return *this;
    }
    CK_ANGELSCRIPT_PROPERTY_REGISTRATION_SETTER_ONLY(_TuningMode);

    auto
        Set_Tuning(
            const FCk_ProceduralAction_Tuning& InValue)
        -> FCk_ProceduralAction_LoadedPlaySpec&
    {
        FCk_ProceduralAction_PlayOptions::Set_Tuning(InValue);
        return *this;
    }
    CK_ANGELSCRIPT_PROPERTY_REGISTRATION_SETTER_ONLY(_Tuning);
};

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
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ControlSpec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_ControlSpec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Resources", meta = (AllowPrivateAccess = true))
    TArray<FName> _LimbIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_AnchorBinding> _AnchorBindings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Resources", meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _BodyChannel = ECk_EnableDisable::Disable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Watchdogs", meta = (AllowPrivateAccess = true))
    FCk_Time _InputTimeout = FCk_Time{0.5};

public:
    CK_PROPERTY_GET(_LimbIds);
    CK_PROPERTY(_AnchorBindings);
    CK_PROPERTY(_BodyChannel);
    CK_PROPERTY(_InputTimeout);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_ControlSpec, _LimbIds);
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
 *  return deadline that fired (AcquisitionTimeout, LoadTimeout, InputTimeout, PublicationTimeout, and RecoveryFailed
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

/** What Play and Play_Loaded return. A valid action reports Valid; its producer outcome arrives through the managed
 *  completion, never before the call returns. A start that could not be created reports Invalid with an invalid
 *  action, no identity and no completion. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ManagedStart
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_ManagedStart);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_Handle_ProceduralAction _Action;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Identity _Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_ValidationReport _Report;

public:
    CK_PROPERTY_GET(_Action);
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_Report);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_ManagedStart, _Action, _Identity, _Report);
};

// --------------------------------------------------------------------------------------------------------------------

/** Fires once per managed identity, after the terminal record and every page before it were delivered and
 *  acknowledged, or once from the player's end of play. _Result is the producer terminal as recorded (Outcome None
 *  when no terminal was delivered); _Delivery says how delivery ended. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ManagedCompletion
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_ManagedCompletion);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Identity _Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Result _Result;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_DeliveryStatus _Delivery = ECk_ProceduralAction_DeliveryStatus::Complete;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_FailureContext _Context;

public:
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_Result);
    CK_PROPERTY_GET(_Delivery);
    CK_PROPERTY_GET(_Context);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_ManagedCompletion, _Identity, _Result, _Delivery, _Context);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_LimbTarget
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_LimbTarget);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FName _LimbId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FTransform _Endpoint = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action|Bend", meta = (AllowPrivateAccess = true))
    bool _HasBend = false;

    // Retained while inactive so toggling the authoring override does not discard its value.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action|Bend", meta = (AllowPrivateAccess = true, EditCondition = "_HasBend"))
    FVector _BendDirection = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_TrackSpace _Space = ECk_ProceduralAction_TrackSpace::BodyLocal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FName _AnchorId = NAME_None;

public:
    CK_PROPERTY_GET(_LimbId);
    CK_PROPERTY_GET(_Endpoint);
    CK_PROPERTY(_HasBend);
    CK_PROPERTY_GET(_BendDirection);

    auto
        Set_BendDirection(
            const FVector& InValue)
        -> FCk_ProceduralAction_LimbTarget&
    {
        _BendDirection = InValue;
        _HasBend = true;
        return *this;
    }
    CK_ANGELSCRIPT_PROPERTY_REGISTRATION_SETTER_ONLY(_BendDirection);

    auto
        Clear_BendDirection()
        -> FCk_ProceduralAction_LimbTarget&
    {
        _HasBend = false;
        return *this;
    }
    CK_PROPERTY(_Space);
    CK_PROPERTY(_AnchorId);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_LimbTarget, _LimbId, _Endpoint);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_PoseBatch
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_PoseBatch);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_LimbTarget> _Limbs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _BodyChannel = ECk_EnableDisable::Disable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FTransform _BodyOffset = FTransform::Identity;

public:
    CK_PROPERTY(_Limbs);
    CK_PROPERTY(_BodyChannel);
    CK_PROPERTY(_BodyOffset);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_Request_ProceduralAction_SubmitPoseBatch : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_ProceduralAction_SubmitPoseBatch);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_ProceduralAction_SubmitPoseBatch);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_PoseBatch _Batch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    int64 _CorrelationToken = 0;

public:
    CK_PROPERTY_GET(_Batch);
    CK_PROPERTY(_CorrelationToken);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_ProceduralAction_SubmitPoseBatch, _Batch);
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
struct CKPROCEDURALANIMATION_API FCk_Request_ProceduralAction_End : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_ProceduralAction_End);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_ProceduralAction_End);
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

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_Request_ProceduralActionPlayer_UpdateTuning : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_ProceduralActionPlayer_UpdateTuning);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_ProceduralActionPlayer_UpdateTuning);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Action", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Tuning _Tuning;

public:
    CK_PROPERTY_GET(_Tuning);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_ProceduralActionPlayer_UpdateTuning, _Tuning);
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


