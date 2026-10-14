#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"

// Native protocol values shared by module fragments; mutation remains in the private Control facade.
struct CKPROCEDURALANIMATION_API FCk_ArticulationControlToken
{
    CK_GENERATED_BODY(FCk_ArticulationControlToken);

private:
    uint64 _InstanceId = 0;
    uint64 _Generation = 0;

public:
    CK_PROPERTY_GET(_InstanceId);
    CK_PROPERTY_GET(_Generation);
    CK_DEFINE_CONSTRUCTORS(FCk_ArticulationControlToken, _InstanceId, _Generation);

    auto operator==(const FCk_ArticulationControlToken& InOther) const -> bool
    { return _InstanceId == InOther._InstanceId && _Generation == InOther._Generation; }

    auto operator!=(const FCk_ArticulationControlToken& InOther) const -> bool
    { return NOT (*this == InOther); }
};

struct CKPROCEDURALANIMATION_API FCk_ArticulationChannelSet
{
    CK_GENERATED_BODY(FCk_ArticulationChannelSet);

private:
    TArray<FName> _LimbIds;
    bool _Body = false;

public:
    CK_PROPERTY_GET(_LimbIds);
    CK_PROPERTY_GET(_Body);
    CK_DEFINE_CONSTRUCTORS(FCk_ArticulationChannelSet, _LimbIds, _Body);
};

enum class ECk_ArticulationReachPolicy : uint8
{
    RejectUnreachable,
    ProjectToReach
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ArticulationReachPolicy);

enum class ECk_ArticulationControlResult : uint8
{
    Accepted,
    Busy,
    InvalidSubject,
    InvalidChannels,
    InvalidCandidate,
    ClaimConflict,
    ClaimLost,
    StaleInput,
    Faulted
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ArticulationControlResult);

enum class ECk_ArticulationSettlement : uint8
{
    Pending,
    Settled,
    ClaimLost,
    Faulted
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ArticulationSettlement);

struct CKPROCEDURALANIMATION_API FCk_ArticulationLimbTarget
{
    CK_GENERATED_BODY(FCk_ArticulationLimbTarget);

private:
    FName _Id = NAME_None;
    FTransform _RequestedEndpointWorld = FTransform::Identity;
    TOptional<FVector> _PoleWorld;
    ECk_ArticulationReachPolicy _ReachPolicy = ECk_ArticulationReachPolicy::RejectUnreachable;

public:
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY_GET(_RequestedEndpointWorld);
    CK_PROPERTY_GET(_PoleWorld);
    CK_PROPERTY_GET(_ReachPolicy);
    CK_DEFINE_CONSTRUCTORS(FCk_ArticulationLimbTarget, _Id, _RequestedEndpointWorld, _PoleWorld, _ReachPolicy);
};

struct CKPROCEDURALANIMATION_API FCk_ArticulationTokenCandidate
{
    CK_GENERATED_BODY(FCk_ArticulationTokenCandidate);

private:
    FCk_ArticulationControlToken _Token;
    // Zero holds a return/current target without producing an Action receipt.
    uint64 _SampleSequence = 0;
    TArray<FCk_ArticulationLimbTarget> _Limbs;
    TOptional<FTransform> _BodyPresentationWorld;

public:
    CK_PROPERTY_GET(_Token);
    CK_PROPERTY_GET(_SampleSequence);
    CK_PROPERTY_GET(_Limbs);
    CK_PROPERTY_GET(_BodyPresentationWorld);
    CK_DEFINE_CONSTRUCTORS(FCk_ArticulationTokenCandidate, _Token, _SampleSequence, _Limbs, _BodyPresentationWorld);
};

struct CKPROCEDURALANIMATION_API FCk_ArticulationStageKey
{
    CK_GENERATED_BODY(FCk_ArticulationStageKey);

private:
    FCk_ArticulationControlToken _Token;
    uint64 _SampleSequence = 0;
    uint64 _BatchNonce = 0;

public:
    CK_PROPERTY_GET(_Token);
    CK_PROPERTY_GET(_SampleSequence);
    CK_PROPERTY_GET(_BatchNonce);
    CK_DEFINE_CONSTRUCTORS(FCk_ArticulationStageKey, _Token, _SampleSequence, _BatchNonce);
};

struct CKPROCEDURALANIMATION_API FCk_ArticulationControlState
{
    CK_GENERATED_BODY(FCk_ArticulationControlState);
    friend class FCk_ProceduralArticulation_Control;

private:
    bool _HasClaims = false;
    bool _HasPendingSample = false;
    uint64 _LastIssuedRevision = 0;
    uint64 _LastSettledRevision = 0;

public:
    CK_PROPERTY_GET(_HasClaims);
    CK_PROPERTY_GET(_HasPendingSample);
    CK_PROPERTY_GET(_LastIssuedRevision);
    CK_PROPERTY_GET(_LastSettledRevision);
};

struct CKPROCEDURALANIMATION_API FCk_ArticulationBatchEvaluation
{
    CK_GENERATED_BODY(FCk_ArticulationBatchEvaluation);
    friend class FCk_ProceduralArticulation_Control;

private:
    // Prediction only. Actual acceptance requires ObserveSettlement.
    FCk_ProceduralArticulation_Pose _PredictedSubjectPose;
    TArray<FCk_ProceduralArticulation_Pose> _PredictedTokenPoses;
    uint64 _InputRevision = 0;
    FCk_Handle_ProceduralArticulation _Subject;
    TArray<FCk_ArticulationTokenCandidate> _Candidates;
    TArray<FCk_Handle_Transform> _BoundParts;
    TArray<FTransform> _PartPoses;
    FTransform _RootAtEvaluation = FTransform::Identity;
    FTransform _BodyOffsetAtEvaluation = FTransform::Identity;
    uint64 _GaitSequenceAtEvaluation = 0;
    uint64 _ClaimsRevisionAtEvaluation = 0;
    uint64 _FrameAtEvaluation = 0;

public:
    CK_PROPERTY_GET(_PredictedSubjectPose);
    CK_PROPERTY_GET(_PredictedTokenPoses);
    CK_PROPERTY_GET(_InputRevision);
};

