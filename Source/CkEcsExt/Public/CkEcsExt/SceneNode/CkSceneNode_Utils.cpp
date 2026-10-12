#include "CkSceneNode_Utils.h"

#include "CkEcsExt/CkEcsExt_Log.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Handle/CkHandle_ReadOnly.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkRecord/RecordEntry/CkRecordEntry_Fragment.h"

#include "CkProfile/Stats/CkCpuWork.h"

#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include <cfloat>

// --------------------------------------------------------------------------------------------------------------------

static auto
Get_SceneNodeAnchorLabel(
    const FCk_Handle& InHandle) -> FString
{
    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InHandle);
    if (NOT DebugName.IsNone())
    { return DebugName.ToString(); }

    return ck::Format_UE(TEXT("#{}"), InHandle.Get_Entity().Get_EntityNumber());
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_scene_node_fresh_anchor
{
    static bool IsFinite(const FTransform& InTransform)
    {
        // ContainsNaN reads raw scalar storage and rejects non-finite values before any
        // diagnostic transform getter or composition can be invoked.
        if (InTransform.ContainsNaN() || NOT InTransform.IsValid()) { return false; }
        const auto P = InTransform.GetTranslation();
        const auto Q = InTransform.GetRotation();
        const auto S = InTransform.GetScale3D();
        return Q.IsNormalized() &&
            FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z) &&
            FMath::IsFinite(Q.X) && FMath::IsFinite(Q.Y) &&
            FMath::IsFinite(Q.Z) && FMath::IsFinite(Q.W) &&
            FMath::IsFinite(S.X) && FMath::IsFinite(S.Y) && FMath::IsFinite(S.Z);
    }

    static bool IsWithinSafeCompositionEnvelope(const FTransform& InTransform)
    {
        // A derived numeric headroom bound for UE transform composition, not a game-unit limit.
        // The nested square root leaves room for the products/sums in rotation, scale, and
        // translation composition, including negative and zero scale components.
        if (NOT IsFinite(InTransform)) { return false; }
        const double Bound = FMath::Sqrt(FMath::Sqrt(DBL_MAX)) / 32.0;
        const auto P = InTransform.GetTranslation();
        const auto S = InTransform.GetScale3D();
        return FMath::Abs(P.X) <= Bound && FMath::Abs(P.Y) <= Bound &&
            FMath::Abs(P.Z) <= Bound && FMath::Abs(S.X) <= Bound &&
            FMath::Abs(S.Y) <= Bound && FMath::Abs(S.Z) <= Bound;
    }

    static bool IsLiveComponentInRegistry(
        const USceneComponent* InComponent, const FCk_Handle& InParent)
    {
        if (NOT IsValid(InComponent) ||
            InComponent->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
            NOT InComponent->IsRegistered())
        { return false; }
        auto* const OwnerActor = InComponent->GetOwner();
        if (OwnerActor != nullptr &&
            (NOT IsValid(OwnerActor) || OwnerActor->IsActorBeingDestroyed()))
        { return false; }
        auto* const World = InComponent->GetWorld();
        if (NOT IsValid(World) || World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        { return false; }
        const auto WorldRoot = UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*World);
        return ck::IsValid(WorldRoot) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(WorldRoot,
                ECk_EntityLifetime_DestructionPhase::BeginDestroy) &&
            WorldRoot.Get_RegistryView().Get_RegistryHandle() ==
                InParent.Get_RegistryView().Get_RegistryHandle();
    }

    static bool HasExactLiveActorBridge(const FCk_Handle& InEntity)
    {
        AActor* OwningActor = nullptr;
        if (UCk_Utils_OwningActor_UE::Has(InEntity))
        {
            OwningActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(InEntity);
            if (NOT IsValid(OwningActor) || OwningActor->IsActorBeingDestroyed())
            { return false; }
            auto* const ActorWorld = OwningActor->GetWorld();
            if (NOT IsValid(ActorWorld) ||
                ActorWorld->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
                UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*ActorWorld) !=
                    UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InEntity))
            { return false; }
        }
        if (InEntity.Has<ck::FFragment_Transform_RootComponent>())
        {
            const auto* const Root = InEntity.Get<ck::FFragment_Transform_RootComponent>()
                .Get_RootComponent().Get();
            if (NOT IsLiveComponentInRegistry(Root, InEntity) ||
                (OwningActor != nullptr && Root->GetOwner() != OwningActor) ||
                NOT IsWithinSafeCompositionEnvelope(Root->GetComponentTransform()))
            { return false; }
        }
        else if (OwningActor != nullptr && OwningActor->GetRootComponent() != nullptr)
        {
            // An actor-backed Transform without its actual root bridge is incomplete.
            return false;
        }
        return true;
    }

    static bool HasLiveRuntimeWorld(const FCk_Handle& InParent)
    {
        const auto WorldRoot = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InParent);
        if (NOT ck::IsValid(WorldRoot) ||
            UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(WorldRoot,
                ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
            NOT WorldRoot.Has<TWeakObjectPtr<UWorld>>())
        { return false; }
        auto* const World = WorldRoot.Get<TWeakObjectPtr<UWorld>>().Get();
        if (NOT IsValid(World) || World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
            NOT World->IsGameWorld() || World->bIsTearingDown)
        { return false; }
        const auto* const Subsystem = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        if (NOT IsValid(Subsystem) ||
            Subsystem->Get_LoadHold() == ECk_EcsWorld_LoadHold::Teardown ||
            UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*World) != WorldRoot)
        { return false; }
        return WorldRoot.Get_RegistryView().Get_RegistryHandle() ==
            InParent.Get_RegistryView().Get_RegistryHandle();
    }

    static int32 CountLayers(const FCk_Handle& InHandle)
    {
        return int32(InHandle.Has<ck::FTag_SceneNode_Layer0>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer1>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer2>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer3>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer4>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer5>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer6>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer7>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer8>()) +
            int32(InHandle.Has<ck::FTag_SceneNode_Layer9>());
    }

    static int32 GetOnlyLayer(const FCk_Handle& H)
    {
        if (H.Has<ck::FTag_SceneNode_Layer0>()) { return 0; }
        if (H.Has<ck::FTag_SceneNode_Layer1>()) { return 1; }
        if (H.Has<ck::FTag_SceneNode_Layer2>()) { return 2; }
        if (H.Has<ck::FTag_SceneNode_Layer3>()) { return 3; }
        if (H.Has<ck::FTag_SceneNode_Layer4>()) { return 4; }
        if (H.Has<ck::FTag_SceneNode_Layer5>()) { return 5; }
        if (H.Has<ck::FTag_SceneNode_Layer6>()) { return 6; }
        if (H.Has<ck::FTag_SceneNode_Layer7>()) { return 7; }
        if (H.Has<ck::FTag_SceneNode_Layer8>()) { return 8; }
        if (H.Has<ck::FTag_SceneNode_Layer9>()) { return 9; }
        return -1;
    }

    static bool TryGetExactLiveParentChain(
        const FCk_Handle_Transform& InParent,
        TArray<FCk_Handle_Transform>& OutAncestors)
    {
        OutAncestors.Reset();
        if (NOT ck::IsValid(InParent)) { return false; }
        TArray<FCk_Handle_Transform> Candidate;
        const auto Registry = InParent.Get_RegistryView().Get_RegistryHandle();
        auto ExpectedLayer = GetOnlyLayer(InParent);
        FCk_Handle Current = InParent;
        for (int32 Depth = 0; Depth <= 10; ++Depth)
        {
            if (NOT ck::IsValid(Current) ||
                Current.Get_RegistryView().Get_RegistryHandle() != Registry ||
                UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(Current,
                    ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
                NOT Current.Has<ck::FFragment_Transform>() ||
                NOT HasExactLiveActorBridge(Current) ||
                NOT IsFinite(Current.Get<ck::FFragment_Transform>().Get_Transform()))
            { return false; }
            if (Current.Has<ck::FFragment_RecordOfEntityExtensions>())
            { return false; }
            Candidate.Add(UCk_Utils_Transform_UE::Cast(Current));
            const auto Layers = CountLayers(Current);
            if (Layers > 1 ||
                Current.Has<ck::FFragment_SceneNode>() != (Layers == 1))
            { return false; }
            if (Layers == 0)
            {
                if (ExpectedLayer != -1 || Current.Has<ck::SceneNodeParent>() ||
                    Current.Has<ck::FTag_SceneNode_RelativeTransformUpdated>())
                { return false; }
                OutAncestors = MoveTemp(Candidate);
                return true;
            }
            if (NOT Current.Has_All<ck::SceneNodeParent,
                    ck::FFragment_Transform_Previous, ck::FFragment_LifetimeOwner>() ||
                NOT IsFinite(Current.Get<ck::FFragment_Transform_Previous>().Get_Transform()) ||
                NOT IsFinite(Current.Get<ck::FFragment_SceneNode>().Get_RelativeTransform()))
            { return false; }
            if (GetOnlyLayer(Current) != ExpectedLayer) { return false; }
            const auto Next = UCk_Utils_SceneNode_UE::Get_Parent(
                UCk_Utils_SceneNode_UE::Cast(Current));
            if (Next == Current || NOT ck::IsValid(Next) ||
                UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Current) != Next ||
                Next.Get_RegistryView().Get_RegistryHandle() != Registry)
            { return false; }
            Current = Next;
            --ExpectedLayer;
        }
        return false;
    }

    static bool HasExactExistingDirectChildren(
        const FCk_Handle_Transform& InParent, int32 InExpectedChildLayer)
    {
        if (InParent.Has<ck::FFragment_RecordOfEntityExtensions>()) { return false; }
        if (NOT InParent.Has<ck::FFragment_RecordOfSceneNodes>()) { return true; }
        const auto Registry = InParent.Get_RegistryView().Get_RegistryHandle();
        // The validated chain rejects record extensions; this public record read is direct here.
        const auto Entries = ck::FUtils_RecordOfSceneNodes::Get_Entries(InParent);
        for (const auto& Child : Entries)
        {
            if (NOT ck::IsValid(Child) ||
                Child.Get_RegistryView().Get_RegistryHandle() != Registry ||
                UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(Child,
                    ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
                NOT Child.Has_All<ck::FFragment_Transform, ck::FFragment_Transform_Previous,
                    ck::FFragment_SceneNode, ck::SceneNodeParent,
                    ck::FFragment_LifetimeOwner, ck::FFragment_RecordEntry>() ||
                CountLayers(Child) != 1 || GetOnlyLayer(Child) != InExpectedChildLayer ||
                NOT IsFinite(Child.Get<ck::FFragment_Transform>().Get_Transform()) ||
                NOT IsFinite(Child.Get<ck::FFragment_Transform_Previous>().Get_Transform()) ||
                NOT IsFinite(Child.Get<ck::FFragment_SceneNode>().Get_RelativeTransform()))
            { return false; }
            if (UCk_Utils_SceneNode_UE::Get_Parent(Child) != InParent ||
                UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Child) != InParent)
            { return false; }
        }
        return true;
    }

    static void AddLayer(FCk_Handle& InHandle, int32 InLayer)
    {
        switch (InLayer)
        {
            case 0: InHandle.Add<ck::FTag_SceneNode_Layer0>(); break;
            case 1: InHandle.Add<ck::FTag_SceneNode_Layer1>(); break;
            case 2: InHandle.Add<ck::FTag_SceneNode_Layer2>(); break;
            case 3: InHandle.Add<ck::FTag_SceneNode_Layer3>(); break;
            case 4: InHandle.Add<ck::FTag_SceneNode_Layer4>(); break;
            case 5: InHandle.Add<ck::FTag_SceneNode_Layer5>(); break;
            case 6: InHandle.Add<ck::FTag_SceneNode_Layer6>(); break;
            case 7: InHandle.Add<ck::FTag_SceneNode_Layer7>(); break;
            case 8: InHandle.Add<ck::FTag_SceneNode_Layer8>(); break;
            case 9: InHandle.Add<ck::FTag_SceneNode_Layer9>(); break;
            default: checkNoEntry(); break; // Caller has already rejected overflow.
        }
    }

    static void RemoveLayer(FCk_Handle& InHandle, int32 InLayer)
    {
        switch (InLayer)
        {
            case 0: InHandle.Try_Remove<ck::FTag_SceneNode_Layer0>(); break;
            case 1: InHandle.Try_Remove<ck::FTag_SceneNode_Layer1>(); break;
            case 2: InHandle.Try_Remove<ck::FTag_SceneNode_Layer2>(); break;
            case 3: InHandle.Try_Remove<ck::FTag_SceneNode_Layer3>(); break;
            case 4: InHandle.Try_Remove<ck::FTag_SceneNode_Layer4>(); break;
            case 5: InHandle.Try_Remove<ck::FTag_SceneNode_Layer5>(); break;
            case 6: InHandle.Try_Remove<ck::FTag_SceneNode_Layer6>(); break;
            case 7: InHandle.Try_Remove<ck::FTag_SceneNode_Layer7>(); break;
            case 8: InHandle.Try_Remove<ck::FTag_SceneNode_Layer8>(); break;
            case 9: InHandle.Try_Remove<ck::FTag_SceneNode_Layer9>(); break;
            default: checkNoEntry(); break;
        }
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
        int32 Layers[3] = {};
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
                                const ck::SceneNodeParent& InParent,
                                const int32 InLayer)
    {
        const auto Node = FCk_Handle{InEntity, Registry.Get_RegistryHandle()};
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
        ++Counts.Layers[InLayer];
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
            InLayer, *NodeName, *ParentName, *AnchorKind, IsExternallyDriven ? 1 : 0));
        ++NodeMarginals.FindOrAdd(FString::Printf(TEXT("L%d|%s"), InLayer, *NodeName));
        ++ParentMarginals.FindOrAdd(FString::Printf(TEXT("L%d|%s"), InLayer, *ParentName));
    };

    Registry.View<ck::FTag_SceneNode_Layer0, ck::FFragment_SceneNode, ck::SceneNodeParent,
                  ck::FFragment_Transform, ck::TExclude<ck::FTag_DestroyEntity_Initiate>>().ForEach(
        [&](const FCk_Entity Entity, const auto&, const ck::SceneNodeParent& Parent, const auto&)
        { CensusNode(Entity, Parent, 0); });
    Registry.View<ck::FTag_SceneNode_Layer1, ck::FFragment_SceneNode, ck::SceneNodeParent,
                  ck::FFragment_Transform, ck::TExclude<ck::FTag_DestroyEntity_Initiate>>().ForEach(
        [&](const FCk_Entity Entity, const auto&, const ck::SceneNodeParent& Parent, const auto&)
        { CensusNode(Entity, Parent, 1); });
    Registry.View<ck::FTag_SceneNode_Layer2, ck::FFragment_SceneNode, ck::SceneNodeParent,
                  ck::FFragment_Transform, ck::TExclude<ck::FTag_DestroyEntity_Initiate>>().ForEach(
        [&](const FCk_Entity Entity, const auto&, const ck::SceneNodeParent& Parent, const auto&)
        { CensusNode(Entity, Parent, 2); });

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
        TEXT("[SceneNodeCensus] label=\"%s\" total=%d L0=%d L1=%d L2=%d anchors={UnrealComponent=%d UnrealMeshSocket=%d RootComponent=%d MeshSocket=%d Bare=%d} externallyDriven=%d propagation={state=%d queued=%d} topN=%d"),
        *InLabel, Counts.Total, Counts.Layers[0], Counts.Layers[1], Counts.Layers[2],
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

