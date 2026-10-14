#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Receipt.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment_Data.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control_Data.h"
#include "CkProceduralAnimation/Core/CkProceduralActionEvaluation.h"
#include "CkResourceLoader/CkResourceLoader_Fragment_Data.h"
#include "CkEcs/Signal/CkSignal_Fragment.h"
#include "CkEcs/Signal/CkSignal_Macros.h"
#include "CkEcs/Signal/CkSignal_Utils.h"

#include <variant>

class UCk_Utils_ProceduralAction_UE;
class UCk_Utils_ProceduralAnimation_Debug_UE;

namespace ck
{
    class FProcessor_ProceduralAction_HandleRequests;
    class FProcessor_ProceduralAction_Service;
    class FProcessor_ProceduralAction_EndPlay;
    class FProcessor_ProceduralActionPlayer_Service;
    class FProcessor_ProceduralActionPlayer_Observe;
    class FProcessor_ProceduralActionPlayer_ReconcileOwnerLoss;

    enum class EProceduralActionSource : uint8
    {
        Clip,
        Manual
    };

    // Selected at creation and immutable: who acknowledges the action's receipt stream.
    enum class EProceduralActionDelivery : uint8
    {
        Explicit,
        Managed
    };

    struct CKPROCEDURALANIMATION_API FProceduralActionClipParams
    {
        CK_GENERATED_BODY(FProceduralActionClipParams);

    private:
        TSoftObjectPtr<UCk_ProceduralAction_Data> _Clip;
        TArray<FCk_ProceduralAction_SlotBinding> _SlotBindings;
        TArray<FCk_ProceduralAction_AnchorBinding> _AnchorBindings;
        TArray<FCk_ProceduralAction_MirrorSpec> _Mirrors;

    public:
        CK_PROPERTY_GET(_Clip);
        CK_PROPERTY_GET(_SlotBindings);
        CK_PROPERTY_GET(_AnchorBindings);
        CK_PROPERTY(_Mirrors);
        CK_DEFINE_CONSTRUCTORS(FProceduralActionClipParams, _Clip, _SlotBindings, _AnchorBindings);
    };

    struct CKPROCEDURALANIMATION_API FProceduralActionManualParams
    {
        CK_GENERATED_BODY(FProceduralActionManualParams);

    private:
        TArray<FName> _LimbIds;
        TArray<FCk_ProceduralAction_AnchorBinding> _AnchorBindings;
        ECk_EnableDisable _BodyChannel = ECk_EnableDisable::Disable;
        FCk_Time _InputTimeout;

    public:
        CK_PROPERTY_GET(_LimbIds);
        CK_PROPERTY_GET(_AnchorBindings);
        CK_PROPERTY_GET(_BodyChannel);
        CK_PROPERTY_GET(_InputTimeout);
        CK_DEFINE_CONSTRUCTORS(FProceduralActionManualParams, _LimbIds, _AnchorBindings,
            _BodyChannel, _InputTimeout);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralAction_Params
    {
        CK_GENERATED_BODY(FFragment_ProceduralAction_Params);

    private:
        FCk_Handle_ProceduralActionPlayer _Player;
        FCk_ProceduralAction_Identity _Identity;
        EProceduralActionSource _Source = EProceduralActionSource::Manual;
        FProceduralActionClipParams _Clip;
        FProceduralActionManualParams _Manual;
        FCk_ProceduralAction_Tuning _Tuning;
        int64 _TuningRevision = 1;

    public:
        CK_PROPERTY_GET(_Player);
        CK_PROPERTY_GET(_Identity);
        CK_PROPERTY_GET(_Source);
        CK_PROPERTY_GET(_Clip);
        CK_PROPERTY_GET(_Manual);
        CK_PROPERTY_GET(_Tuning);
        CK_PROPERTY_GET(_TuningRevision);

        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralAction_Params,
            _Player, _Identity, _Source, _Clip, _Manual, _Tuning, _TuningRevision);
    };

    // Exists only until the rooted load, validation and binding transaction completes.
    struct CKPROCEDURALANIMATION_API FFragment_ProceduralAction_Loading
    {
        CK_GENERATED_BODY(FFragment_ProceduralAction_Loading);
        friend class FProcessor_ProceduralActionPlayer_Service;
        friend class ::UCk_Utils_ProceduralAnimation_Debug_UE;

    private:
        FCk_ResourceLoader_RootedAssetBatch _Assets;
        FCk_Time _Elapsed;
        int64 _Generation = 1;

