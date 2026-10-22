#include "CkGameSettings_StorageProvider.h"

#include "CkCore/Ensure/CkEnsure.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_StorageProvider_UE::
    Get_StoredValues_Implementation(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId,
        TArray<FCk_GameSettings_StoredValue>& OutValues) const
    -> ECk_GameSettings_StorageReadResult
{
    OutValues.Reset();
    return ECk_GameSettings_StorageReadResult::Failed;
}

auto
    UCk_GameSettings_StorageProvider_UE::
    Request_StoreValue_Implementation(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId,
        FName InKey,
        const FString& InValue)
    -> bool
{
    return Request_StoreValues({FCk_GameSettings_StoreValue{InScope, InPlatformUserId, InKey, InValue}});
}

auto
    UCk_GameSettings_StorageProvider_UE::
    Request_StoreValues_Implementation(
        const TArray<FCk_GameSettings_StoreValue>& InValues)
    -> bool
{
    if (InValues.IsEmpty())
    { return true; }
    return false;
}

auto
    UCk_GameSettings_StorageProvider_UE::
    Request_RemoveValue_Implementation(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId,
        FName InKey)
    -> bool
{
    return false;
}

auto
    UCk_GameSettings_StorageProvider_UE::
    Request_Flush_Implementation()
    -> bool
{
    return false;
}
