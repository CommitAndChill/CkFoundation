#include "CkSceneNode_Processor.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkEcsExt/Transform/CkTransform_RestoreRebase.h"

#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"
#include "CkEcs/Tag/CkTag_HydrationQuarantine.h"

#include "CkProfile/Stats/CkCpuWork.h"

#include "Components/SceneComponent.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_SceneNode_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_SceneNode_CancelPendingRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_SceneNode_FollowUnrealAnchor);
CK_REGISTER_PROCESSOR(ck::FProcessor_SceneNode_LateFollowUnrealAnchor);
CK_REGISTER_PROCESSOR(ck::FProcessor_SceneNode_QueueChildren_BeforeCleanup);
CK_REGISTER_PROCESSOR(ck::FProcessor_SceneNode_QueueRootChildren);

namespace ck_scene_node_processor
{
    // A late write is the normal direct write: Previous takes the value this frame's TG_PrePhysics consumers already
    // read. If nothing writes the entity early next frame, the promoted pair (early -> late) is what they read; if an
    // early write follows, DoWrite_EarlyTransform keeps that Previous, so their pair runs early -> next early.
    auto
        DoWrite_LateTransform(
            FCk_Handle& InHandle,
            ck::FFragment_Transform& InTransform,
            ck::FFragment_Transform_Previous& InPrevTransform,
            const FTransform& InNewTransform)
        -> bool
    {
        const auto ComponentsModified = UCk_Utils_Transform_UE::Apply_SetTransform_DirectWrite(
            InTransform, InPrevTransform, InNewTransform);

        if (NOT EnumHasAnyFlags(ComponentsModified,
            ECk_TransformComponents::Location |
            ECk_TransformComponents::Rotation |
            ECk_TransformComponents::Scale))
        { return false; }

        InHandle.AddOrGet<ck::FTag_Transform_LateUpdated>();
        return true;
    }

    // An early write (TG_PrePhysics). After a promoted late write (FTag_Transform_LatePromoted) it writes Current only:
    // Previous still holds last frame's early value, the one last frame's TG_PrePhysics consumers read, so this frame's
    // pair covers the late step and the early one. A plain direct write would start the pair at the late value and
    // drop the early -> late step from every pair (the mesh moved between the late pass and this sample: a character
    // that moved before the ECS tick, a script that moved the actor). The modified components are this frame's pair's.
    auto
        DoWrite_EarlyTransform(
            bool InFollowsPromotedLateWrite,
            ck::FFragment_Transform& InTransform,
            ck::FFragment_Transform_Previous& InPrevTransform,
            const FTransform& InNewTransform)
        -> ECk_TransformComponents
    {
        if (NOT InFollowsPromotedLateWrite)
        { return UCk_Utils_Transform_UE::Apply_SetTransform_DirectWrite(InTransform, InPrevTransform, InNewTransform); }

        const auto KeptPrevious = InPrevTransform;
        UCk_Utils_Transform_UE::Apply_SetTransform_DirectWrite(InTransform, InPrevTransform, InNewTransform);
        InPrevTransform = KeptPrevious;

        const auto& From = KeptPrevious.Get_Transform();
        auto ComponentsModified = ECk_TransformComponents::None;

        if (NOT InNewTransform.GetLocation().Equals(From.GetLocation()))
        { ComponentsModified |= ECk_TransformComponents::Location; }

        if (NOT InNewTransform.GetRotation().Equals(From.GetRotation()))
        { ComponentsModified |= ECk_TransformComponents::Rotation; }

        if (NOT InNewTransform.GetScale3D().Equals(From.GetScale3D()))
        { ComponentsModified |= ECk_TransformComponents::Scale; }

        InTransform.Set_ComponentsModified(ComponentsModified);
        return ComponentsModified;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SceneNode_HandleRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_SceneNode& InSceneNode,
            const FFragment_SceneNode_Requests& InRequestsComp) const
        -> void
    {
        InHandle.CopyAndRemove(InRequestsComp, [&](FFragment_SceneNode_Requests& InRequests)
        {
            algo::ForEachRequest(InRequests._Requests, ck::Visitor([&](const auto& InRequest)
            {
                // DoHandleRequest is void and has no rejection path, so reaching the line after the
                // call IS the success condition.
                auto Result = ECk_Request_OperationResult::Failed;
                const auto Guard = MakeCompletionGuard(InRequest, InHandle, Result);

                DoHandleRequest(InHandle, InSceneNode, InRequest);

                Result = ECk_Request_OperationResult::Succeeded;

                if (InRequest.Get_IsRequestHandleValid())
                {
                    InRequest.GetAndDestroyRequestHandle();
                }
            }));
        });
    }

