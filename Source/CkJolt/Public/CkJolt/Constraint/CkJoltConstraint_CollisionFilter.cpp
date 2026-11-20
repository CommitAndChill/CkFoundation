#include "CkJoltConstraint_CollisionFilter.h"

#include "CkCore/Macros/CkMacros.h"

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_constraint_collision_filter
{
    // Set while this thread is inside another filter's CanCollide on this filter's behalf: a filter that defers the same
    // way would otherwise hand the pair straight back. A pair neither filter owns collides.
    thread_local bool GDeferringToForeignFilter = false;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::jolt
{
    auto
        FCk_JoltConstraint_CollisionFilter::
        Request_DisablePair(
            JPH::PhysicsSystem& InPhysicsSystem,
            JPH::BodyID InBodyA,
            JPH::BodyID InBodyB)
        -> ECk_JoltConstraint_CollisionFilter_JoinResult
    {
        if (InBodyA.IsInvalid() || InBodyB.IsInvalid() || InBodyA == InBodyB)
        { return ECk_JoltConstraint_CollisionFilter_JoinResult::RefusedMissingBody; }

        const auto& LockInterface = InPhysicsSystem.GetBodyLockInterface();

        // Check both before touching either, so a refusal changes nothing.
        for (const auto& BodyId : {InBodyA, InBodyB})
        {
            const auto Lock = JPH::BodyLockRead{LockInterface, BodyId};
            if (NOT Lock.Succeeded())
            { return ECk_JoltConstraint_CollisionFilter_JoinResult::RefusedMissingBody; }

            const auto* Filter = Lock.GetBody().GetCollisionGroup().GetGroupFilter();
            if (Filter != nullptr && Filter != this)
            { return ECk_JoltConstraint_CollisionFilter_JoinResult::RefusedForeignFilter; }
        }

        for (const auto& BodyId : {InBodyA, InBodyB})
        {
            const auto Key = BodyId.GetIndexAndSequenceNumber();
            auto* Member = _Members.Find(Key);
            if (Member == nullptr)
            {
                auto Lock = JPH::BodyLockWrite{LockInterface, BodyId};
                auto& Body = Lock.GetBody();

                Member = &_Members.Add(Key, FMember{Body.GetCollisionGroup(), 0});
                Body.SetCollisionGroup(JPH::CollisionGroup{this, Member->_OriginalGroup.GetGroupID(), Key});
            }

            ++Member->_NumPairs;
        }

        ++_DisabledPairs.FindOrAdd(Make_PairKey(InBodyA.GetIndexAndSequenceNumber(), InBodyB.GetIndexAndSequenceNumber()));
        return ECk_JoltConstraint_CollisionFilter_JoinResult::Joined;
    }

    auto
        FCk_JoltConstraint_CollisionFilter::
        Request_EnablePair(
            JPH::PhysicsSystem& InPhysicsSystem,
            JPH::BodyID InBodyA,
            JPH::BodyID InBodyB)
        -> void
    {
        const auto KeyA = InBodyA.GetIndexAndSequenceNumber();
        const auto KeyB = InBodyB.GetIndexAndSequenceNumber();
        const auto PairKey = Make_PairKey(KeyA, KeyB);

        auto* Count = _DisabledPairs.Find(PairKey);
        if (Count == nullptr)
        { return; }

        if (--(*Count) <= 0)
        { _DisabledPairs.Remove(PairKey); }

        DoRelease(InPhysicsSystem, KeyA, 1);
        DoRelease(InPhysicsSystem, KeyB, 1);
    }

    auto
        FCk_JoltConstraint_CollisionFilter::
        Request_ForgetBody(
            JPH::PhysicsSystem& InPhysicsSystem,
            JPH::BodyID InBody)
        -> void
    {
        const auto Key = InBody.GetIndexAndSequenceNumber();
        if (NOT _Members.Contains(Key))
        { return; }

        auto Partners = TArray<TPair<uint32, int32>>{};
        for (auto It = _DisabledPairs.CreateIterator(); It; ++It)
        {
            const auto Low = static_cast<uint32>(It.Key() & 0xFFFFFFFFull);
            const auto High = static_cast<uint32>(It.Key() >> 32);
            if (Low != Key && High != Key)
            { continue; }

            Partners.Emplace(Low == Key ? High : Low, It.Value());
            It.RemoveCurrent();
        }

        for (const auto& Partner : Partners)
        { DoRelease(InPhysicsSystem, Partner.Key, Partner.Value); }

        const auto* Member = _Members.Find(Key);
        if (Member != nullptr)
        { DoRelease(InPhysicsSystem, Key, Member->_NumPairs); }
    }

    auto
        FCk_JoltConstraint_CollisionFilter::
        Get_IsPairDisabled(
            JPH::BodyID InBodyA,
            JPH::BodyID InBodyB) const
        -> bool
    {
        return _DisabledPairs.Contains(Make_PairKey(InBodyA.GetIndexAndSequenceNumber(), InBodyB.GetIndexAndSequenceNumber()));
    }

    auto
        FCk_JoltConstraint_CollisionFilter::
        Get_IsJoined(
            JPH::BodyID InBody) const
        -> bool
    {
        return _Members.Contains(InBody.GetIndexAndSequenceNumber());
    }

    auto
        FCk_JoltConstraint_CollisionFilter::
        CanCollide(
            const JPH::CollisionGroup& InGroupA,
            const JPH::CollisionGroup& InGroupB) const
        -> bool
    {
        using namespace ck_jolt_constraint_collision_filter;

        const auto* FilterA = InGroupA.GetGroupFilter();
        const auto* FilterB = InGroupB.GetGroupFilter();

        if (FilterA == this && FilterB == this)
        { return NOT _DisabledPairs.Contains(Make_PairKey(InGroupA.GetSubGroupID(), InGroupB.GetSubGroupID())); }

        // One side is another system's: its filter decides, asked with its own group first, as Jolt asked it before
        // this body joined.
        const auto& Foreign = FilterA == this ? InGroupB : InGroupA;
        const auto& Ours = FilterA == this ? InGroupA : InGroupB;
        const auto* ForeignFilter = Foreign.GetGroupFilter();
        if (ForeignFilter == nullptr || ForeignFilter == this || GDeferringToForeignFilter)
        { return true; }

        GDeferringToForeignFilter = true;
        const auto Verdict = ForeignFilter->CanCollide(Foreign, Ours);
        GDeferringToForeignFilter = false;
        return Verdict;
    }

    auto
        FCk_JoltConstraint_CollisionFilter::
        Make_PairKey(
            uint32 InA,
            uint32 InB)
        -> uint64
    {
        const auto Low = FMath::Min(InA, InB);
        const auto High = FMath::Max(InA, InB);
        return (static_cast<uint64>(High) << 32) | static_cast<uint64>(Low);
    }

    auto
        FCk_JoltConstraint_CollisionFilter::
        DoRelease(
            JPH::PhysicsSystem& InPhysicsSystem,
            uint32 InBodyKey,
            int32 InNumPairs)
        -> void
    {
        auto* Member = _Members.Find(InBodyKey);
        if (Member == nullptr)
        { return; }

        Member->_NumPairs -= InNumPairs;
        if (Member->_NumPairs > 0)
        { return; }

        const auto OriginalGroup = Member->_OriginalGroup;
        _Members.Remove(InBodyKey);

        const auto BodyId = JPH::BodyID{InBodyKey};

        // A body that was destroyed under its pairs has nothing to restore.
        {
            auto Lock = JPH::BodyLockWrite{InPhysicsSystem.GetBodyLockInterface(), BodyId};
            if (NOT Lock.Succeeded())
            { return; }

            auto& Body = Lock.GetBody();
            if (Body.GetCollisionGroup().GetGroupFilter() == this)
            { Body.SetCollisionGroup(OriginalGroup); }
        }

        // A sleeping body never re-tests its pairs: wake it so the restored collision takes effect.
        auto& BodyInterface = InPhysicsSystem.GetBodyInterface();
        if (BodyInterface.IsAdded(BodyId))
        { BodyInterface.ActivateBody(BodyId); }
    }

    // --------------------------------------------------------------------------------------------------------------------

    FCk_JoltConstraint_CollisionFilter_Context::
        FCk_JoltConstraint_CollisionFilter_Context()
        : Filter(new FCk_JoltConstraint_CollisionFilter{}) {}
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::jolt
{
    JPH_IMPLEMENT_RTTI_VIRTUAL(FCk_JoltConstraint_CollisionFilter)
    {
        JPH_ADD_BASE_CLASS(FCk_JoltConstraint_CollisionFilter, JPH::GroupFilter)
    }
}

// --------------------------------------------------------------------------------------------------------------------