    public:
        CK_PROPERTY_GET(_Assets);
        CK_PROPERTY_GET(_Generation);
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralAction_Loading, _Generation);
    };

    // Added once after successful compilation; the asset root is then released.
    struct CKPROCEDURALANIMATION_API FFragment_ProceduralAction_Compiled
    {
        CK_GENERATED_BODY(FFragment_ProceduralAction_Compiled);

    private:
        TSharedPtr<const procedural_action::FCompiledClip> _Clip;
        TArray<FName> _BoundLimbIds;

    public:
        CK_PROPERTY_GET(_Clip);
        CK_PROPERTY_GET(_BoundLimbIds);
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralAction_Compiled, _Clip, _BoundLimbIds);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralAction
    {
        CK_GENERATED_BODY(FFragment_ProceduralAction);
        friend class ::UCk_Utils_ProceduralAnimation_Debug_UE;
        friend class FProcessor_ProceduralAction_HandleRequests;
        friend class FProcessor_ProceduralAction_Service;
        friend class FProcessor_ProceduralAction_EndPlay;
        friend class FProcessor_ProceduralActionPlayer_Service;
        friend class FProcessor_ProceduralActionPlayer_Observe;
        friend class FProcessor_ProceduralActionPlayer_ReconcileOwnerLoss;
        friend class ::UCk_Utils_ProceduralAction_UE;

    private:
        ECk_ProceduralAction_State _State = ECk_ProceduralAction_State::PendingAdmission;
        FCk_ProceduralAction_Result _Result;
        FCk_ProceduralAction_FailureContext _Context;
        FCk_ProceduralArticulation_Pose _EntryPose;
        FCk_ProceduralArticulation_Pose _MostRecentObservedPose;
        procedural_action::FInitialPose _InitialPose;
        bool _HasHoldSinceLastReceipt = false;
        int64 _Generation = 1;
        bool _ReleaseWhenFinished = false;
        bool _HasClaim = false;
        bool _OwnerLossReconciled = false;
        bool _HasPendingBatch = false;
        FCk_ProceduralAction_PoseBatch _PendingBatch;
        bool _HasAcceptedBatch = false;
        FCk_ProceduralAction_PoseBatch _AcceptedBatch;
        TOptional<FCk_Request_ProceduralAction_SubmitPoseBatch> _PendingSubmit;
        TOptional<FCk_ArticulationStageKey> _PendingStageKey;
        TOptional<FCk_ArticulationStageKey> _PendingHoldStageKey;
        FCk_Time _AdmissionElapsed;
        FCk_Time _PublicationElapsed;
        FCk_Time _InputElapsed;
        FCk_Time _ServiceElapsed;
        FCk_Time _AcceptedTime;
        FCk_Time _PendingToTime;
        FCk_Time _EntryBlendAccepted;
        FCk_Time _PendingBlendTo;
        FCk_Time _PlaybackBacklog;
        FCk_Time _PendingBacklog;
        bool _PendingIsEntryBlend = false;
        bool _HasAuthoredReceipt = false;
        FCk_ProceduralAction_Interval _PendingClipInterval;
        TArray<FCk_ProceduralAction_Sample> _PendingIntermediateSamples;
        FCk_ProceduralAction_SamplingReport _PendingClipSampling;
        FCk_ProceduralArticulation_Pose _PendingPredictedPose;
        int64 _NextSampleSequence = 1;
        int64 _PendingCorrelationToken = 0;

        auto
            CommitState(
                const FCk_Handle_ProceduralAction& InHandle,
                const FFragment_ProceduralAction_Params& InParams,
                ECk_ProceduralAction_State InState)
            -> void;

        // Replaces the context with where the action stands now; the caller adds what its failing check measured.
        auto
            BeginFailureContext(
                const FCk_Handle_ProceduralAction& InHandle,
                const FFragment_ProceduralAction_Params& InParams)
            -> FCk_ProceduralAction_FailureContext&;

    public:
        CK_PROPERTY_GET(_State);
        CK_PROPERTY_GET(_Result);
        CK_PROPERTY_GET(_Context);
        CK_PROPERTY_GET(_EntryPose);
        CK_PROPERTY_GET(_Generation);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralAction_Requests
    {
        CK_GENERATED_BODY(FFragment_ProceduralAction_Requests);
        friend class ::UCk_Utils_ProceduralAction_UE;
        friend class FProcessor_ProceduralAction_HandleRequests;
        friend class FProcessor_ProceduralAction_EndPlay;

    public:
        using RequestType = std::variant<FCk_Request_ProceduralAction_SubmitPoseBatch,
            FCk_Request_ProceduralAction_Cancel, FCk_Request_ProceduralAction_End,
            FCk_Request_ProceduralAction_Release>;

    private:
        TArray<RequestType> _Requests;

    public:
        CK_PROPERTY_GET(_Requests);
    };

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKPROCEDURALANIMATION_API,
        OnProceduralAction_StateChanged, FCk_Delegate_ProceduralAction_OnStateChanged,
        FCk_Handle_ProceduralAction, ECk_ProceduralAction_State);
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKPROCEDURALANIMATION_API,
        OnProceduralAction_Finished, FCk_Delegate_ProceduralAction_OnFinished,
        FCk_Handle_ProceduralAction, FCk_ProceduralAction_Terminal);
}
