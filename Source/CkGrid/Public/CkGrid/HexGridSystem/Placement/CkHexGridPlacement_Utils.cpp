#include "CkHexGridPlacement_Utils.h"

#include "CkHexGridPlacement_Fragment.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGridOccupancy_Fragment.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGridOccupancy_Transaction.h"

bool UCk_Utils_HexGridPlacement_UE::Has(const FCk_Handle& InHandle)
{
    return ck::IsValid(InHandle) && InHandle.Has<ck::FFragment_HexGridPlacement>();
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::BindTo_OnOccupantBindingConflict(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGridPlacement_BindingConflict& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior)
{
    if (ck::hex_rules::IsLive(InGrid))
    { CK_SIGNAL_BIND(ck::UUtils_Signal_HexGridPlacement_BindingConflict, InGrid, InDelegate, InBindingPolicy, InPostFireBehavior); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::UnbindFrom_OnOccupantBindingConflict(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGridPlacement_BindingConflict& InDelegate)
{
    if (ck::IsValid(InGrid))
    { CK_SIGNAL_UNBIND(ck::UUtils_Signal_HexGridPlacement_BindingConflict, InGrid, InDelegate); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::BindTo_OnOccupantBindingFailed(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGrid_OccupantBindingFailed& InDelegate, ECk_Signal_BindingPolicy InBindingPolicy,
    ECk_Signal_PostFireBehavior InPostFireBehavior)
{
    if (ck::hex_rules::IsLive(InGrid))
    { CK_SIGNAL_BIND(ck::UUtils_Signal_HexGrid_OccupantBindingFailed, InGrid, InDelegate, InBindingPolicy, InPostFireBehavior); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::UnbindFrom_OnOccupantBindingFailed(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGrid_OccupantBindingFailed& InDelegate)
{
    if (ck::IsValid(InGrid))
    { CK_SIGNAL_UNBIND(ck::UUtils_Signal_HexGrid_OccupantBindingFailed, InGrid, InDelegate); }
    return InGrid;
}

FCk_Handle_HexGridPlacement UCk_Utils_HexGridPlacement_UE::DoCast(FCk_Handle& InHandle, ECk_SucceededFailed& OutResult)
{
    const auto Placement = Cast(InHandle);
    OutResult = ck::IsValid(Placement) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Placement;
}

FCk_Handle_HexGridPlacement UCk_Utils_HexGridPlacement_UE::DoCastChecked(FCk_Handle InHandle)
{ return CastChecked(InHandle); }

FCk_HexGridPlacement_Result UCk_Utils_HexGridPlacement_UE::Get_CanPlace(const FCk_Handle_HexGrid& InGrid,
    const FCk_HexGridPlacement_Spec& InSpec)
{ return ck::hex_grid::PreviewPlace(InGrid, InSpec); }

FCk_HexGridPlacement_Result UCk_Utils_HexGridPlacement_UE::Get_CanMove(const FCk_Handle_HexGrid& InGrid,
    FGuid InPlacementId, const FCk_HexCoord& InAnchor, ECk_HexRotation InRotation)
{ return ck::hex_grid::PreviewMove(InGrid, InPlacementId, InAnchor, InRotation); }

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::Request_Place(FCk_Handle_HexGrid& InGrid,
    const FCk_Request_HexGridPlacement_Place& InRequest, const FCk_Delegate_Request_OnCompleted& InDelegate)
{
    auto Request = InRequest;
    Request.Set_CompletionDelegate(InDelegate);
    ck::hex_rules::Enqueue(InGrid, InGrid, MoveTemp(Request));
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::Request_Move(FCk_Handle_HexGrid& InGrid,
    const FCk_Request_HexGridPlacement_Move& InRequest, const FCk_Delegate_Request_OnCompleted& InDelegate)
{
    auto Request = InRequest;
    Request.Set_CompletionDelegate(InDelegate);
    ck::hex_rules::Enqueue(InGrid, InGrid, MoveTemp(Request));
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::Request_Remove(FCk_Handle_HexGrid& InGrid,
    const FCk_Request_HexGridPlacement_Remove& InRequest, const FCk_Delegate_Request_OnCompleted& InDelegate)
{
    auto Request = InRequest;
    Request.Set_CompletionDelegate(InDelegate);
    ck::hex_rules::Enqueue(InGrid, InGrid, MoveTemp(Request));
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::BindTo_OnObjectPlaced(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGridPlacement_ObjectPlaced& InDelegate,
    ECk_Signal_BindingPolicy InBindingPolicy, ECk_Signal_PostFireBehavior InPostFireBehavior)
{
    if (ck::hex_rules::IsLive(InGrid))
    { CK_SIGNAL_BIND(ck::UUtils_Signal_HexGridPlacement_ObjectPlaced, InGrid, InDelegate, InBindingPolicy, InPostFireBehavior); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::UnbindFrom_OnObjectPlaced(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGridPlacement_ObjectPlaced& InDelegate)
{
    if (ck::IsValid(InGrid))
    { CK_SIGNAL_UNBIND(ck::UUtils_Signal_HexGridPlacement_ObjectPlaced, InGrid, InDelegate); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::BindTo_OnObjectMoved(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGridPlacement_ObjectMoved& InDelegate,
    ECk_Signal_BindingPolicy InBindingPolicy, ECk_Signal_PostFireBehavior InPostFireBehavior)
{
    if (ck::hex_rules::IsLive(InGrid))
    { CK_SIGNAL_BIND(ck::UUtils_Signal_HexGridPlacement_ObjectMoved, InGrid, InDelegate, InBindingPolicy, InPostFireBehavior); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::UnbindFrom_OnObjectMoved(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGridPlacement_ObjectMoved& InDelegate)
{
    if (ck::IsValid(InGrid))
    { CK_SIGNAL_UNBIND(ck::UUtils_Signal_HexGridPlacement_ObjectMoved, InGrid, InDelegate); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::BindTo_OnObjectRemoved(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGridPlacement_ObjectRemoved& InDelegate,
    ECk_Signal_BindingPolicy InBindingPolicy, ECk_Signal_PostFireBehavior InPostFireBehavior)
{
    if (ck::hex_rules::IsLive(InGrid))
    { CK_SIGNAL_BIND(ck::UUtils_Signal_HexGridPlacement_ObjectRemoved, InGrid, InDelegate, InBindingPolicy, InPostFireBehavior); }
    return InGrid;
}

FCk_Handle_HexGrid UCk_Utils_HexGridPlacement_UE::UnbindFrom_OnObjectRemoved(FCk_Handle_HexGrid& InGrid,
    const FCk_Delegate_HexGridPlacement_ObjectRemoved& InDelegate)
{
    if (ck::IsValid(InGrid))
    { CK_SIGNAL_UNBIND(ck::UUtils_Signal_HexGridPlacement_ObjectRemoved, InGrid, InDelegate); }
    return InGrid;
}
