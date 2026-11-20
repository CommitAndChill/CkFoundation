#include "CkHexGridObject_Utils.h"

#include "CkHexGridObject_Fragment.h"
#include "CkHexGridObject_Geometry.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Rules.h"
#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Utils.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridPersistence_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridObjectBinding_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridReplication_Processor.h"
#include "CkEcs/Tag/CkTag.h"

#include <Misc/ScopeExit.h>

namespace ck::hex_grid
{
    using FObjectFailure = ECk_HexGridObject_Failure;
    using FGeometry = UCk_Utils_HexGeometry_UE;
    using FLifetime = UCk_Utils_EntityLifetime_UE;

    CK_DEFINE_ECS_TAG(FTag_HexGridObject_Composing);

    auto CanMintObjectBinding(const FCk_Handle& InObject) -> bool
    {
        using FNet = UCk_Utils_Net_UE;
        const auto Source = FNet::Has(InObject) ? InObject : FLifetime::Get_EntityInOwnershipChain_If(InObject,
            [](const FCk_Handle& Entity) { return ck::IsValid(Entity) && FNet::Has(Entity); }, ECk_PendingKill_Policy::IncludePendingKill);
        if (!ck::IsValid(Source) || !FNet::Get_HasAuthority(Source)) { return false; }
        const auto Mode = FNet::Get_EntityNetMode(Source);
        return Mode == ECk_Net_NetModeType::Host || Mode == ECk_Net_NetModeType::ClientAndHost ||
            (Mode == ECk_Net_NetModeType::Client && FNet::Get_EntityReplication(Source) == ECk_Replication::DoesNotReplicate);
    }

    auto UpdateObjectIdentity(FCk_Handle InObject, const ck::persistence::FInitializationStatus& InStatus) -> void
    {
        if (!hex_rules::IsLive(InObject) || NOT InObject.Has<ck::FFragment_HexGridObjectIdentity>())
        { return; }

        if (InStatus.Get_Disposition() == ck::persistence::EInitializationDisposition::Fresh &&
            CanMintObjectBinding(InObject) &&
            NOT InObject.Get<ck::FFragment_HexGridObjectIdentity>().Get_BindingId().IsValid())
        { RegisterObjectBinding(InObject, FGuid::NewGuid()); }
        ResolveObjectBinding(InObject);
        if (InStatus.Get_IsReleased()) { ArmObjectReplication(InObject); }
    }

    auto GetAcceptedObjectBindingId(const FCk_Handle& InObject) -> FGuid
    {
        if (!hex_rules::IsLive(InObject) || NOT InObject.Has<ck::FFragment_HexGridObjectIdentity>())
        { return {}; }

        const auto& Identity = InObject.Get<ck::FFragment_HexGridObjectIdentity>();
        if (UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(InObject))
        { return Identity.Get_HasNetBaseline() ? Identity.Get_BindingId() : FGuid{}; }
        const auto State = ck::persistence::Query(InObject, FCk_RepData_HexGridObjectIdentity::StaticStruct()).Get_Disposition();
        if (State != ck::persistence::EInitializationDisposition::Fresh &&
            State != ck::persistence::EInitializationDisposition::BaselineCommitted &&
            State != ck::persistence::EInitializationDisposition::Released)
        { return {}; }

        return InObject.Get<ck::FFragment_HexGridObjectIdentity>().Get_BindingId();
    }

    auto ValidateOffsets(const TArray<FCk_HexCoord>& InOffsets) -> FObjectFailure
    {
        if (InOffsets.IsEmpty())
        { return FObjectFailure::EmptyFootprint; }
        if (InOffsets.Num() > ck::hex_grid::DomainCellLimit)
        { return FObjectFailure::TooManyCells; }
        const auto Unique = TSet<FCk_HexCoord>{InOffsets};
        return Unique.Num() == InOffsets.Num() ? FObjectFailure::None : FObjectFailure::DuplicateOffset;
    }

