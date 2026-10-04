#include "CkOnlineDirect_ReadyLobbyBinding.h"

#include "CkOnline/Direct/CkOnlineDirect_Transport.h"

auto
    FCk_OnlineDirect_ReadyLobbyBinding::
    TryCapture(
        const FCk_OnlineDirect_Snapshot& InSnapshot,
        bool InHosting,
        int32 InExpectedMembers,
        FCk_OnlineDirect_ReadyLobbyBinding& OutBinding)
    -> bool
{
    OutBinding = {};
    if (InExpectedMembers < 1 || InExpectedMembers > 8)
    { return false; }
    const auto ExpectedState = InHosting ? ECk_OnlineDirect_State::Hosting : ECk_OnlineDirect_State::Joined;
    if (InSnapshot.Get_State() != ExpectedState || NOT InSnapshot.Get_ServiceGeneration().IsValid() ||
        NOT InSnapshot.Get_LobbyEpoch().IsValid() || NOT InSnapshot.Get_LocalMemberId().IsValid() ||
        InSnapshot.Get_Members().Num() != InExpectedMembers ||
        NOT ck::direct::FTransport::ValidateHostAddress(InSnapshot.Get_HostAddress()))
    { return false; }

    auto Candidate = FCk_OnlineDirect_ReadyLobbyBinding{};
    Candidate._Generation = InSnapshot.Get_ServiceGeneration();
    Candidate._Epoch = InSnapshot.Get_LobbyEpoch();
    Candidate._LocalMemberId = InSnapshot.Get_LocalMemberId();
    Candidate._HostAddress = InSnapshot.Get_HostAddress();
    Candidate._Hosting = InHosting;
    for (const auto& Member : InSnapshot.Get_Members())
    {
        if (NOT Member.Get_Id().IsValid() || NOT Member.Get_Ready() || Candidate._Members.Contains(Member.Get_Id()))
        { return false; }
        Candidate._Members.Add(Member.Get_Id());
    }
    if (NOT Candidate._Members.Contains(Candidate._LocalMemberId))
    { return false; }
    Candidate._Members.Sort([](const FGuid& InA, const FGuid& InB)
    { return InA.ToString(EGuidFormats::Digits) < InB.ToString(EGuidFormats::Digits); });
    OutBinding = MoveTemp(Candidate);
    return true;
}

auto
    FCk_OnlineDirect_ReadyLobbyBinding::
    Get_IsValid() const
    -> bool
{
    if (NOT _Generation.IsValid() || NOT _Epoch.IsValid() || NOT _LocalMemberId.IsValid() ||
        _Members.Num() < 1 || _Members.Num() > 8 ||
        NOT ck::direct::FTransport::ValidateHostAddress(_HostAddress))
    { return false; }
    auto Previous = FString{};
    for (const auto& Member : _Members)
    {
        if (NOT Member.IsValid())
        { return false; }
        const auto Current = Member.ToString(EGuidFormats::Digits);
        if (NOT Previous.IsEmpty() && NOT (Previous < Current))
        { return false; }
        Previous = Current;
    }
    return _Members.Contains(_LocalMemberId);
}

auto
    FCk_OnlineDirect_ReadyLobbyBinding::
    Matches(
        const FCk_OnlineDirect_Snapshot& InSnapshot) const
    -> bool
{
    if (NOT Get_IsValid())
    { return false; }
    auto Current = FCk_OnlineDirect_ReadyLobbyBinding{};
    return TryCapture(InSnapshot, _Hosting, _Members.Num(), Current) &&
        _Generation == Current._Generation && _Epoch == Current._Epoch &&
        _LocalMemberId == Current._LocalMemberId && _HostAddress == Current._HostAddress &&
        _Members == Current._Members;
}
