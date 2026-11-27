#pragma once

#include "CkSceneNode_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkEcs/Processor/CkParallelProcessor.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

#include "CkEcsExt/Transform/CkTransform_Processor.h"

#include <type_traits>

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    // --------------------------------------------------------------------------------------------------------------------

    class CKECSEXT_API FProcessor_SceneNode_HandleRequests : public ck_exp::TProcessor<
            FProcessor_SceneNode_HandleRequests,
            FCk_Handle_SceneNode,
            ck::TReadWrite<FFragment_SceneNode>,
            ck::TReadOnly<FFragment_SceneNode_Requests>,
            TExclude<FTag_DestroyEntity_Initiate>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform;
        using MarkedDirtyBy = FFragment_SceneNode_Requests;

    public:
        using TProcessor::TProcessor;

    public:
        auto
    	ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_SceneNode& InSceneNode,
            const FFragment_SceneNode_Requests& InRequestsComp) const -> void;

    private:
        static auto
        DoHandleRequest(
            HandleType InHandle,
            FFragment_SceneNode& InSceneNode,
            const FCk_Request_SceneNode_UpdateRelativeTransform& InRequest) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // HandleRequests excludes owners already tagged for destruction, so a destroyed scene-node's still-queued
    // requests are never drained. This fires each pending request's completion delegate with
    // Failed_Cancelled so a caller awaiting completion terminates instead of hanging.
    class CKECSEXT_API FProcessor_SceneNode_CancelPendingRequests : public ck_exp::TProcessor<
        FProcessor_SceneNode_CancelPendingRequests,
        FCk_Handle_SceneNode,
        ck::TReadOnly<FFragment_SceneNode_Requests>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SceneNode_Requests& InRequestsComp)
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // entity.world = offset * anchor.world, every tick. Read-only w.r.t. the anchor (never writes back).
    // Runs in the SyncFrom group so descendant TProcessor_SceneNode_Update layers (Transform group) see this
    // node's updated world + FTag_Transform_Updated.
    class CKECSEXT_API FProcessor_SceneNode_FollowUnrealAnchor : public TParallelProcessor<
            FProcessor_SceneNode_FollowUnrealAnchor,
            FCk_Handle_SceneNode,
            TReadOnly<FFragment_SceneNode_UnrealAnchor>,
            TReadOnly<FFragment_SceneNode>,
            TReadWrite<FFragment_Transform>,
            TReadWrite<FFragment_Transform_Previous>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_SyncFrom;
        // After the promotion, so its write can tell a promoted late write (FTag_Transform_LatePromoted) from none.
        using RunAfter = TDepList<FProcessor_Transform_PromoteLateUpdated>;

    public:
        using TParallelProcessor::TParallelProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SceneNode_UnrealAnchor& InAnchor,
            const FFragment_SceneNode& InSceneNode,
            FFragment_Transform& InTransform,
            FFragment_Transform_Previous& InPrevTransform) -> void;

    public:
        // The anchor component's or socket's world transform as it reads now; unset once the component is gone.
        // Both follow passes and UCk_Utils_SceneNode_UE::Get_DriverWorldTransform compose against this.
        static auto
        Get_AnchorWorldTransform(
            const FFragment_SceneNode_UnrealAnchor& InAnchor) -> TOptional<FTransform>;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // The re-sample of a late anchor (FTag_SceneNode_LateAnchor) once the frame's animation is final, and the
    // composition of its scene-node subtree against it. Every write carries FTag_Transform_LateUpdated: TG_PrePhysics
    // consumers see it next frame, through FProcessor_Transform_PromoteLateUpdated, and the late consumers in this group
    // see it now. A ck_exp::TProcessor (not parallel) because the subtree walk writes other entities' transforms.
    class CKECSEXT_API FProcessor_SceneNode_LateFollowUnrealAnchor : public ck_exp::TProcessor<
            FProcessor_SceneNode_LateFollowUnrealAnchor,
            FCk_Handle_SceneNode,
            ck::TReadOnly<FFragment_SceneNode_UnrealAnchor>,
            ck::TReadOnly<FFragment_SceneNode>,
            ck::TReadWrite<FFragment_Transform>,
            ck::TReadWrite<FFragment_Transform_Previous>,
            FTag_SceneNode_LateAnchor,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_LatePose;
        using RunBefore = TDepList<FProcessor_Transform_SyncToActor_Late>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SceneNode_UnrealAnchor& InAnchor,
            const FFragment_SceneNode& InSceneNode,
            FFragment_Transform& InTransform,
            FFragment_Transform_Previous& InPrevTransform) -> void;

    private:
        static auto
        DoCompose_Children(
            FCk_Handle& InParent,
            const FTransform& InParentWorld) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Tail-pump writes outlive the normal propagation pass, but not next frame's transform cleanup.
    // Preserve their descendants in the consumed SceneNode queue before that marker is discarded.
    class CKECSEXT_API FProcessor_SceneNode_QueueChildren_BeforeCleanup : public ck_exp::TProcessor<
            FProcessor_SceneNode_QueueChildren_BeforeCleanup,
            FCk_Handle_Transform,
            ck::TReadOnly<FFragment_Transform>,
            ck::TReadOnly<FFragment_RecordOfSceneNodes>,
            FTag_Transform_Updated,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using RunAfter = TDepList<FGroup_Physics>;
        using RunBefore = TDepList<FProcessor_Transform_Cleanup, FGroup_Transform_SyncFrom>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Transform& InTransform,
            const FFragment_RecordOfSceneNodes& InChildren) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKECSEXT_API FProcessor_SceneNode_QueueRootChildren : public ck_exp::TProcessor<
            FProcessor_SceneNode_QueueRootChildren,
            FCk_Handle_Transform,
            ck::TReadOnly<FFragment_Transform>,
            ck::TReadOnly<FFragment_RecordOfSceneNodes>,
            FTag_Transform_Updated,
            TExclude<FTag_SceneNode_Layered>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform;
        using RunAfter = TDepList<
            FProcessor_SceneNode_FollowUnrealAnchor,
            FProcessor_Transform_HandleRequests>;
        using LocalSettleAfter = FGroup_Transform_Derived;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Transform& InTransform,
            const FFragment_RecordOfSceneNodes& InChildren) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // One Update per layer and one QueueChildren per layer but the last (no node sits below the last layer), all
    // registered in CkSceneNode_Processor.cpp from MaxSceneNodeLayers.
    template <int32 T_LayerIndex>
    class TProcessor_SceneNode_Update;

    template <int32 T_LayerIndex>
    class TProcessor_SceneNode_QueueChildren;

    // The canonical depth chain runs in FGroup_Transform, then may be replayed in the same order by the
    // scheduler's local settle barrier after derived transform producers.
    template <int32 T_LayerIndex>
    class CKECSEXT_API TProcessor_SceneNode_Update : public TParallelProcessor<
            TProcessor_SceneNode_Update<T_LayerIndex>,
            FCk_Handle_SceneNode,
            TTag_SceneNode_Layer<T_LayerIndex>,
            TReadOnly<SceneNodeParent>,
            TReadOnly<FFragment_SceneNode>,
            TReadWrite<FFragment_Transform>,
            TReadWrite<FFragment_Transform_Previous>,
            TReadOnly<FFragment_SceneNode_PropagationState>,
            FTag_SceneNode_PropagationQueued,
            TExclude<FFragment_SceneNode_UnrealAnchor>,
            CK_IGNORE_PENDING_KILL>
    {
        using Super = TParallelProcessor<TProcessor_SceneNode_Update<T_LayerIndex>, FCk_Handle_SceneNode,
            TTag_SceneNode_Layer<T_LayerIndex>,
            TReadOnly<SceneNodeParent>, TReadOnly<FFragment_SceneNode>,
            TReadWrite<FFragment_Transform>, TReadWrite<FFragment_Transform_Previous>,
            TReadOnly<FFragment_SceneNode_PropagationState>, FTag_SceneNode_PropagationQueued,
            TExclude<FFragment_SceneNode_UnrealAnchor>,
            CK_IGNORE_PENDING_KILL>;

    public:
        using Group = FGroup_Transform;

        // Layer 0 runs after the request drains and the root publish; layer N after layer N-1 has published its
        // children. Dropping either dependency stops motion propagating past the first scene-node link
        // (CkEcsExt/Claude.md § "SceneNode layer ordering").
        using RunAfter = std::conditional_t<
            T_LayerIndex == 0,
            TDepList<FProcessor_SceneNode_HandleRequests, FProcessor_SceneNode_QueueRootChildren>,
            TDepList<TProcessor_SceneNode_QueueChildren<(T_LayerIndex > 0 ? T_LayerIndex - 1 : 0)>>>;

        using MarkedDirtyBy = FTag_SceneNode_PropagationQueued;
        using LocalSettleAfter = FGroup_Transform_Derived;

    public:
        explicit TProcessor_SceneNode_Update(
            const typename Super::RegistryType& InRegistry);

    public:
        static auto
        ForEachEntity(
            typename Super::TimeType InDeltaT,
            typename Super::HandleType InHandle,
            const SceneNodeParent& InParent,
            const FFragment_SceneNode& InSceneNode,
            FFragment_Transform& InTransform,
            FFragment_Transform_Previous& InPrevTransform,
            const FFragment_SceneNode_PropagationState& InPropagationState) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    template <int32 T_LayerIndex>
    class CKECSEXT_API TProcessor_SceneNode_QueueChildren : public ck_exp::TProcessor<
            TProcessor_SceneNode_QueueChildren<T_LayerIndex>,
            FCk_Handle_Transform,
            TTag_SceneNode_Layer<T_LayerIndex>,
            ck::TReadOnly<FFragment_Transform>,
            ck::TReadOnly<FFragment_RecordOfSceneNodes>,
            FTag_Transform_Updated,
            CK_IGNORE_PENDING_KILL>
    {
        static_assert(T_LayerIndex < MaxSceneNodeLayers - 1,
            "The last scene-node layer has no children to queue: it has no QueueChildren processor");

        using Super = ck_exp::TProcessor<
            TProcessor_SceneNode_QueueChildren<T_LayerIndex>, FCk_Handle_Transform,
            TTag_SceneNode_Layer<T_LayerIndex>,
            ck::TReadOnly<FFragment_Transform>, ck::TReadOnly<FFragment_RecordOfSceneNodes>,
            FTag_Transform_Updated, CK_IGNORE_PENDING_KILL>;

    public:
        using Group = FGroup_Transform;
        using RunAfter = TDepList<TProcessor_SceneNode_Update<T_LayerIndex>>;
        using LocalSettleAfter = FGroup_Transform_Derived;

    public:
        using Super::Super;

    public:
        static auto
        ForEachEntity(
            typename Super::TimeType InDeltaT,
            typename Super::HandleType InHandle,
            const FFragment_Transform& InTransform,
            const FFragment_RecordOfSceneNodes& InChildren) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------
}

// --------------------------------------------------------------------------------------------------------------------
