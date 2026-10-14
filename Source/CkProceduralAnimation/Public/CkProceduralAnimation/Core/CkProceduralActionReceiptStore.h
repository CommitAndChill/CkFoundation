#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Receipt.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action
{
    // Release-1 visible ceilings, not measured memory-optimal values.
    namespace receipt_limits
    {
        constexpr int32 MaxConcurrentInstances = 64;
        constexpr int32 MaxRetainedInstances = 256;
        constexpr int32 MaxOrdinaryRecordsPerInstance = 4096;
        // Sub-steps the service may evaluate inside one published receipt interval.
        constexpr int32 MaxSamplingSteps = 64;
        constexpr int32 MaxEventsPerReceipt = 64;
        constexpr int32 MaxWindowsPerReceipt = 32;
        constexpr int32 MaxClaimedLimbs = 64;
        constexpr int32 MaxSegmentsPerLimb = 8;
        constexpr int64 MaxRetainedRecords = 1049088;
        constexpr int64 MaxRetainedPoseTransforms = 1048576;
        constexpr int32 RetainedInstancesPerConcurrent = 4;
        constexpr int64 PoseTransformsPerRecord = 32;
        constexpr auto OrphanRetention = FCk_Time{2.0};
        // A terminal Explicit stream nobody acknowledges is released after this, so a consumer that never reads
        // cannot hold the player's capacity forever.
        constexpr auto UnacknowledgedTerminalRetention = FCk_Time{5.0};
    }

    // The store's derived budget: see FCk_ProceduralAction_Capacity.
    CKPROCEDURALANIMATION_API auto
        Get_MaxRetainedInstances(
            const FCk_ProceduralAction_Capacity& InCapacity)
        -> int32;

    CKPROCEDURALANIMATION_API auto
        Get_MaxRetainedRecords(
            const FCk_ProceduralAction_Capacity& InCapacity)
        -> int64;

    CKPROCEDURALANIMATION_API auto
        Get_MaxRetainedPoseTransforms(
            const FCk_ProceduralAction_Capacity& InCapacity)
        -> int64;

    // The finite budget checks a player is added with.
    CKPROCEDURALANIMATION_API auto
        ValidateTuning(
            const FCk_ProceduralAction_Tuning& InTuning,
            FName& OutInvalidField)
        -> bool;

    CKPROCEDURALANIMATION_API auto
        ValidateCapacity(
            const FCk_ProceduralAction_Capacity& InCapacity,
            FName& OutInvalidField)
        -> bool;

    enum class EReceiptStoreWrite : uint8
    {
        Accepted,
        Overflow,
        InvalidIdentity,
        InvalidRecord,
        AlreadyClosed,
        AlreadyTerminal,
        CapacityExceeded
    };

    // Player-owned value store. The ECS layer owns action lifetime, admission and recovery.
    class CKPROCEDURALANIMATION_API FReceiptStore
    {
    private:
        struct FStream
        {
            FCk_ProceduralAction_Identity Identity;
            TArray<FName> ClaimedLimbIds;
            bool HasChannelBinding = false;
            bool HasBodyClaim = false;
            bool EntityRemoved = false;
            bool WindowsClosed = false;
            bool Terminal = false;
            bool AutoDelivered = false;
            int32 PendingOrdinaryCount = 0;
            int64 LastSequence = 0;
            int64 LastSampleSequence = 0;
            int64 Generation = 0;
            int64 AcknowledgedThrough = 0;
            FCk_Time TerminalServiceTime;
            TArray<FName> ActiveWindowIds;
            TArray<FCk_ProceduralAction_Record> Records;
            TOptional<FCk_ProceduralAction_Receipt> LastConsumedAccepted;
            int64 LastConsumedTransforms = 0;
        };

        FCk_ProceduralAction_Capacity _Capacity;
        FCk_Time _ServiceTime;
        int64 _IdentityHighWater = 0;
        bool _Configured = false;
        int64 _RetainedRecordSlots = 0;
        int64 _RetainedPoseTransforms = 0;
        TMap<int64, FStream> _Streams;

        auto
            RemoveStream(
                int64 InIdentity)
            -> bool;

    public:
        auto
            Configure(
                const FCk_ProceduralAction_Capacity& InCapacity)
            -> bool;

        auto
            Reserve(
                const TArray<FName>& InClaimedLimbIds,
                bool InHasBodyClaim,
                FCk_ProceduralAction_Identity& OutIdentity)
            -> EReceiptStoreWrite;

        auto
            ReserveUnbound(
                FCk_ProceduralAction_Identity& OutIdentity)
            -> EReceiptStoreWrite;

        auto
            BindChannels(
                FCk_ProceduralAction_Identity InIdentity,
                const TArray<FName>& InClaimedLimbIds,
                bool InHasBodyClaim)
            -> EReceiptStoreWrite;

        // Hands the stream's acknowledgement to the player for the rest of its life. Only right after reservation,
        // before any record is published.
        auto
            SealAuto(
                FCk_ProceduralAction_Identity InIdentity)
            -> bool;

        // True while a sealed stream is retained; false once reclaimed.
        auto
            Get_IsAutoDelivered(
                FCk_ProceduralAction_Identity InIdentity) const
            -> bool;

        // Acknowledges every record published so far on every sealed stream.
        auto
            AcknowledgeAutoDelivered()
            -> void;

        // True once the stream's terminal record is acknowledged; false for a stream that is not retained.
        auto
            Get_IsTerminalAcknowledged(
                FCk_ProceduralAction_Identity InIdentity) const
            -> bool;

        // Roll back a reservation only before any record is published or claim acquired.
        auto
            AbortUnpublished(
                FCk_ProceduralAction_Identity InIdentity)
            -> bool;

        auto
            PublishAccepted(
                FCk_ProceduralAction_Identity InIdentity,
                const FCk_ProceduralAction_Receipt& InReceipt)
            -> EReceiptStoreWrite;

        auto
            CloseWindows(
                FCk_ProceduralAction_Identity InIdentity,
                FName InReason)
            -> EReceiptStoreWrite;

        auto
            Finish(
                FCk_ProceduralAction_Identity InIdentity,
                const FCk_ProceduralAction_Result& InResult)
            -> EReceiptStoreWrite;

        auto
            Finish(
                FCk_ProceduralAction_Identity InIdentity,
                const FCk_ProceduralAction_Result& InResult,
                const FCk_ProceduralAction_FailureContext& InContext)
            -> EReceiptStoreWrite;

        auto
            MarkEntityRemoved(
                FCk_ProceduralAction_Identity InIdentity)
            -> bool;

        auto
            Read(
                FCk_ProceduralAction_Identity InIdentity,
                int64 InAfterStreamSequence,
                int32 InMaxRecords) const
            -> FCk_ProceduralAction_ReadPage;

        auto
            Consume(
                FCk_ProceduralAction_Identity InIdentity,
                int64 InThroughStreamSequence)
            -> ECk_ProceduralAction_ReadResult;

        auto
            AdvanceService(
                FCk_Time InDelta)
            -> bool;

        auto
            HasAccepted(
                FCk_ProceduralAction_Identity InIdentity) const
            -> bool;

        // The highest stream sequence a consumer acknowledged. False for a stream that is not retained.
        auto
            Get_AcknowledgedThrough(
                FCk_ProceduralAction_Identity InIdentity,
                int64& OutAcknowledgedThrough) const
            -> bool;

        // Unacknowledged accepted-pose records: the count RecordsPerInstance bounds. 0 for an unknown stream.
        auto
            Get_PendingRecordCount(
                FCk_ProceduralAction_Identity InIdentity) const
            -> int32;

        auto
            Get_LastAccepted(
                FCk_ProceduralAction_Identity InIdentity) const
            -> FCk_ProceduralAction_Receipt;

        // Includes terminal streams retained after their action handle is destroyed.
        auto
            Get_RetainedIdentities() const
            -> TArray<FCk_ProceduralAction_Identity>;

        auto
            Get_IdentityHighWater() const
            -> int64
        {
            return _IdentityHighWater;
        }

        auto
            Get_RetainedCount() const
            -> int32
        {
            return _Streams.Num();
        }

        auto
            Get_ServiceTime() const
            -> FCk_Time
        {
            return _ServiceTime;
        }

        auto
            Get_RetainedRecordSlots() const
            -> int64
        {
            return _RetainedRecordSlots;
        }

        auto
            Get_RetainedPoseTransforms() const
            -> int64
        {
            return _RetainedPoseTransforms;
        }

        // On-demand owned container capacity, including nested receipt arrays and map slack.
        // Excludes sizeof(*this), allocator bookkeeping and shared assets/global FName storage.
        auto
            Get_AllocatedSize() const
            -> SIZE_T;
    };
}

// --------------------------------------------------------------------------------------------------------------------
