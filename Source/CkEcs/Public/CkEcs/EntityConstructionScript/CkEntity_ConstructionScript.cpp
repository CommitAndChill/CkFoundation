#include "CkEntity_ConstructionScript.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Object/CkObject_Utils.h"

#include "CkEcs/CkEcsLog.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Entity_ConstructionScript_PDA::
    Construct(
        FCk_Handle& InHandle,
        UObject* InOptionalObjectConstructionScript) const
    -> void
{
    _CurrentWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);

    UCk_Utils_Handle_UE::Set_DebugName(
        InHandle, UCk_Utils_Debug_UE::Get_DebugName(this, ECk_DebugNameVerbosity_Policy::Compact), ECk_Override::DoNotOverride);

    DoConstruct(InHandle);

    if (ck::IsValid(InOptionalObjectConstructionScript) && InOptionalObjectConstructionScript->Implements<UCk_Entity_ConstructionScript_Interface>())
    {
        ICk_Entity_ConstructionScript_Interface::Execute_DoConstruct(InOptionalObjectConstructionScript, InHandle);
    }
}

auto
    UCk_Entity_ConstructionScript_PDA::
    Request_Construct(
        FCk_Handle& InHandle,
        TSubclassOf<UCk_Entity_ConstructionScript_PDA> InConstructionScript,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle
{
    const auto& ConstructionScriptCDO = UCk_Utils_Object_UE::Get_ClassDefaultObject<UCk_Entity_ConstructionScript_PDA>(InConstructionScript);
    return Request_Construct_Instanced(InHandle, ConstructionScriptCDO, InDelegate);
}

auto
    UCk_Entity_ConstructionScript_PDA::
    Request_Construct_Instanced(
        FCk_Handle& InHandle,
        const UCk_Entity_ConstructionScript_PDA* InConstructionScript,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle
{
    CK_ENSURE_IF_NOT(ck::IsValid(InConstructionScript),
        TEXT("Unable to proceed with Entity Construction as the Construction Script [{}] is INVALID."), InConstructionScript)
    {
        InDelegate.ExecuteIfBound(InHandle, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InHandle;
    }

    InConstructionScript->Construct(InHandle, nullptr);

    // Immediate mutation — nothing is enqueued, so completion is synchronous on this stack.
    InDelegate.ExecuteIfBound(InHandle, ECk_Request_OperationResult::Succeeded);

    return InHandle;
}

auto
    UCk_Entity_ConstructionScript_PDA::
    Request_Construct_Multiple(
        FCk_Handle& InHandle,
        TArray<TSubclassOf<UCk_Entity_ConstructionScript_PDA>> InConstructionScript,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle
{
    CK_ENSURE_IF_NOT(ck::IsValid(InHandle),
        TEXT("Unable to construct a script batch on an invalid entity"))
    {
        InDelegate.ExecuteIfBound(InHandle, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InHandle;
    }

#if NOT CK_DISABLE_ENSURE_CHECKS
    for (const auto& ConstructionScript : InConstructionScript)
    {
        CK_ENSURE_IF_NOT(ck::IsValid(ConstructionScript),
            TEXT("Unable to construct script batch: every construction script class must be valid"))
        {
            InDelegate.ExecuteIfBound(InHandle, ECk_Request_OperationResult::Failed_NotEnqueued);
            return InHandle;
        }
    }
#endif

    auto ConstructionScripts = TArray<const UCk_Entity_ConstructionScript_PDA*>{};
    ConstructionScripts.Reserve(InConstructionScript.Num());
    for (const auto& ConstructionScript : InConstructionScript)
    {
        // Resolving a CDO can construct it. Resolve once as required execution data, outside validation.
        const auto* ConstructionScriptCDO = ConstructionScript.GetDefaultObject();
        CK_ENSURE_IF_NOT(ck::IsValid(ConstructionScriptCDO),
            TEXT("Unable to construct script batch: every construction script CDO must be valid"))
        {
            InDelegate.ExecuteIfBound(InHandle, ECk_Request_OperationResult::Failed_NotEnqueued);
            return InHandle;
        }
        ConstructionScripts.Emplace(ConstructionScriptCDO);
    }

    for (const auto* ConstructionScript : ConstructionScripts)
    {
        Request_Construct_Instanced(InHandle, ConstructionScript, {});
    }

    // All entries are admitted before the first script runs; Construct itself has no failure result.
    InDelegate.ExecuteIfBound(InHandle, ECk_Request_OperationResult::Succeeded);

    return InHandle;
}

auto
    UCk_Entity_ConstructionScript_PDA::
    ShowReplicationInEditor() const
    -> bool
{
    return true;
}

auto
    UCk_Entity_ConstructionScript_PDA::
    DoConstruct_Implementation(
        FCk_Handle& InHandle) const
    -> void
{
}

// --------------------------------------------------------------------------------------------------------------------
