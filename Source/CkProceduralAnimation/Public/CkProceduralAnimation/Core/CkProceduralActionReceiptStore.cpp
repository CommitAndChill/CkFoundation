#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"
#include "CkProceduralAnimation/Core/CkProceduralActionMotion.h"

#include <limits>

namespace ck::procedural_action
{
    namespace receipt_store_detail
    {
        auto
        Get_PoseAllocatedSize(
            const FCk_ProceduralArticulation_Pose& InPose)
            -> SIZE_T
        {
            auto Bytes = InPose.Get_Limbs().GetAllocatedSize();
            for (const auto& Limb : InPose.Get_Limbs())
            { Bytes += Limb.Get_Segments().GetAllocatedSize(); }
            return Bytes;
        }

        auto
        Get_ReceiptAllocatedSize(
            const FCk_ProceduralAction_Receipt& InReceipt)
            -> SIZE_T
        {
            auto Bytes = Get_PoseAllocatedSize(InReceipt.Get_PreviousPose())
                + Get_PoseAllocatedSize(InReceipt.Get_CurrentPose())
                + InReceipt.Get_Interval().Get_Windows().GetAllocatedSize()
                + InReceipt.Get_Interval().Get_Events().GetAllocatedSize()
                + InReceipt.Get_IntermediateSamples().GetAllocatedSize();
            for (const auto& Sample : InReceipt.Get_IntermediateSamples())
            { Bytes += Get_PoseAllocatedSize(Sample.Get_Pose()); }
            return Bytes;
        }

        auto
        CountFilteredPoseTransforms(
            const FCk_ProceduralArticulation_Pose& InPose,
            const TArray<FName>& InClaimed,
            bool InHasBodyClaim)
            -> int64
        {
            auto Count = int64{1} + (InHasBodyClaim ? 1 : 0);
            for (const auto Id : InClaimed)
            {
                const auto* Limb = InPose.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
                { return InLimb.Get_Id() == Id; });
                if (Limb != nullptr) { Count += 2 + Limb->Get_Segments().Num(); }
            }
            return Count;
        }

        auto
        CountFilteredReceiptTransforms(
            const FCk_ProceduralAction_Receipt& InReceipt,
            const TArray<FName>& InClaimed,
            bool InHasBodyClaim)
            -> int64
        {
            auto Count = CountFilteredPoseTransforms(InReceipt.Get_PreviousPose(), InClaimed, InHasBodyClaim)
                + CountFilteredPoseTransforms(InReceipt.Get_CurrentPose(), InClaimed, InHasBodyClaim);
            for (const auto& Sample : InReceipt.Get_IntermediateSamples())
            { Count += CountFilteredPoseTransforms(Sample.Get_Pose(), InClaimed, InHasBodyClaim); }
            return Count;
        }

        auto
        CountReceiptTransforms(const FCk_ProceduralAction_Receipt& InReceipt)
            -> int64
        {
            auto CountPose = [](const FCk_ProceduralArticulation_Pose& InPose) -> int64
            {
                auto Count = int64{1} + (InPose.Get_HasBodyPresentation() ? 1 : 0);
                for (const auto& Limb : InPose.Get_Limbs())
                { Count += 2 + Limb.Get_Segments().Num(); }
                return Count;
            };
            auto Count = CountPose(InReceipt.Get_PreviousPose()) + CountPose(InReceipt.Get_CurrentPose());
            for (const auto& Sample : InReceipt.Get_IntermediateSamples())
            { Count += CountPose(Sample.Get_Pose()); }
            return Count;
        }

