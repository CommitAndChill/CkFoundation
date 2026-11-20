#include "CkHexGridReplication_Processor.h"

#include "CkHexGridPersistence_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Fragment.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"
#include "Engine/World.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_HexGrid_Publish);

namespace ck::hex_grid
{
    namespace
    {
        using Net = UCk_Utils_Net_UE;
        auto ShouldPublish(const FCk_Handle& InEntity) -> bool
        {
            if (!hex_rules::IsLive(InEntity) || !Net::Has(InEntity) || !Net::Get_HasAuthority(InEntity) ||
                Net::Get_EntityReplication(InEntity) != ECk_Replication::Replicates) { return false; }
            const auto Mode = Net::Get_EntityNetMode(InEntity);
            return Mode == ECk_Net_NetModeType::Host || Mode == ECk_Net_NetModeType::ClientAndHost;
        }

        template<typename T>
        auto Publish(FCk_Handle& InEntity, const T& InPayload) -> bool
        {
            switch (Net::TryAddContainerFragment<T>(InEntity, InPayload))
            {
                case ECk_AddedOrNot::Added: return true;
                case ECk_AddedOrNot::AlreadyExists: return Net::TryUpdateContainerFragment<T>(InEntity, InPayload);
                default: return false;
            }
        }
    }

    bool IsLinkedReplicaAwaitingRecipe(const FCk_Handle& InEntity)
    {
        if (!hex_rules::IsLive(InEntity) || !InEntity.Has<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>())
        { return false; }
        const auto* Driver = InEntity.Get<TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>>().Get();
        const auto* World = IsValid(Driver) ? Driver->GetWorld() : nullptr;
        return World && !World->bIsTearingDown && World->IsNetMode(NM_Client) && Driver->Get_AssociatedEntity() == InEntity;
    }

    void ArmBoardReplication(FCk_Handle InBoard)
    {
        if (!ShouldPublish(InBoard) || !InBoard.Has<FFragment_HexGrid_Params>() ||
            InBoard.Get<FFragment_HexGrid_Params>().Get_RecipeIdentity().Get_AssetPath().IsEmpty() ||
            !UCk_Utils_HexGrid_UE::Get_IsReady(UCk_Utils_HexGrid_UE::Cast(InBoard))) { return; }
        InBoard.AddOrGet<FFragment_HexGrid_PendingPublish>()._Board = true;
    }

    void ArmObjectReplication(FCk_Handle InObject)
    {
        if (!ShouldPublish(InObject) || !GetAcceptedObjectBindingId(InObject).IsValid()) { return; }
        InObject.AddOrGet<FFragment_HexGrid_PendingPublish>()._Object = true;
    }
}

void ck::FProcessor_HexGrid_Publish::ForEachEntity(TimeType, HandleType InEntity,
    FFragment_HexGrid_PendingPublish& InPending, const TObjectPtr<UCk_Fragment_EntityReplicationDriver_Rep>& InDriver) const
{
    using namespace hex_grid;
    if (!hex_rules::IsLive(InEntity) || !IsValid(InDriver.Get())) { return; }
    if (InPending._Board && UCk_Utils_HexGrid_UE::Get_IsReady(UCk_Utils_HexGrid_UE::Cast(InEntity)))
    {
        auto Payload = UCk_Utils_Net_UE::TryProduce<FCk_RepData_HexGridState>(InEntity);
        if (Payload.IsSet())
        {
            // Each object's own driver carries its stable key. A physical weak handle would
            // couple board decoding to that driver's mapping order and relevancy.
            for (auto& Entry : Payload->Get_Placements()) { Entry.Set_Occupant({}); }
            if (Publish(InEntity, *Payload)) { InPending._Board = false; }
        }
    }
    if (InPending._Object)
    {
        const auto Payload = UCk_Utils_Net_UE::TryProduce<FCk_RepData_HexGridObjectIdentity>(InEntity);
        if (Payload.IsSet() && Publish(InEntity, *Payload)) { InPending._Object = false; }
    }
    if (!InPending._Board && !InPending._Object) { InEntity.Remove<FFragment_HexGrid_PendingPublish>(); }
}