    auto ComposeObject(FCk_Handle InOwner, const FCk_HexGridObject_Spec& InSpec,
        const FCk_HexGridInitialization& InInitialization, bool InCreateChild,
        FCk_Handle_HexGridObject& OutObject, FObjectFailure& OutFailure) -> bool
    {
        OutObject = {};
        if (!IsInGameThread() || !FLifetime::Get_CanCreateEntity(InOwner)
            || FLifetime::Get_IsPendingDestroy(InOwner, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
        { OutFailure = FObjectFailure::InvalidOwner; return false; }
        if (!InCreateChild && (UCk_Utils_HexGridObject_UE::Has(InOwner) ||
            InOwner.Has<ck::FFragment_HexGridObjectIdentity>() || InOwner.Has<FTag_HexGridObject_Composing>()))
        { OutFailure = FObjectFailure::AlreadyComposed; return false; }
        OutFailure = ValidateOffsets(InSpec.Get_FootprintOffsets());
        if (OutFailure != FObjectFailure::None)
        { return false; }
        auto Target = InCreateChild ? FLifetime::Request_CreateEntity(InOwner) : InOwner;
        if (ck::Is_NOT_Valid(Target))
        { OutFailure = FObjectFailure::CompositionFailed; return false; }

        Target.Add<FTag_HexGridObject_Composing>();
        ON_SCOPE_EXIT
        {
            if (ck::IsValid(Target, ck::IsValid_Policy_IncludePendingKill{}))
            { Target.Try_Remove<FTag_HexGridObject_Composing>(); }
        };

        if (UCk_Utils_EntityReplicationDriver_UE::Get_IsIncomingReplica(Target))
        {
            // This object's authority-issued key arrives on its own driver independently of the board.
            Target.Add<ck::FFragment_HexGridObjectIdentity>();
            Target.Add<ck::FFragment_HexGridObject_Params>(InSpec);
            OutObject = UCk_Utils_HexGridObject_UE::CastChecked(Target);
            return true;
        }
        auto Identity = ck::persistence::FInitializationIdentity{};
        if (NOT InInitialization.TryResolve(Target, Identity))
        {
            if (InCreateChild)
            { FLifetime::Request_DestroyEntity(Target); }
            OutFailure = FObjectFailure::InvalidInitializationIdentity;
            return false;
        }

        const auto* Type = FCk_RepData_HexGridObjectIdentity::StaticStruct();
        const auto Initialization = ck::persistence::RequestInitialization(Target, Type, Identity);
        if (ck::Is_NOT_Valid(Target) ||
            FLifetime::Get_IsPendingDestroy(Target, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
        {
            OutFailure = FObjectFailure::CompositionFailed;
            return false;
        }

        if (Initialization.Get_Disposition() == ck::persistence::EInitializationDisposition::Undeclared ||
            (Initialization.Get_Disposition() == ck::persistence::EInitializationDisposition::Unavailable &&
                Initialization.Get_Failure() == ck::persistence::EInitializationFailure::Rejected))
        {
            if (InCreateChild && ck::IsValid(Target))
            { FLifetime::Request_DestroyEntity(Target); }
            OutFailure = FObjectFailure::InitializationIdentityRequired;
            return false;
        }

        Target.Add<ck::FFragment_HexGridObjectIdentity>();
        Target.Add<ck::FFragment_HexGridObject_Params>(InSpec);
        if (NOT ck::persistence::Subscribe(Target, Type, TEXT("HexGridObjectIdentity"), UpdateObjectIdentity))
        {
            Target.Remove<ck::FFragment_HexGridObject_Params>();
            Target.Remove<ck::FFragment_HexGridObjectIdentity>();
            if (InCreateChild)
            { FLifetime::Request_DestroyEntity(Target); }
            OutFailure = FObjectFailure::CompositionFailed;
            return false;
        }

        // A surrounding coherent-release batch may defer the subscription's initial edge.
        if (!hex_rules::IsLive(Target)) { OutFailure = FObjectFailure::CompositionFailed; return false; }
        UpdateObjectIdentity(Target, ck::persistence::Query(Target, Type));
        if (!hex_rules::IsLive(Target)) { OutFailure = FObjectFailure::CompositionFailed; return false; }
        if (ck::persistence::Query(Target, Type).Get_Disposition() == ck::persistence::EInitializationDisposition::Fresh &&
            CanMintObjectBinding(Target) && !GetAcceptedObjectBindingId(Target).IsValid())
        {
            ck::persistence::Unsubscribe(Target, Type, TEXT("HexGridObjectIdentity"));
            Target.Remove<ck::FFragment_HexGridObject_Params>();
            Target.Remove<ck::FFragment_HexGridObjectIdentity>();
            if (InCreateChild) { FLifetime::Request_DestroyEntity(Target); }
            OutFailure = FObjectFailure::CompositionFailed;
            return false;
        }
        OutObject = UCk_Utils_HexGridObject_UE::CastChecked(Target);
        return true;
    }

    auto IsConnected(const TArray<FCk_HexCoord>& InCells) -> bool
    {
        if (InCells.IsEmpty()) { return false; }
        auto Remaining = TSet<FCk_HexCoord>{InCells};
        TArray<FCk_HexCoord> Frontier;
        Frontier.Add(InCells[0]);
        Remaining.Remove(InCells[0]);
        while (!Frontier.IsEmpty() && !Remaining.IsEmpty())
        {
            const auto Cell = Frontier.Pop(EAllowShrinking::No);
            for (int32 Index = 0; Index < 6; ++Index)
            {
                FCk_HexCoord Neighbor;
                ECk_HexGeometryFailure Reason{};
                if (FGeometry::TryGet_Neighbor(Cell, static_cast<ECk_HexDirection>(Index), Neighbor, Reason)
                    && Remaining.Remove(Neighbor) != 0)
                { Frontier.Add(Neighbor); }
            }
        }
        return Remaining.IsEmpty();
    }
}

bool UCk_Utils_HexGridObject_UE::Has(const FCk_Handle& InHandle)
{
    return ck::IsValid(InHandle) && InHandle.Has<ck::FFragment_HexGridObject_Params>();
}

FCk_Handle_HexGridObject UCk_Utils_HexGridObject_UE::DoCast(FCk_Handle& InHandle, ECk_SucceededFailed& OutResult)
{
    const auto Object = Cast(InHandle);
    OutResult = ck::IsValid(Object) ? ECk_SucceededFailed::Succeeded : ECk_SucceededFailed::Failed;
    return Object;
}

FCk_Handle_HexGridObject UCk_Utils_HexGridObject_UE::DoCastChecked(FCk_Handle InHandle)
{ return CastChecked(InHandle); }

bool UCk_Utils_HexGridObject_UE::TryAdd(FCk_Handle& InHandle, const FCk_HexGridObject_Spec& InSpec,
    FCk_Handle_HexGridObject& OutObject, ECk_HexGridObject_Failure& OutFailure)
{ return TryAddWithInitialization(InHandle, InSpec, {}, OutObject, OutFailure); }

bool UCk_Utils_HexGridObject_UE::TryCreate(FCk_Handle& InOwner, const FCk_HexGridObject_Spec& InSpec,
    FCk_Handle_HexGridObject& OutObject, ECk_HexGridObject_Failure& OutFailure)
{ return TryCreateWithInitialization(InOwner, InSpec, {}, OutObject, OutFailure); }

auto
    UCk_Utils_HexGridObject_UE::TryAddWithInitialization(FCk_Handle& InHandle, const FCk_HexGridObject_Spec& InSpec,
        const FCk_HexGridInitialization& InInitialization,
        FCk_Handle_HexGridObject& OutObject, ECk_HexGridObject_Failure& OutFailure)
    -> bool
{
    return ck::hex_grid::ComposeObject(InHandle, InSpec, InInitialization, false, OutObject, OutFailure);
}

auto
    UCk_Utils_HexGridObject_UE::TryCreateWithInitialization(FCk_Handle& InOwner, const FCk_HexGridObject_Spec& InSpec,
        const FCk_HexGridInitialization& InInitialization,
        FCk_Handle_HexGridObject& OutObject, ECk_HexGridObject_Failure& OutFailure)
    -> bool
{
    return ck::hex_grid::ComposeObject(InOwner, InSpec, InInitialization, true, OutObject, OutFailure);
}

FCk_HexGridObject_Spec UCk_Utils_HexGridObject_UE::Get_Spec(const FCk_Handle_HexGridObject& InObject)
{
    if (!Has(InObject))
    { return {}; }
    return InObject.Get<ck::FFragment_HexGridObject_Params>().Get_Spec();
}

FCk_HexGridObject_ResolveResult UCk_Utils_HexGridObject_UE::TryResolve_Cells(const FCk_Handle_HexGridObject& InObject,
    const FCk_HexCoord& InAnchor, ECk_HexRotation InRotation, ECk_HexGridConnectivity InConnectivity)
{
    using FResult = FCk_HexGridObject_ResolveResult;
    if (!Has(InObject))
    { return FResult{ECk_HexGridObject_Failure::InvalidObject}; }
    return ck::hex_grid::ResolveOffsets(InObject.Get<ck::FFragment_HexGridObject_Params>().Get_Spec().Get_FootprintOffsets(), InAnchor, InRotation, InConnectivity);
}

FCk_HexGridObject_ResolveResult ck::hex_grid::ResolveOffsets(const TArray<FCk_HexCoord>& Offsets, const FCk_HexCoord& InAnchor,
    ECk_HexRotation InRotation, ECk_HexGridConnectivity InConnectivity)
{
    using FResult = FCk_HexGridObject_ResolveResult;
    if (InConnectivity != ECk_HexGridConnectivity::Ignore && InConnectivity != ECk_HexGridConnectivity::RequireConnected)
    { return FResult{ECk_HexGridObject_Failure::InvalidConnectivity}; }

    TArray<FCk_HexCoord> Cells;
    Cells.Reserve(Offsets.Num());
    for (const auto& Offset : Offsets)
    {
        FCk_HexCoord Rotated;
        FCk_HexCoord Cell;
        ECk_HexGeometryFailure Reason{};
        if (!UCk_Utils_HexGeometry_UE::TryRotate(Offset, InRotation, Rotated, Reason))
        { return FResult{Reason == ECk_HexGeometryFailure::InvalidRotation ? ECk_HexGridObject_Failure::InvalidRotation : ECk_HexGridObject_Failure::Overflow}; }
        if (!UCk_Utils_HexGeometry_UE::TryTranslate(InAnchor, Rotated, Cell, Reason))
        { return FResult{ECk_HexGridObject_Failure::Overflow}; }
        Cells.Add(Cell);
    }
    if (InConnectivity == ECk_HexGridConnectivity::RequireConnected && !ck::hex_grid::IsConnected(Cells))
    { return FResult{ECk_HexGridObject_Failure::Disconnected}; }
    Cells.Sort([](const FCk_HexCoord& A, const FCk_HexCoord& B)
    {
        return A.Get_R() == B.Get_R() ? A.Get_Q() < B.Get_Q() : A.Get_R() < B.Get_R();
    });
    return FResult{MoveTemp(Cells)};
}