bool UCk_Utils_SceneNode_UE::TryAdd_Anchor(
    FCk_Handle& InRawChild, FCk_Handle_Transform& InAttachTo,
    FTransform InLocalTransform, FCk_Handle_SceneNode& OutSceneNode)
{
    return TryAdd_FreshAnchorImpl(InRawChild, InAttachTo, nullptr, InLocalTransform, OutSceneNode);
}

bool UCk_Utils_SceneNode_UE::TryAdd_AnchorToUnrealComponent(
    FCk_Handle& InRawChild, FCk_Handle_Transform& InAttachTo,
    USceneComponent* InSceneComponent, FTransform InLocalTransform,
    FCk_Handle_SceneNode& OutSceneNode)
{
    if (InSceneComponent == nullptr)
    {
        if (static_cast<const void*>(&OutSceneNode) != static_cast<const void*>(&InRawChild) &&
            static_cast<const void*>(&OutSceneNode) != static_cast<const void*>(&InAttachTo))
        { OutSceneNode = {}; }
        return false;
    }
    return TryAdd_FreshAnchorImpl(InRawChild, InAttachTo, InSceneComponent,
        InLocalTransform, OutSceneNode);
}

bool UCk_Utils_SceneNode_UE::TryAdd_AnchorToExistingTransform(
    FCk_Handle_Transform& InChild, FCk_Handle_Transform& InAttachTo,
    USceneComponent* InSceneComponent, FTransform InLocalTransform,
    FCk_Handle_SceneNode& OutSceneNode)
{
    return TryAdd_FreshAnchorImpl(InChild, InAttachTo, InSceneComponent,
        InLocalTransform, OutSceneNode, true);
}

