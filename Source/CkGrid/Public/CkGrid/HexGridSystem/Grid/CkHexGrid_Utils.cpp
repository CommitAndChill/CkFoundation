#include "CkHexGrid_Utils.h"

#include "CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridReplication_Processor.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkGrid/HexGridSystem/Authoring/CkHexGridInitialization_Data.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridPersistence_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGrid_Projection.h"
#include "CkEcs/Persistence/CkPersistenceInitialization.h"
#include "CkGrid/HexGridSystem/Blocker/CkHexGridBlocker_Utils.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Utils.h"
#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include <Components/SceneComponent.h>
#include <GameFramework/Actor.h>
#include <Misc/ScopeExit.h>

namespace ck_hex_grid_composition
{
    using EFailure = ECk_HexBoardFailure;
    using FGeometry = UCk_Utils_HexGeometry_UE;
    using FLifetime = UCk_Utils_EntityLifetime_UE;
    using FTransformUtils = UCk_Utils_Transform_UE;

    CK_DEFINE_ECS_TAG(FTag_HexGrid_Composing);

    struct FBoardPlan
    {
        TArray<FCk_HexCoord> Coordinates;
        TSet<FCk_HexCoord> Disabled;
        TMap<FCk_HexCoord, FGameplayTagContainer> Overrides;
    };

    auto
        ValidateWorldLayout(
            const FCk_HexLayout& InLayout,
            const FTransform& InTransform)
        -> EFailure
    {
        auto Reason = ECk_HexGeometryFailure::None;
        if (FGeometry::TryValidateWorldLayout(InLayout, InTransform, Reason))
        { return EFailure::None; }
        return Reason == ECk_HexGeometryFailure::InvalidOrientation || Reason == ECk_HexGeometryFailure::InvalidLayout
            ? EFailure::InvalidSpec : EFailure::UnsupportedTransform;
    }

    auto
        Resolve(
            const FCk_HexGrid_Spec& InSpec,
            FBoardPlan& OutPlan)
        -> EFailure
    {
        const auto LayoutFailure = ValidateWorldLayout(InSpec.Get_Layout(), InSpec.Get_Pivot());
        if (LayoutFailure != EFailure::None)
        { return LayoutFailure; }

        auto Reason = ECk_HexGeometryFailure::None;
        if (NOT FGeometry::TryResolve_Domain(InSpec.Get_Domain(), OutPlan.Coordinates, Reason))
        { return Reason == ECk_HexGeometryFailure::LimitExceeded ? EFailure::CapacityExceeded : EFailure::InvalidSpec; }

        // A private world uses one entity for its transient root. EnTT reserves entity_mask as the null sentinel.
        if (static_cast<uint64>(OutPlan.Coordinates.Num()) + 1 > entt::entt_traits<FCk_Entity::IdType>::entity_mask)
        { return EFailure::CapacityExceeded; }

        if (InSpec.Get_InitialDisabledCells().Num() > OutPlan.Coordinates.Num()
            || InSpec.Get_PerCellTags().Num() > OutPlan.Coordinates.Num())
        { return EFailure::InvalidSpec; }

        const auto Members = TSet<FCk_HexCoord>{OutPlan.Coordinates};
        for (const auto& Coordinate : InSpec.Get_InitialDisabledCells())
        {
            if (NOT Members.Contains(Coordinate) || OutPlan.Disabled.Contains(Coordinate))
            { return EFailure::InvalidSpec; }
            OutPlan.Disabled.Add(Coordinate);
        }

        for (const auto& Override : InSpec.Get_PerCellTags())
        {
            const auto& Coordinate = Override.Get_Coordinate();
            if (NOT Members.Contains(Coordinate) || OutPlan.Overrides.Contains(Coordinate))
            { return EFailure::InvalidSpec; }
            OutPlan.Overrides.Add(Coordinate, Override.Get_Tags());
        }

        TSet<FGuid> NamedBlockerIds;
        for (const auto& Blocker : InSpec.Get_Blockers())
        {
            const auto& Cells = Blocker.Get_Cells();
            if (Cells.IsEmpty() || Cells.Num() > Members.Num())
            { return EFailure::InvalidSpec; }

            const auto Name = Blocker.Get_Name();
            if (Name.IsValid())
            {
                const auto Id = FGuid::NewDeterministicGuid(Name.ToString());
                if (!Id.IsValid() || NamedBlockerIds.Contains(Id))
                { return EFailure::InvalidSpec; }
                NamedBlockerIds.Add(Id);
            }

            TSet<FCk_HexCoord> Seen;
            Seen.Reserve(Cells.Num());
            for (const auto& Coordinate : Cells)
            {
                if (NOT Members.Contains(Coordinate) || Seen.Contains(Coordinate))
                { return EFailure::InvalidSpec; }
                Seen.Add(Coordinate);
            }
        }
        return EFailure::None;
    }

