#pragma once

#include "CkProcessorTraits.inl.h"

// --------------------------------------------------------------------------------------------------------------------
// Group processors define the top-level ordering of the ECS pipeline; a processor joins one with
// `using Group = FGroup_X;`. The RunAfter chain below IS the order — all TG_PrePhysics except
// FGroup_Overlap (TG_PostPhysics) and FGroup_Transform_LatePose (TG_PostUpdateWork), each its own scheduler
// partition. Group membership rosters and the tick-by-tick destruction timeline: CkEcs/CLAUDE.md.
// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    struct FGroup_DestructionPipeline;
    struct FGroup_Gameplay_TimeDelta;
    struct FGroup_Gameplay;
    struct FGroup_Gameplay_AI;
    struct FGroup_Gameplay_Audio;
    struct FGroup_Gameplay_Rendering;
    struct FGroup_Gameplay_Script;
    struct FGroup_Gameplay_Chaos;
    struct FGroup_Physics;
    struct FGroup_Transform_SyncFrom;
    struct FGroup_Transform;
    struct FGroup_Transform_Finalize;
    struct FGroup_Gameplay_Camera;
    struct FGroup_PostTransform;
    struct FGroup_DeferredApply;
    struct FGroup_Replication;
    struct FGroup_EntityLifecycle;
    struct FGroup_EndPlay;
    struct FGroup_Teardown;
    struct FGroup_Overlap;
    struct FGroup_Transform_LatePose;

    // ----------------------------------------------------------------------------------------------------------------

    struct FGroup_DestructionPipeline
    {
    };

    struct FGroup_Gameplay_TimeDelta
    {
        using RunAfter = TDepList<FGroup_DestructionPipeline>;
    };

    struct FGroup_Gameplay
    {
        using RunAfter = TDepList<FGroup_Gameplay_TimeDelta>;
    };

    // Decision-logic tier — state machines, state trees, planners. SM transition cascades
    // converge at the Gameplay_Script settle barrier, after their EntityScript spawn pipeline.
    struct FGroup_Gameplay_AI
    {
        using RunAfter = TDepList<FGroup_Gameplay>;
    };

    struct FGroup_Gameplay_Audio
    {
        using RunAfter = TDepList<FGroup_Gameplay_AI>;
    };

    struct FGroup_Gameplay_Rendering
    {
        using RunAfter = TDepList<FGroup_Gameplay_Audio>;
    };

    struct FGroup_Gameplay_Script
    {
        using RunAfter = TDepList<FGroup_Gameplay_Rendering>;
    };

    struct FGroup_Gameplay_Chaos
    {
        using RunAfter = TDepList<FGroup_Gameplay_Script>;
    };

    struct FGroup_Physics
    {
        using RunAfter = TDepList<FGroup_Gameplay_Chaos>;
    };

    struct FGroup_Transform_SyncFrom
    {
        using RunAfter = TDepList<FGroup_Physics>;
    };

    struct FGroup_Transform
    {
        using RunAfter = TDepList<FGroup_Transform_SyncFrom>;
    };

    // Consumers of the settled transform pass that DERIVE new transform inputs from it: work that both
    // reads finished first-pass poses and enqueues transform requests of its own — a composition whose
    // output other transforms then attach to. Canonical transform-resolution processors opt into the
    // scheduler's local settle barrier after this group, so deferred outputs land before Finalize.
    struct FGroup_Transform_Derived
    {
        using RunAfter = TDepList<FGroup_Transform>;
    };

    struct FGroup_Transform_Finalize
    {
        using RunAfter = TDepList<FGroup_Transform_Derived>;
    };

    // Runs after every transform-resolution point has settled (still TG_PrePhysics) and before the
    // component push: the slot for readers that want final poses and the final composed view without
    // being transform writers themselves.
    struct FGroup_Gameplay_Camera
    {
        using RunAfter = TDepList<FGroup_Transform_Finalize>;
    };

    struct FGroup_PostTransform
    {
        using RunAfter = TDepList<FGroup_Gameplay_Camera>;
    };

    // The replicated-fragment + save-load dispatchers apply their payloads AFTER FGroup_PostTransform
    // (so every feature composed this frame exists) and BEFORE FGroup_Replication (so applied values
    // are visible before OnReplicationComplete broadcasts the same frame).
    struct FGroup_DeferredApply
    {
        using RunAfter = TDepList<FGroup_PostTransform>;
    };

    struct FGroup_Replication
    {
        using RunAfter = TDepList<FGroup_DeferredApply>;
    };

    // Hosts the tag-add processors that OPEN the EndPlay window (DestructionPhase_Endplay). The
    // matching close (DestructionPhase_Teardown) lives in FGroup_Teardown so FGroup_EndPlay runs
    // between them, while the (EndPlay && !Teardown) filter still matches.
    struct FGroup_EntityLifecycle
    {
        using RunAfter = TDepList<FGroup_Replication>;
    };

    // The window during which CK_IF_END_PLAY (= EndPlay + !Teardown) matches: every feature-level
    // *_EndPlay processor lives here and fires in the same tick Destroy() was called.
    struct FGroup_EndPlay
    {
        using RunAfter = TDepList<FGroup_EntityLifecycle>;
    };

    // Closes the EndPlay window by adding FTag_DestroyEntity_Teardown. The group-level RunAfter is what
    // saves every individual *_EndPlay processor from declaring its own ordering.
    struct FGroup_Teardown
    {
        using RunAfter = TDepList<FGroup_EndPlay>;
    };

    struct FGroup_Overlap
    {
        static constexpr auto TickGroup = TG_PostPhysics;
    };

    // Re-samples what follows an animated pose once the frame's animation is final: a skinned mesh publishes its
    // pose as late as the end of TG_PostPhysics, after every TG_PrePhysics processor has already read last frame's.
    // Its writes carry FTag_Transform_LateUpdated, never FTag_Transform_Updated (next frame's cleanup would clear that
    // before any TG_PrePhysics consumer read it); FGroup_Transform_SyncFrom promotes the late tag after that cleanup,
    // so every TG_PrePhysics consumer sees each late write, once, next frame.
    struct FGroup_Transform_LatePose
    {
        static constexpr auto TickGroup = TG_PostUpdateWork;
    };
}

// --------------------------------------------------------------------------------------------------------------------
