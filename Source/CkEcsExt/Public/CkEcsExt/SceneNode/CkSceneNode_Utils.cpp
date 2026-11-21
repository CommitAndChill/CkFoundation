#include "CkSceneNode_Utils.h"

#include "CkEcsExt/CkEcsExt_Log.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Handle/CkHandle_ReadOnly.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Processor.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkProfile/Stats/CkCpuWork.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include "Algo/Reverse.h"

#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkinnedMeshComponent.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_scene_node_utils
{
    auto
        Get_SceneNodeAnchorLabel(
            const FCk_Handle& InHandle)
        -> FString
    {
        const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InHandle);
        if (NOT DebugName.IsNone())
        { return DebugName.ToString(); }

        return ck::Format_UE(TEXT("#{}"), InHandle.Get_Entity().Get_EntityNumber());
    }

    // ----------------------------------------------------------------------------------------------------------------

    // What one layer's tag does to a handle. The table below is the only place a runtime layer index meets a tag type.
    struct FLayerTag
    {
        bool (*_Has)(const FCk_Handle&);
        void (*_Add)(FCk_Handle&);
        void (*_Remove)(FCk_Handle&);
    };

    template <int32 T_LayerIndex>
    struct TLayerTag
    {
        using TagType = ck::TTag_SceneNode_Layer<T_LayerIndex>;

        static auto Has(const FCk_Handle& InHandle) -> bool { return InHandle.Has<TagType>(); }
        static auto Add(FCk_Handle& InHandle) -> void { InHandle.Add<TagType>(); }
        static auto Remove(FCk_Handle& InHandle) -> void { InHandle.Remove<TagType>(); }
    };

    template <typename T_LayerIndices>
    struct TLayerTags;

    template <int32... T_LayerIndices>
    struct TLayerTags<std::integer_sequence<int32, T_LayerIndices...>>
    {
        static constexpr FLayerTag Rows[] =
        {
            FLayerTag{
                &TLayerTag<T_LayerIndices>::Has,
                &TLayerTag<T_LayerIndices>::Add,
                &TLayerTag<T_LayerIndices>::Remove}...
        };
    };

    // Indexed by layer.
    auto
        Get_LayerTags()
        -> TConstArrayView<FLayerTag>
    {
        return MakeArrayView(TLayerTags<ck::FSceneNodeLayerIndices>::Rows);
    }

    // ----------------------------------------------------------------------------------------------------------------

    // The depth a node attached under InAttachTo takes: one below a layered parent, 0 under anything else.
    auto
        Get_ChildDepth(
            const FCk_Handle& InAttachTo)
        -> int32
    {
        const auto ParentDepth = UCk_Utils_SceneNode_UE::Get_Depth(InAttachTo);
        return ParentDepth.IsSet() ? ParentDepth.GetValue() + 1 : 0;
    }

    // The node furthest below a node along scene-node links, and how many links away it is.
    struct FSubtreeReach
    {
        FCk_Handle_Transform _DeepestNode;
        int32 _Height = 0;
    };

    // InNode itself at height 0 when it has no scene-node child, or is INVALID (a node still to be created). Of
    // equally deep nodes, the first. It stops looking InLinkBudget links down: a subtree as tall as every layer fits
    // nowhere, and a link cycle cannot hang the query.
    auto
        Get_SubtreeReach(
            const FCk_Handle_Transform& InNode,
            int32 InLinkBudget = ck::MaxSceneNodeLayers)
        -> FSubtreeReach
    {
        auto Reach = FSubtreeReach{InNode, 0};

        if (ck::Is_NOT_Valid(InNode) || InLinkBudget <= 0)
        { return Reach; }

        UCk_Utils_SceneNode_UE::ForEach_SceneNode(InNode, [&](FCk_Handle_SceneNode InChild)
        {
            const auto ChildReach = Get_SubtreeReach(UCk_Utils_Transform_UE::Cast(InChild), InLinkBudget - 1);

            if (ChildReach._Height + 1 > Reach._Height)
            { Reach = FSubtreeReach{ChildReach._DeepestNode, ChildReach._Height + 1}; }
        });

        return Reach;
    }

    // Where an attach would put a node, and how deep its subtree would then reach.
    struct FAttachFit
    {
        int32 _NodeDepth = 0;
        FSubtreeReach _Reach;

        auto Get_DeepestDepth() const -> int32 { return _NodeDepth + _Reach._Height; }
        auto Get_Fits() const -> bool { return Get_DeepestDepth() < ck::MaxSceneNodeLayers; }
    };

    // InNode INVALID asks for one node still to be created.
    auto
        Get_AttachFit(
            const FCk_Handle_Transform& InNode,
            const FCk_Handle_Transform& InAttachTo)
        -> FAttachFit
    {
        return FAttachFit{Get_ChildDepth(InAttachTo), Get_SubtreeReach(InNode)};
    }

    // The labels from InLinksUp scene-node parents above InNode down to InNode (fewer when the chain is shorter).
    auto
        Get_ChainDownTo(
            const FCk_Handle_Transform& InNode,
            int32 InLinksUp)
        -> TArray<FString>
    {
        auto Chain = TArray<FString>{};

        auto Node = InNode;
        for (auto Link = 0; Link <= InLinksUp && ck::IsValid(Node); ++Link)
        {
            Chain.Add(Get_SceneNodeAnchorLabel(Node));

            if (NOT Node.Has<ck::SceneNodeParent>())
            { break; }

            Node = ck::USceneNodeParent_Utils::Get_StoredEntity_AsTypeSafe<FCk_Handle_Transform>(Node);
        }

        Algo::Reverse(Chain);
        return Chain;
    }

    // The one overflow decision behind every attach path: the depth InNode takes under InAttachTo, or unset with
    // the refusal logged. An error on every refusal instead of an ensure is a maintainer-approved exception to
    // DEVELOPMENT.md non-negotiable 3: the attach is refused, not continued; callers that expect the cap ask the quiet
    // queries; and a Ck ensure reports a site only once per process outside automation.
    auto
        TryGet_AttachDepth_LogIfRefused(
            const FCk_Handle_Transform& InNode,
            const FCk_Handle_Transform& InAttachTo)
        -> TOptional<int32>
    {
        const auto Fit = Get_AttachFit(InNode, InAttachTo);

        if (Fit.Get_Fits())
        { return Fit._NodeDepth; }

        const auto NodeLabel = ck::IsValid(InNode)
            ? Get_SceneNodeAnchorLabel(InNode)
            : FString{TEXT("(new scene node)")};

        // A layered node has Get_Depth() + 1 links to its plain root, so MaxSceneNodeLayers reaches any root.
        auto Chain = Get_ChainDownTo(InAttachTo, ck::MaxSceneNodeLayers);
        Chain.Append(ck::IsValid(InNode)
            ? Get_ChainDownTo(Fit._Reach._DeepestNode, Fit._Reach._Height)
            : TArray<FString>{NodeLabel});

        ck::ecs_extension::Error(
            TEXT("SceneNode depth overflow: attaching [{}] under [{}] would put a node at depth [{}], and depths run "
                 "from 0 to [{}]. The attach is refused and nothing changed. Chain: [{}]"),
            NodeLabel,
            Get_SceneNodeAnchorLabel(InAttachTo),
            Fit.Get_DeepestDepth(),
            ck::MaxSceneNodeLayers - 1,
            FString::Join(Chain, TEXT(" > ")));

        return {};
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::FUtils_SceneNodePropagation::
    Queue(
        FCk_Handle& InSceneNode)
    -> void
{
    if (ck::Is_NOT_Valid(InSceneNode) ||
        NOT InSceneNode.Has_All<ck::SceneNodeParent, ck::FFragment_SceneNode>() ||
        InSceneNode.Has<ck::FFragment_SceneNode_UnrealAnchor>())
    { return; }

    auto& State = InSceneNode.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
    ++State._QueueGeneration;
    InSceneNode.AddOrGet<ck::FTag_SceneNode_PropagationQueued>();
}

auto
    ck::FUtils_SceneNodePropagation::
    DeferConsume(
        const FCk_Handle_ReadOnly& InSceneNode,
        uint64 InGeneration)
    -> void
{
    InSceneNode.DeferCustom([InGeneration](FCk_Handle& InDeferredSceneNode)
    {
        if (ck::Is_NOT_Valid(InDeferredSceneNode) ||
            NOT InDeferredSceneNode.Has<ck::FFragment_SceneNode_PropagationState>())
        { return; }

        const auto& State = InDeferredSceneNode.Get<ck::FFragment_SceneNode_PropagationState>();
        if (State.Get_QueueGeneration() == InGeneration)
        { InDeferredSceneNode.Try_Remove<ck::FTag_SceneNode_PropagationQueued>(); }
    });
}

auto
    ck::FUtils_SceneNodePropagation::
    PublishChildrenIfChanged(
        FCk_Handle& InParent,
        const FTransform& InWorldTransform)
    -> void
{
    const auto Enabled = cpu_work::Get_Enabled();
    TRACE_CPUPROFILER_EVENT_SCOPE_CONDITIONAL(CkCpuWork_ScenePublishChildren, Enabled);
    if (Enabled)
    { cpu_work::Add(ECk_CpuWorkCounter::SceneParentsVisited, 1); }

    if (ck::Is_NOT_Valid(InParent) || NOT InParent.Has<ck::FFragment_RecordOfSceneNodes>())
    {
        if (Enabled)
        { cpu_work::Add(ECk_CpuWorkCounter::SceneMissingRecord, 1); }
        return;
    }

    auto& State = InParent.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
    if (State._HasPublishedWorldTransform && State._LastPublishedWorldTransform.Equals(InWorldTransform))
    {
        if (Enabled)
        { cpu_work::Add(ECk_CpuWorkCounter::SceneUnchangedParents, 1); }
        return;
    }

    State._LastPublishedWorldTransform = InWorldTransform;
    State._HasPublishedWorldTransform = true;
    if (Enabled)
    { cpu_work::Add(ECk_CpuWorkCounter::SceneChangedParents, 1); }

    auto ChildrenVisited = int32{0};
    ck::FUtils_RecordOfSceneNodes::ForEach_ValidEntry(InParent, [&](FCk_Handle_SceneNode InChild)
    {
        if (Enabled)
        {
            ++ChildrenVisited;
            cpu_work::Add(ECk_CpuWorkCounter::SceneQueueAttempts, 1);
            if (ck::IsValid(InChild) &&
                InChild.Has_All<ck::SceneNodeParent, ck::FFragment_SceneNode>() &&
                NOT InChild.Has<ck::FFragment_SceneNode_UnrealAnchor>())
            {
                cpu_work::Add(ECk_CpuWorkCounter::SceneQueueAccepted, 1);
            }
        }
        Queue(InChild);
    });
    if (Enabled)
    { cpu_work::Add(ECk_CpuWorkCounter::SceneChildrenVisited, ChildrenVisited); }
}

auto
    ck::FUtils_SceneNodePropagation::
    MarkPublished(
        FCk_Handle& InParent,
        const FTransform& InWorldTransform)
    -> void
{
    auto& State = InParent.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
    State._LastPublishedWorldTransform = InWorldTransform;
    State._HasPublishedWorldTransform = true;
}

// --------------------------------------------------------------------------------------------------------------------

void
    UCk_Utils_SceneNode_UE::
    Log_ProvenanceCensus(
        const FCk_Handle& InContext,
        const FString& InLabel,
        int32 InTopN)
{
#if !UE_BUILD_SHIPPING
    if (NOT ck::IsValid(InContext))
    {
        UE_LOG(CkEcsExt, Display, TEXT("[SceneNodeCensus] label=\"%s\" invalid context"), *InLabel);
        return;
    }

    const auto TopN = FMath::Clamp(InTopN, 1, 20);
    const auto Registry = InContext.Get_RegistryView();

    struct FCounts
    {
        int32 Total = 0;
        int32 Layers[ck::MaxSceneNodeLayers] = {};
        int32 UnrealComponent = 0;
        int32 UnrealMeshSocket = 0;
        int32 RootComponent = 0;
        int32 MeshSocket = 0;
        int32 Bare = 0;
        int32 ExternallyDriven = 0;
        int32 UnrealComponentExternal = 0;
        int32 UnrealMeshSocketExternal = 0;
        int32 RootComponentExternal = 0;
        int32 MeshSocketExternal = 0;
        int32 BareExternal = 0;
        int32 PropagationState = 0;
        int32 PropagationQueued = 0;
    } Counts;

    auto CompositeBuckets = TMap<FString, int32>{};
    auto NodeMarginals = TMap<FString, int32>{};
    auto ParentMarginals = TMap<FString, int32>{};

    const auto GetName = [](const FCk_Handle& InHandle) -> FString
    {
        if (NOT ck::IsValid(InHandle))
        { return TEXT("<invalid>"); }

        const auto Name = UCk_Utils_Handle_UE::Get_DebugName(InHandle);
        return Name.IsNone()
            ? ck::Format_UE(TEXT("#{}"), InHandle.Get_Entity().Get_EntityNumber())
            : Name.ToString();
    };

    const auto CensusNode = [&](const FCk_Entity InEntity,
                                const ck::SceneNodeParent& InParent)
    {
        const auto Node = FCk_Handle{InEntity, Registry.Get_RegistryHandle()};
        const auto Layer = Get_Depth(Node).Get(INDEX_NONE);
        const auto Parent = FCk_Handle{InParent.Get_Entity().Get_Entity(), Registry.Get_RegistryHandle()};
        const auto NodeName = GetName(Node);
        const auto ParentName = GetName(Parent);
        const auto IsExternallyDriven = Node.Has<ck::FTag_Transform_ExternallyDriven>();

        auto AnchorKind = FString{TEXT("Bare")};
        if (Node.Has<ck::FFragment_SceneNode_UnrealAnchor>())
        {
            const auto& Anchor = Node.Get<ck::FFragment_SceneNode_UnrealAnchor>();
            AnchorKind = Anchor.Get_Socket().IsNone() ? TEXT("UnrealComponent") : TEXT("UnrealMeshSocket");
        }
        else if (Node.Has<ck::FFragment_Transform_RootComponent>())
        { AnchorKind = TEXT("RootComponent"); }
        else if (Node.Has<ck::FFragment_Transform_MeshSocket>())
        { AnchorKind = TEXT("MeshSocket"); }

        ++Counts.Total;
        if (Layer != INDEX_NONE)
        { ++Counts.Layers[Layer]; }
        if (IsExternallyDriven)
        { ++Counts.ExternallyDriven; }
        if (Node.Has<ck::FFragment_SceneNode_PropagationState>())
        { ++Counts.PropagationState; }
        if (Node.Has<ck::FTag_SceneNode_PropagationQueued>())
        { ++Counts.PropagationQueued; }

        if (AnchorKind == TEXT("UnrealComponent"))
        {
            ++Counts.UnrealComponent;
            Counts.UnrealComponentExternal += IsExternallyDriven ? 1 : 0;
        }
        else if (AnchorKind == TEXT("UnrealMeshSocket"))
        {
            ++Counts.UnrealMeshSocket;
            Counts.UnrealMeshSocketExternal += IsExternallyDriven ? 1 : 0;
        }
        else if (AnchorKind == TEXT("RootComponent"))
        {
            ++Counts.RootComponent;
            Counts.RootComponentExternal += IsExternallyDriven ? 1 : 0;
        }
        else if (AnchorKind == TEXT("MeshSocket"))
        {
            ++Counts.MeshSocket;
            Counts.MeshSocketExternal += IsExternallyDriven ? 1 : 0;
        }
        else
        {
            ++Counts.Bare;
            Counts.BareExternal += IsExternallyDriven ? 1 : 0;
        }

        ++CompositeBuckets.FindOrAdd(FString::Printf(TEXT("L%d|%s|%s|%s|external=%d"),
            Layer, *NodeName, *ParentName, *AnchorKind, IsExternallyDriven ? 1 : 0));
        ++NodeMarginals.FindOrAdd(FString::Printf(TEXT("L%d|%s"), Layer, *NodeName));
        ++ParentMarginals.FindOrAdd(FString::Printf(TEXT("L%d|%s"), Layer, *ParentName));
    };

    Registry.View<ck::FTag_SceneNode_Layered, ck::FFragment_SceneNode, ck::SceneNodeParent,
                  ck::FFragment_Transform, ck::TExclude<ck::FTag_DestroyEntity_Initiate>>().ForEach(
        [&](const FCk_Entity Entity, const auto&, const ck::SceneNodeParent& Parent, const auto&)
        { CensusNode(Entity, Parent); });

    auto LayerCounts = FString{};
    for (auto LayerIndex = 0; LayerIndex < ck::MaxSceneNodeLayers; ++LayerIndex)
    { LayerCounts += FString::Printf(TEXT(" L%d=%d"), LayerIndex, Counts.Layers[LayerIndex]); }

    const auto LogTop = [TopN, &InLabel](const TCHAR* InKind, const TMap<FString, int32>& InBuckets)
    {
        auto Rows = TArray<TPair<FString, int32>>{};
        Rows.Reserve(InBuckets.Num());
        for (const auto& Pair : InBuckets)
        { Rows.Emplace(Pair.Key, Pair.Value); }
        Rows.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
        {
            return A.Value != B.Value ? A.Value > B.Value : A.Key < B.Key;
        });

        const auto NumRows = FMath::Min(TopN, Rows.Num());
        auto ShownCount = int32{0};
        for (int32 Index = 0; Index < NumRows; ++Index)
        {
            const auto& Row = Rows[Index];
            ShownCount += Row.Value;
            UE_LOG(CkEcsExt, Display, TEXT("[SceneNodeCensus] label=\"%s\" %s rank=%d count=%d key=\"%s\""),
                *InLabel, InKind, Index + 1, Row.Value, *Row.Key);
        }

        auto TotalCount = int32{0};
        for (const auto& Pair : InBuckets)
        { TotalCount += Pair.Value; }
        UE_LOG(CkEcsExt, Display,
            TEXT("[SceneNodeCensus] label=\"%s\" %s distinct=%d shown=%d shownCount=%d otherCount=%d"),
            *InLabel, InKind, Rows.Num(), NumRows, ShownCount, TotalCount - ShownCount);
    };

    UE_LOG(CkEcsExt, Display,
        TEXT("[SceneNodeCensus] label=\"%s\" total=%d%s anchors={UnrealComponent=%d UnrealMeshSocket=%d RootComponent=%d MeshSocket=%d Bare=%d} externallyDriven=%d propagation={state=%d queued=%d} topN=%d"),
        *InLabel, Counts.Total, *LayerCounts,
        Counts.UnrealComponent, Counts.UnrealMeshSocket, Counts.RootComponent, Counts.MeshSocket, Counts.Bare,
        Counts.ExternallyDriven, Counts.PropagationState, Counts.PropagationQueued, TopN);
    UE_LOG(CkEcsExt, Display,
        TEXT("[SceneNodeCensus] label=\"%s\" anchorExternal={UnrealComponent=%d/%d UnrealMeshSocket=%d/%d RootComponent=%d/%d MeshSocket=%d/%d Bare=%d/%d}"),
        *InLabel,
        Counts.UnrealComponentExternal, Counts.UnrealComponent,
        Counts.UnrealMeshSocketExternal, Counts.UnrealMeshSocket,
        Counts.RootComponentExternal, Counts.RootComponent,
        Counts.MeshSocketExternal, Counts.MeshSocket,
        Counts.BareExternal, Counts.Bare);
    LogTop(TEXT("composite"), CompositeBuckets);
    LogTop(TEXT("node"), NodeMarginals);
    LogTop(TEXT("parent"), ParentMarginals);
#else
    static_cast<void>(InContext);
    static_cast<void>(InLabel);
    static_cast<void>(InTopN);
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_SceneNode_UE::
    Add(
        FCk_Handle_Transform& InHandle,
        FCk_Handle_Transform& InAttachTo,
        FTransform InLocalTransform)
    -> FCk_Handle_SceneNode
{
    // Parent-driven: anchor-bound children follow the scene-node parent like Unreal AttachToComponent.
    return DoAdd(InHandle, InAttachTo, InLocalTransform, ECk_SceneNode_DrivenBy::Parent);
}

auto
    UCk_Utils_SceneNode_UE::
    Create(
        FCk_Handle_Transform& InOwner,
        FTransform InLocalTransform)
    -> FCk_Handle_SceneNode
{
    auto SceneNodeEntity = DoTryCreate_NodeEntity(InOwner,
        ck::Format_UE(TEXT("SceneNode({})"), ck_scene_node_utils::Get_SceneNodeAnchorLabel(InOwner)));

    if (ck::Is_NOT_Valid(SceneNodeEntity))
    { return {}; }

    const auto& OwnerTransform = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(InOwner);

    auto SceneNodeWithTransform = UCk_Utils_Transform_UE::Add(SceneNodeEntity, InLocalTransform * OwnerTransform, ECk_Replication::DoesNotReplicate);

    return DoAdd(SceneNodeWithTransform, InOwner, InLocalTransform, ECk_SceneNode_DrivenBy::Anchor);
}

auto
    UCk_Utils_SceneNode_UE::
    CreateAndAttachToUnrealComponent(
        FCk_Handle_Transform& InAttachTo,
        USceneComponent* InSceneComponent,
        FTransform InLocalTransform)
    -> FCk_Handle_SceneNode
{
    CK_ENSURE_IF_NOT(ck::IsValid(InSceneComponent),
        TEXT("Unable to attach SceneNode to [{}]: the Unreal SceneComponent is INVALID"), InAttachTo)
    { return {}; }

    auto SceneNodeEntity = DoTryCreate_NodeEntity(InAttachTo, ck::Format_UE(TEXT("SceneNode({} > {})"),
        ck_scene_node_utils::Get_SceneNodeAnchorLabel(InAttachTo), GetNameSafe(InSceneComponent)));

    if (ck::Is_NOT_Valid(SceneNodeEntity))
    { return {}; }

    // Read-only follower: a plain Transform seeded at the composed world plus the SceneNode-owned anchor
    // fragment — NOT FFragment_Transform_RootComponent, so SyncFromActor / SyncToActor never engage.
    auto SceneNodeWithTransform = UCk_Utils_Transform_UE::Add(SceneNodeEntity, InLocalTransform * InSceneComponent->GetComponentTransform(), ECk_Replication::DoesNotReplicate);
    SceneNodeWithTransform.Add<ck::FFragment_SceneNode_UnrealAnchor>(InSceneComponent, NAME_None);

    return DoAdd(SceneNodeWithTransform, InAttachTo, InLocalTransform, ECk_SceneNode_DrivenBy::Anchor);
}

auto
    UCk_Utils_SceneNode_UE::
    CreateAndAttachToUnrealMesh(
        FCk_Handle_Transform& InAttachTo,
        const UMeshComponent* InMeshComponent,
        FName InSocketName,
        FTransform InLocalTransform)
    -> FCk_Handle_SceneNode
{
    CK_ENSURE_IF_NOT(ck::IsValid(InMeshComponent),
        TEXT("Unable to attach SceneNode to [{}]: the Unreal MeshComponent is INVALID"), InAttachTo)
    { return {}; }

    CK_ENSURE_IF_NOT(InMeshComponent->DoesSocketExist(InSocketName),
        TEXT("Socket [{}] does NOT exist on MeshComponent [{}]. If you wanted the component's world transform "
             "(no socket), use CreateAndAttachToUnrealComponent instead; otherwise check the name for typos."),
        InSocketName, InMeshComponent)
    { return {}; }

    auto SceneNodeEntity = DoTryCreate_NodeEntity(InAttachTo, ck::Format_UE(TEXT("SceneNode({} > {}:{})"),
        ck_scene_node_utils::Get_SceneNodeAnchorLabel(InAttachTo), GetNameSafe(InMeshComponent), InSocketName));

    if (ck::Is_NOT_Valid(SceneNodeEntity))
    { return {}; }

    // Read-only follower (see CreateAndAttachToUnrealComponent for the fragment rationale).
    auto SceneNodeWithTransform = UCk_Utils_Transform_UE::Add(SceneNodeEntity, InLocalTransform * InMeshComponent->GetSocketTransform(InSocketName), ECk_Replication::DoesNotReplicate);
    SceneNodeWithTransform.Add<ck::FFragment_SceneNode_UnrealAnchor>(InMeshComponent, InSocketName);

    if (InMeshComponent->IsA<USkinnedMeshComponent>())
    { SceneNodeWithTransform.Add<ck::FTag_SceneNode_LateAnchor>(); }

    return DoAdd(SceneNodeWithTransform, InAttachTo, InLocalTransform, ECk_SceneNode_DrivenBy::Anchor);
}

auto
    UCk_Utils_SceneNode_UE::
    DoAdd(
        FCk_Handle_Transform& InHandle,
        FCk_Handle_Transform& InAttachTo,
        FTransform InLocalTransform,
        ECk_SceneNode_DrivenBy InDrivenBy)
    -> FCk_Handle_SceneNode
{
    CK_ENSURE_IF_NOT(ck::IsValid(InHandle),
        TEXT("InHandle [{}] is INVALID. Unable to proceed with SceneNode attaching to [{}]"), InHandle, InAttachTo)
    { return {}; }

    CK_ENSURE_IF_NOT(ck::IsValid(InAttachTo),
        TEXT("InAttachTo [{}] is INVALID. Unable to proceed with adding the SceneNode feature to [{}]"), InAttachTo, InHandle)
    { return {}; }

    CK_ENSURE_IF_NOT(NOT Has(InHandle),
        TEXT("SceneNode Add refused: [{}] already is a scene node (under [{}]). Request_Detach it first, then Add it "
             "under [{}]"),
        InHandle, Get_Parent(Cast(InHandle)), InAttachTo)
    { return {}; }

    // Decided before the first write, so a refusal leaves every node's layer and parent as it was.
    const auto MyLayerIndex = ck_scene_node_utils::TryGet_AttachDepth_LogIfRefused(InHandle, InAttachTo);

    if (NOT MyLayerIndex.IsSet())
    { return {}; }

    AssignLayerByIndex(InHandle, MyLayerIndex.GetValue());
    PropagateLayerToChildren(InHandle, MyLayerIndex.GetValue() + 1);

    InHandle.Add<ck::FFragment_SceneNode>(InLocalTransform);

    ck::USceneNodeParent_Utils::AddOrReplace(InHandle, InAttachTo);
    InHandle.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
    InAttachTo.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
    ck::FUtils_SceneNodePropagation::Queue(InHandle);

    if (InDrivenBy == ECk_SceneNode_DrivenBy::Parent)
    {
        const auto HasAnchor = InHandle.Has_Any<ck::FFragment_Transform_RootComponent, ck::FFragment_Transform_MeshSocket>();
        const auto AnchorIsMovable = InHandle.Has<ck::FTag_Transform_Movable>();

        CK_ENSURE_IF_NOT(NOT HasAnchor || AnchorIsMovable,
            TEXT("SceneNode::Add on entity [{}]: child has an anchor (RootComponent / MeshSocket) that is "
                 "NOT Movable. The parent-driven transform will update the ECS Transform fragment but "
                 "FProcessor_Transform_SyncToActor (which requires FTag_Transform_Movable) will NOT push "
                 "it onto the anchor — the visible actor will stay glued to its spawn pose. Set the "
                 "RootComponent's Mobility to EComponentMobility::Movable, or use a Create*-flavored "
                 "overload if you wanted anchor-authoritative behavior."),
            InHandle)
        { }

        InHandle.AddOrGet<ck::FTag_Transform_ExternallyDriven>();

        UCk_Utils_Transform_UE::OnSceneNode_AttachedToParent(InHandle);
    }

    auto SceneNodeHandle = Cast(InHandle);

    ck::FUtils_RecordOfSceneNodes::AddIfMissing(InAttachTo);
    ck::FUtils_RecordOfSceneNodes::Request_Connect(InAttachTo, SceneNodeHandle, ECk_Record_LabelRequirementPolicy::Optional);

    return SceneNodeHandle;
}

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_SceneNode_UE, FCk_Handle_SceneNode,
    ck::SceneNodeParent, ck::FFragment_SceneNode);

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_SceneNode_UE::
    Get_Offset(
        const FCk_Handle_SceneNode& InSceneNode)
    -> FTransform
{
    return InSceneNode.Get<ck::FFragment_SceneNode>().Get_RelativeTransform();
}

