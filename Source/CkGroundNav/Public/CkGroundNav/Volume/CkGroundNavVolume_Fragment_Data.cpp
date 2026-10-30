#include "CkGroundNavVolume_Fragment_Data.h"

#include <UObject/StrongObjectPtr.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Request_GroundNavVolume_Build::
    Make_Submission() const
    -> ThisType
{
    auto Submission = *this;
    Submission._Admission = ECk_GroundNav_BuildAdmission::NotEvaluated;
    Submission._AttemptSequence = 0;
    Submission._HasResolved = false;
    return Submission;
}

auto
    FCk_Request_GroundNavVolume_Build::
    Set_BuildAdmission(
        ECk_GroundNav_BuildAdmission InAdmission,
        int64 InAttemptSequence) const
    -> void
{
    _Admission = InAdmission;
    _AttemptSequence = InAttemptSequence;
}

auto
    FCk_Request_GroundNavVolume_Build::
    TryFireCompletion(
        const FCk_Handle& InOwner,
        ECk_Request_OperationResult InResult) const
    -> void
{
    DoFireCompletion(InOwner, InResult, {});
}

auto
    FCk_Request_GroundNavVolume_Build::
    TryFireCompletion(
        const FCk_Handle& InOwner,
        ECk_Request_OperationResult InResult,
        ECk_GroundNav_BakeStatus InBakeStatus) const
    -> void
{
    DoFireCompletion(InOwner, InResult, TOptional<ECk_GroundNav_BakeStatus>{InBakeStatus});
}

auto
    FCk_Request_GroundNavVolume_Build::
    DoFireCompletion(
        const FCk_Handle& InOwner,
        ECk_Request_OperationResult InResult,
        TOptional<ECk_GroundNav_BakeStatus> InBakeStatus) const
    -> void
{
    if (_HasResolved)
    { return; }
    _HasResolved = true;

    const auto Outcome = FCk_GroundNavVolume_BuildOutcome{
        _RequestSequence, _AttemptSequence, _Admission, InResult,
        InBakeStatus.IsSet(), InBakeStatus.Get(ECk_GroundNav_BakeStatus::Completed)};
    const auto Volume = ck::StaticCast<FCk_Handle_GroundNavVolume>(InOwner);
    auto ResolvedDelegate = _OnBuildResolved;
    _OnBuildResolved.Unbind();

    // The generic callback can collect garbage or cancel the owner before typed delivery.
    const auto ResolvedTarget = TStrongObjectPtr<UObject>{ResolvedDelegate.GetUObject()};
    FCk_Request_Base::TryFireCompletion(InOwner, InResult);
    ResolvedDelegate.ExecuteIfBound(Volume, Outcome);
}
