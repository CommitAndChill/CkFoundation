#pragma once

#include "CkSceneNode_Fragment_Data.h"

#include "CkEcsExt/EntityHolder/CkEntityHolder_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"

#include "CkRecord/Record/CkRecord_Utils.h"

#include <type_traits>
#include <utility>

// --------------------------------------------------------------------------------------------------------------------

class UCk_Utils_SceneNode_UE;
class USceneComponent;
struct FCk_Handle_ReadOnly;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    CK_DEFINE_ECS_TAG(FTag_SceneNode_RelativeTransformUpdated);
    CK_DEFINE_ECS_TAG(FTag_SceneNode_PropagationQueued);

    // An Unreal-anchored node whose anchor moves with an animated pose (a skinned-mesh socket or bone): it is
    // re-sampled in FGroup_Transform_LatePose, after the frame's animation is final, on top of the early sample.
    CK_DEFINE_ECS_TAG(FTag_SceneNode_LateAnchor);

    // How many scene-node layers exist: a node's depth (its layer) runs from 0 to MaxSceneNodeLayers - 1. It sizes
    // the layer tags, the per-layer Update and QueueChildren processors and their RunAfter chain. An attach that
    // would put any node deeper is refused whole (CkEcsExt/Claude.md § "SceneNode depth").
    inline constexpr int32 MaxSceneNodeLayers = 16;

    // Every layer index, in order. Code that needs one thing per layer specializes on this
    // (std::integer_sequence<int32, T_LayerIndices...>) and expands the pack once.
    using FSceneNodeLayerIndices = std::make_integer_sequence<int32, MaxSceneNodeLayers>;

    // The layer a parent-driven node composes in: TProcessor_SceneNode_Update<N> runs after layer N-1 has published
    // its children, so a node composes against a parent already composed this pass.
    template <int32 T_LayerIndex>
    struct TTag_SceneNode_Layer : public TTag<TTag_SceneNode_Layer<T_LayerIndex>>
    {
        static_assert(T_LayerIndex >= 0 && T_LayerIndex < MaxSceneNodeLayers,
            "SceneNode layer index outside [0, MaxSceneNodeLayers)");

        static constexpr int32 LayerIndex = T_LayerIndex;
    };

    static_assert(std::is_empty_v<TTag_SceneNode_Layer<0>>, "Tags must not carry any data");

    // Carried by every node that holds a layer tag, beside it, so a view can take or exclude every layered node with
    // one type (FProcessor_SceneNode_QueueRootChildren, the census).
    CK_DEFINE_ECS_TAG(FTag_SceneNode_Layered);

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECSEXT_API FFragment_SceneNode_PropagationState
    {
    public:
        CK_GENERATED_BODY(FFragment_SceneNode_PropagationState);

        friend struct FUtils_SceneNodePropagation;

    private:
        uint64 _QueueGeneration = 0;
        FTransform _LastPublishedWorldTransform = FTransform::Identity;
        bool _HasPublishedWorldTransform = false;

    public:
        CK_PROPERTY_GET(_QueueGeneration);
    };

    // Sparse propagation plumbing shared by SceneNode composition and its ordered processors. All state is
    // runtime-derived: none of it participates in snapshot persistence.
    struct CKECSEXT_API FUtils_SceneNodePropagation
    {
        static auto Queue(FCk_Handle& InSceneNode) -> void;
        static auto DeferConsume(const FCk_Handle_ReadOnly& InSceneNode, uint64 InGeneration) -> void;
        static auto PublishChildrenIfChanged(FCk_Handle& InParent, const FTransform& InWorldTransform) -> void;
        // Records that InParent's children already compose against InWorldTransform, so the next
        // PublishChildrenIfChanged at that transform queues nothing.
        static auto MarkPublished(FCk_Handle& InParent, const FTransform& InWorldTransform) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECSEXT_API FFragment_SceneNode
    {
    public:
        CK_GENERATED_BODY(FFragment_SceneNode);

    public:
        friend class FProcessor_SceneNode_Setup;
        friend class FProcessor_SceneNode_HandleRequests;
        friend class FProcessor_SceneNode_EndPlay;
        friend class FProcessor_SceneNode_FollowUnrealAnchor;
        friend class UCk_Utils_SceneNode_UE;

    private:
        FTransform _RelativeTransform;

    public:
        CK_PROPERTY(_RelativeTransform);

        CK_DEFINE_CONSTRUCTORS(FFragment_SceneNode, _RelativeTransform);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Foreign Unreal anchor a scene-node follows: a USceneComponent (Socket == None) or a mesh socket.
    // Deliberately NOT the Transform module's FFragment_Transform_RootComponent / _MeshSocket, which would
    // engage the bidirectional actor bridge. See CkEcsExt/CLAUDE.md § "SceneNode Unreal anchors".
    struct CKECSEXT_API FFragment_SceneNode_UnrealAnchor
    {
    public:
        CK_GENERATED_BODY(FFragment_SceneNode_UnrealAnchor);

    public:
        friend class FProcessor_SceneNode_FollowUnrealAnchor;
        friend class UCk_Utils_SceneNode_UE;

    public:
        FFragment_SceneNode_UnrealAnchor() = default;

        explicit
        FFragment_SceneNode_UnrealAnchor(
            const USceneComponent* InComponent,
            FName InSocket);

    private:
        TWeakObjectPtr<const USceneComponent> _Component;
        FName _Socket = NAME_None;

    public:
        CK_PROPERTY_GET(_Component);
        CK_PROPERTY_GET(_Socket);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKECSEXT_API FFragment_SceneNode_Requests
    {
    public:
        CK_GENERATED_BODY(FFragment_SceneNode_Requests);

    public:
        friend class FProcessor_SceneNode_HandleRequests;
        friend class UCk_Utils_SceneNode_UE;

    public:
        using RequestType = std::variant
        <
            FCk_Request_SceneNode_UpdateRelativeTransform
        >;
        using RequestList = TArray<RequestType>;

    private:
        RequestList _Requests;

    public:
        CK_PROPERTY_GET(_Requests);
    };

    // --------------------------------------------------------------------------------------------------------------------

    CK_DEFINE_ENTITY_HOLDER_AND_UTILS_ROUNDTRIP(USceneNodeParent_Utils, SceneNodeParent, FCk_Handle_Transform);

    // --------------------------------------------------------------------------------------------------------------------

    CK_DEFINE_RECORD_OF_ENTITIES_AND_UTILS_ROUNDTRIP(FUtils_RecordOfSceneNodes, FFragment_RecordOfSceneNodes, FCk_Handle_SceneNode);

    // --------------------------------------------------------------------------------------------------------------------
}

// --------------------------------------------------------------------------------------------------------------------
