#include "CkPersistenceInitialization.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include <Engine/World.h>
#include <UObject/Class.h>
#include <UObject/ObjectKey.h>

namespace ck::persistence::initialization_detail
{
    struct FSubscription
    {
        uint64 _Generation = 0;
        FInitializationChanged _Callback;
    };

    struct FEntry
    {
        FInitializationStatus _Status;
        TMap<FName, FSubscription> _Subscribers;
        TSet<FName> _Executing;
        uint64 _NextSubscription = 0;
    };

    struct FFragment_Initialization
    {
        TMap<FObjectKey, FEntry> _Entries;
    };

    struct FPendingNotification
    {
        FCk_Handle _Entity;
        TWeakObjectPtr<const UScriptStruct> _Type;
        FName _Subscriber;
    };

    struct FFragment_InitializationWorld
    {
        uint64 _Epoch = 0;
        FInitializationResolver _Resolver;
        int32 _NotificationDepth = 0;
        TArray<FPendingNotification> _Notifications;
    };

    TFunction<void(UWorld&, FCk_Handle&)> GResolverBootstrap;

    auto SameStatus(const FInitializationStatus& InLeft, const FInitializationStatus& InRight) -> bool
    {
        return InLeft.Get_Epoch() == InRight.Get_Epoch() &&
            InLeft.Get_Disposition() == InRight.Get_Disposition() && InLeft.Get_Failure() == InRight.Get_Failure() &&
            InLeft.Get_IsReleased() == InRight.Get_IsReleased();
    }

    auto FindEntry(const FCk_Handle& InEntity, const UScriptStruct* InType) -> const FEntry*
    {
        if (ck::Is_NOT_Valid(InEntity) || ck::Is_NOT_Valid(InType) ||
            NOT InEntity.Has<FFragment_Initialization>())
        { return nullptr; }

        return InEntity.Get<FFragment_Initialization>()._Entries.Find(FObjectKey{InType});
    }

    auto FindEntry(FCk_Handle& InEntity, const UScriptStruct* InType) -> FEntry*
    {
        if (ck::Is_NOT_Valid(InEntity) || ck::Is_NOT_Valid(InType) ||
            NOT InEntity.Has<FFragment_Initialization>())
        { return nullptr; }

        return InEntity.Get<FFragment_Initialization>()._Entries.Find(FObjectKey{InType});
    }