bool UCk_Utils_SceneNode_UE::TryAdd_FreshAnchorImpl(
    FCk_Handle& InRawChild, FCk_Handle_Transform& InAttachTo,
    USceneComponent* InSceneComponent, FTransform InLocalTransform,
    FCk_Handle_SceneNode& OutSceneNode, bool InExistingTransform)
{
    // An adversarial C++ alias must not erase the caller's only raw creator identity.
    if (static_cast<const void*>(&OutSceneNode) == static_cast<const void*>(&InRawChild) ||
        static_cast<const void*>(&OutSceneNode) == static_cast<const void*>(&InAttachTo))
    { return false; }
    OutSceneNode = {};
    if (NOT IsInGameThread() || NOT ck::IsValid(InRawChild) ||
        NOT ck::IsValid(InAttachTo) || InRawChild == InAttachTo ||
        InRawChild.Get_RegistryView().Get_RegistryHandle() !=
            InAttachTo.Get_RegistryView().Get_RegistryHandle())
    { return false; }
    if (ck::registry_table::Get_IsInParallelRegion(
            InRawChild.Get_RegistryView().Get_RegistryHandle()))
    { return false; }
    if (UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InRawChild,
            ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
        UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InAttachTo,
            ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
        NOT InRawChild.Has<ck::FFragment_LifetimeOwner>() ||
        UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InRawChild) != InAttachTo ||
        NOT InAttachTo.Has<ck::FFragment_Transform>() ||
        NOT ck_scene_node_fresh_anchor::IsWithinSafeCompositionEnvelope(InLocalTransform))
    { return false; }

    // Neither overload repairs or reparents an existing node.
    if (InRawChild.Has_Any<ck::FFragment_SceneNode, ck::SceneNodeParent,
            ck::FFragment_SceneNode_UnrealAnchor, ck::FFragment_SceneNode_Requests,
            ck::FFragment_SceneNode_PropagationState, ck::FFragment_RecordOfSceneNodes,
            ck::FFragment_RecordOfEntityExtensions, ck::FFragment_RecordEntry,
            ck::FFragment_Transform_Requests,
            ck::FFragment_Transform_MeshSocket,
            ck::FTag_Transform_ExternallyDriven,
            ck::FTag_SceneNode_RelativeTransformUpdated,
            ck::FTag_SceneNode_PropagationQueued>() ||
        ck_scene_node_fresh_anchor::CountLayers(InRawChild) != 0)
    { return false; }

    if (InExistingTransform)
    {
        if (InSceneComponent == nullptr ||
            NOT InRawChild.Has_All<ck::FFragment_Transform, ck::FFragment_Transform_Previous,
                ck::FFragment_Transform_RootComponent, ck::FTag_Transform_Movable>() ||
            NOT UCk_Utils_OwningActor_UE::Has(InRawChild) ||
            NOT ck_scene_node_fresh_anchor::HasExactLiveActorBridge(InRawChild) ||
            NOT ck_scene_node_fresh_anchor::IsFinite(
                InRawChild.Get<ck::FFragment_Transform>().Get_Transform()) ||
            NOT ck_scene_node_fresh_anchor::IsFinite(
                InRawChild.Get<ck::FFragment_Transform_Previous>().Get_Transform()))
        { return false; }
        const auto* const Root = InRawChild.Get<ck::FFragment_Transform_RootComponent>()
            .Get_RootComponent().Get();
        const auto* const Actor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(InRawChild);
        if (Root != Actor->GetRootComponent() ||
            Root->Mobility != EComponentMobility::Movable)
        { return false; }
        TSet<const USceneComponent*> AnchorChain;
        for (const auto* Component = InSceneComponent; Component != nullptr;
             Component = Component->GetAttachParent())
        {
            if (NOT ck_scene_node_fresh_anchor::IsLiveComponentInRegistry(Component, InRawChild) ||
                Component == Root || Component->GetOwner() == Actor || AnchorChain.Contains(Component))
            { return false; }
            AnchorChain.Add(Component);
        }
        if (InRawChild.Has<ck::FFragment_LifetimeDependents>())
        {
            // Lifetime history is retained after request-carrier deallocation.
            for (const auto& Dependent : InRawChild.Get<ck::FFragment_LifetimeDependents>().Get_Entities())
            { if (ck::IsValid(Dependent, ck::IsValid_Policy_IncludePendingKill{})) { return false; } }
        }
    }
    else if (InRawChild.Has_Any<ck::FFragment_Transform, ck::FFragment_Transform_Previous,
            ck::FFragment_LifetimeDependents, ck::FFragment_Transform_RootComponent,
            ck::FFragment_Transform_RootComponentTeleportType,
            ck::FTag_Transform_Movable, ck::FTag_Transform_Updated>() ||
        UCk_Utils_OwningActor_UE::Has(InRawChild))
    { return false; }

    TArray<FCk_Handle_Transform> Ancestors;
    if (NOT ck_scene_node_fresh_anchor::HasLiveRuntimeWorld(InAttachTo) ||
        NOT ck_scene_node_fresh_anchor::TryGetExactLiveParentChain(InAttachTo, Ancestors))
    { return false; }
    const auto ParentLayers = ck_scene_node_fresh_anchor::CountLayers(InAttachTo);
    if (ParentLayers > 1 ||
        (InAttachTo.Has<ck::FFragment_SceneNode>() != (ParentLayers == 1)) ||
        (ParentLayers == 1 && NOT InAttachTo.Has<ck::SceneNodeParent>()) ||
        (ParentLayers == 0 &&
            InAttachTo.Has<ck::FTag_SceneNode_RelativeTransformUpdated>()))
    { return false; }
    const auto ParentLayer = ParentLayers == 1 ? Get_LayerIndex(InAttachTo).GetValue() : -1;
    const auto ChildLayer = ParentLayer + 1;
    if (ChildLayer < 0 || ChildLayer > 9) { return false; }
    // Every ancestor's direct scene record must be canonical before adding to the leaf.
    // This includes the forward link from each ancestor to the next chain member; holder
    // and lifetime-owner fragments alone cannot prove a connected scene graph.
    for (int32 Index = 0; Index < Ancestors.Num(); ++Index)
    {
        auto& Ancestor = Ancestors[Index];
        const int32 ExpectedDirectLayer =
            ck_scene_node_fresh_anchor::GetOnlyLayer(Ancestor) + 1;
        if (NOT ck_scene_node_fresh_anchor::HasExactExistingDirectChildren(
                Ancestor, ExpectedDirectLayer) ||
            NOT ck::FUtils_RecordOfSceneNodes::CanConnectFreshUnlabeled(
                Ancestor, InRawChild, InExistingTransform))
        { return false; }
        if (Index == 0) { continue; }
        if (NOT Ancestor.Has<ck::FFragment_RecordOfSceneNodes>()) { return false; }
        // The ancestor preflight above rejects extensions, so public Get_Entries
        // returns precisely this record's direct entries.
        const auto Entries = ck::FUtils_RecordOfSceneNodes::Get_Entries(Ancestor);
        int32 ForwardLinks = 0;
        for (const auto& Entry : Entries)
        { if (Entry == Ancestors[Index - 1]) { ++ForwardLinks; } }
        if (ForwardLinks != 1) { return false; }
    }

    auto AnchorWorld = FTransform::Identity;
    if (InSceneComponent != nullptr)
    {
        if (NOT ck_scene_node_fresh_anchor::IsLiveComponentInRegistry(
                InSceneComponent, InAttachTo))
        { return false; }
        AnchorWorld = InSceneComponent->GetComponentTransform();
    }
    else
    {
        // The common parent-chain preflight has already proved any root component bridge,
        // including when this is an ancestor or this overload is component-anchored.
        AnchorWorld = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(InAttachTo);
    }
    if (NOT ck_scene_node_fresh_anchor::IsWithinSafeCompositionEnvelope(AnchorWorld))
    { return false; }
    const auto SeedWorld = InLocalTransform * AnchorWorld;
    if (NOT ck_scene_node_fresh_anchor::IsFinite(SeedWorld))
    { return false; }

    TWeakObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep> PublicationDriver;
    if (InExistingTransform &&
        NOT UCk_Utils_Transform_UE::TryPrepare_ExistingSceneNodePublication(InRawChild, PublicationDriver))
    { return false; }

    if (NOT InExistingTransform &&
        NOT UCk_Utils_Transform_UE::TryStage_FreshSceneNodeTransform(InRawChild, SeedWorld))
    { return false; }
    auto TransformChild = UCk_Utils_Transform_UE::Cast(InRawChild);
    if (InSceneComponent != nullptr)
    { InRawChild.Add<ck::FFragment_SceneNode_UnrealAnchor>(InSceneComponent, NAME_None); }
    ck_scene_node_fresh_anchor::AddLayer(InRawChild, ChildLayer);
    InRawChild.Add<ck::FTag_SceneNode_RelativeTransformUpdated>();
    InRawChild.Add<ck::FFragment_SceneNode>(InLocalTransform);
    ck::USceneNodeParent_Utils::AddOrReplace(InRawChild, InAttachTo);
    auto Node = Cast(InRawChild);
    if (NOT ck::FUtils_RecordOfSceneNodes::TryConnectFreshUnlabeled(InAttachTo, Node, InExistingTransform))
    {
        InRawChild.Try_Remove<ck::SceneNodeParent>();
        InRawChild.Try_Remove<ck::FFragment_SceneNode>();
        InRawChild.Try_Remove<ck::FTag_SceneNode_RelativeTransformUpdated>();
        ck_scene_node_fresh_anchor::RemoveLayer(InRawChild, ChildLayer);
        if (InSceneComponent != nullptr)
        { InRawChild.Try_Remove<ck::FFragment_SceneNode_UnrealAnchor>(); }
        if (NOT InExistingTransform)
        {
            InRawChild.Try_Remove<ck::FFragment_Transform_Previous>();
            InRawChild.Try_Remove<ck::FFragment_Transform>();
        }
        return false;
    }

    InRawChild.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
    InAttachTo.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
    const auto DebugName = InSceneComponent == nullptr
        ? ck::Format_UE(TEXT("SceneNode({})"), Get_SceneNodeAnchorLabel(InAttachTo))
        : ck::Format_UE(TEXT("SceneNode({} > {})"),
            Get_SceneNodeAnchorLabel(InAttachTo), GetNameSafe(InSceneComponent));
    UCk_Utils_Handle_UE::Set_DebugName(InRawChild, *DebugName);
    ck::FUtils_SceneNodePropagation::Queue(InRawChild);
    OutSceneNode = Node;
    if (InExistingTransform)
    {
        InRawChild.Add<ck::FTag_Transform_ExternallyDriven>();
        UCk_Utils_Transform_UE::Apply_SetTransform_DirectWrite(
            InRawChild.Get<ck::FFragment_Transform>(),
            InRawChild.Get<ck::FFragment_Transform_Previous>(), SeedWorld);
        InRawChild.AddOrGet<ck::FTag_Transform_Updated>();
        // Publish the committed seed before an observation-only refresh can clear its dirty mask.
        if (PublicationDriver.IsValid())
        { UCk_Utils_Transform_UE::Publish_ExistingSceneNodeTransform(*PublicationDriver.Get(), SeedWorld); }
        // A call after the Transform pass must still publish on the next frame.
        UCk_Utils_Transform_TypeUnsafe_UE::Request_ForceRefresh(TransformChild, {});
    }
    else
    { UCk_Utils_Transform_UE::Publish_StagedSceneNodeTransform(TransformChild); }
    // A synchronous editor observer may retire Node. Caller revalidates before admission.
    return true;
}

