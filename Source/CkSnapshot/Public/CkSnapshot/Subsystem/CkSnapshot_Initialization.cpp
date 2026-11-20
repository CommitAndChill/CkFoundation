#include "CkSnapshot/Subsystem/CkSnapshot_Subsystem.h"

#include "CkCore/Format/CkFormat.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"
#include "CkEcs/Snapshot/CkSaveKey_Fragment.h"
#include "CkLabel/CkLabel_Utils.h"

#include <GameFramework/Actor.h>
#include <Misc/PackageName.h>
#include <ProfilingDebugging/CpuProfilerTrace.h>
#include <UObject/SoftObjectPath.h>

auto
    UCk_Snapshot_Subsystem_UE::
    DoPrepare_InitializationManifest()
    -> void
{
    DoClear_InitializationManifest();
    ++_InitializationEpoch;
    _InitializationReleased = false;
    _InitializationFinalized = false;

    for (const auto& Payload : _V3Tables.Get_Payloads())
    {
        const auto& Source = Payload.Get_TypePath();
        if (NOT _InitializationCanonicalPaths.Contains(Source))
        {
            auto Canonical = Source;
            if (FPackageName::IsValidObjectPath(Source))
            {
                auto Path = FSoftObjectPath{Source};
                if (Path.FixupCoreRedirects())
                { Canonical = Path.ToString(); }
            }
            _InitializationCanonicalPaths.Add(Source, MoveTemp(Canonical));
        }
        ++_InitializationPayloads.FindOrAdd(Payload.Get_OwnerSavedId()).FindOrAdd(DoGet_InitializationTypePath(Source));
    }

    for (const auto& Entry : _V3Tables.Get_Entities())
    {
        const auto Row = Entry.Get_SavedId();
        const auto Origin = Entry.Get_Provenance();
        if (Origin == ECk_Snapshot_V3_Provenance::ConstructSpawned)
        {
            _InitializationChildOrder.Add(Row, _InitializationChildOrder.Num());
            _InitializationChildRows.FindOrAdd(Entry.Get_LifetimeOwnerSavedId())
                .FindOrAdd(Entry.Get_Label()).Add(Row);
        }
        else if (Origin == ECk_Snapshot_V3_Provenance::EngineOwned ||
            (Origin == ECk_Snapshot_V3_Provenance::RuntimeSpawned && NOT Entry.Get_ActorClassPath().IsEmpty()))
        {
            if (Entry.Get_SaveKey().IsValid())
            { _InitializationKeyRows.FindOrAdd(Entry.Get_SaveKey()).Add(Row); }
            else if (Origin == ECk_Snapshot_V3_Provenance::EngineOwned)
            { _InitializationPlayerRows.FindOrAdd(Entry.Get_PlayerId(), Row); }
        }
    }
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoGet_InitializationTypePath(const FString& InPath) const
    -> const FString&
{
    const auto* Found = _InitializationCanonicalPaths.Find(InPath);
    return Found != nullptr ? *Found : InPath;
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoClear_InitializationManifest()
    -> void
{
    _InitializationTransient = FCk_Handle{};
    _InitializationRowsByEntity.Reset();
    _InitializationLoaderRows.Reset();
    _InitializationFresh.Reset();
    _InitializationRequests.Reset();
    _InitializationWaitingOnOwner.Reset();
    _InitializationCanonicalPaths.Reset();
    _InitializationPayloads.Reset();
    _InitializationKeyRows.Reset();
    _InitializationPlayerRows.Reset();
    _InitializationChildRows.Reset();
    _InitializationChildOrder.Reset();
}

auto
    UCk_Snapshot_Subsystem_UE::
    TryInstall_InitializationResolver(UWorld& InWorld, FCk_Handle& InTransient)
    -> void
{
    if (NOT _LoadInProgress || _InitializationReleased || NOT DoIs_WorldOwnedByThisLoad(InWorld))
    { return; }

    const auto Epoch = _InitializationEpoch;
    const auto WeakThis = TWeakObjectPtr<UCk_Snapshot_Subsystem_UE>{this};
    if (ck::persistence::InstallResolver(InTransient, Epoch,
        [WeakThis, Epoch](FCk_Handle& InEntity, const UScriptStruct* InType, const FInitializationIdentity& InIdentity)
        {
            if (auto* Self = WeakThis.Get(); Self != nullptr && Self->_InitializationEpoch == Epoch)
            { Self->DoResolve_Initialization(InEntity, InType, InIdentity); }
        }))
    { _InitializationTransient = InTransient; }
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoBind_InitializationRow(uint32 InRow, const FCk_Handle& InEntity)
    -> void
{
    // Engine-owned channel groups intentionally map N saved rows onto one live channel.
    _InitializationRowsByEntity.FindOrAdd(InEntity).AddUnique(InRow);
    DoResolve_InitializationChildren(InEntity);
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoGet_InitializationOccurrence(const FCk_Handle& InEntity, const FCk_Handle& InOwner, const FString& InLabel) const
    -> int32
{
    if (ck::Is_NOT_Valid(InOwner) || NOT InOwner.Has<ck::FFragment_LifetimeDependents>())
    { return INDEX_NONE; }
    int32 Occurrence = 0;
    for (const auto& Sibling : InOwner.Get<ck::FFragment_LifetimeDependents>().Get_Entities())
    {
        if (Sibling == InEntity)
        { return Occurrence; }
        if (ck::IsValid(Sibling) && NOT _InitializationFresh.Contains(Sibling) &&
            Sibling.Has<ck::FTag_ConstructSpawned>() && UCk_Utils_GameplayLabel_UE::Has(Sibling) &&
            UCk_Utils_GameplayLabel_UE::Get_Label(Sibling).ToString() == InLabel)
        { ++Occurrence; }
    }
    return INDEX_NONE;
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoFind_InitializationIdentity(const FCk_Handle& InEntity, FInitializationIdentity& InOutIdentity) const
    -> bool
{
    using EKind = ck::persistence::EInitializationIdentity;
    if (InOutIdentity._Kind != EKind::Automatic || _InitializationLoaderRows.Contains(InEntity) ||
        _InitializationRowsByEntity.Contains(InEntity))
    { return true; }

    if (InEntity.Has<FFragment_SaveKey>())
    {
        InOutIdentity._Kind = EKind::SaveKey;
        InOutIdentity._SaveKey = InEntity.Get<FFragment_SaveKey>().Get_Key();
        return true;
    }
    if (InEntity.Has_All<ck::FTag_ConstructSpawned, ck::FFragment_LifetimeOwner>() &&
        UCk_Utils_GameplayLabel_UE::Has(InEntity) && NOT UCk_Utils_GameplayLabel_UE::Get_IsUnnamedLabel(InEntity))
    {
        InOutIdentity._Owner = InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        InOutIdentity._Label = UCk_Utils_GameplayLabel_UE::Get_Label(InEntity).ToString();
        InOutIdentity._Occurrence = DoGet_InitializationOccurrence(InEntity, InOutIdentity._Owner, InOutIdentity._Label);
        if (InOutIdentity._Occurrence != INDEX_NONE)
        { InOutIdentity._Kind = EKind::OwnedLabel; return true; }
    }
    return DoFind_InitializationPlayerRow(InEntity).IsSet();
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoGet_InitializationRows(const FCk_Handle& InEntity, const FInitializationIdentity& InIdentity,
        TArray<uint32>& OutCandidates) const
    -> bool
{
    using EKind = ck::persistence::EInitializationIdentity;
    const auto AppendKey = [this](const FGuid& InKey, TArray<uint32>& OutRows)
    {
        if (const auto* Rows = _InitializationKeyRows.Find(InKey))
        {
            for (const auto Row : *Rows)
            { OutRows.AddUnique(Row); }
        }
    };

    switch (InIdentity._Kind)
    {
        case EKind::Fresh:
        { return true; }
        case EKind::SaveKey:
        {
            AppendKey(InIdentity._SaveKey, OutCandidates);
            if (InEntity.Has<FFragment_SaveKey>())
            {
                const auto& Actual = InEntity.Get<FFragment_SaveKey>();
                if (InIdentity._SaveKey == Actual.Get_Key() || Actual.Get_Aliases().Contains(InIdentity._SaveKey))
                {
                    AppendKey(Actual.Get_Key(), OutCandidates);
                    for (const auto& Alias : Actual.Get_Aliases())
                    { AppendKey(Alias, OutCandidates); }
                }
            }
            return true;
        }
        case EKind::Player:
        {
            if (const auto* Row = _InitializationPlayerRows.Find(InIdentity._PlayerId))
            { OutCandidates.Add(*Row); }
            return true;
        }
        case EKind::OwnedLabel:
        {
            const auto Owner = InIdentity._Owner;
            if (ck::Is_NOT_Valid(Owner))
            { return false; }
            if (_InitializationFresh.Contains(Owner))
            { return true; }
            TArray<uint32> Owners;
            if (const auto* Mapped = _InitializationRowsByEntity.Find(Owner))
            { Owners = *Mapped; }
            if (const auto* LoaderRow = _InitializationLoaderRows.Find(Owner))
            { Owners.AddUnique(*LoaderRow); }
            if (Owner.Has<FFragment_SaveKey>() && Owner.Get<FFragment_SaveKey>().Get_Key().IsValid())
            {
                const auto& Key = Owner.Get<FFragment_SaveKey>();
                AppendKey(Key.Get_Key(), Owners);
                for (const auto& Alias : Key.Get_Aliases())
                { AppendKey(Alias, Owners); }
            }
            else if (Owners.IsEmpty())
            { return false; }

            TArray<uint32> Siblings;
            for (const auto OwnerRow : Owners)
            {
                if (const auto* Labels = _InitializationChildRows.Find(OwnerRow))
                {
                    if (const auto* Rows = Labels->Find(InIdentity._Label))
                    { Siblings.Append(*Rows); }
                }
            }
            if (Owners.Num() > 1)
            {
                Siblings.Sort([this](uint32 InLeft, uint32 InRight)
                { return _InitializationChildOrder.FindChecked(InLeft) < _InitializationChildOrder.FindChecked(InRight); });
            }
            if (Siblings.IsValidIndex(InIdentity._Occurrence))
            { OutCandidates.Add(Siblings[InIdentity._Occurrence]); }
            return true;
        }
        case EKind::Automatic:
        {
            if (const auto* Mapped = _InitializationRowsByEntity.Find(InEntity))
            { OutCandidates = *Mapped; return true; }
            if (const auto Player = DoFind_InitializationPlayerRow(InEntity); Player.IsSet())
            { OutCandidates.Add(Player.GetValue()); return true; }
            return false;
        }
        default:
        { return false; }
    }
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoIs_InitializationIdentityPresent(const FCk_Handle& InEntity, const FInitializationIdentity& InIdentity) const
    -> bool
{
    using EKind = ck::persistence::EInitializationIdentity;
    switch (InIdentity._Kind)
    {
        case EKind::Fresh:
        { return true; }
        case EKind::SaveKey:
        {
            if (NOT InEntity.Has<FFragment_SaveKey>())
            { return false; }
            const auto& Actual = InEntity.Get<FFragment_SaveKey>();
            return InIdentity._SaveKey == Actual.Get_Key() || Actual.Get_Aliases().Contains(InIdentity._SaveKey);
        }
        case EKind::Player:
        { return DoIs_InitializationPlayer(InEntity, InIdentity._PlayerId); }
        case EKind::OwnedLabel:
        {
            if (NOT InEntity.Has_All<ck::FTag_ConstructSpawned, ck::FFragment_LifetimeOwner>() ||
                InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity() != InIdentity._Owner ||
                NOT UCk_Utils_GameplayLabel_UE::Has(InEntity) || UCk_Utils_GameplayLabel_UE::Get_IsUnnamedLabel(InEntity))
            { return false; }
            return UCk_Utils_GameplayLabel_UE::Get_Label(InEntity).ToString() == InIdentity._Label &&
                DoGet_InitializationOccurrence(InEntity, InIdentity._Owner, InIdentity._Label) == InIdentity._Occurrence;
        }
        default:
        { return false; }
    }
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoFind_PendingBridgeRow(const AActor& InActor) const
    -> const uint32*
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkSnapshot_FindPendingBridgeRow);
    return _PendingBridgeRowsByActor.Find(&InActor);
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoResolve_Initialization(FCk_Handle& InEntity, const UScriptStruct* InType,
        const FInitializationIdentity& InIdentity)
    -> void
{
    using namespace ck::persistence;
    if (_InitializationReleased || ck::Is_NOT_Valid(InEntity) || InType == nullptr)
    { return; }

    auto Identity = InIdentity;
    const auto Current = Query(InEntity, InType);
    if (Current.Get_Epoch() == _InitializationEpoch &&
        Current.Get_Disposition() != EInitializationDisposition::Undeclared &&
        Current.Get_Disposition() != EInitializationDisposition::IdentityUnresolved)
    { return; }

    // The bridge handle may appear after FinishSpawning has entered BeginPlay. Only the actor's exact
    // linked entity can consume its loader row. Other subjects still need their own declaration.
    bool AwaitingBridge = false;
    if (NOT _InitializationLoaderRows.Contains(InEntity) && NOT _PendingBridgeRowsByActor.IsEmpty())
    {
        if (const auto* Actor = UCk_Utils_EntityBridge_UE::TryGet_Actor(InEntity))
        {
            if (const auto* SavedId = DoFind_PendingBridgeRow(*Actor))
            {
                const auto Linked = UCk_Utils_EntityBridge_UE::TryGet_Entity(Actor);
                if (Linked == InEntity)
                { _InitializationLoaderRows.Add(InEntity, *SavedId); }
                else if (ck::Is_NOT_Valid(Linked))
                { AwaitingBridge = true; }
            }
        }
    }

    if (NOT AwaitingBridge && NOT _InitializationFresh.Contains(InEntity) && NOT DoFind_InitializationIdentity(InEntity, Identity))
    { return; }

    const auto Exact = _InitializationLoaderRows.Contains(InEntity);
    if (Exact || AwaitingBridge)
    { Identity = {}; } // replayed spawn parameters describe an earlier construction
    if (_InitializationFresh.Contains(InEntity))
    { Identity._Kind = EInitializationIdentity::Fresh; }

    _InitializationRequests.FindOrAdd(InEntity).Add(InType, Identity);
    if (_InitializationFinalized)
    { DoConfirm_Initialization(InEntity, InType, Identity); return; }

    if (Exact)
    { TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::BaselineExpected); return; }

    if ((Identity._Kind == EInitializationIdentity::SaveKey && NOT Identity._SaveKey.IsValid()) ||
        (Identity._Kind == EInitializationIdentity::OwnedLabel && (ck::Is_NOT_Valid(Identity._Owner) ||
            NOT InEntity.Has<ck::FFragment_LifetimeOwner>() || InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity() != Identity._Owner ||
            Identity._Label.IsEmpty() || Identity._Occurrence < 0)) ||
        Identity._Kind > EInitializationIdentity::Player)
    {
        TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::IdentityUnresolved);
        TryDiscard(InEntity, InType, _InitializationEpoch, EInitializationFailure::Rejected);
        return;
    }

    TArray<uint32> Candidates;
    const auto IdentityKnown = DoGet_InitializationRows(InEntity, Identity, Candidates);
    if (NOT AwaitingBridge && IdentityKnown && Candidates.IsEmpty() && NOT _InitializationRowsByEntity.Contains(InEntity) &&
        DoIs_InitializationIdentityPresent(InEntity, Identity))
    {
        _InitializationFresh.Add(InEntity);
        TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::Fresh);
        DoResolve_InitializationChildren(InEntity);
        return;
    }
    if (Identity._Kind == EInitializationIdentity::OwnedLabel && (NOT IdentityKnown || Candidates.IsEmpty()))
    { _InitializationWaitingOnOwner.AddUnique(Identity._Owner, InEntity); }

    TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::IdentityUnresolved);
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoConfirm_Initialization(FCk_Handle InEntity, const UScriptStruct* InType,
        const FInitializationIdentity& InIdentity)
    -> void
{
    using namespace ck::persistence;
    if (ck::Is_NOT_Valid(InEntity) || InType == nullptr)
    { return; }
    const auto Status = Query(InEntity, InType);
    if (Status.Get_Epoch() == _InitializationEpoch &&
        (Status.Get_Disposition() == EInitializationDisposition::Unavailable ||
            Status.Get_Disposition() == EInitializationDisposition::Fresh))
    { return; }

    // Rebuilding has finished. Every external claim now needs its original, physical rendezvous. The
    // declared key/label/player may have been supplied before construction published that identity.
    const auto* Mapped = _InitializationRowsByEntity.Find(InEntity);
    TArray<uint32> Candidates;
    const auto Known = DoGet_InitializationRows(InEntity, InIdentity, Candidates);
    if (Mapped == nullptr)
    {
        const auto IsUnselectedPoolMember = InEntity.Has<FFragment_SaveKey>() &&
            InEntity.Get<FFragment_SaveKey>().Get_IsSharedRendezvousGroup() &&
            InIdentity._Kind == EInitializationIdentity::SaveKey &&
            InIdentity._SaveKey == InEntity.Get<FFragment_SaveKey>().Get_Key();
        if ((Known && Candidates.IsEmpty() && DoIs_InitializationIdentityPresent(InEntity, InIdentity)) || IsUnselectedPoolMember)
        { TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::Fresh); }
        else
        {
            TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::IdentityUnresolved);
            TryDiscard(InEntity, InType, _InitializationEpoch, EInitializationFailure::Rejected);
        }
        return;
    }

    const auto Path = InType->GetPathName();
    TArray<uint32> Producers;
    for (const auto Row : *Mapped)
    {
        if (const auto* Types = _InitializationPayloads.Find(Row))
        {
            for (int32 Index = 0, Count = Types->FindRef(Path); Index < Count; ++Index)
            { Producers.Add(Row); }
        }
    }
    if (Producers.Num() > 1)
    {
        Producers.Sort();
        FCk_Snapshot_InitializationConflict Conflict;
        Conflict.Set_PayloadType(Path).Set_OwnerIdentity(ck::Format_UE(TEXT("{}"), InEntity)).Set_SavedRows(Producers);
        _V3LoadReport.Get_InitializationConflicts().Add(MoveTemp(Conflict));
        TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::IdentityUnresolved);
        TryDiscard(InEntity, InType, _InitializationEpoch, EInitializationFailure::Rejected);
        return;
    }

    const auto IdentityMatches = _InitializationLoaderRows.Contains(InEntity) ||
        Mapped->ContainsByPredicate([&Candidates](uint32 Row) { return Candidates.Contains(Row); });
    if (NOT IdentityMatches)
    {
        TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::IdentityUnresolved);
        TryDiscard(InEntity, InType, _InitializationEpoch, EInitializationFailure::Rejected);
        return;
    }

    TryDeclare(InEntity, InType, _InitializationEpoch, EInitializationDisposition::BaselineExpected);
    if (Producers.IsEmpty())
    { TryDiscard(InEntity, InType, _InitializationEpoch, EInitializationFailure::MissingBaseline); }
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoResolve_InitializationChildren(const FCk_Handle& InOwner)
    -> void
{
    TArray<FCk_Handle> Children;
    _InitializationWaitingOnOwner.MultiFind(InOwner, Children);
    _InitializationWaitingOnOwner.Remove(InOwner);
    for (auto Child : Children)
    {
        if (ck::Is_NOT_Valid(Child))
        { continue; }
        if (const auto* Requests = _InitializationRequests.Find(Child))
        {
            // A disposition change can run user code which composes another feature on this entity.
            const auto Interests = *Requests;
            for (const auto& Pair : Interests)
            {
                if (const auto* Type = Pair.Key.Get())
                { DoResolve_Initialization(Child, Type, Pair.Value); }
            }
        }
    }
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoFinalize_Initialization()
    -> void
{
    _InitializationFinalized = true;
    if (ck::Is_NOT_Valid(_InitializationTransient))
    { return; }
    const auto Batch = ck::persistence::FScopedInitializationNotifications{_InitializationTransient};
    for (const auto& Pair : _InitializationRequests)
    {
        for (const auto& Request : Pair.Value)
        {
            if (const auto* Type = Request.Key.Get())
            { DoConfirm_Initialization(Pair.Key, Type, Request.Value); }
        }
    }
}

auto
    UCk_Snapshot_Subsystem_UE::
    DoRelease_Initialization(ck::persistence::EInitializationFailure InPendingFailure)
    -> void
{
    using namespace ck::persistence;
    if (_InitializationReleased || _InitializationEpoch == 0)
    { return; }

    _InitializationReleased = true;
    if (ck::Is_NOT_Valid(_InitializationTransient))
    { DoClear_InitializationManifest(); return; }

    const auto Batch = FScopedInitializationNotifications{_InitializationTransient};
    RemoveResolver(_InitializationTransient, _InitializationEpoch);
    for (const auto& Pair : _InitializationRequests)
    {
        auto Entity = Pair.Key;
        if (ck::Is_NOT_Valid(Entity))
        { continue; }
        for (const auto& Request : Pair.Value)
        {
            if (const auto* Type = Request.Key.Get())
            {
                const auto Disposition = Query(Entity, Type).Get_Disposition();
                if (InPendingFailure != EInitializationFailure::None &&
                    (Disposition == EInitializationDisposition::IdentityUnresolved || Disposition == EInitializationDisposition::BaselineExpected))
                { TryDiscard(Entity, Type, _InitializationEpoch, InPendingFailure); }
                TryRelease(Entity, Type, _InitializationEpoch);
            }
        }
    }
    DoClear_InitializationManifest();
}
