#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkEcs/Request/CkRequest_Data.h"

#include <GameplayTagContainer.h>

#include "CkEntityTag_Fragment_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_EntityTagUpdate : uint8
{
    Added,
    Removed,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_EntityTagUpdate);

// --------------------------------------------------------------------------------------------------------------------

// Save-only transport for both counted views. Version zero is the legacy names-only payload.
USTRUCT()
struct CKENTITYTAG_API FCk_SaveData_EntityTags
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_SaveData_EntityTags);

private:
    UPROPERTY()
    TArray<FName> _TagNames;

    // Reference counts, parallel to _TagNames.
    UPROPERTY()
    TArray<int32> _Counts;

    UPROPERTY()
    int32 _Version = 0;

    UPROPERTY()
    TArray<FGameplayTag> _GameplayTags;

    UPROPERTY()
    TArray<int32> _GameplayCounts;

public:
    CK_PROPERTY(_TagNames);
    CK_PROPERTY(_Counts);
    CK_PROPERTY(_Version);
    CK_PROPERTY(_GameplayTags);
    CK_PROPERTY(_GameplayCounts);
};

//--------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKENTITYTAG_API FCk_EntityTag_Spec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_EntityTag_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories="EntityTag"))
    FName _Tag;

public:
    CK_PROPERTY_GET(_Tag);

    CK_DEFINE_CONSTRUCTORS(FCk_EntityTag_Spec, _Tag);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKENTITYTAG_API FCk_Request_EntityTag_Add : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_EntityTag_Add);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_EntityTag_Add);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories="EntityTag"))
    FName _Tag;

public:
    CK_PROPERTY_GET(_Tag);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_EntityTag_Add, _Tag);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKENTITYTAG_API FCk_Request_EntityTag_TryRemove : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_EntityTag_TryRemove);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_EntityTag_TryRemove);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories="EntityTag"))
    FName _Tag;

public:
    CK_PROPERTY_GET(_Tag);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_EntityTag_TryRemove, _Tag);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKENTITYTAG_API FCk_Request_EntityTag_AddGameplayTag : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_EntityTag_AddGameplayTag);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_EntityTag_AddGameplayTag);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories="EntityTag"))
    FGameplayTag _Tag;

public:
    CK_PROPERTY_GET(_Tag);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_EntityTag_AddGameplayTag, _Tag);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKENTITYTAG_API FCk_Request_EntityTag_TryRemoveGameplayTag : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_EntityTag_TryRemoveGameplayTag);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_EntityTag_TryRemoveGameplayTag);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories="EntityTag"))
    FGameplayTag _Tag;

public:
    CK_PROPERTY_GET(_Tag);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_EntityTag_TryRemoveGameplayTag, _Tag);
};

// --------------------------------------------------------------------------------------------------------------------

// Composite restore-set request carrying the complete saved counted views: drain replaces both views atomically,
// diffing against whatever is live THEN. Enqueued only by the persistence handler's HydrationApply,
// never from Blueprint/AngelScript. See CkEntityTag/CLAUDE.md § "Save/load restore".
USTRUCT()
struct CKENTITYTAG_API FCk_Request_EntityTag_RestoreSet : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_EntityTag_RestoreSet);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_EntityTag_RestoreSet);

private:
    UPROPERTY()
    TArray<FName> _TagNames;

    // Reference counts, parallel to _TagNames.
    UPROPERTY()
    TArray<int32> _Counts;

    UPROPERTY()
    int32 _Version = 0;

    UPROPERTY()
    TArray<FGameplayTag> _GameplayTags;

    UPROPERTY()
    TArray<int32> _GameplayCounts;

public:
    CK_PROPERTY_GET(_TagNames);
    CK_PROPERTY_GET(_Counts);
    CK_PROPERTY(_Version);
    CK_PROPERTY(_GameplayTags);
    CK_PROPERTY(_GameplayCounts);

    CK_DEFINE_CONSTRUCTORS(FCk_Request_EntityTag_RestoreSet, _TagNames, _Counts);

    auto Get_IsValid() const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DYNAMIC_DELEGATE_ThreeParams(
    FCk_Delegate_EntityTag_OnTagUpdated,
    FCk_Handle, InOwner,
    FName, InTagName,
    ECk_EntityTagUpdate, InUpdateType);

DECLARE_DYNAMIC_DELEGATE_ThreeParams(
    FCk_Delegate_EntityTag_OnGameplayTagUpdated,
    FCk_Handle, InOwner,
    FGameplayTag, InTag,
    ECk_EntityTagUpdate, InUpdateType);

DECLARE_DYNAMIC_DELEGATE_ThreeParams(
    FCk_Delegate_EntityTag_OnTagUpdated_AnyEntity,
    FName, InTag,
    FCk_Handle, InEntity,
    ECk_EntityTagUpdate, InUpdateType);

// --------------------------------------------------------------------------------------------------------------------
