#pragma once

#include "CkSceneNode_Fragment_Data.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Request/CkRequest_Completion.h"

#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"

#include "CkEcs/Net/CkNet_Utils.h"

#include "CkSceneNode_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_SceneNode"))
class CKECSEXT_API UCk_Utils_SceneNode_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_SceneNode_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_SceneNode);

public:
    // Every attach below (Add, Create, CreateAndAttachToUnreal*) puts the node one layer below InAttachTo (depth 0 when
    // InAttachTo is not itself a layered scene node) and, for Add, moves InHandle's existing scene-node subtree with it.
    // An attach that would put any node of that subtree at a depth >= Get_MaxDepth() is refused whole, before anything
    // changes: it logs an error naming the root-to-leaf chain every time, returns an INVALID handle, and leaves every
    // node's layer and parent as they were (Create* creates no entity; Add leaves InHandle not a scene node). Ask
    // Get_CanCreateChild / Get_CanAttach first to fail without the error; ask Get_CanAttach before a re-parent's detach.
    // Add on a handle that already is a scene node is a caller error (an ensure, nothing changes): Request_Detach first.
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Add Feature")
    static FCk_Handle_SceneNode
    Add(
        UPARAM(ref) FCk_Handle_Transform& InHandle,
        UPARAM(ref) FCk_Handle_Transform& InAttachTo,
        FTransform InLocalTransform);

    // InLocalTransform is the offset from the owner (KeepRelative semantics): the node's world becomes
    // InLocalTransform * ownerWorld and tracks the owner as it moves. Identity == glued exactly to the
    // owner. Offset is runtime-mutable via Request_UpdateOffset.
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Create")
    static FCk_Handle_SceneNode
    Create(
        UPARAM(ref) FCk_Handle_Transform& InAttachTo,
        FTransform InLocalTransform);

    // InLocalTransform is the offset from the anchor (KeepRelative semantics): the node's world becomes
    // InLocalTransform * componentWorld and tracks the component as it moves. The node is a read-only
    // follower — it never writes back onto the component (unlike an actor-bridge Transform). Identity ==
    // glued exactly to the component. Offset is runtime-mutable via Request_UpdateOffset.
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Create (AttachTo Unreal Component)")
    static FCk_Handle_SceneNode
    CreateAndAttachToUnrealComponent(
        UPARAM(ref) FCk_Handle_Transform& InAttachTo,
        USceneComponent* InSceneComponent,
        FTransform InLocalTransform);

    // InLocalTransform is the offset from the socket (KeepRelative semantics): the node's world becomes
    // InLocalTransform * socketWorld and tracks the socket as it moves. Read-only follower; identity ==
    // glued exactly to the socket. Offset is runtime-mutable via Request_UpdateOffset.
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Create (AttachTo Unreal Mesh Socket)")
    static FCk_Handle_SceneNode
    CreateAndAttachToUnrealMesh(
        UPARAM(ref) FCk_Handle_Transform& InAttachTo,
        const UMeshComponent* InMeshComponent,
        FName InSocketName,
        FTransform InLocalTransform);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Has Feature")
    static bool
    Has(
        const FCk_Handle& InHandle);

    // The node's depth: 0 under a parent that is not itself a layered scene node, its parent's depth + 1 otherwise,
    // always < Get_MaxDepth(). Failed (unset in C++) when InHandle holds no layer: not a scene node, or detached.
    // Depth is per machine: two machines that compose different chains above a node can disagree.
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Get Depth",
              meta = (ExpandEnumAsExecs = "OutResult"))
    static int32
    Get_Depth(
        const FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);
    static auto
    Get_Depth(
        const FCk_Handle& InHandle) -> TOptional<int32>;

    // How many depths exist (ck::MaxSceneNodeLayers): a node's depth runs from 0 to Get_MaxDepth() - 1.
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Get Max Depth")
    static int32
    Get_MaxDepth();

    // True when ONE new node created under InAttachTo would fit: InAttachTo is valid and either not a layered scene
    // node (a plain transform's child lands at depth 0) or its depth + 1 < Get_MaxDepth(). False for an INVALID
    // InAttachTo, which Create refuses too. A quiet query: no log, no ensure. It answers for creating a single node
    // only; an existing node brings its subtree, which Get_CanAttach counts.
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Can Create Child")
    static bool
    Get_CanCreateChild(
        const FCk_Handle_Transform& InAttachTo);

    // True when Add(InNode, InAttachTo, ...) would fit: InNode one layer below InAttachTo and its whole existing
    // scene-node subtree below it (the check Add itself makes: the node's depth plus its subtree's height stays below
    // Get_MaxDepth()). False when either handle is INVALID, which Add refuses too. A quiet query: no log, no ensure. A re-parent is Request_Detach then Add, and a refused Add
    // leaves the node detached, so a caller that must not lose the attachment asks this BEFORE the detach.
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] Can Attach")
    static bool
    Get_CanAttach(
        const FCk_Handle_Transform& InNode,
        const FCk_Handle_Transform& InAttachTo);

    // Development-only, one-shot runtime attribution for SceneNode frontier investigations. The census only
    // reads the current registry and logs bounded aggregate/provenance rows; it never retains a registry view,
    // fragment reference, or entity handle beyond this call.
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|SceneNode|Debug",
              DisplayName="[Ck][SceneNode] Log Provenance Census",
              meta=(DevelopmentOnly))
    static void
    Log_ProvenanceCensus(
        const FCk_Handle& InContext,
        const FString& InLabel,
        int32 InTopN = 20);

