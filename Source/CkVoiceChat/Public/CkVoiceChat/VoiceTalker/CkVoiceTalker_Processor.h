#pragma once

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Processor/CkProcessor_NetModePolicy.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

#include "CkVoiceChat/VoiceChannel/CkVoiceChannel_Processor.h"
#include "CkVoiceChat/VoiceTalker/CkVoiceTalker_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKVOICECHAT_API FProcessor_VoiceTalker_Setup : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_Setup,
        FCk_Handle_VoiceTalker,
        ck::TReadOnly<FFragment_VoiceTalker_Params>,
        FTag_VoiceTalker_NeedsSetup,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using MarkedDirtyBy = FTag_VoiceTalker_NeedsSetup;

    public:
        using TProcessor::TProcessor;

    public:
        // Tunables are seeded at Add, not here - Setup exists to clear the gate tag that holds
        // the request/capture processors off until composition has settled.
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            const FFragment_VoiceTalker_Params& InParams)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKVOICECHAT_API FProcessor_VoiceTalker_HandleRequests : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_HandleRequests,
        FCk_Handle_VoiceTalker,
        ck::TReadOnly<FFragment_VoiceTalker_Params>,
        ck::TReadWrite<FFragment_VoiceTalker_Tunables>,
        ck::TReadWrite<FFragment_VoiceTalker_Capture>,
        ck::TReadWrite<FFragment_VoiceTalker_Playout>,
        ck::TReadWrite<FFragment_VoiceTalker_Requests>,
        TExclude<FTag_VoiceTalker_NeedsSetup>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using RunAfter = TDepList<FProcessor_VoiceTalker_Setup>;
        using MarkedDirtyBy = FFragment_VoiceTalker_Requests;

    public:
        using TProcessor::TProcessor;

    public:
        auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            const FFragment_VoiceTalker_Params& InParams,
            FFragment_VoiceTalker_Tunables& InTunables,
            FFragment_VoiceTalker_Capture& InCapture,
            FFragment_VoiceTalker_Playout& InPlayout,
            FFragment_VoiceTalker_Requests& InRequests) const -> void;

    public:
        // The one way a transmit closes: StopTransmit, a mode that leaves OpenMic or enters Disabled, and an open mic
        // whose talker is no longer controlled here. Stops capture, resets the per-transmit state and fires the stop
        // edges. The talker must be transmitting.
        static auto
        Stop_Transmit(
            HandleType InHandle,
            FFragment_VoiceTalker_Capture& InCapture)
            -> void;

    private:
        // The overloads share one leading parameter list because the request visitor dispatches
        // them through a single call expression - a handler that ignores a fragment still takes it.
        static auto
        DoHandleRequest(
            HandleType InHandle,
            const FFragment_VoiceTalker_Params& InParams,
            FFragment_VoiceTalker_Tunables& InTunables,
            FFragment_VoiceTalker_Capture& InCapture,
            FFragment_VoiceTalker_Playout& InPlayout,
            const FCk_Request_VoiceTalker_StartTransmit& InRequest) -> bool;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            const FFragment_VoiceTalker_Params& InParams,
            FFragment_VoiceTalker_Tunables& InTunables,
            FFragment_VoiceTalker_Capture& InCapture,
            FFragment_VoiceTalker_Playout& InPlayout,
            const FCk_Request_VoiceTalker_StopTransmit& InRequest) -> bool;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            const FFragment_VoiceTalker_Params& InParams,
            FFragment_VoiceTalker_Tunables& InTunables,
            FFragment_VoiceTalker_Capture& InCapture,
            FFragment_VoiceTalker_Playout& InPlayout,
            const FCk_Request_VoiceTalker_SetTransmitMode& InRequest) -> bool;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            const FFragment_VoiceTalker_Params& InParams,
            FFragment_VoiceTalker_Tunables& InTunables,
            FFragment_VoiceTalker_Capture& InCapture,
            FFragment_VoiceTalker_Playout& InPlayout,
            const FCk_Request_VoiceTalker_SetInputGain& InRequest) -> bool;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            const FFragment_VoiceTalker_Params& InParams,
            FFragment_VoiceTalker_Tunables& InTunables,
            FFragment_VoiceTalker_Capture& InCapture,
            FFragment_VoiceTalker_Playout& InPlayout,
            const FCk_Request_VoiceTalker_SetSelfMute& InRequest) -> bool;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // The open mic, level-triggered on the arming: a pending talker opens its transmit through the ordinary
    // StartTransmit request, once, the first time this machine's player controls it (a possessed pawn bridged to the
    // talker's entity), and stays pending until then, so late possession converges. A talker with no bridged actor can
    // never be controlled and is disarmed. Runs before the request drain so the Start lands in the frame it is issued.
    class CKVOICECHAT_API FProcessor_VoiceTalker_OpenMic : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_OpenMic,
        FCk_Handle_VoiceTalker,
        ck::TReadOnly<FFragment_VoiceTalker_Tunables>,
        FTag_VoiceTalker_OpenMicPending,
        TExclude<FTag_VoiceTalker_NeedsSetup>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using RunAfter = TDepList<FProcessor_VoiceTalker_Setup>;
        using RunBefore = TDepList<FProcessor_VoiceTalker_HandleRequests>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            const FFragment_VoiceTalker_Tunables& InTunables)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Authority-side auto-join, level-triggered: whenever the world's channel registry has gained an entry since
    // this talker last compared against it (or the talker is new), the talker is joined to every registry channel
    // that opts in (_AutoJoinTalkers) and whose name its AutoJoinChannels lists exactly - so it converges whichever
    // of the two was composed first. Each (talker, channel) pair is joined at most once and always through the
    // ordinary Join request (control-plane push, proximity reconcile), so an explicit Leave afterwards stands.
    // Runs before the channel request drain so the join lands in the frame it is issued, while the talker is
    // still alive.
    class CKVOICECHAT_API FProcessor_VoiceTalker_AutoJoin : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_AutoJoin,
        FCk_Handle_VoiceTalker,
        ck::TReadOnly<FFragment_VoiceTalker_Params>,
        ck::TReadWrite<FFragment_VoiceTalker_AutoJoin>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using RunAfter = TDepList<FProcessor_VoiceChannel_AssignIdx>;
        using RunBefore = TDepList<FProcessor_VoiceChannel_HandleRequests>;
        static constexpr auto NetModeRequirement = ECk_ProcessorNetModeRequirement::AuthorityOnly;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            const FFragment_VoiceTalker_Params& InParams,
            FFragment_VoiceTalker_AutoJoin& InAutoJoin)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // HandleRequests excludes owners already tagged for destruction, so a destroyed talker's
    // still-queued requests are never drained; this fires each pending request's completion
    // delegate with Failed_Cancelled so a caller awaiting completion terminates instead of hanging.
    class CKVOICECHAT_API FProcessor_VoiceTalker_CancelPendingRequests : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_CancelPendingRequests,
        FCk_Handle_VoiceTalker,
        ck::TReadOnly<FFragment_VoiceTalker_Requests>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            const FFragment_VoiceTalker_Requests& InRequests)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Drives the capture pipeline while transmitting: drain -> gain -> per-frame RMS/VAD -> Opus
    // encode -> loopback playout. Time-stepping, so deliberately no MarkedDirtyBy (not
    // pump-eligible).
    class CKVOICECHAT_API FProcessor_VoiceTalker_Capture : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_Capture,
        FCk_Handle_VoiceTalker,
        ck::TReadOnly<FFragment_VoiceTalker_Params>,
        ck::TReadOnly<FFragment_VoiceTalker_Tunables>,
        ck::TReadWrite<FFragment_VoiceTalker>,
        ck::TReadWrite<FFragment_VoiceTalker_Capture>,
        ck::TReadWrite<FFragment_VoiceTalker_Playout>,
        FTag_VoiceTalker_IsTransmitting,
        TExclude<FTag_VoiceTalker_NeedsSetup>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using RunAfter = TDepList<FProcessor_VoiceTalker_HandleRequests>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            const FFragment_VoiceTalker_Params& InParams,
            const FFragment_VoiceTalker_Tunables& InTunables,
            FFragment_VoiceTalker& InVoiceTalkerComp,
            FFragment_VoiceTalker_Capture& InCapture,
            FFragment_VoiceTalker_Playout& InPlayout)
            -> void;

    private:
        // The once-per-transmit line naming the stage where a transmit's frames stopped: no audio from the source,
        // under the voice gate, nothing out of the encoder, or encoded but never sent (and why).
        static auto
        Report_NoProgress(
            HandleType InVoiceTalkerEntity,
            const FFragment_VoiceTalker_Params& InParams,
            const FFragment_VoiceTalker_Tunables& InTunables,
            const FFragment_VoiceTalker_Capture& InCapture)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Plays out a REMOTE talker's forwarded voice on this machine: drains the receive inbox into
    // the talker's jitter buffer, then runs the shared playout drain (pop -> decode -> PCM ->
    // synth). Runs on every net mode - a listen host receives other players' voice like any
    // client. Time-stepping (conceal and tail drain continue without new bundles), so
    // deliberately no MarkedDirtyBy.
    class CKVOICECHAT_API FProcessor_VoiceTalker_ReceivePlayback : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_ReceivePlayback,
        FCk_Handle_VoiceTalker,
        ck::TReadWrite<FFragment_VoiceTalker>,
        ck::TReadWrite<FFragment_VoiceTalker_Playout>,
        ck::TReadWrite<FFragment_VoiceTalker_ReceiveInbox>,
        TExclude<FTag_VoiceTalker_NeedsSetup>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using RunAfter = TDepList<FProcessor_VoiceTalker_Capture>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            FFragment_VoiceTalker& InVoiceTalkerComp,
            FFragment_VoiceTalker_Playout& InPlayout,
            FFragment_VoiceTalker_ReceiveInbox& InInbox)
            -> void;

    public:
        // The one playout drain both consumers share (Capture's loopback and this processor's
        // receive path): advances the pop accumulator, pops/conceals/decodes into the decoded-PCM
        // buffer + synth, and trims the buffer. No-op (accumulator cleared) without a decoder.
        static auto
        Drain_Playout(
            FFragment_VoiceTalker_Playout& InPlayout,
            FCk_Time InDeltaT)
            -> void;

        // Creates + starts the playback synth if the world can render audio; no-op otherwise
        // (headless PIE) - the decode path still fills the test-readable PCM buffer. Attaches to
        // the Spec's PlaybackAttachComponent (else the owning actor's root) at its
        // PlaybackAttachSocketName, applies the config-channel render settings before the first
        // Start, then the local listener's volume for this talker.
        static auto
        TryCreate_PlaybackSynth(
            HandleType InVoiceTalkerEntity,
            FFragment_VoiceTalker_Playout& InPlayout)
            -> void;

        // (Re)applies _PlaybackConfigChannel's spatialization/attenuation/effect chain/SoundClass
        // to the synth via Stop -> set -> Start (USynthComponent::Start copies the fields to its
        // audio component; the PCM queue survives the cycle by design). Invalid config channel =
        // flat non-spatialized playback with the engine's default SoundClass (capture loopback,
        // unresolved control plane). Spatialization additionally requires the synth to be
        // attached - an unattached synth has no world position and MUST render flat regardless
        // of policy.
        static auto
        Apply_SynthChannelConfig(
            HandleType InVoiceTalkerEntity,
            FFragment_VoiceTalker_Playout& InPlayout)
            -> void;

        // Gives the synth this machine's listener volume for the talker (1.0 with no local
        // listener or no entry) when it differs from the volume last given. Local only: the
        // volume never travels upstream. No-op without a synth.
        static auto
        Apply_LocalListenerVolume(
            HandleType InVoiceTalkerEntity,
            FFragment_VoiceTalker_Playout& InPlayout)
            -> void;

        // HybridRadio's per-recipient render decision (ADR-5: "prefer 3D if near", ONE wire
        // copy): listener-to-talker distance against the channel's AudibleRange with the module
        // hysteresis asymmetry mirrored from Route - become near INSIDE the range, stay near
        // until range + margin. Re-applies the synth config on a mode flip. Bundle-arrival
        // cadence: runs per drain, so a silent talker never flips modes.
        static auto
        Evaluate_HybridRenderMode(
            HandleType InVoiceTalkerEntity,
            FFragment_VoiceTalker_Playout& InPlayout)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // The prompt half of membership integrity (FProcessor_VoiceChannel_SweepMembers is the eventual half): when a
    // talker begins teardown on the authority it leaves every channel it is a member of, through the ordinary Leave
    // request, so the control plane and the proximity bookkeeping update exactly as they do for an explicit Leave.
    // A channel that is itself being destroyed is skipped - its membership goes with it.
    class CKVOICECHAT_API FProcessor_VoiceTalker_LeaveChannels : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_LeaveChannels,
        FCk_Handle_VoiceTalker,
        ck::TReadOnly<FFragment_VoiceTalker_Params>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        static constexpr auto NetModeRequirement = ECk_ProcessorNetModeRequirement::AuthorityOnly;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            const FFragment_VoiceTalker_Params& InParams)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Teardown order per spec §7.8: stop capture BEFORE releasing the encoder/decoder.
    class CKVOICECHAT_API FProcessor_VoiceTalker_EndPlay : public ck_exp::TProcessor<
        FProcessor_VoiceTalker_EndPlay,
        FCk_Handle_VoiceTalker,
        ck::TReadWrite<FFragment_VoiceTalker_Capture>,
        ck::TReadWrite<FFragment_VoiceTalker_Playout>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceTalkerEntity,
            FFragment_VoiceTalker_Capture& InCapture,
            FFragment_VoiceTalker_Playout& InPlayout)
            -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