auto
    UCk_Utils_SceneNode_UE::
    Get_Parent(
        const FCk_Handle_SceneNode& InSceneNode)
    -> FCk_Handle_Transform
{
    return ck::USceneNodeParent_Utils::Get_StoredEntity_AsTypeSafe<FCk_Handle_Transform>(InSceneNode);
}

auto
    UCk_Utils_SceneNode_UE::
    Get_DriverWorldTransform(
        const FCk_Handle_SceneNode& InSceneNode)
    -> FTransform
{
    if (InSceneNode.Has<ck::FFragment_SceneNode_UnrealAnchor>())
    {
        const auto AnchorWorld = ck::FProcessor_SceneNode_FollowUnrealAnchor::Get_AnchorWorldTransform(
            InSceneNode.Get<ck::FFragment_SceneNode_UnrealAnchor>());

        return AnchorWorld.Get(FTransform::Identity);
    }

    const auto Parent = Get_Parent(InSceneNode);

    if (ck::Is_NOT_Valid(Parent))
    { return FTransform::Identity; }

    return UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Parent);
}

auto
    UCk_Utils_SceneNode_UE::
    Request_UpdateOffset(
        FCk_Handle_SceneNode& InSceneNode,
        const FCk_Request_SceneNode_UpdateRelativeTransform& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_SceneNode
{
    if (InDelegate.IsBound())
    { InRequest.Set_CompletionDelegate(InDelegate); }

    InSceneNode.AddOrGet<ck::FFragment_SceneNode_Requests>()._Requests.Emplace(InRequest);
    return InSceneNode;
}

auto
    UCk_Utils_SceneNode_UE::
    Request_Detach(
        FCk_Handle_SceneNode& InSceneNode,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_SceneNode
{
    const auto SceneNodeIsValid = ck::IsValid(InSceneNode);

    CK_ENSURE_IF_NOT(SceneNodeIsValid,
        TEXT("InSceneNode [{}] is INVALID. Unable to detach"), InSceneNode)
    {
        InDelegate.ExecuteIfBound(InSceneNode, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InSceneNode;
    }

    // The Transform fragment already holds the composed world pose, so removing parent / current / layer
    // below stops further composition and the world transform stays put without an explicit write.
    auto NodeAsTransform = UCk_Utils_Transform_UE::Cast(InSceneNode);

    auto Parent = ck::USceneNodeParent_Utils::Get_StoredEntity_AsTypeSafe<FCk_Handle_Transform>(InSceneNode);
    if (ck::IsValid(Parent))
    { ck::FUtils_RecordOfSceneNodes::Request_Disconnect(Parent, InSceneNode); }

    InSceneNode.Try_Remove<ck::SceneNodeParent>();
    InSceneNode.Try_Remove<ck::FFragment_SceneNode>();
    InSceneNode.Try_Remove<ck::FTag_SceneNode_PropagationQueued>();
    InSceneNode.Try_Remove<ck::FTag_SceneNode_LateAnchor>();

    RemoveExistingLayerTag(NodeAsTransform);
    NodeAsTransform.Try_Remove<ck::FTag_Transform_ExternallyDriven>();

    // The node is a plain root now, so its children sit at depth 0 below it; the walk can only move them shallower.
    PropagateLayerToChildren(NodeAsTransform, 0);

    UCk_Utils_Transform_UE::OnSceneNode_Detached(NodeAsTransform);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InSceneNode, ECk_Request_OperationResult::Succeeded);

    return InSceneNode;
}

auto
    UCk_Utils_SceneNode_UE::
    ForEach_SceneNode(
        FCk_Handle_Transform& InHandle,
        const FInstancedStruct& InOptionalPayload,
        const FCk_Lambda_InHandle& InDelegate)
    -> TArray<FCk_Handle_SceneNode>
{
    auto Abilities = TArray<FCk_Handle_SceneNode>{};

    ForEach_SceneNode
    (
        InHandle,
        [&](const FCk_Handle_SceneNode& InSceneNode)
        {
            if (InDelegate.IsBound())
            { InDelegate.Execute(InSceneNode, InOptionalPayload); }
            else
            { Abilities.Emplace(InSceneNode); }
        }
    );

    return Abilities;
}

auto
    UCk_Utils_SceneNode_UE::
    ForEach_SceneNode(
        FCk_Handle_Transform InHandle,
        const TFunction<void(FCk_Handle_SceneNode)>& InFunc)
    -> void
{
    ck::FUtils_RecordOfSceneNodes::ForEach_ValidEntry(InHandle, InFunc);
}

auto
    UCk_Utils_SceneNode_UE::
    Get_Depth(
        const FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult)
    -> int32
{
    const auto Depth = Get_Depth(InHandle);
    OutResult = Depth.IsSet() ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Depth.Get(0);
}

auto
    UCk_Utils_SceneNode_UE::
    Get_Depth(
        const FCk_Handle& InHandle)
    -> TOptional<int32>
{
    if (ck::Is_NOT_Valid(InHandle) || NOT InHandle.Has<ck::FTag_SceneNode_Layered>())
    { return {}; }

    const auto LayerIndex = ck::algo::FindIndex(ck_scene_node_utils::Get_LayerTags(),
        [&](const ck_scene_node_utils::FLayerTag& InLayerTag) { return InLayerTag._Has(InHandle); });

    if (LayerIndex == INDEX_NONE)
    { return {}; }

    return LayerIndex;
}

auto
    UCk_Utils_SceneNode_UE::
    Get_MaxDepth()
    -> int32
{
    return ck::MaxSceneNodeLayers;
}

auto
    UCk_Utils_SceneNode_UE::
    Get_CanCreateChild(
        const FCk_Handle_Transform& InAttachTo)
    -> bool
{
    if (ck::Is_NOT_Valid(InAttachTo))
    { return false; }

    return ck_scene_node_utils::Get_AttachFit({}, InAttachTo).Get_Fits();
}

auto
    UCk_Utils_SceneNode_UE::
    Get_CanAttach(
        const FCk_Handle_Transform& InNode,
        const FCk_Handle_Transform& InAttachTo)
    -> bool
{
    if (ck::Is_NOT_Valid(InNode) || ck::Is_NOT_Valid(InAttachTo))
    { return false; }

    return ck_scene_node_utils::Get_AttachFit(InNode, InAttachTo).Get_Fits();
}

auto
    UCk_Utils_SceneNode_UE::
    DoTryCreate_NodeEntity(
        FCk_Handle_Transform& InAttachTo,
        const FString& InDebugName)
    -> FCk_Handle
{
    CK_ENSURE_IF_NOT(ck::IsValid(InAttachTo),
        TEXT("InAttachTo [{}] is INVALID. Unable to create a SceneNode [{}] under it"), InAttachTo, InDebugName)
    { return {}; }

    if (NOT ck_scene_node_utils::TryGet_AttachDepth_LogIfRefused({}, InAttachTo).IsSet())
    { return {}; }

    auto NodeEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InAttachTo);
    UCk_Utils_Handle_UE::Set_DebugName(NodeEntity, *InDebugName);

    return NodeEntity;
}

auto
    UCk_Utils_SceneNode_UE::
    RemoveExistingLayerTag(
        FCk_Handle_Transform& InHandle)
    -> void
{
    InHandle.Try_Remove<ck::FTag_SceneNode_RelativeTransformUpdated>();

    if (const auto Depth = Get_Depth(InHandle);
        Depth.IsSet())
    { ck_scene_node_utils::Get_LayerTags()[Depth.GetValue()]._Remove(InHandle); }

    InHandle.Try_Remove<ck::FTag_SceneNode_Layered>();
}

auto
    UCk_Utils_SceneNode_UE::
    AssignLayerByIndex(
        FCk_Handle_Transform& InHandle,
        int32 InLayerIndex)
    -> void
{
    const auto LayerTags = ck_scene_node_utils::Get_LayerTags();

    // Every caller decided the depth through the attach check, or moved the node towards the root.
    CK_ENSURE_IF_NOT(LayerTags.IsValidIndex(InLayerIndex),
        TEXT("SceneNode [{}] was assigned layer [{}] outside [0, {}) without a depth check"),
        InHandle, InLayerIndex, ck::MaxSceneNodeLayers)
    { return; }

    LayerTags[InLayerIndex]._Add(InHandle);
    InHandle.AddOrGet<ck::FTag_SceneNode_Layered>();
    InHandle.AddOrGet<ck::FTag_SceneNode_RelativeTransformUpdated>();
}

auto
    UCk_Utils_SceneNode_UE::
    PropagateLayerToChildren(
        FCk_Handle_Transform& InParent,
        int32 InChildLayerIndex)
    -> void
{
    ForEach_SceneNode(InParent, [InChildLayerIndex](FCk_Handle_SceneNode InChild)
    {
        if (const auto CurrentLayerIndex = Get_Depth(InChild);
            CurrentLayerIndex.IsSet() && CurrentLayerIndex.GetValue() == InChildLayerIndex)
        { return; }

        auto ChildTransform = UCk_Utils_Transform_UE::Cast(InChild);

        RemoveExistingLayerTag(ChildTransform);
        AssignLayerByIndex(ChildTransform, InChildLayerIndex);

        ChildTransform.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
        ck::FUtils_SceneNodePropagation::Queue(ChildTransform);

        PropagateLayerToChildren(ChildTransform, InChildLayerIndex + 1);
    });
}

// --------------------------------------------------------------------------------------------------------------------
