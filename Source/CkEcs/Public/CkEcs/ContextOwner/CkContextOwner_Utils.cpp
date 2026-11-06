#include "CkContextOwner_Utils.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkEcs/ContextOwner/CkContextOwner_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ContextOwner_UE::
    Get_ContextOwner(
        const FCk_Handle& InHandle)
    -> FCk_Handle
{
    return InHandle.Get<ck::FFragment_ContextOwner, ck::IsValid_Policy_IncludePendingKill>().Get_Entity();
}

auto
    UCk_Utils_ContextOwner_UE::
    Request_Override(
        FCk_Handle& InEntity,
        const FCk_Handle& InNewContextOwner,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> void
{
    const auto EntityIsValid = ck::IsValid(InEntity);
    CK_ENSURE_IF_NOT(EntityIsValid,
        TEXT("Cannot override context owner of invalid entity"))
    {
        InDelegate.ExecuteIfBound(InEntity, ECk_Request_OperationResult::Failed_NotEnqueued);
        return;
    }

    const auto NewContextOwnerIsValid = ck::IsValid(InNewContextOwner);
    CK_ENSURE_IF_NOT(NewContextOwnerIsValid,
        TEXT("Cannot override context owner with invalid entity"))
    {
        InDelegate.ExecuteIfBound(InEntity, ECk_Request_OperationResult::Failed_NotEnqueued);
        return;
    }

    if (NOT Has(InEntity))
    {
        Request_SetupEntityWithContextOwner(InEntity, InNewContextOwner);
    }

    const auto CurrentContextOwner = Get_ContextOwner(InEntity);

    if (CurrentContextOwner == InNewContextOwner)
    {
        // Already the requested owner — the caller's intent holds, so this is a success, not a rejection.
        InDelegate.ExecuteIfBound(InEntity, ECk_Request_OperationResult::Succeeded);
        return;
    }

    InEntity.Replace<ck::FFragment_ContextOwner>(InNewContextOwner);

    UCk_Utils_EntityLifetime_UE::ForEach_LifetimeDependent(InEntity, [&](FCk_Handle& InDependent)
    {
        if (ck::Is_NOT_Valid(InDependent) || NOT Has(InDependent))
        { return; }

        // Only dependents that inherited the owner being replaced follow it; an overridden dependent keeps its own.
        if (Get_ContextOwner(InDependent) == CurrentContextOwner)
        { Request_Override(InDependent, InNewContextOwner, {}); }
    });

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InEntity, ECk_Request_OperationResult::Succeeded);
}

auto
    UCk_Utils_ContextOwner_UE::
    Request_OverrideToSelf(
        FCk_Handle& InEntity,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> void
{
    Request_Override(InEntity, InEntity, InDelegate);
}

auto
    UCk_Utils_ContextOwner_UE::
    Has(
        const FCk_Handle& InHandle)
    -> bool
{
     return InHandle.Has<ck::FFragment_ContextOwner>();
}

auto
    UCk_Utils_ContextOwner_UE::
    Request_SetupEntityWithContextOwner(
        FCk_Handle& InNewEntity,
        const FCk_Handle& InContextOwner)
    -> void
{
    if (InNewEntity.Has<ck::FFragment_ContextOwner>())
    {
        const auto& CurrentContextOwner = InNewEntity.Get<ck::FFragment_ContextOwner>().Get_Entity();
        CK_ENSURE
        (
            CurrentContextOwner == InContextOwner,
            TEXT("Trying to Setup Entity [{}] with ContextOwner [{}] but it is already setup with [{}]"),
            InNewEntity,
            InContextOwner,
            CurrentContextOwner
        );

        return;
    }

    InNewEntity.Add<ck::FFragment_ContextOwner>(InContextOwner);
}

// --------------------------------------------------------------------------------------------------------------------