    auto
        FProcessor_SceneNode_HandleRequests::
        DoHandleRequest(
            HandleType InHandle,
            FFragment_SceneNode& InSceneNode,
            const FCk_Request_SceneNode_UpdateRelativeTransform& InRequest)
        -> void
    {
        if (InSceneNode.Get_RelativeTransform().Equals(InRequest.Get_NewRelativeTransform()))
        { return; }

        InSceneNode._RelativeTransform = InRequest.Get_NewRelativeTransform();

        InHandle.AddOrGet<FTag_SceneNode_RelativeTransformUpdated>();
        FUtils_SceneNodePropagation::Queue(InHandle);
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SceneNode_CancelPendingRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SceneNode_Requests& InRequestsComp)
        -> void
    {
        request::FireCancelledForPending(InHandle, InRequestsComp.Get_Requests());
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SceneNode_FollowUnrealAnchor::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SceneNode_UnrealAnchor& InAnchor,
            const FFragment_SceneNode& InSceneNode,
            FFragment_Transform& InTransform,
            FFragment_Transform_Previous& InPrevTransform)
        -> void
    {
        // This node is excluded from TProcessor_SceneNode_Update (which is where bare/parent nodes strip
        // this tag), so clean it up here — a permanently-set tag would block downstream probe setup.
        if (InHandle.template Has<FTag_SceneNode_RelativeTransformUpdated>())
        { InHandle.template DeferRemove<FTag_SceneNode_RelativeTransformUpdated>(); }

        const auto AnchorWorld = Get_AnchorWorldTransform(InAnchor);

        if (NOT AnchorWorld.IsSet())
        { return; }

        const auto NewTransform = InSceneNode.Get_RelativeTransform() * AnchorWorld.GetValue();

        if (InTransform.Get_Transform().Equals(NewTransform))
        { return; }

        const auto ComponentsModified = ck_scene_node_processor::DoWrite_EarlyTransform(
            InHandle.template Has<FTag_Transform_LatePromoted>(), InTransform, InPrevTransform, NewTransform);

        if (EnumHasAnyFlags(ComponentsModified,
            ECk_TransformComponents::Location |
            ECk_TransformComponents::Rotation |
            ECk_TransformComponents::Scale))
        {
            InHandle.template DeferAddOrGet<FTag_Transform_Updated>();
        }
    }

    auto
        FProcessor_SceneNode_FollowUnrealAnchor::
        Get_AnchorWorldTransform(
            const FFragment_SceneNode_UnrealAnchor& InAnchor)
        -> TOptional<FTransform>
    {
        const auto& Component = InAnchor.Get_Component();

        if (ck::Is_NOT_Valid(Component))
        { return {}; }

        return InAnchor.Get_Socket().IsNone()
            ? Component->GetComponentTransform()
            : Component->GetSocketTransform(InAnchor.Get_Socket());
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SceneNode_LateFollowUnrealAnchor::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_SceneNode_UnrealAnchor& InAnchor,
            const FFragment_SceneNode& InSceneNode,
            FFragment_Transform& InTransform,
            FFragment_Transform_Previous& InPrevTransform)
        -> void
    {
        const auto AnchorWorld = FProcessor_SceneNode_FollowUnrealAnchor::Get_AnchorWorldTransform(InAnchor);

        if (NOT AnchorWorld.IsSet())
        { return; }

        const auto NewTransform = InSceneNode.Get_RelativeTransform() * AnchorWorld.GetValue();

        if (InTransform.Get_Transform().Equals(NewTransform))
        { return; }

        if (NOT ck_scene_node_processor::DoWrite_LateTransform(InHandle, InTransform, InPrevTransform, NewTransform))
        { return; }

        DoCompose_Children(InHandle, NewTransform);
    }

