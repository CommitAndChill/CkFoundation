#include "CkHexGridObjectBinding_Utils.h"
#include "CkHexGridReplication_Processor.h"

#include "CkHexGridPersistence_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Fragment.h"
#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Utils.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGridOccupancy_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.inl.h"

namespace ck::hex_grid
{
    namespace
    {
        struct FClaim
        {
            FCk_Handle_HexGrid Board;
            FGuid PlacementId;
            FCk_Handle Expected;
        };
        struct FBindingDirectory
        {
            TMap<FGuid, FCk_Handle> Objects;
            TMap<FGuid, FClaim> Claims;
            TMultiMap<FCk_Handle, FGuid> WaitingForHandle;
        };
        struct FObjectRegistration
        {
            FCk_Handle Scope;
            FGuid Key;
            bool Replica;
        };
        struct FBoardRegistration
        {
            FCk_Handle Scope;
            TSet<FGuid> Keys;
        };

        auto Directory(const FCk_Handle& InScope) -> FBindingDirectory*
        {
            if (!ck::IsValid(InScope) || !InScope.Has<FBindingDirectory>()) { return nullptr; }
            auto Scope = InScope;
            return &Scope.Get<FBindingDirectory>();
        }
        auto ForgetObject(FCk_Handle InObject) -> void
        {
            if (!InObject.Has<FObjectRegistration>()) { return; }
            const auto Registration = InObject.Get<FObjectRegistration, ck::IsValid_Policy_IncludePendingKill>();
            auto* Index = Directory(Registration.Scope);
            if (Index && Index->Objects.FindRef(Registration.Key) == InObject)
            {
                Index->Objects.Remove(Registration.Key);
                const auto* Claim = Index->Claims.Find(Registration.Key);
                if (Registration.Replica && Claim)
                { SuspendReplicaOccupant(Claim->Board, Claim->PlacementId, InObject, Registration.Key); }
            }
        }
        auto ForgetClaim(FBindingDirectory& InDirectory, FGuid InKey) -> void
        {
            FClaim Removed;
            if (InDirectory.Claims.RemoveAndCopyValue(InKey, Removed) && Removed.Expected != FCk_Handle{})
            { InDirectory.WaitingForHandle.RemoveSingle(Removed.Expected, InKey); }
        }
        auto ForgetBoard(FCk_Handle InBoard) -> void
        {
            if (!InBoard.Has<FBoardRegistration>()) { return; }
            const auto& Registration = InBoard.Get<FBoardRegistration, ck::IsValid_Policy_IncludePendingKill>();
            if (auto* Index = Directory(Registration.Scope))
            {
                for (const auto Key : Registration.Keys)
                {
                    const auto* Claim = Index->Claims.Find(Key);
                    if (Claim && Claim->Board == InBoard) { ForgetClaim(*Index, Key); }
                }
            }
        }
    }

