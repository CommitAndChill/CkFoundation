#pragma once

#include "CkCore/Macros/CkMacros.h"

#include <CoreMinimal.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Collision/GroupFilter.h>

// --------------------------------------------------------------------------------------------------------------------

// ReSharper disable once CppInconsistentNaming
namespace JPH
{
    class PhysicsSystem;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::jolt
{
    using JPH::RTTI;

    enum class ECk_JoltConstraint_CollisionFilter_JoinResult : uint8
    {
        Joined,
        // A body already carries another system's collision group filter: nothing changed, the pair keeps colliding.
        RefusedForeignFilter,
        // A body could not be locked (not created, or already destroyed): nothing changed.
        RefusedMissingBody
    };

    /**
     * The collision group filter of the bodies a constraint with ConnectedBodiesCollide = Disable joins. One per physics
     * world. A joined body carries this filter with its own body id (index + sequence) as sub-group id, and keeps its
     * original group id; the filter holds the disabled pairs, counted, so two such constraints between one pair both have
     * to go before the pair collides again. A body left with no pair gets its original group back and is woken.
     *
     * FIRST-FILTER RULE: JPH::CollisionGroup::CanCollide asks only the first non-null filter of a pair
     * (Jolt/Physics/Collision/CollisionGroup.h:71-80). Asked about a joined body and a body carrying another filter, this
     * filter returns that filter's verdict, called with that filter's own group first: the call Jolt made before the body
     * joined. A body that already carries another filter never joins (Request_DisablePair refuses), so this filter never
     * silences another system's filtering and never has its own pairs decided by someone else.
     *
     * THREAD CONTRACT: the Request_* calls are game-thread only and only while no physics step is in flight (every caller
     * waits a pending async step first). CanCollide runs on Jolt workers, inside PhysicsSystem::Update only
     * (Body::sFindCollidingPairsCanCollide, the CCD pass), so its reads never overlap a write.
     */
    class CKJOLT_API FCk_JoltConstraint_CollisionFilter final : public JPH::GroupFilter
    {
    public:
        JPH_DECLARE_RTTI_VIRTUAL(CKJOLT_API, FCk_JoltConstraint_CollisionFilter)

    public:
        // Both bodies join (when they have not already) and the pair stops colliding. A refusal changes nothing.
        auto
        Request_DisablePair(
            JPH::PhysicsSystem& InPhysicsSystem,
            JPH::BodyID InBodyA,
            JPH::BodyID InBodyB) -> ECk_JoltConstraint_CollisionFilter_JoinResult;

        // Undoes one Request_DisablePair of this pair; a pair never disabled is a no-op.
        auto
        Request_EnablePair(
            JPH::PhysicsSystem& InPhysicsSystem,
            JPH::BodyID InBodyA,
            JPH::BodyID InBodyB) -> void;

        // The body is about to be destroyed: every pair it is in goes, and a partner left with none goes back.
        auto
        Request_ForgetBody(
            JPH::PhysicsSystem& InPhysicsSystem,
            JPH::BodyID InBody) -> void;

        auto
        Get_IsPairDisabled(
            JPH::BodyID InBodyA,
            JPH::BodyID InBodyB) const -> bool;

        auto
        Get_IsJoined(
            JPH::BodyID InBody) const -> bool;

        auto
        CanCollide(
            const JPH::CollisionGroup& InGroupA,
            const JPH::CollisionGroup& InGroupB) const -> bool override;

    private:
        struct FMember
        {
            JPH::CollisionGroup _OriginalGroup;
            int32 _NumPairs = 0;
        };

        static auto
        Make_PairKey(
            uint32 InA,
            uint32 InB) -> uint64;

        auto
        DoRelease(
            JPH::PhysicsSystem& InPhysicsSystem,
            uint32 InBodyKey,
            int32 InNumPairs) -> void;

    private:
        TMap<uint32, FMember> _Members;
        TMap<uint64, int32> _DisabledPairs;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Registry context owner for the one per-world filter. A joined body's collision group holds its own reference.
    struct CKJOLT_API FCk_JoltConstraint_CollisionFilter_Context
    {
        JPH::Ref<FCk_JoltConstraint_CollisionFilter> Filter;

        FCk_JoltConstraint_CollisionFilter_Context();
    };
}

// --------------------------------------------------------------------------------------------------------------------
