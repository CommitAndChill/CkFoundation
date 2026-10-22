#include "CkGameSettings_SpecSupport.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_SpecStorageProvider_UE::
    Get_StoredValues_Implementation(
        ECk_GameSettings_Scope InScope,
        int32 InUserId,
        TArray<FCk_GameSettings_StoredValue>& OutValues) const
    -> ECk_GameSettings_StorageReadResult
{
    OutValues.Reset();
    if (Get_RejectPlayerRead() && InScope == ECk_GameSettings_Scope::Player)
    {
        return ECk_GameSettings_StorageReadResult::Failed;
    }
    for (const auto& Value : Get_Values())
    {
        if (Value.Get_Scope() == InScope && Value.Get_PlatformUserId() == InUserId)
        {
            OutValues.Emplace(Value.Get_Key(), Value.Get_Value());
        }
    }
    return ECk_GameSettings_StorageReadResult::Succeeded;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_SpecStorageProvider_UE::
    Request_StoreValues_Implementation(
        const TArray<FCk_GameSettings_StoreValue>& InValues)
    -> bool
{
    ++_BatchCount;
    if (Get_RejectWrites())
    {
        return false;
    }
    auto Next = Get_Values();
    for (const auto& Value : InValues)
    {
        Next.RemoveAll([&](const FCk_GameSettings_StoreValue& Existing)
        {
            return Existing.Get_Scope() == Value.Get_Scope() && Existing.Get_PlatformUserId() == Value.Get_PlatformUserId()
                && Existing.Get_Key() == Value.Get_Key();
        });
        Next.Add(Value);
    }
    Set_Values(MoveTemp(Next));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_SpecStorageProvider_UE::
    Request_Flush_Implementation()
    -> bool
{
    return NOT Get_RejectFlush();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_GameSettings_SpecAccess::
    Request_SetProvider(
        UCk_GameSettings_Subsystem_UE* InSubsystem,
        UCk_GameSettings_StorageProvider_UE* InProvider)
    -> void
{
    InSubsystem->_StorageProvider = InProvider;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_SpecCallback_UE::
    Get_ExternalBool_ForDelegate()
    -> bool
{
    return Get_ExternalBool();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_SpecCallback_UE::
    Request_SetBool(
        bool InValue)
    -> void
{
    _ExternalBool = InValue;
    ++_ApplyCount;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_SpecCallback_UE::
    Request_ApplyAndReenter(
        const FString& InValue)
    -> void
{
    ++_ApplyCount;
    _ObservedB = Get_Subsystem()->Get_SettingValue_String(TEXT("atomic.b"));
    _ObservedPending = Get_Subsystem()->Get_HasPendingChanges();
    _ObservedUnapplied = Get_Subsystem()->Get_HasUnappliedChange(TEXT("atomic.a"));
    if (NOT Get_Reenter())
    {
        return;
    }
    _NestedAccepted = Get_Subsystem()->Request_SetSettingValue_String(
        FCk_Request_GameSettings_SetValue_String{TEXT("atomic.b"), TEXT("nested")});
}
