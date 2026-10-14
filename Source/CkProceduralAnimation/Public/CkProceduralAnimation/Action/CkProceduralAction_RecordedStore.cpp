#include "CkProceduralAnimation/Action/CkProceduralAction_RecordedStore.h"

namespace ck_procedural_action_recorded_store
{
    auto
        TryCountPoseTransforms(
            const FCk_ProceduralArticulation_Pose& InPose,
            int64 InBudget,
            int64& InOutCount)
        -> bool
    {
        const auto TryAdd = [InBudget, &InOutCount](int64 InAmount) -> bool
        {
            if (InAmount > InBudget - InOutCount)
            { return false; }
            InOutCount += InAmount;
            return true;
        };

        if (NOT TryAdd(1 + static_cast<int64>(InPose.Get_HasBodyPresentation())))
        { return false; }
        for (const auto& Limb : InPose.Get_Limbs())
        {
            if (NOT TryAdd(2 + static_cast<int64>(Limb.Get_Segments().Num())))
            { return false; }
        }
        return true;
    }

    auto
        TryCountReceiptTransforms(
            const FCk_ProceduralAction_Receipt& InReceipt,
            int64 InBudget,
            int64& InOutCount)
        -> bool
    {
        if (InReceipt.Get_HasAcceptedPose())
        {
            if (NOT TryCountPoseTransforms(InReceipt.Get_PreviousPose(), InBudget, InOutCount)
                || NOT TryCountPoseTransforms(InReceipt.Get_CurrentPose(), InBudget, InOutCount))
            { return false; }
        }
        return true;
    }
}

auto
    FCk_ProceduralActionRecordedStore::
    Reset(
        FCk_ProceduralAction_Identity InIdentity,
        FProvenance InProvenance)
    -> void
{
    _Identity = InIdentity;
    _Provenance = MoveTemp(InProvenance);
    _Records.Reset();
    _Gaps.Reset();
    _LastCopiedSequence = 0;
    _CopiedPoseTransforms = 0;
    _LastReadResult = ECk_ProceduralAction_ReadResult::Pending;
    _LastError.Reset();
    _Terminal = false;
}

auto
    FCk_ProceduralActionRecordedStore::
    AppendPage(
        const FCk_ProceduralAction_ReadPage& InPage)
    -> bool
{
    _LastReadResult = InPage.Get_Result();
    if (_Identity.Get_Value() <= 0
        || (InPage.Get_Result() != ECk_ProceduralAction_ReadResult::Succeeded
            && InPage.Get_Result() != ECk_ProceduralAction_ReadResult::Pending
            && InPage.Get_Result() != ECk_ProceduralAction_ReadResult::Gap))
    { _LastError = TEXT("Receipt page has an invalid identity or read result."); return false; }

    const auto HasGap = InPage.Get_Result() == ECk_ProceduralAction_ReadResult::Gap;
    if ((HasGap && (InPage.Get_FirstMissingSequence() <= 0
            || InPage.Get_LastMissingSequence() < InPage.Get_FirstMissingSequence()
            || InPage.Get_LastMissingSequence() > InPage.Get_LastStreamSequence()))
        || (NOT HasGap && (InPage.Get_FirstMissingSequence() != 0 || InPage.Get_LastMissingSequence() != 0)))
    {
        _LastError = TEXT("Receipt page has an invalid missing-record range.");
        return false;
    }

    auto NewCount = int32{0};
    auto NewPoseTransforms = int64{0};
    auto PreviousSequence = int64{0};
    for (const auto& Record : InPage.Get_Records())
    {
        const auto Sequence = Record.Get_StreamSequence();
        if (Record.Get_Identity().Get_Value() != _Identity.Get_Value() || Sequence <= PreviousSequence)
        { _LastError = TEXT("Receipt page has a wrong identity or unordered sequence."); return false; }
        if (HasGap && Sequence <= InPage.Get_LastMissingSequence())
        {
            _LastError = TEXT("Receipt page includes a record inside its missing-record range.");
            return false;
        }
        if (Sequence <= _LastCopiedSequence)
        {
            if (NOT _Records.ContainsByPredicate([Sequence](const auto& InExisting)
                { return InExisting.Get_StreamSequence() == Sequence; }))
            { _LastError = TEXT("Receipt page supplied a new record behind the copied cursor."); return false; }
        }
        else
        {
            ++NewCount;
            if (NOT ck_procedural_action_recorded_store::TryCountReceiptTransforms(
                    Record.Get_Receipt(),
                    _MaximumCopiedPoseTransforms - _CopiedPoseTransforms,
                    NewPoseTransforms))
            {
                _LastError = FString::Printf(
                    TEXT("Recording exceeded its %lld-pose-transform budget; recording stopped without discarding data."),
                    _MaximumCopiedPoseTransforms);
                return false;
            }
        }
        PreviousSequence = Sequence;
    }
    if (NewCount > MaximumCopiedRecords - _Records.Num())
    { _LastError = TEXT("Recording exceeded its 4096-record budget; recording stopped without discarding data."); return false; }

    if (HasGap && InPage.Get_LastMissingSequence() > _LastCopiedSequence)
    {
        // An empty gap page still advances the observation cursor. Previously copied records remain owned here.
        const auto FirstMissing = _LastCopiedSequence + 1;
        if (NOT _Gaps.IsEmpty() && _Gaps.Last().Get_Last() == FirstMissing - 1)
        { _Gaps.Last() = FGap{_Gaps.Last().Get_First(), InPage.Get_LastMissingSequence()}; }
        else
        { _Gaps.Add(FGap{FirstMissing, InPage.Get_LastMissingSequence()}); }
        _LastCopiedSequence = InPage.Get_LastMissingSequence();
    }

    for (const auto& Record : InPage.Get_Records())
    {
        const auto Sequence = Record.Get_StreamSequence();
        if (Sequence <= _LastCopiedSequence)
        { continue; }
        if (Sequence > _LastCopiedSequence + 1)
        { _Gaps.Add(FGap{_LastCopiedSequence + 1, Sequence - 1}); }
        _Records.Add(Record);
        _LastCopiedSequence = Sequence;
    }
    _CopiedPoseTransforms += NewPoseTransforms;
    _Terminal |= InPage.Get_Terminal();
    _LastError.Reset();
    return true;
}
