#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"

#include "CkProceduralAction_Receipt.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_SampleEvidence : uint8
{
    Evaluated,
    Settled
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_SampleEvidence);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_EventKind : uint8
{
    WindowExit,
    Point,
    WindowEnter
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_EventKind);

// --------------------------------------------------------------------------------------------------------------------

// Clip event order is stable at equal times: exits, points, then enters.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Event
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Event);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_Time _Time;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_EventKind _Kind = ECk_ProceduralAction_EventKind::Point;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FName _Id = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _Sequence = 0;

public:
    CK_PROPERTY_GET(_Time);
    CK_PROPERTY_GET(_Kind);
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY_GET(_Sequence);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Event, _Time, _Kind, _Id, _Sequence);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Stamp
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Stamp);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Identity _Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _Generation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _SampleSequence = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _SubjectPoseRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _TopologyRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _ContentRevision = 0;

public:
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_SampleSequence);
    CK_PROPERTY_GET(_SubjectPoseRevision);
    CK_PROPERTY_GET(_TopologyRevision);
    CK_PROPERTY_GET(_ContentRevision);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Stamp, _Identity, _Generation, _SampleSequence, _SubjectPoseRevision, _TopologyRevision, _ContentRevision);
};

// --------------------------------------------------------------------------------------------------------------------

// Windows apply to this interval. Process records in order; a later closure does not erase accepted history.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Interval
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Interval);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_Time _FromTime;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_Time _ToTime;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FName _PhaseId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Continuity _Continuity = ECk_ProceduralAction_Continuity::Incomplete;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    // Active throughout [FromTime, ToTime). Events at ToTime apply after this interval.
    TArray<FName> _Windows;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_Event> _Events;

public:
    CK_PROPERTY_GET(_FromTime);
    CK_PROPERTY_GET(_ToTime);
    CK_PROPERTY_GET(_PhaseId);
    CK_PROPERTY_GET(_Continuity);
    CK_PROPERTY_GET(_Windows);
    CK_PROPERTY_GET(_Events);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Interval, _FromTime, _ToTime, _PhaseId, _Continuity, _Windows, _Events);
};

// --------------------------------------------------------------------------------------------------------------------

// Evaluated samples are solver candidates, not observed intermediate rendered frames or physical contact.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Sample
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Sample);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_Time _Time;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_Pose _Pose;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_SampleEvidence _Evidence = ECk_ProceduralAction_SampleEvidence::Evaluated;

public:
    CK_PROPERTY_GET(_Time);
    CK_PROPERTY_GET(_Pose);
    CK_PROPERTY_GET(_Evidence);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Sample, _Time, _Pose, _Evidence);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_SamplingReport
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_SamplingReport);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int32 _EvaluatedSamples = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int32 _AcceptedSamples = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    float _MaxEndpointTravelCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    float _MaxSegmentAngularStepDegrees = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_Time _Backlog;

public:
    CK_PROPERTY_GET(_EvaluatedSamples);
    CK_PROPERTY_GET(_AcceptedSamples);
    CK_PROPERTY_GET(_MaxEndpointTravelCm);
    CK_PROPERTY_GET(_MaxSegmentAngularStepDegrees);
    CK_PROPERTY_GET(_Backlog);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_SamplingReport, _EvaluatedSamples, _AcceptedSamples, _MaxEndpointTravelCm, _MaxSegmentAngularStepDegrees, _Backlog);
};

// --------------------------------------------------------------------------------------------------------------------

// Immutable accepted-motion evidence. Consumers own collision, obstruction and effect deduplication.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Receipt
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Receipt);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    bool _HasAcceptedPose = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Stamp _Stamp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Interval _Interval;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_Pose _PreviousPose;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_Pose _CurrentPose;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_Sample> _IntermediateSamples;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_SamplingReport _Sampling;

public:
    CK_PROPERTY_GET(_HasAcceptedPose);
    CK_PROPERTY_GET(_Stamp);
    CK_PROPERTY_GET(_Interval);
    CK_PROPERTY_GET(_PreviousPose);
    CK_PROPERTY_GET(_CurrentPose);
    CK_PROPERTY_GET(_IntermediateSamples);
    CK_PROPERTY_GET(_Sampling);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Receipt, _HasAcceptedPose, _Stamp, _Interval, _PreviousPose, _CurrentPose, _IntermediateSamples, _Sampling);
};

// --------------------------------------------------------------------------------------------------------------------

// StreamSequence is action-local and distinct from SampleSequence. Closure and terminal records have reserved capacity.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Record
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Record);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Identity _Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _StreamSequence = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_RecordKind _Kind = ECk_ProceduralAction_RecordKind::AcceptedPose;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Receipt _Receipt;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    TArray<FName> _ClosedWindowIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Result _Result;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_FailureContext _Context;

public:
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_StreamSequence);
    CK_PROPERTY_GET(_Kind);
    CK_PROPERTY_GET(_Receipt);
    CK_PROPERTY_GET(_ClosedWindowIds);
    CK_PROPERTY_GET(_Result);
    CK_PROPERTY(_Context);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Record, _Identity, _StreamSequence, _Kind, _Receipt, _ClosedWindowIds, _Result);
};

// --------------------------------------------------------------------------------------------------------------------

// Reading copies a bounded page; only a consume request, or the player for an Auto action, acknowledges records.
// LastStreamSequence is the last returned record, or the input cursor for an empty ordinary page.
// An empty gap page advances it to the acknowledged high-water sequence.
// Gap reports the inclusive sequence range already consumed by another reader; it never silently claims success.
// Terminal means publication ended; HasMore can still be true until all pages are drained.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ReadPage
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_ProceduralAction_ReadPage);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_ReadResult _Result = ECk_ProceduralAction_ReadResult::Pending;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_Record> _Records;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _LastStreamSequence = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    bool _Terminal = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    bool _HasMore = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _FirstMissingSequence = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Observation", meta = (AllowPrivateAccess = true))
    int64 _LastMissingSequence = 0;

public:
    CK_PROPERTY_GET(_Result);
    CK_PROPERTY_GET(_Records);
    CK_PROPERTY_GET(_LastStreamSequence);
    CK_PROPERTY_GET(_Terminal);
    CK_PROPERTY_GET(_HasMore);
    CK_PROPERTY_GET(_FirstMissingSequence);
    CK_PROPERTY_GET(_LastMissingSequence);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_ReadPage, _Result, _Records, _LastStreamSequence, _Terminal, _HasMore);

    FCk_ProceduralAction_ReadPage(
        ECk_ProceduralAction_ReadResult InResult,
        TArray<FCk_ProceduralAction_Record> InRecords,
        int64 InLastStreamSequence,
        bool InTerminal,
        bool InHasMore,
        int64 InFirstMissingSequence,
        int64 InLastMissingSequence)
        : _Result(InResult)
        , _Records(MoveTemp(InRecords))
        , _LastStreamSequence(InLastStreamSequence)
        , _Terminal(InTerminal)
        , _HasMore(InHasMore)
        , _FirstMissingSequence(InFirstMissingSequence)
        , _LastMissingSequence(InLastMissingSequence)
    {}
};

// --------------------------------------------------------------------------------------------------------------------