auto
    UCk_Utils_SceneNode_UE::
    Create(
        FCk_Handle_Transform& InOwner,
        FTransform InLocalTransform)
    -> FCk_Handle_SceneNode
{
    auto SceneNodeEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
    UCk_Utils_Handle_UE::Set_DebugName(SceneNodeEntity, *ck::Format_UE(TEXT("SceneNode({})"), Get_SceneNodeAnchorLabel(InOwner)));

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

    auto SceneNodeEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InAttachTo);
    UCk_Utils_Handle_UE::Set_DebugName(SceneNodeEntity, *ck::Format_UE(TEXT("SceneNode({} > {})"), Get_SceneNodeAnchorLabel(InAttachTo), GetNameSafe(InSceneComponent)));

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

    auto SceneNodeEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InAttachTo);
    UCk_Utils_Handle_UE::Set_DebugName(SceneNodeEntity, *ck::Format_UE(TEXT("SceneNode({} > {}:{})"), Get_SceneNodeAnchorLabel(InAttachTo), GetNameSafe(InMeshComponent), InSocketName));

    // Read-only follower (see CreateAndAttachToUnrealComponent for the fragment rationale).
    auto SceneNodeWithTransform = UCk_Utils_Transform_UE::Add(SceneNodeEntity, InLocalTransform * InMeshComponent->GetSocketTransform(InSocketName), ECk_Replication::DoesNotReplicate);
    SceneNodeWithTransform.Add<ck::FFragment_SceneNode_UnrealAnchor>(InMeshComponent, InSocketName);

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

    const auto ParentLayerIndex = Get_LayerIndex(InAttachTo);
    const auto MyLayerIndex = ParentLayerIndex.IsSet() ? ParentLayerIndex.GetValue() + 1 : 0;

    if (NOT AssignLayerByIndex(InHandle, MyLayerIndex))
    { return {}; }

    PropagateLayerToChildren(InHandle, MyLayerIndex);

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
        const auto& Anchor = InSceneNode.Get<ck::FFragment_SceneNode_UnrealAnchor>();
        const auto& Component = Anchor.Get_Component();

        if (ck::Is_NOT_Valid(Component))
        { return FTransform::Identity; }

        // Same expression FProcessor_SceneNode_FollowUnrealAnchor composes against.
        return Anchor.Get_Socket().IsNone()
            ? Component->GetComponentTransform()
            : Component->GetSocketTransform(Anchor.Get_Socket());
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

    RemoveExistingLayerTag(NodeAsTransform);
    NodeAsTransform.Try_Remove<ck::FTag_Transform_ExternallyDriven>();

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
    Get_LayerIndex(
        const FCk_Handle& InHandle)
    -> TOptional<int32>
{
    if (InHandle.Has<ck::FTag_SceneNode_Layer0>())
    { return 0; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer1>())
    { return 1; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer2>())
    { return 2; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer3>())
    { return 3; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer4>())
    { return 4; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer5>())
    { return 5; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer6>())
    { return 6; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer7>())
    { return 7; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer8>())
    { return 8; }
    if (InHandle.Has<ck::FTag_SceneNode_Layer9>())
    { return 9; }
    return {};
}