    auto RegisterObjectBinding(FCk_Handle InObject, FGuid InBindingId) -> bool
    {
        if (!InBindingId.IsValid() || !hex_rules::IsLive(InObject) || !InObject.Has<FFragment_HexGridObjectIdentity>())
        { return false; }
        auto& Identity = InObject.Get<FFragment_HexGridObjectIdentity>();
        if (Identity.Get_BindingId().IsValid() && Identity.Get_BindingId() != InBindingId) { return false; }
        auto Scope = ck::persistence::GetWorldTransient(InObject);
        if (!ck::IsValid(Scope)) { return false; }
        auto& Index = Scope.AddOrGet<FBindingDirectory>();
        const auto Incumbent = Index.Objects.FindRef(InBindingId);
        if (Incumbent != InObject && hex_rules::IsLive(Incumbent)) { return false; }
        if (Incumbent != InObject && Incumbent != FCk_Handle{} &&
            UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InObject))
        {
            if (const auto* Claim = Index.Claims.Find(InBindingId))
            { SuspendReplicaOccupant(Claim->Board, Claim->PlacementId, Incumbent, InBindingId); }
        }
        if (!InObject.Has<FObjectRegistration>())
        {
            InObject.Add<FObjectRegistration>(FObjectRegistration{Scope, InBindingId,
                UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InObject)});
            (void)UUtils_Signal_OnEntityBeginDestroy::Bind<&ForgetObject,
                ECk_Signal_BindingPolicy::IgnorePayloadInFlight, ECk_Signal_PostFireBehavior::DoNothing>(InObject);
        }
        Identity.Set_BindingId(InBindingId);
        Index.Objects.Add(InBindingId, InObject);
        return true;
    }

    auto ResolveObjectBinding(FCk_Handle InObject) -> void
    {
        const auto Key = GetAcceptedObjectBindingId(InObject);
        if (!Key.IsValid()) { return; }
        const auto Scope = ck::persistence::GetWorldTransient(InObject);
        auto* Index = Directory(Scope);
        if (!Index) { return; }
        TArray<FGuid> Interested;
        Index->WaitingForHandle.MultiFind(InObject, Interested);
        Interested.AddUnique(Key);
        for (const auto Binding : Interested)
        {
            Index = Directory(Scope);
            if (!Index) { return; }
            if (const auto* Claim = Index->Claims.Find(Binding))
            { ResolvePlacementOccupant(Claim->Board, Claim->PlacementId,
                UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InObject) ||
                ck::persistence::Query(InObject, FCk_RepData_HexGridObjectIdentity::StaticStruct()).Get_IsReleased()); }
        }
    }

    auto CanClaimPlacement(const FCk_Handle_HexGrid& InGrid, FGuid InKey, FGuid InPlacementId,
        bool InReplacingBoard) -> bool
    {
        const auto Scope = ck::persistence::GetWorldTransient(InGrid);
        if (!ck::IsValid(Scope)) { return false; }
        const auto* Index = Directory(Scope);
        const auto* Claim = Index ? Index->Claims.Find(InKey) : nullptr;
        return !Claim || !hex_rules::IsLive(Claim->Board) ||
            (Claim->Board == InGrid && (InReplacingBoard || Claim->PlacementId == InPlacementId));
    }

    auto PreparePlacementAssociation(const FCk_Handle_HexGrid& InGrid, FCk_HexGridPlacement_View& InOut) -> ECk_HexGridFailure
    {
        using EState = ECk_HexGridOccupantState;
        using EFailure = ECk_HexGridFailure;
        const auto State = InOut.Get_OccupantState();
        const auto Hint = InOut.Get_ExpectedOccupant();
        const auto Key = InOut.Get_OccupantBindingId();
        if (State == EState::Anonymous)
        { return !Key.IsValid() && Hint == FCk_Handle{} && InOut.Get_Occupant() == FCk_Handle{} ? EFailure::None : EFailure::InvalidProjection; }
        if (State == EState::Bound)
        {
            const auto Object = InOut.Get_Occupant();
            const auto ActualKey = GetAcceptedObjectBindingId(Object);
            if (!ActualKey.IsValid() || (Key.IsValid() && Key != ActualKey) ||
                (Hint != FCk_Handle{} && Hint != Object))
            { return EFailure::InvalidObject; }
            InOut.Set_OccupantBindingId(ActualKey);
            return EFailure::None;
        }
        if (!Key.IsValid() || InOut.Get_Occupant() != FCk_Handle{} ||
            (State != EState::AwaitingBinding && State != EState::BindingConflict))
        { return EFailure::InvalidProjection; }
        if (State == EState::BindingConflict) { return EFailure::None; }
        if (InGrid.Has<FFragment_HexGridOccupancy>())
        {
            const auto* Prior = InGrid.Get<FFragment_HexGridOccupancy>()._Placements.Find(InOut.Get_PlacementId());
            if (Prior && Prior->Get_OccupantState() == EState::BindingConflict &&
                Prior->Get_OccupantBindingId() == Key && Prior->Get_ExpectedOccupant() == Hint)
            {
                InOut.Set_OccupantState(EState::BindingConflict);
                return EFailure::None;
            }
        }
        const auto HintKey = GetAcceptedObjectBindingId(Hint);
        if (HintKey.IsValid() && HintKey != Key) { return EFailure::LateOccupantConflict; }
        const auto* Index = Directory(ck::persistence::GetWorldTransient(InGrid));
        const auto Object = Index ? Index->Objects.FindRef(Key) : FCk_Handle{};
        if (GetAcceptedObjectBindingId(Object) != Key) { return EFailure::None; }
        if ((Hint != FCk_Handle{} && Hint != Object) ||
            Object.Get_RegistryView().Get_RegistryHandle() != InGrid.Get_RegistryView().Get_RegistryHandle())
        { return EFailure::LateOccupantConflict; }
        InOut.Set_Occupant(Object).Set_OccupantState(EState::Bound);
        return EFailure::None;
    }

    auto PublishPlacementClaim(const FCk_HexGridPlacement_View& InPlacement) -> void
    {
        const auto Key = InPlacement.Get_OccupantBindingId();
        if (!Key.IsValid()) { return; }
        auto Board = InPlacement.Get_Grid();
        auto Scope = ck::persistence::GetWorldTransient(Board);
        if (!Board.Has<FBoardRegistration>())
        {
            Board.Add<FBoardRegistration>(FBoardRegistration{Scope, {}});
            (void)UUtils_Signal_OnEntityBeginDestroy::Bind<&ForgetBoard, ECk_Signal_BindingPolicy::IgnorePayloadInFlight,
                ECk_Signal_PostFireBehavior::DoNothing>(Board);
        }
        Board.Get<FBoardRegistration>().Keys.Add(Key);
        auto& Index = Scope.AddOrGet<FBindingDirectory>();
        ForgetClaim(Index, Key);
        const auto Hint = InPlacement.Get_OccupantState() == ECk_HexGridOccupantState::AwaitingBinding ?
            InPlacement.Get_ExpectedOccupant() : FCk_Handle{};
        Index.Claims.Add(Key, FClaim{Board, InPlacement.Get_PlacementId(), Hint});
        if (Hint != FCk_Handle{}) { Index.WaitingForHandle.Add(Hint, Key); }
    }

    auto ReleasePlacementClaim(const FCk_HexGridPlacement_View& InPlacement) -> void
    {
        auto Board = InPlacement.Get_Grid();
        const auto Key = InPlacement.Get_OccupantBindingId();
        if (!Key.IsValid() || !Board.Has<FBoardRegistration>()) { return; }
        auto& Registration = Board.Get<FBoardRegistration>();
        if (auto* Index = Directory(Registration.Scope))
        {
            const auto* Claim = Index->Claims.Find(Key);
            if (Claim && Claim->Board == Board && Claim->PlacementId == InPlacement.Get_PlacementId())
            { ForgetClaim(*Index, Key); }
        }
        Registration.Keys.Remove(Key);
    }

    namespace
    {
        struct FObjectIdentityRegistrar
        {
            FObjectIdentityRegistrar()
            {
                using FPayload = FCk_RepData_HexGridObjectIdentity;
                using EApply = ECk_Persistence_ApplyResult;
                FCk_PersistenceHandlerRegistry::Register_NetAndSave_SplitApply<FCk_RepData_HexGridObjectIdentity>({
                    .Posture = ECk_Snapshot_Posture::Durable,
                    .Produce = [](FCk_Handle& Entity) -> TOptional<FInstancedStruct>
                    {
                        const auto Key = GetAcceptedObjectBindingId(Entity);
                        if (!Key.IsValid()) { return {}; }
                        return FInstancedStruct::Make(FPayload{}.Set_BindingId(Key));
                    },
                    .NetApply = [](FCk_Handle& Entity, const FInstancedStruct& Data, const TOptional<FInstancedStruct>&) -> EApply
                    {
                        if (!hex_rules::IsLive(Entity) ||
                            Entity.Has<FTag_HexGridObject_RecipeRejected>()) { return EApply::Rejected; }
                        if (!UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(Entity))
                        {
                            return !UCk_Utils_HexGridObject_UE::Has(Entity) && IsLinkedReplicaAwaitingRecipe(Entity) ?
                                EApply::NotReady : EApply::Rejected;
                        }
                        if (!UCk_Utils_HexGridObject_UE::Has(Entity)) { return EApply::NotReady; }
                        const auto* Payload = Data.GetPtr<FPayload>();
                        if (!Payload || Payload->Get_SchemaVersion() != 1 || !RegisterObjectBinding(Entity, Payload->Get_BindingId()))
                        { return EApply::Rejected; }
                        Entity.Get<FFragment_HexGridObjectIdentity>().Set_HasNetBaseline(true);
                        ResolveObjectBinding(Entity);
                        return EApply::Applied;
                    },
                    .HydrationApply = [](FCk_Handle& Entity, const FInstancedStruct& Data, const TOptional<FInstancedStruct>&) -> EApply
                    {
                        if (UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(Entity)) { return EApply::Rejected; }
                        const auto* Payload = Data.GetPtr<FPayload>();
                        if (!Payload || Payload->Get_SchemaVersion() != 1 || !Payload->Get_BindingId().IsValid()) { return EApply::Rejected; }
                        if (!hex_rules::IsLive(Entity) || Entity.Has<FTag_HexGridObject_RecipeRejected>()) { return EApply::Rejected; }
                        if (!UCk_Utils_HexGridObject_UE::Has(Entity)) { return EApply::NotReady; }
                        using D = ck::persistence::EInitializationDisposition;
                        const auto State = ck::persistence::Query(Entity, FPayload::StaticStruct()).Get_Disposition();
                        if (State == D::Unavailable) { return EApply::Rejected; }
                        if (State == D::IdentityUnresolved || State == D::Undeclared) { return EApply::NotReady; }
                        return RegisterObjectBinding(Entity, Payload->Get_BindingId()) ? EApply::Applied : EApply::Rejected;
                    },
                    .NetRemove = [](FCk_Handle& Entity)
                    {
                        if (!hex_rules::IsLive(Entity) || !UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(Entity) ||
                            !Entity.Has<FFragment_HexGridObjectIdentity>()) { return; }
                        Entity.Get<FFragment_HexGridObjectIdentity>().Set_HasNetBaseline(false);
                        ForgetObject(Entity);
                    }});
            }
        };
        const FObjectIdentityRegistrar ObjectIdentityRegistrar;
    }
}