private:
    static FCk_Handle_SceneNode
    DoAdd(
        FCk_Handle_Transform& InHandle,
        FCk_Handle_Transform& InAttachTo,
        FTransform InLocalTransform,
        ECk_SceneNode_DrivenBy InDrivenBy);

    UFUNCTION(BlueprintCallable,
        Category = "Ck|Utils|SceneNode",
        DisplayName="[Ck][SceneNode] Cast",
        meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_SceneNode
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure,
        Category = "Ck|Utils|SceneNode",
        DisplayName="[Ck][SceneNode] Handle -> SceneNode Handle",
        meta = (CompactNodeTitle = "<AsSceneNode>", BlueprintAutocast))
    static FCk_Handle_SceneNode
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure,
        DisplayName = "[Ck] Get Invalid SceneNode Handle",
        Category = "Ck|Utils|SceneNode",
        meta = (CompactNodeTitle = "INVALID_SceneNodeHandle", Keywords = "make"))
    static FCk_Handle_SceneNode
    Get_InvalidHandle() { return {}; };

public:
    UFUNCTION(BlueprintPure,
        Category = "Ck|Utils|SceneNode",
        DisplayName="[Ck][SceneNode] Get Offset")
    static FTransform
    Get_Offset(
        const FCk_Handle_SceneNode& InSceneNode);

    UFUNCTION(BlueprintPure,
    Category = "Ck|Utils|SceneNode",
        DisplayName = "[Ck][SceneNode] Get Parent")
    static FCk_Handle_Transform
    Get_Parent(
            const FCk_Handle_SceneNode& InSceneNode);

    // The world frame this node composes against: the anchor component/socket world for an anchor-driven
    // node (CreateAndAttachToUnreal*), otherwise the scene-node parent's current world transform. Identity
    // when the anchor component is gone. Read-only; a feature that derives motion from "whatever drives this
    // node" (CkSway) reads this instead of guessing the parent.
    UFUNCTION(BlueprintPure,
        Category = "Ck|Utils|SceneNode",
        DisplayName = "[Ck][SceneNode] Get Driver World Transform")
    static FTransform
    Get_DriverWorldTransform(
        const FCk_Handle_SceneNode& InSceneNode);

public:
    UFUNCTION(BlueprintCallable,
        Category = "Ck|Utils|SceneNode",
        DisplayName="[Ck][SceneNode] Request UpdateOffset",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_SceneNode
    Request_UpdateOffset(
        UPARAM(ref) FCk_Handle_SceneNode& InSceneNode,
        const FCk_Request_SceneNode_UpdateRelativeTransform& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

    // Severs the scene-node's parent link, keeping the current world transform. After detach,
    // TProcessor_SceneNode_Update no longer iterates this entity, so the Transform fragment stays where it
    // was and can be authoritatively set by downstream callers. The node becomes a plain root, so its scene-node
    // subtree is re-layered from depth 0 below it.
    // Immediate mutator: the links are removed inline and nothing is enqueued, so the completion
    // delegate fires synchronously on the caller's stack.
    UFUNCTION(BlueprintCallable,
        Category = "Ck|Utils|SceneNode",
        DisplayName="[Ck][SceneNode] Request Detach (Keep World)",
        meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_SceneNode
    Request_Detach(
        UPARAM(ref) FCk_Handle_SceneNode& InSceneNode,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

public:
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|SceneNode",
              DisplayName="[Ck][SceneNode] For Each SceneNode",
              meta=(AutoCreateRefTerm="InOptionalPayload, InDelegate", KeyWords = "get,all,scenenodes"))
    static TArray<FCk_Handle_SceneNode>
    ForEach_SceneNode(
        UPARAM(ref) FCk_Handle_Transform& InHandle,
        const FInstancedStruct& InOptionalPayload,
        const FCk_Lambda_InHandle& InDelegate);
    static auto
    ForEach_SceneNode(
        FCk_Handle_Transform InHandle,
        const TFunction<void(FCk_Handle_SceneNode)>& InFunc) -> void;

private:
    // The entity a Create* builds its node on, named InDebugName. INVALID, with the overflow error logged and no
    // entity created, when one more node does not fit under InAttachTo.
    static auto
    DoTryCreate_NodeEntity(
        FCk_Handle_Transform& InAttachTo,
        const FString& InDebugName) -> FCk_Handle;

    // Gives every scene-node child of InParent the layer InChildLayerIndex and its descendants the layers below.
    // Only for a subtree that fits (the attach check, or a move towards the root).
    static auto
    PropagateLayerToChildren(
        FCk_Handle_Transform& InParent,
        int32 InChildLayerIndex) -> void;

    static auto
    RemoveExistingLayerTag(
        FCk_Handle_Transform& InHandle) -> void;

    static auto
    AssignLayerByIndex(
        FCk_Handle_Transform& InHandle,
        int32 InLayerIndex) -> void;
};

// --------------------------------------------------------------------------------------------------------------------
