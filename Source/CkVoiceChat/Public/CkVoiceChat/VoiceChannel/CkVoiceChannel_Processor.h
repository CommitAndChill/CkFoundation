#pragma once

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Processor/CkProcessor_NetModePolicy.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

#include "CkVoiceChat/VoiceChannel/CkVoiceChannel_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKVOICECHAT_API FProcessor_VoiceChannel_Setup : public ck_exp::TProcessor<
        FProcessor_VoiceChannel_Setup,
        FCk_Handle_VoiceChannel,
        ck::TReadOnly<FFragment_VoiceChannel_Params>,
        ck::TReadWrite<FFragment_VoiceChannel>,
        FTag_VoiceChannel_NeedsSetup,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using MarkedDirtyBy = FTag_VoiceChannel_NeedsSetup;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceChannelEntity,
            const FFragment_VoiceChannel_Params& InParams,
            FFragment_VoiceChannel& InVoiceChannel)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Allocates the channel's wire index from the world registry - authority only. Indices are
    // session-append-only (never reused) so a stale wire ChannelIdx resolves to nothing rather
    // than to a different channel. Clients receive their index via the control-plane replication;
    // the NeedsIdx tag is inert on them (this processor is net-gated off).
    class CKVOICECHAT_API FProcessor_VoiceChannel_AssignIdx : public ck_exp::TProcessor<
        FProcessor_VoiceChannel_AssignIdx,
        FCk_Handle_VoiceChannel,
        ck::TReadOnly<FFragment_VoiceChannel_Params>,
        ck::TReadWrite<FFragment_VoiceChannel>,
        FTag_VoiceChannel_NeedsIdx,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using RunAfter = TDepList<FProcessor_VoiceChannel_Setup>;
        using MarkedDirtyBy = FTag_VoiceChannel_NeedsIdx;
        static constexpr auto NetModeRequirement = ECk_ProcessorNetModeRequirement::AuthorityOnly;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceChannelEntity,
            const FFragment_VoiceChannel_Params& InParams,
            FFragment_VoiceChannel& InVoiceChannel)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKVOICECHAT_API FProcessor_VoiceChannel_HandleRequests : public ck_exp::TProcessor<
        FProcessor_VoiceChannel_HandleRequests,
        FCk_Handle_VoiceChannel,
        ck::TReadWrite<FFragment_VoiceChannel>,
        ck::TReadWrite<FFragment_VoiceChannel_Requests>,
        TExclude<FTag_VoiceChannel_NeedsSetup>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using RunAfter = TDepList<FProcessor_VoiceChannel_Setup>;
        using MarkedDirtyBy = FFragment_VoiceChannel_Requests;

    public:
        using TProcessor::TProcessor;

    public:
        auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceChannelEntity,
            FFragment_VoiceChannel& InVoiceChannel,
            FFragment_VoiceChannel_Requests& InRequests) const -> void;

    private:
        static auto
        DoHandleRequest(
            HandleType InHandle,
            FFragment_VoiceChannel& InVoiceChannel,
            const FCk_Request_VoiceChannel_Join& InRequest) -> bool;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            FFragment_VoiceChannel& InVoiceChannel,
            const FCk_Request_VoiceChannel_Leave& InRequest) -> bool;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            FFragment_VoiceChannel& InVoiceChannel,
            const FCk_Request_VoiceChannel_SetMemberFlags& InRequest) -> bool;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            FFragment_VoiceChannel& InVoiceChannel,
            const FCk_Request_VoiceChannel_ServerMute& InRequest) -> bool;

        static auto
        DoHandleRequest(
            HandleType InHandle,
            FFragment_VoiceChannel& InVoiceChannel,
            const FCk_Request_VoiceChannel_ServerUnmute& InRequest) -> bool;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // The eventual half of membership integrity, owned by the channel so no consumer has to guard against it: every
    // frame on the authority, member keys (and server-mute keys) whose entity no longer exists at all are removed
    // and the control plane re-pushed. The prompt half is the talker's own teardown
    // (FProcessor_VoiceTalker_LeaveChannels), which leaves through the ordinary Leave request; a key reaching this
    // sweep is one whose talker exit never ran (an entity destroyed without its EndPlay, a restore), so each sweep
    // logs a Warning naming the channel. No MemberLeft fires for a swept key - its talker no longer exists to name,
    // matching the client-side control-plane diff, which skips the signal for a talker it cannot resolve.
    class CKVOICECHAT_API FProcessor_VoiceChannel_SweepMembers : public ck_exp::TProcessor<
        FProcessor_VoiceChannel_SweepMembers,
        FCk_Handle_VoiceChannel,
        ck::TReadWrite<FFragment_VoiceChannel>,
        TExclude<FTag_VoiceChannel_NeedsSetup>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Audio;
        using RunAfter = TDepList<FProcessor_VoiceChannel_HandleRequests>;
        static constexpr auto NetModeRequirement = ECk_ProcessorNetModeRequirement::AuthorityOnly;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InVoiceChannelEntity,
            FFragment_VoiceChannel& InVoiceChannel)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // A dying channel releases its proximity claims: every member talker gets re-reconciled so
    // its range probe shrinks (or disappears) now that this channel's audible range no longer
    // contributes. Authority-only - probes exist only where routing runs.
    class CKVOICECHAT_API FProcessor_VoiceChannel_EndPlay : public ck_exp::TProcessor<
        FProcessor_VoiceChannel_EndPlay,
        FCk_Handle_VoiceChannel,
        ck::TReadOnly<FFragment_VoiceChannel>,
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
            HandleType InVoiceChannelEntity,
            const FFragment_VoiceChannel& InVoiceChannel)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // HandleRequests excludes owners already tagged for destruction, so a destroyed channel's
    // still-queued requests are never drained; this fires each pending request's completion
    // delegate with Failed_Cancelled so a caller awaiting completion terminates instead of hanging.
    class CKVOICECHAT_API FProcessor_VoiceChannel_CancelPendingRequests : public ck_exp::TProcessor<
        FProcessor_VoiceChannel_CancelPendingRequests,
        FCk_Handle_VoiceChannel,
        ck::TReadOnly<FFragment_VoiceChannel_Requests>,
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
            HandleType InVoiceChannelEntity,
            const FFragment_VoiceChannel_Requests& InRequests)
            -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
