#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkGameSettings/CkGameSettings_Common.h"

#include "CkGameSettings_StorageProvider.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMESETTINGS_API FCk_GameSettings_StoredValue
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameSettings_StoredValue);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FName _Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FString _Value;

public:
    CK_PROPERTY_GET(_Key);
    CK_PROPERTY_GET(_Value);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameSettings_StoredValue, _Key, _Value);
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_GameSettings_StorageReadResult : uint8
{
    Failed = 0,
    Succeeded,
    NotFound
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_GameSettings_StorageReadResult);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMESETTINGS_API FCk_GameSettings_StoreValue
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameSettings_StoreValue);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_GameSettings_Scope _Scope = ECk_GameSettings_Scope::Machine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _PlatformUserId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FName _Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FString _Value;

public:
    CK_PROPERTY_GET(_Scope);
    CK_PROPERTY_GET(_PlatformUserId);
    CK_PROPERTY_GET(_Key);
    CK_PROPERTY_GET(_Value);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameSettings_StoreValue, _Scope, _PlatformUserId, _Key, _Value);
};

// --------------------------------------------------------------------------------------------------------------------

/** Ordered storage seam. Failed reads leave OutValues empty. NotFound is authoritative absence.
 * StoreValues accepts the entire batch atomically, rejects duplicate scope/user/key identities,
 * and treats an empty batch as success. A successful flush means accepted values reached storage.
 * Implementations must report rejection; base methods never assume an unimplemented write succeeded. */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CKGAMESETTINGS_API UCk_GameSettings_StorageProvider_UE : public UObject
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameSettings_StorageProvider_UE);

public:
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ck|GameSettings|Storage")
    ECk_GameSettings_StorageReadResult
    Get_StoredValues(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId,
        TArray<FCk_GameSettings_StoredValue>& OutValues) const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ck|GameSettings|Storage")
    bool
    Request_StoreValue(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId,
        FName InKey,
        const FString& InValue);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ck|GameSettings|Storage")
    bool
    Request_StoreValues(
        const TArray<FCk_GameSettings_StoreValue>& InValues);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ck|GameSettings|Storage")
    bool
    Request_RemoveValue(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId,
        FName InKey);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ck|GameSettings|Storage")
    bool
    Request_Flush();
};

// --------------------------------------------------------------------------------------------------------------------
