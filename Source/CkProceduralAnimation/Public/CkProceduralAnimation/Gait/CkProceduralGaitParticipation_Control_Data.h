#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control_Data.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

enum class ECk_ProceduralGaitParticipation_Result : uint8
{
    Accepted,
    Busy,
    InvalidGait,
    InvalidToken,
    InvalidLimbSet,
    SupportUnavailable,
    NotReady,
    Stale,
    Faulted
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralGaitParticipation_Result);

enum class ECk_ProceduralGaitParticipation_State : uint8
{
    None,
    Protected,
    Interrupted,
    Recovering,
    ReadyForHandoff,
    Unplanted
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralGaitParticipation_State);

// --------------------------------------------------------------------------------------------------------------------

struct CKPROCEDURALANIMATION_API FCk_ProceduralGaitParticipation_Snapshot
{
    CK_GENERATED_BODY(FCk_ProceduralGaitParticipation_Snapshot);

private:
    FTransform _RootFrame = FTransform::Identity;
    uint64 _SolveSequence = 0;
    FVector _SupportUp = FVector::UpVector;

public:
    CK_PROPERTY_GET(_RootFrame);
    CK_PROPERTY_GET(_SolveSequence);
    CK_PROPERTY_GET(_SupportUp);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralGaitParticipation_Snapshot, _RootFrame, _SolveSequence, _SupportUp);
};

struct CKPROCEDURALANIMATION_API FCk_ProceduralGaitParticipation_Observation
{
    CK_GENERATED_BODY(FCk_ProceduralGaitParticipation_Observation);

private:
    ECk_ProceduralGaitParticipation_State _State = ECk_ProceduralGaitParticipation_State::None;
    bool _ProtectionActive = false;
    bool _TrustedSupport = false;
    bool _Interrupted = false;
    FName _Reason = NAME_None;
    uint64 _RecoveryGeneration = 0;
    uint64 _SolveSequence = 0;
    FCk_Time _AcceptedElapsed;

public:
    CK_PROPERTY_GET(_State);
    CK_PROPERTY_GET(_ProtectionActive);
    CK_PROPERTY_GET(_TrustedSupport);
    CK_PROPERTY_GET(_Interrupted);
    CK_PROPERTY_GET(_Reason);
    CK_PROPERTY_GET(_RecoveryGeneration);
    CK_PROPERTY_GET(_SolveSequence);
    CK_PROPERTY_GET(_AcceptedElapsed);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralGaitParticipation_Observation, _State, _ProtectionActive,
        _TrustedSupport, _Interrupted, _Reason, _RecoveryGeneration, _SolveSequence, _AcceptedElapsed);
};

struct CKPROCEDURALANIMATION_API FCk_ProceduralGaitParticipation_LimbObservation
{
    CK_GENERATED_BODY(FCk_ProceduralGaitParticipation_LimbObservation);

private:
    bool _Exists = false;
    bool _StructurallyEnabled = false;
    bool _Participating = false;
    bool _TrustedSupport = false;
    bool _Unplanted = false;
    bool _Protected = false;
    FName _Reason = NAME_None;

public:
    CK_PROPERTY_GET(_Exists);
    CK_PROPERTY_GET(_StructurallyEnabled);
    CK_PROPERTY_GET(_Participating);
    CK_PROPERTY_GET(_TrustedSupport);
    CK_PROPERTY_GET(_Unplanted);
    CK_PROPERTY_GET(_Protected);
    CK_PROPERTY_GET(_Reason);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralGaitParticipation_LimbObservation, _Exists, _StructurallyEnabled,
        _Participating, _TrustedSupport, _Unplanted, _Protected, _Reason);
};

struct CKPROCEDURALANIMATION_API FCk_ProceduralGaitParticipation_RecoveryCandidate
{
    CK_GENERATED_BODY(FCk_ProceduralGaitParticipation_RecoveryCandidate);

private:
    FCk_ArticulationControlToken _Token;
    uint64 _Generation = 0;
    uint64 _SolveSequence = 0;
    FCk_Time _ProposedElapsed;
    TArray<FCk_ArticulationLimbTarget> _Targets;

public:
    CK_PROPERTY_GET(_Token);
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_SolveSequence);
    CK_PROPERTY_GET(_ProposedElapsed);
    CK_PROPERTY_GET(_Targets);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralGaitParticipation_RecoveryCandidate, _Token, _Generation,
        _SolveSequence, _ProposedElapsed, _Targets);
};

struct CKPROCEDURALANIMATION_API FCk_ProceduralGaitParticipation_HandoffKey
{
    CK_GENERATED_BODY(FCk_ProceduralGaitParticipation_HandoffKey);

private:
    FCk_ArticulationControlToken _Token;
    uint64 _Generation = 0;
    uint64 _SolveSequence = 0;
    FTransform _RootFrame = FTransform::Identity;

public:
    CK_PROPERTY_GET(_Token);
    CK_PROPERTY_GET(_Generation);
    CK_PROPERTY_GET(_SolveSequence);
    CK_PROPERTY_GET(_RootFrame);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralGaitParticipation_HandoffKey, _Token, _Generation,
        _SolveSequence, _RootFrame);
};

// --------------------------------------------------------------------------------------------------------------------

// Gait owns the simulation roster, foothold provenance, and every locomotor protection lease. This facade only
// prepares endpoints for the Articulation publisher; it never writes a Transform or releases Articulation claims.
