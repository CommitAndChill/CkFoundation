#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control_Data.h"

class FCk_ProceduralArticulation_Control;

class UCk_Utils_ProceduralAnimation_UE;
class UCk_Utils_ProceduralBodyPose_UE;

namespace ck
{
    class FProcessor_ProceduralArticulation_Update;
    class FProcessor_ProceduralArticulation_Observe;
    class FProcessor_ProceduralRig_Update;
    class FProcessor_ProceduralBodyPose_Update;

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralArticulation_Claims
    {
        CK_GENERATED_BODY(FFragment_ProceduralArticulation_Claims);
        friend class ::FCk_ProceduralArticulation_Control;
        friend class FProcessor_ProceduralArticulation_Update;
        friend class FProcessor_ProceduralArticulation_Observe;

    private:
        TArray<FCk_ArticulationControlToken> _LimbOwners;
        // Tokens whose claimed limb died since their claims were last released; read by the controller's
        // interruption query and cleared by ReleaseClaims.
        TArray<FCk_ArticulationControlToken> _LostLimbOwners;
        FCk_ArticulationControlToken _BodyOwner;
        uint64 _Revision = 0;
        uint64 _NextBatchNonce = 1;
        uint64 _BatchNonce = 0;
        TArray<FCk_ArticulationStageKey> _Tickets;
        TArray<FCk_ArticulationChannelSet> _TicketChannels;
        TArray<FCk_ProceduralArticulation_Pose> _TokenPoses;
        TArray<uint64> _TicketRevisions;
        bool _BatchStaged = false;
        bool _BatchPublicationPending = false;

    public:
        CK_PROPERTY_GET(_Revision);
        CK_PROPERTY_GET(_LostLimbOwners);
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralArticulation_Claims, _LimbOwners);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralArticulation_Params
    {
        CK_GENERATED_BODY(FFragment_ProceduralArticulation_Params);

    private:
        FCk_ProceduralArticulation_Layout _Layout;
        FCk_ProceduralArticulation_ClearanceSettings _ClearanceSettings;

    public:
        CK_PROPERTY_GET(_Layout);
        CK_PROPERTY_GET(_ClearanceSettings);
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralArticulation_Params, _Layout, _ClearanceSettings);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralArticulation_Bindings
    {
        CK_GENERATED_BODY(FFragment_ProceduralArticulation_Bindings);
        friend class ::UCk_Utils_ProceduralBodyPose_UE;

    private:
        TArray<FCk_ProceduralLimb_Binding> _Bindings;
        TArray<FCk_Handle_ProceduralLimb> _Limbs;
        FCk_Handle_Transform _Presentation;

    public:
        CK_PROPERTY_GET(_Bindings);
        CK_PROPERTY_GET(_Limbs);
        CK_PROPERTY_GET(_Presentation);
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralArticulation_Bindings, _Bindings, _Limbs, _Presentation);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralArticulation_Pose
    {
        CK_GENERATED_BODY(FFragment_ProceduralArticulation_Pose);
        friend class FProcessor_ProceduralArticulation_Update;
        friend class FProcessor_ProceduralArticulation_Observe;
        friend class ::FCk_ProceduralArticulation_Control;
        friend class FProcessor_ProceduralRig_Update;
        friend class FProcessor_ProceduralBodyPose_Update;
        friend class ::UCk_Utils_ProceduralAnimation_UE;

    private:
        TArray<FTransform> _ExpectedParts;
        TArray<FCk_Handle_Transform> _ExpectedHandles;
        TArray<FCk_Handle_Transform> _ScratchHandles;
        TArray<FTransform> _ScratchPoses;
        uint64 _IssuedRevision = 0;
        uint64 _SettledRevision = 0;
        bool _PendingObservation = false;
        bool _LegacyWalker = false;
        bool _HasLegacyCandidate = false;
        TArray<FCk_Handle_Transform> _LegacyHandles;
        TArray<FTransform> _LegacyPoses;
        FTransform _LegacyPresentation = FTransform::Identity;
        bool _HasLegacyPresentation = false;
        bool _HasControlCandidate = false;
        TArray<FCk_Handle_Transform> _ControlHandles;
        TArray<FTransform> _ControlPoses;
        FCk_ProceduralArticulation_Pose _PredictedSubjectPose;
        FCk_ProceduralArticulation_Pose _SettledSubjectPose;

    public:
        CK_PROPERTY_GET(_IssuedRevision);
        CK_PROPERTY_GET(_SettledRevision);
        CK_PROPERTY_GET(_SettledSubjectPose);
        CK_PROPERTY_GET(_PendingObservation);
        CK_PROPERTY_GET(_LegacyWalker);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralArticulation_Health
    {
        CK_GENERATED_BODY(FFragment_ProceduralArticulation_Health);
        friend class FProcessor_ProceduralArticulation_Update;
        friend class FProcessor_ProceduralArticulation_Observe;
        friend class ::FCk_ProceduralArticulation_Control;

    private:
        ECk_ProceduralArticulation_Health _State = ECk_ProceduralArticulation_Health::Operational;
        ECk_ProceduralArticulation_Fault _Fault = ECk_ProceduralArticulation_Fault::None;
        FName _OffendingLimb;
        int32 _OffendingPart = INDEX_NONE;
        FTransform _ObservedPart = FTransform::Identity;
        TArray<FCk_Handle_Transform> _FaultObservedHandles;
        TArray<FTransform> _FaultObservedPoses;

    public:
        auto
            Clear()
            -> void
        {
            _State = ECk_ProceduralArticulation_Health::Operational;
            _Fault = ECk_ProceduralArticulation_Fault::None;
            _OffendingLimb = NAME_None;
            _OffendingPart = INDEX_NONE;
            _ObservedPart = FTransform::Identity;
            _FaultObservedHandles.Reset();
            _FaultObservedPoses.Reset();
        }

    public:
        CK_PROPERTY_GET(_State);
        CK_PROPERTY_GET(_Fault);
        CK_PROPERTY_GET(_OffendingLimb);
        CK_PROPERTY_GET(_OffendingPart);
        CK_PROPERTY_GET(_ObservedPart);
        CK_PROPERTY_GET(_FaultObservedHandles);
        CK_PROPERTY_GET(_FaultObservedPoses);
    };
}