        auto
        IsRigid(
            const FTransform& InValue)
            -> bool
        {
            const auto& Position = InValue.GetTranslation();
            const auto& Rotation = InValue.GetRotation();
            const auto& Scale = InValue.GetScale3D();
            return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z)
                && FMath::IsFinite(Rotation.X) && FMath::IsFinite(Rotation.Y)
                && FMath::IsFinite(Rotation.Z) && FMath::IsFinite(Rotation.W)
                && FMath::Abs(Rotation.SizeSquared() - 1.0) <= 1.0e-4
                && FMath::IsFinite(Scale.X) && FMath::IsFinite(Scale.Y) && FMath::IsFinite(Scale.Z)
                && Scale.Equals(FVector::OneVector, 1.0e-6);
        }

        auto
        PositiveFinite(
            FCk_Time InValue)
            -> bool
        { return FMath::IsFinite(InValue.Get_Seconds()) && InValue.Get_Seconds() > 0.0; }

        auto
        NonnegativeFinite(
            FCk_Time InValue)
            -> bool
        { return FMath::IsFinite(InValue.Get_Seconds()) && InValue.Get_Seconds() >= 0.0; }

        auto
        ValidPose(
            const FCk_ProceduralArticulation_Pose& InPose,
            const TArray<FName>& InClaimed,
            bool InHasBodyClaim)
            -> bool
        {
            if (InHasBodyClaim && NOT InPose.Get_HasBodyPresentation()) { return false; }
            if (NOT IsRigid(InPose.Get_RootFrame())
                || (InHasBodyClaim && NOT IsRigid(InPose.Get_BodyPresentation())))
            { return false; }
            auto Claimed = TSet<FName>{};
            for (const auto Id : InClaimed) { Claimed.Add(Id); }
            auto Seen = TSet<FName>{};
            if (InPose.Get_Limbs().Num() > receipt_limits::MaxClaimedLimbs) { return false; }
            for (const auto& Limb : InPose.Get_Limbs())
            {
                if (Limb.Get_Id().IsNone() || Seen.Contains(Limb.Get_Id())
                    || Limb.Get_Segments().Num() > receipt_limits::MaxSegmentsPerLimb)
                { return false; }
                Seen.Add(Limb.Get_Id());
                if (NOT Claimed.Contains(Limb.Get_Id())) { continue; }
                if (Limb.Get_Segments().IsEmpty()
                    || NOT IsRigid(Limb.Get_RequestedEndpoint())
                    || NOT IsRigid(Limb.Get_AcceptedEndpoint())
                    || NOT FMath::IsFinite(Limb.Get_EndpointErrorCm())
                    || Limb.Get_EndpointErrorCm() < 0.0f
                    || (Limb.Get_ChainState() != ECk_ProceduralRig_ChainState::Clear
                        && Limb.Get_ChainState() != ECk_ProceduralRig_ChainState::Crossing)
                    || (Limb.Get_ContactEvidence() != ECk_ProceduralArticulation_ContactEvidence::None
                        && Limb.Get_ContactEvidence() != ECk_ProceduralArticulation_ContactEvidence::Untrusted
                        && Limb.Get_ContactEvidence() != ECk_ProceduralArticulation_ContactEvidence::Trusted))
                { return false; }
                for (const auto& Segment : Limb.Get_Segments())
                { if (NOT IsSegmentPoseValid(Segment)) { return false; } }
            }
            for (const auto Id : InClaimed)
            { if (NOT Seen.Contains(Id)) { return false; } }
            return true;
        }

        auto
        FilterPose(
            const FCk_ProceduralArticulation_Pose& InPose,
            const TArray<FName>& InClaimed,
            bool InHasBodyClaim)
            -> FCk_ProceduralArticulation_Pose
        {
            auto Limbs = TArray<FCk_ProceduralArticulation_LimbPose>{};
            Limbs.Reserve(InClaimed.Num());
            for (const auto Id : InClaimed)
            {
                const auto* Limb = InPose.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
                { return InLimb.Get_Id() == Id; });
                if (Limb != nullptr) { Limbs.Add(*Limb); }
            }
            return FCk_ProceduralArticulation_Pose{InPose.Get_RootFrame(),
                InHasBodyClaim ? InPose.Get_BodyPresentation() : FTransform::Identity,
                InHasBodyClaim, MoveTemp(Limbs)};
        }

        auto
        FilterReceipt(
            const FCk_ProceduralAction_Receipt& InReceipt,
            const TArray<FName>& InClaimed,
            bool InHasBodyClaim)
            -> FCk_ProceduralAction_Receipt
        {
            auto Samples = TArray<FCk_ProceduralAction_Sample>{};
            Samples.Reserve(InReceipt.Get_IntermediateSamples().Num());
            for (const auto& Sample : InReceipt.Get_IntermediateSamples())
            {
                Samples.Add(FCk_ProceduralAction_Sample{Sample.Get_Time(),
                    FilterPose(Sample.Get_Pose(), InClaimed, InHasBodyClaim), Sample.Get_Evidence()});
            }
            return FCk_ProceduralAction_Receipt{true, InReceipt.Get_Stamp(), InReceipt.Get_Interval(),
                FilterPose(InReceipt.Get_PreviousPose(), InClaimed, InHasBodyClaim),
                FilterPose(InReceipt.Get_CurrentPose(), InClaimed, InHasBodyClaim),
                MoveTemp(Samples), InReceipt.Get_Sampling(), InReceipt.Get_CorrelationToken()};
        }
    }

    auto
    ValidateTuning(
        const FCk_ProceduralAction_Tuning& InTuning,
        FName& OutInvalidField)
        -> bool
    {
        OutInvalidField = NAME_None;
        const auto Check = [&](bool InValid, FName InField) -> bool
        {
            if (NOT InValid) { OutInvalidField = InField; }
            return InValid;
        };
        const auto& Playback = InTuning.Get_Playback();
        const auto& Transition = InTuning.Get_Transition();
        const auto& Watchdogs = InTuning.Get_Watchdogs();
        const auto& Sampling = InTuning.Get_Sampling();
        return Check(FMath::IsFinite(Playback.Get_PlayRate()) && Playback.Get_PlayRate() > 0.0f, TEXT("Playback.PlayRate"))
            && Check(receipt_store_detail::NonnegativeFinite(Transition.Get_EntryBlend()), TEXT("Transition.EntryBlend"))
            && Check(receipt_store_detail::PositiveFinite(Transition.Get_DedicatedReturn()), TEXT("Transition.DedicatedReturn"))
            && Check(receipt_store_detail::PositiveFinite(Transition.Get_ReturnTimeout())
                && Transition.Get_ReturnTimeout() >= Transition.Get_DedicatedReturn(), TEXT("Transition.ReturnTimeout"))
            && Check(receipt_store_detail::PositiveFinite(Watchdogs.Get_LoadTimeout()), TEXT("Watchdogs.LoadTimeout"))
            && Check(receipt_store_detail::PositiveFinite(Watchdogs.Get_AcquisitionTimeout()), TEXT("Watchdogs.AcquisitionTimeout"))
            && Check(receipt_store_detail::PositiveFinite(Watchdogs.Get_PublicationTimeout()), TEXT("Watchdogs.PublicationTimeout"))
            && Check(receipt_store_detail::PositiveFinite(Watchdogs.Get_MaxTimeBacklog()), TEXT("Watchdogs.MaxTimeBacklog"))
            && Check(receipt_store_detail::PositiveFinite(Sampling.Get_MaxAdvance()), TEXT("Sampling.MaxAdvance"))
            && Check(FMath::IsFinite(Sampling.Get_MaxEndpointTravelCm())
                && Sampling.Get_MaxEndpointTravelCm() > 0.0f, TEXT("Sampling.MaxEndpointTravelCm"))
            && Check(FMath::IsFinite(Sampling.Get_MaxSegmentAngularStepDegrees())
                && Sampling.Get_MaxSegmentAngularStepDegrees() > 0.0f, TEXT("Sampling.MaxSegmentAngularStepDegrees"))
            && Check(Sampling.Get_MaxSamples() > 0
                && Sampling.Get_MaxSamples() <= receipt_limits::MaxSamplesPerReceipt, TEXT("Sampling.MaxSamples"));
    }

    auto
    ValidateCapacity(
        const FCk_ProceduralAction_Capacity& InCapacity,
        FName& OutInvalidField)
        -> bool
    {
        OutInvalidField = NAME_None;
        if (InCapacity.Get_MaxConcurrentInstances() <= 0
            || InCapacity.Get_MaxConcurrentInstances() > receipt_limits::MaxConcurrentInstances)
        { OutInvalidField = TEXT("Capacity.MaxConcurrentInstances"); return false; }
        if (InCapacity.Get_MaxRetainedInstances() < InCapacity.Get_MaxConcurrentInstances()
            || InCapacity.Get_MaxRetainedInstances() > receipt_limits::MaxRetainedInstances)
        { OutInvalidField = TEXT("Capacity.MaxRetainedInstances"); return false; }
        if (InCapacity.Get_RecordsPerInstance() <= 0
            || InCapacity.Get_RecordsPerInstance() > receipt_limits::MaxOrdinaryRecordsPerInstance)
        { OutInvalidField = TEXT("Capacity.RecordsPerInstance"); return false; }
        if (InCapacity.Get_MaxRetainedRecords()
                < static_cast<int64>(InCapacity.Get_MaxConcurrentInstances()) * 3
            || InCapacity.Get_MaxRetainedRecords() > receipt_limits::MaxRetainedRecords)
        { OutInvalidField = TEXT("Capacity.MaxRetainedRecords"); return false; }
        if (InCapacity.Get_MaxRetainedPoseTransforms() <= 0
            || InCapacity.Get_MaxRetainedPoseTransforms() > receipt_limits::MaxRetainedPoseTransforms)
        { OutInvalidField = TEXT("Capacity.MaxRetainedPoseTransforms"); return false; }
        if (InCapacity.Get_MaxSamplesPerReceipt() <= 0
            || InCapacity.Get_MaxSamplesPerReceipt() > receipt_limits::MaxSamplesPerReceipt)
        { OutInvalidField = TEXT("Capacity.MaxSamplesPerReceipt"); return false; }
        if (InCapacity.Get_MaxEventsPerReceipt() <= 0
            || InCapacity.Get_MaxEventsPerReceipt() > receipt_limits::MaxEventsPerReceipt)
        { OutInvalidField = TEXT("Capacity.MaxEventsPerReceipt"); return false; }
        if (InCapacity.Get_MaxWindowsPerReceipt() <= 0
            || InCapacity.Get_MaxWindowsPerReceipt() > receipt_limits::MaxWindowsPerReceipt)
        { OutInvalidField = TEXT("Capacity.MaxWindowsPerReceipt"); return false; }
        if (NOT receipt_store_detail::PositiveFinite(InCapacity.Get_OrphanRetentionTimeout()))
        { OutInvalidField = TEXT("Capacity.OrphanRetentionTimeout"); return false; }
        if (InCapacity.Get_ManagedRecordsPerTick() <= 0
            || InCapacity.Get_ManagedRecordsPerTick() > receipt_limits::MaxOrdinaryRecordsPerInstance + 2)
        { OutInvalidField = TEXT("Capacity.ManagedRecordsPerTick"); return false; }
        return true;
    }

    auto
    ValidateControlSpec(
        const FCk_ProceduralAction_ControlSpec& InSpec,
        FName& OutInvalidField)
        -> bool
    {
        OutInvalidField = NAME_None;
        if (NOT receipt_store_detail::PositiveFinite(InSpec.Get_InputTimeout()))
        { OutInvalidField = TEXT("Control.InputTimeout"); return false; }
        if (InSpec.Get_LimbIds().IsEmpty() && InSpec.Get_BodyChannel() != ECk_EnableDisable::Enable)
        { OutInvalidField = TEXT("Control.Resources"); return false; }
        if (InSpec.Get_BodyChannel() != ECk_EnableDisable::Enable
            && InSpec.Get_BodyChannel() != ECk_EnableDisable::Disable)
        { OutInvalidField = TEXT("Control.BodyChannel"); return false; }
        if (InSpec.Get_LimbIds().Num() > receipt_limits::MaxClaimedLimbs)
        { OutInvalidField = TEXT("Control.LimbIds"); return false; }
        if (InSpec.Get_AnchorBindings().Num() > receipt_limits::MaxClaimedLimbs)
        { OutInvalidField = TEXT("Control.AnchorBindings"); return false; }
        auto Ids = TSet<FName>{};
        for (const auto Id : InSpec.Get_LimbIds())
        {
            if (Id.IsNone() || Ids.Contains(Id))
            { OutInvalidField = TEXT("Control.LimbIds"); return false; }
            Ids.Add(Id);
        }
        auto Anchors = TSet<FName>{};
        for (const auto& Anchor : InSpec.Get_AnchorBindings())
        {
            if (Anchor.Get_Id().IsNone() || Anchors.Contains(Anchor.Get_Id())
                || NOT receipt_store_detail::IsRigid(Anchor.Get_Frame()))
            { OutInvalidField = TEXT("Control.AnchorBindings"); return false; }
            Anchors.Add(Anchor.Get_Id());
        }
        return true;
    }

    auto
    FReceiptStore::Configure(
        const FCk_ProceduralAction_Capacity& InCapacity)
        -> bool
    {
        auto InvalidField = FName{};
        if (_Configured || NOT ValidateCapacity(InCapacity, InvalidField)) { return false; }
        _Capacity = InCapacity;
        _Configured = true;
        return true;
    }

    auto
    FReceiptStore::RemoveStream(int64 InIdentity)
        -> bool
    {
        const auto* Stream = _Streams.Find(InIdentity);
        if (Stream == nullptr) { return false; }
        _RetainedRecordSlots -= 2 + Stream->PendingOrdinaryCount;
        _RetainedPoseTransforms -= Stream->LastConsumedTransforms;
        for (const auto& Record : Stream->Records)
        {
            if (Record.Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose)
            { _RetainedPoseTransforms -= receipt_store_detail::CountReceiptTransforms(Record.Get_Receipt()); }
        }
        return _Streams.Remove(InIdentity) == 1;
    }

    auto
    FReceiptStore::Reserve(
        const TArray<FName>& InClaimedLimbIds,
        bool InHasBodyClaim,
        FCk_ProceduralAction_Identity& OutIdentity)
        -> EReceiptStoreWrite
    {
        const auto Reservation = ReserveUnbound(OutIdentity);
        if (Reservation != EReceiptStoreWrite::Accepted) { return Reservation; }
        const auto Binding = BindChannels(OutIdentity, InClaimedLimbIds, InHasBodyClaim);
        if (Binding != EReceiptStoreWrite::Accepted)
        {
            AbortUnpublished(OutIdentity);
            OutIdentity = {};
        }
        return Binding;
    }

    auto
    FReceiptStore::ReserveUnbound(
        FCk_ProceduralAction_Identity& OutIdentity)
        -> EReceiptStoreWrite
    {
        OutIdentity = {};
        if (NOT _Configured || _Streams.Num() >= _Capacity.Get_MaxRetainedInstances()
            || _RetainedRecordSlots > _Capacity.Get_MaxRetainedRecords() - 2
            || _IdentityHighWater == std::numeric_limits<int64>::max())
        { return EReceiptStoreWrite::CapacityExceeded; }
        auto Active = 0;
        for (const auto& Entry : _Streams)
        { if (NOT Entry.Value.Terminal) { ++Active; } }
        if (Active >= _Capacity.Get_MaxConcurrentInstances()) { return EReceiptStoreWrite::CapacityExceeded; }
        const auto Id = ++_IdentityHighWater;
        auto Stream = FStream{};
        Stream.Identity = FCk_ProceduralAction_Identity{Id};
        _Streams.Add(Id, MoveTemp(Stream));
        _RetainedRecordSlots += 2;
        OutIdentity = FCk_ProceduralAction_Identity{Id};
        return EReceiptStoreWrite::Accepted;
    }

    auto
    FReceiptStore::BindChannels(
        FCk_ProceduralAction_Identity InIdentity,
        const TArray<FName>& InClaimedLimbIds,
        bool InHasBodyClaim)
        -> EReceiptStoreWrite
    {
        auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr) { return EReceiptStoreWrite::InvalidIdentity; }
        if (Stream->HasChannelBinding || Stream->LastSequence != 0
            || (InClaimedLimbIds.IsEmpty() && NOT InHasBodyClaim)
            || InClaimedLimbIds.Num() > receipt_limits::MaxClaimedLimbs)
        { return EReceiptStoreWrite::InvalidRecord; }
        auto Seen = TSet<FName>{};
        for (const auto Id : InClaimedLimbIds)
        {
            if (Id.IsNone() || Seen.Contains(Id)) { return EReceiptStoreWrite::InvalidRecord; }
            Seen.Add(Id);
        }
        Stream->ClaimedLimbIds = InClaimedLimbIds;
        Stream->HasBodyClaim = InHasBodyClaim;
        Stream->HasChannelBinding = true;
        return EReceiptStoreWrite::Accepted;
    }

    auto
    FReceiptStore::SealManaged(
        FCk_ProceduralAction_Identity InIdentity)
        -> bool
    {
        auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr || Stream->LastSequence != 0)
        { return false; }

        Stream->ManagedDelivery = true;
        return true;
    }

    auto
    FReceiptStore::Get_IsManagedDelivery(
        FCk_ProceduralAction_Identity InIdentity) const
        -> bool
    {
        const auto* Stream = _Streams.Find(InIdentity.Get_Value());
        return Stream != nullptr && Stream->ManagedDelivery;
    }

    auto
    FReceiptStore::AbortUnpublished(
        FCk_ProceduralAction_Identity InIdentity)
        -> bool
    {
        const auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr || Stream->LastSequence != 0 || Stream->LastSampleSequence != 0)
        { return false; }
        return RemoveStream(InIdentity.Get_Value());
    }

    auto
    FReceiptStore::PublishAccepted(
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_ProceduralAction_Receipt& InReceipt)
        -> EReceiptStoreWrite
    {
        auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr) { return EReceiptStoreWrite::InvalidIdentity; }
        if (Stream->Terminal) { return EReceiptStoreWrite::AlreadyTerminal; }
        if (Stream->WindowsClosed) { return EReceiptStoreWrite::AlreadyClosed; }
        if (NOT Stream->HasChannelBinding) { return EReceiptStoreWrite::InvalidRecord; }
        if (Stream->EntityRemoved) { return EReceiptStoreWrite::InvalidRecord; }
        const auto& Stamp = InReceipt.Get_Stamp();
        const auto& Interval = InReceipt.Get_Interval();
        const auto From = Interval.Get_FromTime().Get_Seconds();
        const auto To = Interval.Get_ToTime().Get_Seconds();
        const auto& Sampling = InReceipt.Get_Sampling();
        const auto ValidContinuity = Interval.Get_Continuity() == ECk_ProceduralAction_Continuity::Continuous
            || Interval.Get_Continuity() == ECk_ProceduralAction_Continuity::FirstSample
            || Interval.Get_Continuity() == ECk_ProceduralAction_Continuity::Teleported
            || Interval.Get_Continuity() == ECk_ProceduralAction_Continuity::ControllerChanged
            || Interval.Get_Continuity() == ECk_ProceduralAction_Continuity::SolverDiscontinuity;
        if (NOT InReceipt.Get_HasAcceptedPose() || Stamp.Get_Identity().Get_Value() != InIdentity.Get_Value()
            || Stamp.Get_Generation() <= 0
            || (Stream->Generation != 0 && Stamp.Get_Generation() != Stream->Generation)
            || Stamp.Get_SampleSequence() <= Stream->LastSampleSequence
            || Stamp.Get_SubjectPoseRevision() <= 0 || Stamp.Get_TopologyRevision() <= 0
            || Stamp.Get_ContentRevision() < 0 || Stamp.Get_TuningRevision() < 0
            || NOT FMath::IsFinite(From) || NOT FMath::IsFinite(To) || From < 0.0 || To < From
            || NOT ValidContinuity
            || (Stream->LastSampleSequence != 0
                && Interval.Get_Continuity() == ECk_ProceduralAction_Continuity::FirstSample)
            || Sampling.Get_EvaluatedSamples() < InReceipt.Get_IntermediateSamples().Num() + 1
            || Sampling.Get_AcceptedSamples() != 1
            || NOT FMath::IsFinite(Sampling.Get_MaxEndpointTravelCm())
            || Sampling.Get_MaxEndpointTravelCm() < 0.0f
            || NOT FMath::IsFinite(Sampling.Get_MaxSegmentAngularStepDegrees())
            || Sampling.Get_MaxSegmentAngularStepDegrees() < 0.0f
            || NOT receipt_store_detail::NonnegativeFinite(Sampling.Get_Backlog())
            || InReceipt.Get_IntermediateSamples().Num() > _Capacity.Get_MaxSamplesPerReceipt()
            || InReceipt.Get_Interval().Get_Events().Num() > _Capacity.Get_MaxEventsPerReceipt()
            || InReceipt.Get_Interval().Get_Windows().Num() > _Capacity.Get_MaxWindowsPerReceipt()
            || NOT receipt_store_detail::ValidPose(InReceipt.Get_CurrentPose(), Stream->ClaimedLimbIds, Stream->HasBodyClaim))
        { return EReceiptStoreWrite::InvalidRecord; }
        if (InReceipt.Get_Interval().Get_Continuity() != ECk_ProceduralAction_Continuity::FirstSample
            && NOT receipt_store_detail::ValidPose(InReceipt.Get_PreviousPose(), Stream->ClaimedLimbIds, Stream->HasBodyClaim))
        { return EReceiptStoreWrite::InvalidRecord; }
        auto LastSampleTime = From;
        for (const auto& Sample : InReceipt.Get_IntermediateSamples())
        {
            const auto Time = Sample.Get_Time().Get_Seconds();
            if (NOT FMath::IsFinite(Time) || Time < LastSampleTime || Time > To
                || Sample.Get_Evidence() != ECk_ProceduralAction_SampleEvidence::Evaluated
                || NOT receipt_store_detail::ValidPose(Sample.Get_Pose(), Stream->ClaimedLimbIds, Stream->HasBodyClaim))
            { return EReceiptStoreWrite::InvalidRecord; }
            LastSampleTime = Time;
        }
        auto WindowIds = TSet<FName>{};
        for (const auto Id : Interval.Get_Windows())
        {
            if (Id.IsNone() || WindowIds.Contains(Id)) { return EReceiptStoreWrite::InvalidRecord; }
            WindowIds.Add(Id);
        }
        auto LastEventTime = From;
        auto LastEventKind = ECk_ProceduralAction_EventKind::WindowExit;
        for (const auto& Event : Interval.Get_Events())
        {
            const auto Time = Event.Get_Time().Get_Seconds();
            const auto Kind = Event.Get_Kind();
            if (Event.Get_Id().IsNone() || NOT FMath::IsFinite(Time) || Time < From || Time > To
                || (Kind != ECk_ProceduralAction_EventKind::WindowExit
                    && Kind != ECk_ProceduralAction_EventKind::Point
                    && Kind != ECk_ProceduralAction_EventKind::WindowEnter)
                || Time < LastEventTime || (Time == LastEventTime && Kind < LastEventKind))
            { return EReceiptStoreWrite::InvalidRecord; }
            LastEventTime = Time;
            LastEventKind = Kind;
        }
        auto NextActiveWindows = Stream->ActiveWindowIds;
        for (const auto& Event : InReceipt.Get_Interval().Get_Events())
        {
            if (Event.Get_Kind() == ECk_ProceduralAction_EventKind::WindowExit)
            { NextActiveWindows.Remove(Event.Get_Id()); }
            else if (Event.Get_Kind() == ECk_ProceduralAction_EventKind::WindowEnter
                && NOT NextActiveWindows.Contains(Event.Get_Id()))
            { NextActiveWindows.Add(Event.Get_Id()); }
        }
        if (NextActiveWindows.Num() > _Capacity.Get_MaxWindowsPerReceipt())
        { return EReceiptStoreWrite::InvalidRecord; }
        if (Stream->PendingOrdinaryCount >= _Capacity.Get_RecordsPerInstance())
        {
            CloseWindows(InIdentity, TEXT("ReceiptOverflow"));
            return EReceiptStoreWrite::Overflow;
        }
        const auto Transforms = receipt_store_detail::CountFilteredReceiptTransforms(
            InReceipt, Stream->ClaimedLimbIds, Stream->HasBodyClaim);
        const auto UsedWithoutOldLatest = _RetainedPoseTransforms - Stream->LastConsumedTransforms;
        if (_RetainedRecordSlots >= _Capacity.Get_MaxRetainedRecords()
            || Transforms > _Capacity.Get_MaxRetainedPoseTransforms() - UsedWithoutOldLatest)
        {
            CloseWindows(InIdentity, TEXT("ReceiptOverflow"));
            return EReceiptStoreWrite::Overflow;
        }
        _RetainedPoseTransforms = UsedWithoutOldLatest;
        Stream->LastConsumedAccepted.Reset();
        Stream->LastConsumedTransforms = 0;
        auto Record = FCk_ProceduralAction_Record{InIdentity, ++Stream->LastSequence,
            ECk_ProceduralAction_RecordKind::AcceptedPose,
            receipt_store_detail::FilterReceipt(InReceipt, Stream->ClaimedLimbIds, Stream->HasBodyClaim),
            {}, {}};
        Stream->Records.Add(MoveTemp(Record));
        ++Stream->PendingOrdinaryCount;
        ++_RetainedRecordSlots;
        _RetainedPoseTransforms += Transforms;
        Stream->Generation = Stamp.Get_Generation();
        Stream->LastSampleSequence = Stamp.Get_SampleSequence();
        Stream->ActiveWindowIds = MoveTemp(NextActiveWindows);
        return EReceiptStoreWrite::Accepted;
    }

    auto
    FReceiptStore::CloseWindows(
        FCk_ProceduralAction_Identity InIdentity,
        FName InReason)
        -> EReceiptStoreWrite
    {
        auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr) { return EReceiptStoreWrite::InvalidIdentity; }
        if (Stream->Terminal) { return EReceiptStoreWrite::AlreadyTerminal; }
        if (Stream->WindowsClosed) { return EReceiptStoreWrite::AlreadyClosed; }
        Stream->Records.Add(FCk_ProceduralAction_Record{InIdentity, ++Stream->LastSequence,
            ECk_ProceduralAction_RecordKind::WindowsClosed, {}, Stream->ActiveWindowIds,
            FCk_ProceduralAction_Result{ECk_ProceduralAction_Outcome::None,
                ECk_ProceduralAction_Recovery::NotRequired, InReason}});
        Stream->ActiveWindowIds.Reset();
        Stream->WindowsClosed = true;
        return EReceiptStoreWrite::Accepted;
    }

    auto
    FReceiptStore::Finish(
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_ProceduralAction_Result& InResult)
        -> EReceiptStoreWrite
    {
        return Finish(InIdentity, InResult, FCk_ProceduralAction_FailureContext{});
    }

    auto
    FReceiptStore::Finish(
        FCk_ProceduralAction_Identity InIdentity,
        const FCk_ProceduralAction_Result& InResult,
        const FCk_ProceduralAction_FailureContext& InContext)
        -> EReceiptStoreWrite
    {
        auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr) { return EReceiptStoreWrite::InvalidIdentity; }
        if (Stream->Terminal) { return EReceiptStoreWrite::AlreadyTerminal; }
        if (InResult.Get_Outcome() == ECk_ProceduralAction_Outcome::None
            || InResult.Get_Recovery() == ECk_ProceduralAction_Recovery::Pending)
        { return EReceiptStoreWrite::InvalidRecord; }
        if (NOT Stream->WindowsClosed) { CloseWindows(InIdentity, InResult.Get_Reason()); }
        auto Record = FCk_ProceduralAction_Record{InIdentity, ++Stream->LastSequence,
            ECk_ProceduralAction_RecordKind::Terminal, {}, {}, InResult};
        Record.Set_Context(InContext);
        Stream->Records.Add(MoveTemp(Record));
        Stream->Terminal = true;
        Stream->TerminalServiceTime = _ServiceTime;
        if (Stream->EntityRemoved && Stream->AcknowledgedThrough == Stream->LastSequence)
        { RemoveStream(InIdentity.Get_Value()); }
        return EReceiptStoreWrite::Accepted;
    }

    auto
    FReceiptStore::MarkEntityRemoved(
        FCk_ProceduralAction_Identity InIdentity)
        -> bool
    {
        auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr) { return false; }
        Stream->EntityRemoved = true;
        if (Stream->Terminal && Stream->AcknowledgedThrough == Stream->LastSequence)
        { RemoveStream(InIdentity.Get_Value()); }
        return true;
    }

    auto
    FReceiptStore::Read(
        FCk_ProceduralAction_Identity InIdentity,
        int64 InAfterStreamSequence,
        int32 InMaxRecords)
        const -> FCk_ProceduralAction_ReadPage
    {
        if (InIdentity.Get_Value() <= 0 || InIdentity.Get_Value() > _IdentityHighWater)
        { return FCk_ProceduralAction_ReadPage{ECk_ProceduralAction_ReadResult::InvalidCursor, {}, 0, false, false}; }
        if (InAfterStreamSequence < 0)
        { return FCk_ProceduralAction_ReadPage{ECk_ProceduralAction_ReadResult::InvalidCursor,
            {}, InAfterStreamSequence, false, false}; }
        const auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr)
        { return FCk_ProceduralAction_ReadPage{ECk_ProceduralAction_ReadResult::StreamExpired, {}, 0, true, false}; }
        if (InAfterStreamSequence > Stream->LastSequence
            || InMaxRecords <= 0 || InMaxRecords > _Capacity.Get_RecordsPerInstance() + 2)
        { return FCk_ProceduralAction_ReadPage{ECk_ProceduralAction_ReadResult::InvalidCursor,
            {}, InAfterStreamSequence, Stream->Terminal, false}; }
        const auto HasGap = InAfterStreamSequence < Stream->AcknowledgedThrough;
        const auto FirstMissingSequence = HasGap ? InAfterStreamSequence + 1 : 0;
        const auto LastMissingSequence = HasGap ? Stream->AcknowledgedThrough : 0;
        const auto EffectiveCursor = FMath::Max(InAfterStreamSequence, Stream->AcknowledgedThrough);
        auto Records = TArray<FCk_ProceduralAction_Record>{};
        for (const auto& Record : Stream->Records)
        {
            if (Record.Get_StreamSequence() <= EffectiveCursor) { continue; }
            Records.Add(Record);
            if (Records.Num() >= InMaxRecords) { break; }
        }
        const auto LastReturnedSequence = Records.IsEmpty()
            ? EffectiveCursor : Records.Last().Get_StreamSequence();
        const auto HasMore = LastReturnedSequence < Stream->LastSequence;
        return FCk_ProceduralAction_ReadPage{HasGap ? ECk_ProceduralAction_ReadResult::Gap
            : Stream->Terminal ? ECk_ProceduralAction_ReadResult::Succeeded
                : ECk_ProceduralAction_ReadResult::Pending,
            MoveTemp(Records), LastReturnedSequence, Stream->Terminal, HasMore,
            FirstMissingSequence, LastMissingSequence};
    }

    auto
    FReceiptStore::Consume(
        FCk_ProceduralAction_Identity InIdentity,
        int64 InThroughStreamSequence)
        -> ECk_ProceduralAction_ReadResult
    {
        if (InIdentity.Get_Value() <= 0 || InIdentity.Get_Value() > _IdentityHighWater)
        { return ECk_ProceduralAction_ReadResult::InvalidCursor; }
        auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr) { return ECk_ProceduralAction_ReadResult::StreamExpired; }
        if (InThroughStreamSequence < 0 || InThroughStreamSequence > Stream->LastSequence)
        { return ECk_ProceduralAction_ReadResult::InvalidCursor; }
        Stream->AcknowledgedThrough = FMath::Max(Stream->AcknowledgedThrough, InThroughStreamSequence);
        const auto* NewestConsumed = static_cast<const FCk_ProceduralAction_Record*>(nullptr);
        for (const auto& Record : Stream->Records)
        {
            if (Record.Get_StreamSequence() <= Stream->AcknowledgedThrough
                && Record.Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose)
            { NewestConsumed = &Record; }
        }
        auto HasNewerQueuedAccepted = false;
        for (const auto& Record : Stream->Records)
        {
            if (Record.Get_StreamSequence() > Stream->AcknowledgedThrough
                && Record.Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose)
            { HasNewerQueuedAccepted = true; break; }
        }
        if (HasNewerQueuedAccepted && Stream->LastConsumedAccepted.IsSet())
        {
            _RetainedPoseTransforms -= Stream->LastConsumedTransforms;
            Stream->LastConsumedAccepted.Reset();
            Stream->LastConsumedTransforms = 0;
        }
        if (NewestConsumed != nullptr && NOT HasNewerQueuedAccepted)
        {
            _RetainedPoseTransforms -= Stream->LastConsumedTransforms;
            Stream->LastConsumedAccepted = NewestConsumed->Get_Receipt();
            Stream->LastConsumedTransforms = receipt_store_detail::CountReceiptTransforms(
                Stream->LastConsumedAccepted.GetValue());
            _RetainedPoseTransforms += Stream->LastConsumedTransforms;
        }
        Stream->Records.RemoveAll([this, Stream](const FCk_ProceduralAction_Record& InRecord)
        {
            if (InRecord.Get_StreamSequence() > Stream->AcknowledgedThrough) { return false; }
            if (InRecord.Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose)
            {
                --Stream->PendingOrdinaryCount;
                --_RetainedRecordSlots;
                _RetainedPoseTransforms -= receipt_store_detail::CountReceiptTransforms(InRecord.Get_Receipt());
            }
            return true;
        });
        if (Stream->Terminal && Stream->EntityRemoved && Stream->AcknowledgedThrough == Stream->LastSequence)
        { RemoveStream(InIdentity.Get_Value()); }
        return ECk_ProceduralAction_ReadResult::Succeeded;
    }

    auto
    FReceiptStore::AdvanceService(
        FCk_Time InDelta)
        -> bool
    {
        if (NOT receipt_store_detail::NonnegativeFinite(InDelta)
            || NOT FMath::IsFinite((_ServiceTime + InDelta).Get_Seconds()))
        { return false; }
        _ServiceTime += InDelta;
        auto Expired = TArray<int64>{};
        for (const auto& Entry : _Streams)
        {
            const auto& Stream = Entry.Value;
            if (Stream.EntityRemoved && Stream.Terminal
                && _ServiceTime - Stream.TerminalServiceTime >= _Capacity.Get_OrphanRetentionTimeout())
            { Expired.Add(Entry.Key); }
        }
        for (const auto Id : Expired) { RemoveStream(Id); }
        return true;
    }

    auto
    FReceiptStore::Get_AcknowledgedThrough(
        FCk_ProceduralAction_Identity InIdentity,
        int64& OutAcknowledgedThrough) const
        -> bool
    {
        const auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr)
        { return false; }

        OutAcknowledgedThrough = Stream->AcknowledgedThrough;
        return true;
    }

    auto
    FReceiptStore::HasAccepted(FCk_ProceduralAction_Identity InIdentity) const
        -> bool
    {
        const auto* Stream = _Streams.Find(InIdentity.Get_Value());
        return Stream != nullptr && Stream->LastSampleSequence > 0;
    }

    auto
    FReceiptStore::Get_PendingRecordCount(FCk_ProceduralAction_Identity InIdentity) const
        -> int32
    {
        const auto* Stream = _Streams.Find(InIdentity.Get_Value());
        return Stream != nullptr ? Stream->PendingOrdinaryCount : 0;
    }

    auto
    FReceiptStore::Get_LastAccepted(FCk_ProceduralAction_Identity InIdentity) const
        -> FCk_ProceduralAction_Receipt
    {
        const auto* Stream = _Streams.Find(InIdentity.Get_Value());
        if (Stream == nullptr) { return {}; }
        for (auto Index = Stream->Records.Num() - 1; Index >= 0; --Index)
        {
            if (Stream->Records[Index].Get_Kind() == ECk_ProceduralAction_RecordKind::AcceptedPose)
            { return Stream->Records[Index].Get_Receipt(); }
        }
        return Stream->LastConsumedAccepted.IsSet()
            ? Stream->LastConsumedAccepted.GetValue() : FCk_ProceduralAction_Receipt{};
    }

    auto
    FReceiptStore::Get_RetainedIdentities() const
        -> TArray<FCk_ProceduralAction_Identity>
    {
        auto Identities = TArray<FCk_ProceduralAction_Identity>{};
        Identities.Reserve(_Streams.Num());
        for (const auto& Entry : _Streams)
        { Identities.Add(Entry.Value.Identity); }
        Identities.Sort([](const auto& InA, const auto& InB)
        { return InA.Get_Value() < InB.Get_Value(); });
        return Identities;
    }

    auto
    FReceiptStore::Get_AllocatedSize() const
        -> SIZE_T
    {
        auto Bytes = _Streams.GetAllocatedSize();
        for (const auto& Entry : _Streams)
        {
            const auto& Stream = Entry.Value;
            Bytes += Stream.ClaimedLimbIds.GetAllocatedSize() + Stream.ActiveWindowIds.GetAllocatedSize()
                + Stream.Records.GetAllocatedSize();
            for (const auto& Record : Stream.Records)
            {
                Bytes += receipt_store_detail::Get_ReceiptAllocatedSize(Record.Get_Receipt())
                    + Record.Get_ClosedWindowIds().GetAllocatedSize()
                    + Record.Get_Context().Get_LimbIds().GetAllocatedSize();
            }
            if (Stream.LastConsumedAccepted.IsSet())
            { Bytes += receipt_store_detail::Get_ReceiptAllocatedSize(Stream.LastConsumedAccepted.GetValue()); }
        }
        return Bytes;
    }
}

// --------------------------------------------------------------------------------------------------------------------