    // The late counterpart of the TProcessor_SceneNode_Update chain, walked depth-first from one late anchor. It skips
    // what that chain skips (nodes with their own anchor, anchor-authoritative nodes and everything under them) and
    // stops a branch where the composed world is unchanged. A restore rebase is the early chain's to propagate.
    auto
        FProcessor_SceneNode_LateFollowUnrealAnchor::
        DoCompose_Children(
            FCk_Handle& InParent,
            const FTransform& InParentWorld)
        -> void
    {
        if (NOT InParent.Has<FFragment_RecordOfSceneNodes>())
        { return; }

        FUtils_SceneNodePropagation::MarkPublished(InParent, InParentWorld);

        FUtils_RecordOfSceneNodes::ForEach_ValidEntry(InParent, [&](FCk_Handle_SceneNode InChild)
        {
            if (InChild.Has_Any<FTag_DestroyEntity_EndPlay, FTag_DestroyEntity_Teardown,
                    FTag_DestroyEntity_Await, FTag_DestroyEntity_Finalize>())
            { return; }

            // The walk reaches children by handle, outside any view, so it states the quarantine the views apply. The
            // parent was just marked published at the new pose, so the early chain would not re-queue this child on its
            // own: queue it, or it keeps its old pose once the quarantine lifts if the anchor holds still from then on.
            if (InChild.Has<FTag_Hydration_Quarantine>())
            {
                auto QuarantinedChild = FCk_Handle{InChild};
                FUtils_SceneNodePropagation::Queue(QuarantinedChild);
                return;
            }

            if (InChild.Has<FFragment_SceneNode_UnrealAnchor>())
            { return; }

            if (InChild.Has_Any<FFragment_Transform_MeshSocket, FFragment_Transform_RootComponent>() &&
                NOT InChild.Has<FTag_Transform_ExternallyDriven>())
            { return; }

            if (NOT InChild.Has_All<FFragment_SceneNode, FFragment_Transform, FFragment_Transform_Previous>())
            { return; }

            const auto ChildWorld = InChild.Get<FFragment_SceneNode>().Get_RelativeTransform() * InParentWorld;
            auto& ChildTransform = InChild.Get<FFragment_Transform>();

            if (ChildTransform.Get_Transform().Equals(ChildWorld))
            { return; }

            auto Child = FCk_Handle{InChild};
            auto& ChildPrevTransform = InChild.Get<FFragment_Transform_Previous>();

            if (NOT ck_scene_node_processor::DoWrite_LateTransform(Child, ChildTransform, ChildPrevTransform, ChildWorld))
            { return; }

            DoCompose_Children(Child, ChildWorld);
        });
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SceneNode_QueueChildren_BeforeCleanup::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Transform& InTransform,
            const FFragment_RecordOfSceneNodes& InChildren)
        -> void
    {
        FUtils_SceneNodePropagation::PublishChildrenIfChanged(InHandle, InTransform.Get_Transform());
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_SceneNode_QueueRootChildren::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Transform& InTransform,
            const FFragment_RecordOfSceneNodes& InChildren)
        -> void
    {
        FUtils_SceneNodePropagation::PublishChildrenIfChanged(InHandle, InTransform.Get_Transform());
    }

    // --------------------------------------------------------------------------------------------------------------------

    template <int32 T_LayerIndex>
    TProcessor_SceneNode_Update<T_LayerIndex>::
        TProcessor_SceneNode_Update(
        const typename Super::RegistryType& InRegistry)
        : Super(InRegistry)
    { }

