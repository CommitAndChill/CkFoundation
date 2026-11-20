#include "CkHexGridPersistence_Utils.h"
#include "CkHexGridReplication_Processor.h"

#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.inl.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGrid_Projection.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"

namespace ck::hex_grid::persistence
{
    struct FHexGridPersistenceRegistrar
    {
        FHexGridPersistenceRegistrar()
        {
            FCk_PersistenceHandlerRegistry::Register_NetAndSave_SplitApply<FCk_RepData_HexGridState>({
                .Posture = ECk_Snapshot_Posture::Durable,
                .Produce = [](FCk_Handle& InEntity) -> TOptional<FInstancedStruct>
                {
                    if (NOT UCk_Utils_HexGrid_UE::Has(InEntity))
                    { return {}; }
                    if (InEntity.Get<FFragment_HexGrid_Params>().Get_RecipeIdentity().Get_AssetPath().IsEmpty())
                    { return {}; }
                    return FInstancedStruct::Make(ck::hex_grid::persistence::ReadCommittedProjection(UCk_Utils_HexGrid_UE::Cast(InEntity)));
                },
                .NetApply = [](FCk_Handle& InEntity, const FInstancedStruct& InNew, const TOptional<FInstancedStruct>&)
                    -> ECk_Persistence_ApplyResult
                {
                    using EResult = ECk_Persistence_ApplyResult;
                    if (!hex_rules::IsLive(InEntity) ||
                        InEntity.Has<FTag_HexGrid_RecipeRejected>()) { return EResult::Rejected; }
                    if (!UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InEntity))
                    {
                        return !UCk_Utils_HexGrid_UE::Has(InEntity) && IsLinkedReplicaAwaitingRecipe(InEntity) ?
                            EResult::NotReady : EResult::Rejected;
                    }
                    if (!UCk_Utils_HexGrid_UE::Has(InEntity)) { return EResult::NotReady; }
                    auto Board = UCk_Utils_HexGrid_UE::Cast(InEntity);
                    if (!CanReceiveNetBaseline(Board)) { return EResult::Rejected; }
                    const auto* Data = InNew.GetPtr<FCk_RepData_HexGridState>();
                    if (!Data)
                    {
                        OnNetBaselineRejected(Board, ECk_HexGridFailure::InvalidSchema);
                        return EResult::Rejected;
                    }
                    const auto Result = TryReplaceCommittedProjection(Board, *Data, ECk_HexGridApplySource::Replication);
                    if (Result.Get_Status() == EResult::Applied) { OnNetBaselineAccepted(Board); }
                    else if (Result.Get_Status() == EResult::Rejected)
                    { OnNetBaselineRejected(Board, Result.Get_Failure(), Result.Get_PlacementId()); }
                    return Result.Get_Status();
                },
                .HydrationApply = [](FCk_Handle& InEntity, const FInstancedStruct& InNew,
                    const TOptional<FInstancedStruct>&) -> ECk_Persistence_ApplyResult
                {
                    const auto* Data = InNew.GetPtr<FCk_RepData_HexGridState>();
                    if (ck::Is_NOT_Valid(InEntity) || UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InEntity))
                    { return ECk_Persistence_ApplyResult::Rejected; }
                    if (NOT UCk_Utils_HexGrid_UE::Has(InEntity))
                    {
                        return InEntity.Has<FTag_HexGrid_RecipeRejected>() ? ECk_Persistence_ApplyResult::Rejected :
                            ECk_Persistence_ApplyResult::NotReady;
                    }
                    auto Board = UCk_Utils_HexGrid_UE::Cast(InEntity);
                    if (Data == nullptr)
                    {
                        RecordRestoreFailure(Board, ECk_HexGridFailure::InvalidSchema);
                        return ECk_Persistence_ApplyResult::Rejected;
                    }
                    const auto Outcome = TryReplaceCommittedProjection(Board, *Data, ECk_HexGridApplySource::Hydration);
                    if (Outcome.Get_Status() == ECk_Persistence_ApplyResult::Rejected)
                    { RecordRestoreFailure(Board, Outcome.Get_Failure(), Outcome.Get_PlacementId()); }
                    return Outcome.Get_Status();
                },
                .NetRemove = [](FCk_Handle& InEntity)
                {
                    if (!UCk_Utils_HexGrid_UE::Has(InEntity)) { return; }
                    auto Board = UCk_Utils_HexGrid_UE::Cast(InEntity);
                    if (!CanReceiveNetBaseline(Board)) { return; }
                    OnNetBaselineRemoved(Board);
                }
            });
        }
    };

    static FHexGridPersistenceRegistrar GCkGridHexPersistenceRegistrar;
}