    auto FindTransient(const FCk_Handle& InEntity) -> FCk_Handle
    {
        auto Current = InEntity;
        auto Visited = TSet<FCk_Handle>{};
        while (ck::IsValid(Current) && NOT Visited.Contains(Current))
        {
            Visited.Add(Current);
            if (Current.Has<TWeakObjectPtr<UWorld>>())
            {
                if (auto* World = Current.Get<TWeakObjectPtr<UWorld>>().Get(); ck::IsValid(World))
                {
                    const auto WorldTransient = UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*World);
                    if (ck::IsValid(WorldTransient))
                    { return WorldTransient; }
                }
            }
            if (NOT Current.Has<ck::FFragment_LifetimeOwner>())
            { break; }

            Current = Current.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        }
        return UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InEntity);
    }

    auto Dispatch(FCk_Handle InEntity, const UScriptStruct* InType, FName InSubscriber) -> void
    {
        auto* Entry = FindEntry(InEntity, InType);
        if (Entry == nullptr)
        { return; }

        const auto Status = Entry->_Status;
        auto Subscriptions = TArray<TPair<FName, uint64>>{};
        for (const auto& Pair : Entry->_Subscribers)
        {
            if (InSubscriber.IsNone() || InSubscriber == Pair.Key)
            { Subscriptions.Emplace(Pair.Key, Pair.Value._Generation); }
        }
        for (const auto& Subscription : Subscriptions)
        {
            Entry = FindEntry(InEntity, InType);
            if (Entry == nullptr || NOT SameStatus(Entry->_Status, Status))
            { return; }

            const auto* Found = Entry->_Subscribers.Find(Subscription.Key);
            if (Found == nullptr || Found->_Generation != Subscription.Value || Entry->_Executing.Contains(Subscription.Key))
            { continue; }

            // A callback can remove its subscription, replace the map, or destroy the entity.
            const auto Callback = Found->_Callback;
            Entry->_Executing.Add(Subscription.Key);
            Callback(InEntity, Status);
            if (auto* After = FindEntry(InEntity, InType); After != nullptr)
            { After->_Executing.Remove(Subscription.Key); }
        }
    }

    auto Notify(FCk_Handle InEntity, const UScriptStruct* InType, FName InSubscriber = NAME_None) -> void
    {
        const auto* Entry = FindEntry(InEntity, InType);
        if (Entry == nullptr || Entry->_Subscribers.IsEmpty() ||
            (NOT InSubscriber.IsNone() && NOT Entry->_Subscribers.Contains(InSubscriber)))
        { return; }

        auto Transient = FindTransient(InEntity);
        if (ck::IsValid(Transient) && Transient.Has<FFragment_InitializationWorld>())
        {
            auto& World = Transient.Get<FFragment_InitializationWorld>();
            if (World._NotificationDepth > 0)
            {
                const auto SameTarget = [&](const FPendingNotification& InPending)
                    { return InPending._Entity == InEntity && InPending._Type.Get() == InType; };
                if (World._Notifications.ContainsByPredicate([&](const FPendingNotification& InPending)
                    { return SameTarget(InPending) && InPending._Subscriber.IsNone(); }))
                { return; }

                if (InSubscriber.IsNone())
                { World._Notifications.RemoveAll(SameTarget); }
                else if (World._Notifications.ContainsByPredicate([&](const FPendingNotification& InPending)
                    { return SameTarget(InPending) && InPending._Subscriber == InSubscriber; }))
                { return; }

                World._Notifications.Add(FPendingNotification{InEntity, InType, InSubscriber});
                return;
            }
        }
        Dispatch(InEntity, InType, InSubscriber);
    }

    auto SetStatus(FCk_Handle& InEntity, const UScriptStruct* InType, const FInitializationStatus& InStatus) -> void
    {
        auto& Entry = InEntity.AddOrGet<FFragment_Initialization>()._Entries.FindOrAdd(FObjectKey{InType});
        if (SameStatus(Entry._Status, InStatus))
        { return; }

        Entry._Status = InStatus;
        Notify(InEntity, InType);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto ck::persistence::GetWorldTransient(const FCk_Handle& InEntity) -> FCk_Handle
{
    return ck::IsValid(InEntity) ? initialization_detail::FindTransient(InEntity) : FCk_Handle{};
}

auto
    ck::persistence::Query(const FCk_Handle& InEntity, const UScriptStruct* InType)
    -> FInitializationStatus
{
    const auto* Entry = initialization_detail::FindEntry(InEntity, InType);
    return Entry == nullptr ? FInitializationStatus{} : Entry->_Status;
}

auto
    ck::persistence::TryDeclare(FCk_Handle& InEntity, const UScriptStruct* InType, uint64 InEpoch,
        EInitializationDisposition InDisposition)
    -> bool
{
    if (ck::Is_NOT_Valid(InEntity) || ck::Is_NOT_Valid(InType))
    { return false; }

    if (InDisposition != EInitializationDisposition::Fresh &&
        InDisposition != EInitializationDisposition::IdentityUnresolved &&
        InDisposition != EInitializationDisposition::BaselineExpected)
    { return false; }

    if (InEpoch == 0 && InDisposition != EInitializationDisposition::Fresh)
    { return false; }

    const auto Current = Query(InEntity, InType);
    if (Current.Get_Epoch() > InEpoch)
    { return false; }

    if (Current.Get_Epoch() == InEpoch && Current.Get_Disposition() != EInitializationDisposition::Undeclared)
    {
        if (Current.Get_Disposition() == InDisposition)
        { return true; }

        if (Current.Get_Disposition() != EInitializationDisposition::IdentityUnresolved)
        { return false; }
    }

    initialization_detail::SetStatus(InEntity, InType,
        {InEpoch, InDisposition, EInitializationFailure::None, InDisposition == EInitializationDisposition::Fresh});
    return true;
}

auto
    ck::persistence::TryAccept(FCk_Handle& InEntity, const UScriptStruct* InType, uint64 InEpoch)
    -> bool
{
    const auto Current = Query(InEntity, InType);
    if (InEpoch == 0 || Current.Get_Epoch() != InEpoch)
    { return false; }

    if (Current.Get_Disposition() == EInitializationDisposition::BaselineCommitted ||
        Current.Get_Disposition() == EInitializationDisposition::Released)
    { return true; }

    if (Current.Get_Disposition() != EInitializationDisposition::BaselineExpected)
    { return false; }

    initialization_detail::SetStatus(InEntity, InType,
        {InEpoch, EInitializationDisposition::BaselineCommitted, EInitializationFailure::None});
    return true;
}

auto
    ck::persistence::TryRelease(FCk_Handle& InEntity, const UScriptStruct* InType, uint64 InEpoch)
    -> bool
{
    const auto Current = Query(InEntity, InType);
    if (InEpoch == 0 || Current.Get_Epoch() != InEpoch)
    { return false; }

    switch (Current.Get_Disposition())
    {
        case EInitializationDisposition::Fresh:
        case EInitializationDisposition::Released:
        { return true; }
        case EInitializationDisposition::Unavailable:
        {
            initialization_detail::SetStatus(InEntity, InType,
                {InEpoch, EInitializationDisposition::Unavailable, Current.Get_Failure(), true});
            return true;
        }
        case EInitializationDisposition::BaselineCommitted:
        {
            initialization_detail::SetStatus(InEntity, InType,
                {InEpoch, EInitializationDisposition::Released, EInitializationFailure::None, true});
            return true;
        }
        case EInitializationDisposition::IdentityUnresolved:
        case EInitializationDisposition::BaselineExpected:
        {
            initialization_detail::SetStatus(InEntity, InType,
                {InEpoch, EInitializationDisposition::Unavailable, EInitializationFailure::MissingBaseline, true});
            return true;
        }
        default:
        { return false; }
    }
}

auto
    ck::persistence::TryDiscard(FCk_Handle& InEntity, const UScriptStruct* InType, uint64 InEpoch,
        EInitializationFailure InFailure)
    -> bool
{
    if (InFailure < EInitializationFailure::MissingBaseline || InFailure > EInitializationFailure::Dropped)
    { return false; }

    const auto Current = Query(InEntity, InType);
    if (InEpoch == 0 || Current.Get_Epoch() != InEpoch)
    { return false; }

    if (Current.Get_Disposition() == EInitializationDisposition::Unavailable)
    { return Current.Get_Failure() == InFailure; }

    if (Current.Get_Disposition() != EInitializationDisposition::IdentityUnresolved &&
        Current.Get_Disposition() != EInitializationDisposition::BaselineExpected &&
        Current.Get_Disposition() != EInitializationDisposition::BaselineCommitted)
    { return false; }

    initialization_detail::SetStatus(InEntity, InType, {InEpoch, EInitializationDisposition::Unavailable, InFailure});
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::persistence::Subscribe(FCk_Handle& InEntity, const UScriptStruct* InType, FName InSubscriber,
        FInitializationChanged InCallback)
    -> bool
{
    if (ck::Is_NOT_Valid(InEntity) || ck::Is_NOT_Valid(InType) || InSubscriber.IsNone() || NOT InCallback)
    { return false; }

    auto& Entry = InEntity.AddOrGet<initialization_detail::FFragment_Initialization>()._Entries.FindOrAdd(FObjectKey{InType});
    if (Entry._Subscribers.Contains(InSubscriber) || Entry._Executing.Contains(InSubscriber))
    { return false; }

    Entry._Subscribers.Add(InSubscriber, initialization_detail::FSubscription{++Entry._NextSubscription, MoveTemp(InCallback)});
    initialization_detail::Notify(InEntity, InType, InSubscriber);
    return true;
}

auto
    ck::persistence::Unsubscribe(FCk_Handle& InEntity, const UScriptStruct* InType, FName InSubscriber)
    -> bool
{
    auto* Entry = initialization_detail::FindEntry(InEntity, InType);
    return Entry != nullptr && Entry->_Subscribers.Remove(InSubscriber) > 0;
}

auto
    ck::persistence::InstallResolver(FCk_Handle& InWorldTransient, uint64 InEpoch,
        FInitializationResolver InResolver)
    -> bool
{
    if (ck::Is_NOT_Valid(InWorldTransient) || InEpoch == 0 || NOT InResolver)
    { return false; }

    using FWorld = initialization_detail::FFragment_InitializationWorld;
    if (InWorldTransient.Has<FWorld>() && InWorldTransient.Get<FWorld>()._Epoch >= InEpoch)
    { return false; }

    auto& World = InWorldTransient.AddOrGet<FWorld>();
    World._Epoch = InEpoch;
    World._Resolver = MoveTemp(InResolver);
    return true;
}

auto
    ck::persistence::RemoveResolver(FCk_Handle& InWorldTransient, uint64 InEpoch)
    -> bool
{
    using FWorld = initialization_detail::FFragment_InitializationWorld;
    if (ck::Is_NOT_Valid(InWorldTransient) || NOT InWorldTransient.Has<FWorld>())
    { return false; }

    auto& World = InWorldTransient.Get<FWorld>();
    if (InEpoch == 0 || World._Epoch != InEpoch || NOT World._Resolver)
    { return false; }

    World._Resolver = {};
    return true;
}

auto
    ck::persistence::SetResolverBootstrap(TFunction<void(UWorld&, FCk_Handle&)> InBootstrap)
    -> void
{
    initialization_detail::GResolverBootstrap = MoveTemp(InBootstrap);
}

auto
    ck::persistence::RequestInitialization(FCk_Handle& InEntity, const UScriptStruct* InType,
        const FInitializationIdentity& InIdentity)
    -> FInitializationStatus
{
    if (ck::Is_NOT_Valid(InEntity) || ck::Is_NOT_Valid(InType))
    { return {}; }

    using FWorld = initialization_detail::FFragment_InitializationWorld;
    auto Transient = initialization_detail::FindTransient(InEntity);
    if (ck::IsValid(Transient))
    {
        if ((NOT Transient.Has<FWorld>() || NOT Transient.Get<FWorld>()._Resolver) &&
            initialization_detail::GResolverBootstrap && Transient.Has<TWeakObjectPtr<UWorld>>())
        {
            if (auto* World = Transient.Get<TWeakObjectPtr<UWorld>>().Get(); ck::IsValid(World))
            {
                const auto Bootstrap = initialization_detail::GResolverBootstrap;
                Bootstrap(*World, Transient);
            }
        }
        if (ck::IsValid(Transient) && Transient.Has<FWorld>() && Transient.Get<FWorld>()._Resolver)
        {
            const auto Resolve = Transient.Get<FWorld>()._Resolver;
            Resolve(InEntity, InType, InIdentity);
            return Query(InEntity, InType);
        }
    }

    if (Query(InEntity, InType).Get_Disposition() == EInitializationDisposition::Undeclared)
    { TryDeclare(InEntity, InType, 0, EInitializationDisposition::Fresh); }

    return Query(InEntity, InType);
}

// --------------------------------------------------------------------------------------------------------------------

ck::persistence::FScopedInitializationNotifications::FScopedInitializationNotifications(
    const FCk_Handle& InWorldTransient)
    : _Transient(initialization_detail::FindTransient(InWorldTransient))
{
    if (ck::IsValid(_Transient))
    { ++_Transient.AddOrGet<initialization_detail::FFragment_InitializationWorld>()._NotificationDepth; }
}

ck::persistence::FScopedInitializationNotifications::~FScopedInitializationNotifications()
{
    using FWorld = initialization_detail::FFragment_InitializationWorld;
    if (ck::Is_NOT_Valid(_Transient) || NOT _Transient.Has<FWorld>())
    { return; }

    auto& World = _Transient.Get<FWorld>();
    if (--World._NotificationDepth != 0)
    { return; }

    const auto Pending = MoveTemp(World._Notifications);
    World._Notifications.Reset();
    for (const auto& Notification : Pending)
    {
        if (const auto* Type = Notification._Type.Get(); ck::IsValid(Type))
        { initialization_detail::Notify(Notification._Entity, Type, Notification._Subscriber); }
    }
}