    template <int32 T_LayerIndex>
    auto
        TProcessor_SceneNode_Update<T_LayerIndex>::
        ForEachEntity(
            typename Super::TimeType InDeltaT,
            typename Super::HandleType InHandle,
            const SceneNodeParent& InParent,
            const FFragment_SceneNode& InSceneNode,
            FFragment_Transform& InTransform,
            FFragment_Transform_Previous& InPrevTransform,
            const FFragment_SceneNode_PropagationState& InPropagationState)
        -> void
    {
        FUtils_SceneNodePropagation::DeferConsume(InHandle, InPropagationState.Get_QueueGeneration());

        const auto HadRelativeTransformUpdatedTag = InHandle.template Has<FTag_SceneNode_RelativeTransformUpdated>();

        const auto ParentEntity = InParent.Get_Entity().Get_Entity();
        auto ReadOnlyParent = InHandle.ReadEntity(ParentEntity);
        const auto ParentHasRestoreRebase = ReadOnlyParent.template Has<FTag_Transform_RestoreRebase>();

        // Cleared BEFORE the early return below, so root-component/mesh-socket entities don't keep it
        // permanently (which would block probe setup)
        if (HadRelativeTransformUpdatedTag)
        {
            InHandle.template DeferRemove<FTag_SceneNode_RelativeTransformUpdated>();
        }

        // Anchored nodes are anchor-authoritative unless FTag_Transform_ExternallyDriven flips the contract.
        // Not expressible as a view include/exclude: bare entities (no anchor, no tag) must still process.
        if (InHandle.template Has_Any<FFragment_Transform_MeshSocket, FFragment_Transform_RootComponent>() &&
            NOT InHandle.template Has<FTag_Transform_ExternallyDriven>())
        { return; }

        const auto& ParentTransform = ReadOnlyParent.template Get<FFragment_Transform>().Get_Transform();
        const auto NewTransform = InSceneNode.Get_RelativeTransform() * ParentTransform;

        const auto ComponentsModified = ck_scene_node_processor::DoWrite_EarlyTransform(
            InHandle.template Has<FTag_Transform_LatePromoted>(), InTransform, InPrevTransform, NewTransform);

        if (EnumHasAnyFlags(ComponentsModified,
            ECk_TransformComponents::Location |
            ECk_TransformComponents::Rotation |
            ECk_TransformComponents::Scale))
        {
            InHandle.template DeferAddOrGet<FTag_Transform_Updated>();

            if (ParentHasRestoreRebase)
            {
                InHandle.template DeferAddOrGet<FTag_Transform_RestoreRebase>();
            }
        }
    }

    // --------------------------------------------------------------------------------------------------------------------

    template <int32 T_LayerIndex>
    auto
        TProcessor_SceneNode_QueueChildren<T_LayerIndex>::
        ForEachEntity(
            typename Super::TimeType InDeltaT,
            typename Super::HandleType InHandle,
            const FFragment_Transform& InTransform,
            const FFragment_RecordOfSceneNodes& InChildren)
        -> void
    {
        if constexpr (T_LayerIndex == 0)
        {
            if (cpu_work::Get_Enabled())
            { cpu_work::Add(ECk_CpuWorkCounter::SceneL0QueueParents, 1); }
        }

        FUtils_SceneNodePropagation::PublishChildrenIfChanged(InHandle, InTransform.Get_Transform());
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_scene_node_processor
{
    template <int32 T_LayerIndex>
    using TLayerProcessors = std::conditional_t<
        T_LayerIndex == ck::MaxSceneNodeLayers - 1,
        entt::type_list<
            ck::TProcessor_SceneNode_Update<T_LayerIndex>>,
        entt::type_list<
            ck::TProcessor_SceneNode_Update<T_LayerIndex>,
            ck::TProcessor_SceneNode_QueueChildren<T_LayerIndex>>>;

    // One FAutoProcessorRegistrar per listed processor, as base classes: they register in list order when the
    // object is constructed and deregister in reverse when it is destroyed, as a row of CK_REGISTER_PROCESSOR
    // statics would.
    template <typename T_Processors>
    struct TProcessorRegistrars;

    template <typename... T_Processors>
    struct TProcessorRegistrars<entt::type_list<T_Processors...>>
        : public ck::FAutoProcessorRegistrar<T_Processors>...
    { };

    template <typename T_LayerIndices>
    struct TLayerProcessorRegistrars;

    template <int32... T_LayerIndices>
    struct TLayerProcessorRegistrars<std::integer_sequence<int32, T_LayerIndices...>>
        : public TProcessorRegistrars<entt::type_list_cat_t<TLayerProcessors<T_LayerIndices>...>>
    { };

    // After the layer processors' member definitions above, which the registrars instantiate.
    static TLayerProcessorRegistrars<ck::FSceneNodeLayerIndices> GCkAutoRegister_SceneNodeLayers;
}

// --------------------------------------------------------------------------------------------------------------------