    enum class EFactory
    {
        Add,
        Create,
        Authoring
    };

    auto
        Compose(
            FCk_Handle& InOwner,
            EFactory InFactory,
            const FTransform& InInitialTransform,
            const FCk_HexGrid_Spec& InSpec,
            FCk_Handle_HexGrid& OutGrid,
            const FCk_HexGridInitialization* InInitialization = nullptr,
            const FCk_HexGridRecipeIdentity* InRecipe = nullptr)
        -> EFailure
    {
        if (NOT IsInGameThread() || NOT FLifetime::Get_CanCreateEntity(InOwner))
        { return EFailure::InvalidHandle; }
        if (InFactory != EFactory::Create &&
            (UCk_Utils_HexGrid_UE::Has(InOwner) || InOwner.Has<FTag_HexGrid_Composing>()))
        { return EFailure::AlreadyComposed; }

        auto Parent = InOwner;
        auto Visited = TSet<FCk_Handle>{};
        while (Parent.Has<ck::FFragment_LifetimeOwner>())
        {
            if (FLifetime::Get_IsPendingDestroy(Parent, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
            { return EFailure::InvalidHandle; }
            Visited.Add(Parent);
            Parent = Parent.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
            if (ck::Is_NOT_Valid(Parent) || Visited.Contains(Parent))
            { return EFailure::InvalidHandle; }
        }
        if (FLifetime::Get_IsPendingDestroy(Parent, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
        { return EFailure::InvalidHandle; }

        auto Transform = FCk_Handle_Transform{};
        if (InFactory != EFactory::Create)
        {
            Transform = FTransformUtils::Cast(InOwner);
            if (InFactory == EFactory::Add && ck::Is_NOT_Valid(Transform))
            { return EFailure::InvalidHandle; }
            // The SceneNode pivot occupies the next layer; there is none below the last.
            if (ck::IsValid(Transform) && NOT UCk_Utils_SceneNode_UE::Get_CanCreateChild(Transform))
            { return EFailure::CapacityExceeded; }
            if (ck::Is_NOT_Valid(Transform) && InOwner.Has_Any<ck::FFragment_Transform_Previous,
                ck::FFragment_Transform_Requests, ck::FFragment_Transform_RootComponent,
                ck::FFragment_Transform_RootComponentTeleportType>())
            { return EFailure::InvalidHandle; }
        }

        auto Plan = FBoardPlan{};
        const auto SpecFailure = Resolve(InSpec, Plan);
        if (SpecFailure != EFailure::None)
        { return SpecFailure; }

        auto WorldTransform = InInitialTransform;
        if (ck::IsValid(Transform))
        { WorldTransform = FTransformUtils::Get_EntityCurrentTransform(Transform); }
        else if (InFactory == EFactory::Authoring)
        {
            if (const auto* Actor = UCk_Utils_EntityBridge_UE::TryGet_Actor(InOwner); IsValid(Actor))
            {
                const auto* Root = Actor->GetRootComponent();
                if (NOT IsValid(Root))
                { return EFailure::InvalidHandle; }
                WorldTransform = Root->GetComponentTransform();
            }
        }

        if (ValidateWorldLayout(InSpec.Get_Layout(), WorldTransform) != EFailure::None ||
            ValidateWorldLayout(InSpec.Get_Layout(), InSpec.Get_Pivot() * WorldTransform) != EFailure::None)
        { return EFailure::UnsupportedTransform; }

        const auto* PayloadType = FCk_RepData_HexGridState::StaticStruct();
        const auto Subscriber = FName{TEXT("HexGrid.Readiness")};
        auto Subscribed = false;
        auto Committed = false;
        // Classifying an adopted entity can synchronously notify another construction listener.
        if (InInitialization) { InOwner.Add<FTag_HexGrid_Composing>(); }
        ON_SCOPE_EXIT
        {
            if (InInitialization && ck::IsValid(InOwner))
            {
                if (Subscribed && !Committed) { ck::persistence::Unsubscribe(InOwner, PayloadType, Subscriber); }
                InOwner.Try_Remove<FTag_HexGrid_Composing>();
            }
        };
        const auto Incoming = InRecipe && !InRecipe->Get_AssetPath().IsEmpty() &&
            UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InOwner);
        if (InInitialization && !Incoming)
        {
            auto Identity = ck::persistence::FInitializationIdentity{};
            if (!InInitialization->TryResolve(InOwner, Identity)) { return EFailure::InvalidSaveIdentity; }
            const auto Status = ck::persistence::RequestInitialization(InOwner, PayloadType, Identity);
            if (ck::Is_NOT_Valid(InOwner) || FLifetime::Get_IsPendingDestroy(InOwner,
                ECk_EntityLifetime_DestructionPhase::BeginDestroy)) { return EFailure::InvalidHandle; }
            if (Status.Get_Disposition() == ck::persistence::EInitializationDisposition::Undeclared ||
                Status.Get_Failure() == ck::persistence::EInitializationFailure::Rejected)
            { return EFailure::InvalidSaveIdentity; }
            Subscribed = ck::persistence::Subscribe(InOwner, PayloadType, Subscriber,
                [](FCk_Handle InEntity, const ck::persistence::FInitializationStatus& InStatus)
                {
                    if (!UCk_Utils_HexGrid_UE::Has(InEntity)) { return; }
                    auto Grid = UCk_Utils_HexGrid_UE::Cast(InEntity);
                    ck::hex_grid::SetReadiness(Grid, UCk_Utils_HexGrid_UE::Get_Readiness(Grid));
                });
            if (!Subscribed) { return EFailure::CompositionFailed; }
        }

        auto Admission = ck::FEcsWorld::ECreateResult::InvalidState;
        auto World = ck::FEcsWorld::TryCreate(Admission);
        if (NOT World)
        { return Admission == ck::FEcsWorld::ECreateResult::CapacityExceeded ? EFailure::CapacityExceeded : EFailure::CompositionFailed; }

        auto Registry = World->Get_Registry();
        TArray<FCk_Handle> Entities;
        Entities.Reserve(Plan.Coordinates.Num());
        for (const auto& Coordinate : Plan.Coordinates)
        {
            auto Entity = FLifetime::Request_CreateEntity(Registry);
            if (ck::Is_NOT_Valid(Entity))
            { return EFailure::CompositionFailed; }
            Entity.Add<ck::FFragment_HexCell_Tags>(Plan.Overrides.FindRef(Coordinate));
            if (Plan.Disabled.Contains(Coordinate))
            { Entity.Add<ck::FTag_HexCell_Disabled>(); }
            Entities.Add(Entity);
        }

        auto Board = InOwner;
        if (InFactory == EFactory::Create)
        {
            Board = FLifetime::Request_CreateEntity(InOwner);
            if (ck::Is_NOT_Valid(Board))
            { return EFailure::CompositionFailed; }
        }

        TMap<FCk_HexCoord, FCk_Handle_HexCell> Index;
        TArray<FCk_Handle_HexCell> Cells;
        Index.Reserve(Entities.Num());
        Cells.Reserve(Entities.Num());
        for (int32 Idx = 0; Idx < Entities.Num(); ++Idx)
        {
            auto& Entity = Entities[Idx];
            FLifetime::Request_SetupEntityWithLifetimeOwner(Entity, Board);
            Entity.Add<ck::FFragment_HexCell_Identity>(Plan.Coordinates[Idx], Board);
            const auto Cell = UCk_Utils_HexCell_UE::CastChecked(Entity);
            Cells.Add(Cell);
            Index.Add(Plan.Coordinates[Idx], Cell);
        }

        // Transform observers can synchronously call back into a factory. Publish membership
        // only when the pivot and every cell are installed.
        if (!InInitialization) { Board.Add<FTag_HexGrid_Composing>(); }
        const auto AddsTransform = ck::Is_NOT_Valid(Transform);
        const auto WasMovable = Board.Has<ck::FTag_Transform_Movable>();
        ON_SCOPE_EXIT
        {
            if (ck::IsValid(Board))
            {
                Board.Try_Remove<FTag_HexGrid_Composing>();
                if (NOT Committed && InFactory == EFactory::Create)
                { FLifetime::Request_DestroyEntity(Board); }
                else if (NOT Committed && AddsTransform)
                {
                    Board.Try_Remove<ck::FFragment_Transform_Requests>();
                    Board.Try_Remove<ck::FFragment_Transform_Previous>();
                    Board.Try_Remove<ck::FFragment_Transform_RootComponentTeleportType>();
                    Board.Try_Remove<ck::FFragment_Transform_RootComponent>();
                    Board.Try_Remove<ck::FFragment_Transform>();
                    if (NOT WasMovable)
                    { Board.Try_Remove<ck::FTag_Transform_Movable>(); }
                }
            }
        };
        if (AddsTransform)
        { Transform = FTransformUtils::Add(Board, WorldTransform, ECk_Replication::DoesNotReplicate); }
        if (ck::Is_NOT_Valid(Transform) || NOT UCk_Utils_SceneNode_UE::Get_CanCreateChild(Transform) ||
            FLifetime::Get_IsPendingDestroy(Board, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
        { return EFailure::CompositionFailed; }

        const auto HadSceneNodeRecord = Board.Has<ck::FFragment_RecordOfSceneNodes>();
        const auto HadPropagationState = Board.Has<ck::FFragment_SceneNode_PropagationState>();
        auto Pivot = UCk_Utils_SceneNode_UE::Create(Transform, InSpec.Get_Pivot());
        // Creating the pivot dispatches Transform OnAdded synchronously. BeginDestroy is observable
        // before ordinary handle validity goes false, so recheck both participants before publication.
        if (ck::Is_NOT_Valid(Board) || ck::Is_NOT_Valid(Pivot) ||
            FLifetime::Get_IsPendingDestroy(Board, ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
            FLifetime::Get_IsPendingDestroy(Pivot, ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
            NOT UCk_Utils_HexGridBlocker_UE::TryInstallAuthored(Board, Index, InSpec.Get_Blockers()))
        {
            if (ck::IsValid(Pivot))
            {
                UCk_Utils_SceneNode_UE::Request_Detach(Pivot, {});
                FLifetime::Request_DestroyEntity(Pivot);
            }
            if (ck::IsValid(Board) && Board.Has<ck::FFragment_RecordOfSceneNodes>() &&
                ck::FUtils_RecordOfSceneNodes::Get_ValidEntriesCount(Board) == 0)
            {
                if (NOT HadSceneNodeRecord)
                { Board.Try_Remove<ck::FFragment_RecordOfSceneNodes>(); }
                if (NOT HadPropagationState)
                { Board.Try_Remove<ck::FFragment_SceneNode_PropagationState>(); }
            }
            return EFailure::CompositionFailed;
        }
        auto& Readiness = Board.AddOrGet<ck::FFragment_HexGridReadiness>();
        if (Incoming)
        {
            Readiness._ObservesReplica = true;
            Readiness._Current = {ECk_HexGridReadiness::AwaitingReplication, ECk_HexGridFailure::NotReady, {}};
            ck::hex_grid::StartNetBaselineDeadline(Board);
        }
        else if (InInitialization)
        {
            Readiness._ObservesRestore = true;
            Readiness._Current = ck::hex_grid::ReadinessFor(ck::persistence::Query(Board, PayloadType));
        }
        Board.Add<ck::FFragment_HexGrid_Params>(InSpec.Get_Layout(), InSpec.Get_DefaultCellTags(),
            MoveTemp(Plan.Overrides), InRecipe ? *InRecipe : FCk_HexGridRecipeIdentity{});
        Board.Add<ck::FFragment_HexGrid>(MoveTemp(World), MoveTemp(Index), MoveTemp(Cells), Pivot);
        OutGrid = UCk_Utils_HexGrid_UE::CastChecked(Board);
        Committed = true;
        ck::hex_grid::ArmBoardReplication(Board);
        return EFailure::None;
    }
}

auto
    UCk_Utils_HexGrid_UE::
    Has(
        const FCk_Handle& InHandle)
    -> bool
{
    return ck::IsValid(InHandle) && InHandle.Has_All<ck::FFragment_HexGrid_Params, ck::FFragment_HexGrid>();
}

auto
    UCk_Utils_HexGrid_UE::
    DoCast(
        FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult)
    -> FCk_Handle_HexGrid
{
    const auto Handle = Cast(InHandle);
    OutResult = ck::IsValid(Handle) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Handle;
}

auto
    UCk_Utils_HexGrid_UE::
    DoCastChecked(
        FCk_Handle InHandle)
    -> FCk_Handle_HexGrid
{
    return CastChecked(InHandle);
}

auto
    UCk_Utils_HexGrid_UE::
    TryValidateSpec(
        const FCk_HexGrid_Spec& InSpec,
        ECk_HexBoardFailure& OutFailure)
    -> bool
{
    auto Plan = ck_hex_grid_composition::FBoardPlan{};
    OutFailure = ck_hex_grid_composition::Resolve(InSpec, Plan);
    return OutFailure == ck_hex_grid_composition::EFailure::None;
}

auto
    UCk_Utils_HexGrid_UE::
    TryAdd(
        FCk_Handle_Transform& InOwner,
        const FCk_HexGrid_Spec& InSpec,
        FCk_Handle_HexGrid& OutGrid,
        ECk_HexBoardFailure& OutFailure)
    -> bool
{
    auto Owner = InOwner;
    OutGrid = {};
    OutFailure = ck_hex_grid_composition::Compose(Owner, ck_hex_grid_composition::EFactory::Add, FTransform::Identity, InSpec, OutGrid);
    return OutFailure == ck_hex_grid_composition::EFailure::None;
}

auto
    UCk_Utils_HexGrid_UE::
    TryCreate(
        FCk_Handle& InOwner,
        const FTransform& InInitialTransform,
        const FCk_HexGrid_Spec& InSpec,
        FCk_Handle_HexGrid& OutGrid,
        ECk_HexBoardFailure& OutFailure)
    -> bool
{
    auto Owner = InOwner;
    OutGrid = {};
    OutFailure = ck_hex_grid_composition::Compose(Owner, ck_hex_grid_composition::EFactory::Create, InInitialTransform, InSpec, OutGrid);
    return OutFailure == ck_hex_grid_composition::EFailure::None;
}

auto
    UCk_Utils_HexGrid_UE::
    TryAddForAuthoring(
        FCk_Handle& InOwner,
        const FTransform& InInitialTransform,
        const FCk_HexGrid_Spec& InSpec,
        FCk_Handle_HexGrid& OutGrid,
        ECk_HexBoardFailure& OutFailure,
        const FCk_HexGridInitialization* InInitialization,
        const FCk_HexGridRecipeIdentity* InRecipe)
    -> bool
{
    auto Owner = InOwner;
    OutGrid = {};
    OutFailure = ck_hex_grid_composition::Compose(Owner, ck_hex_grid_composition::EFactory::Authoring,
        InInitialTransform, InSpec, OutGrid, InInitialization, InRecipe);
    return OutFailure == ck_hex_grid_composition::EFailure::None;
}

auto
    UCk_Utils_HexGrid_UE::
    TryGet_CellAt(
        const FCk_Handle_HexGrid& InGrid,
        const FCk_HexCoord& InCoordinate,
        FCk_Handle_HexCell& OutCell,
        ECk_HexBoardFailure& OutFailure)
    -> bool
{
    OutCell = {};
    if (NOT Has(InGrid))
    {
        OutFailure = ck_hex_grid_composition::EFailure::InvalidHandle;
        return false;
    }
    const auto* Found = InGrid.Get<ck::FFragment_HexGrid>().Get_Index().Find(InCoordinate);
    if (Found == nullptr || ck::Is_NOT_Valid(*Found))
    {
        OutFailure = ck_hex_grid_composition::EFailure::MissingCell;
        return false;
    }
    OutCell = *Found;
    OutFailure = ck_hex_grid_composition::EFailure::None;
    return true;
}

auto
    UCk_Utils_HexGrid_UE::
    TryGet_NeighborCell(
        const FCk_Handle_HexGrid& InGrid,
        const FCk_HexCoord& InFrom,
        ECk_HexDirection InDirection,
        FCk_Handle_HexCell& OutCell,
        ECk_HexBoardFailure& OutFailure)
    -> bool
{
    OutCell = {};
    auto Source = FCk_Handle_HexCell{};
    if (NOT TryGet_CellAt(InGrid, InFrom, Source, OutFailure))
    { return false; }
    auto Neighbor = FCk_HexCoord{};
    auto Reason = ECk_HexGeometryFailure::None;
    if (NOT ck_hex_grid_composition::FGeometry::TryGet_Neighbor(InFrom, InDirection, Neighbor, Reason))
    {
        OutFailure = Reason == ECk_HexGeometryFailure::InvalidDirection ? ck_hex_grid_composition::EFailure::InvalidDirection : ck_hex_grid_composition::EFailure::CoordinateOverflow;
        return false;
    }
    return TryGet_CellAt(InGrid, Neighbor, OutCell, OutFailure);
}

auto
    UCk_Utils_HexGrid_UE::
    Get_Cells(
        const FCk_Handle_HexGrid& InGrid)
    -> TArray<FCk_Handle_HexCell>
{
    if (NOT Has(InGrid))
    { return {}; }
    return InGrid.Get<ck::FFragment_HexGrid>().Get_Cells();
}