auto
    UCk_Utils_SceneNode_UE::
    RemoveExistingLayerTag(
        FCk_Handle_Transform& InHandle)
    -> void
{
    InHandle.Try_Remove<ck::FTag_SceneNode_RelativeTransformUpdated>();

    InHandle.Try_Remove<ck::FTag_SceneNode_Layer0>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer1>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer2>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer3>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer4>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer5>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer6>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer7>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer8>();
    InHandle.Try_Remove<ck::FTag_SceneNode_Layer9>();
}

auto
    UCk_Utils_SceneNode_UE::
    AssignLayerByIndex(
        FCk_Handle_Transform& InHandle,
        int32 InLayerIndex)
    -> bool
{
    InHandle.AddOrGet<ck::FTag_SceneNode_RelativeTransformUpdated>();

    switch (InLayerIndex)
    {
        case 0: InHandle.Add<ck::FTag_SceneNode_Layer0>(); return true;
        case 1: InHandle.Add<ck::FTag_SceneNode_Layer1>(); return true;
        case 2: InHandle.Add<ck::FTag_SceneNode_Layer2>(); return true;
        case 3: InHandle.Add<ck::FTag_SceneNode_Layer3>(); return true;
        case 4: InHandle.Add<ck::FTag_SceneNode_Layer4>(); return true;
        case 5: InHandle.Add<ck::FTag_SceneNode_Layer5>(); return true;
        case 6: InHandle.Add<ck::FTag_SceneNode_Layer6>(); return true;
        case 7: InHandle.Add<ck::FTag_SceneNode_Layer7>(); return true;
        case 8: InHandle.Add<ck::FTag_SceneNode_Layer8>(); return true;
        case 9: InHandle.Add<ck::FTag_SceneNode_Layer9>(); return true;
        default:
        {
            CK_TRIGGER_ENSURE(TEXT("Layer index [{}] exceeds maximum supported SceneNode layers"), InLayerIndex);
            return false;
        }
    }
}

auto
    UCk_Utils_SceneNode_UE::
    PropagateLayerToChildren(
        FCk_Handle_Transform& InParent,
        int32 InParentLayerIndex)
    -> void
{
    const auto ChildLayerIndex = InParentLayerIndex + 1;

    ForEach_SceneNode(InParent, [ChildLayerIndex](FCk_Handle_SceneNode InChild)
    {
        if (const auto& CurrentLayerIndex = Get_LayerIndex(InChild);
            CurrentLayerIndex.IsSet() && CurrentLayerIndex.GetValue() == ChildLayerIndex)
        { return; }

        auto ChildTransform = UCk_Utils_Transform_UE::Cast(InChild);

        RemoveExistingLayerTag(ChildTransform);

        if (NOT AssignLayerByIndex(ChildTransform, ChildLayerIndex))
        { return; }

        ChildTransform.AddOrGet<ck::FFragment_SceneNode_PropagationState>();
        ck::FUtils_SceneNodePropagation::Queue(ChildTransform);

        PropagateLayerToChildren(ChildTransform, ChildLayerIndex);
    });
}

// --------------------------------------------------------------------------------------------------------------------
