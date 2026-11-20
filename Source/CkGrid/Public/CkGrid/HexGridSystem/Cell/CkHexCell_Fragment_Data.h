#pragma once

#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkEcs/Request/CkRequest_Data.h"
#include <GameplayTagContainer.h>

#include "CkHexCell_Fragment_Data.generated.h"

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKGRID_API FCk_Handle_HexCell : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_HexCell);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_HexCell);

USTRUCT(BlueprintType)
struct CKGRID_API FCk_Request_HexCell_SetBlocked : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_HexCell_SetBlocked);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_HexCell_SetBlocked);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _Blocked = true;

public:
    CK_PROPERTY(_Blocked);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_HexCell_SetBlocked, _Blocked);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_Request_HexCell_AddTag : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_HexCell_AddTag);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_HexCell_AddTag);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGameplayTag _Tag;

public:
    CK_PROPERTY(_Tag);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_HexCell_AddTag, _Tag);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_Request_HexCell_RemoveTag : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_HexCell_RemoveTag);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_HexCell_RemoveTag);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGameplayTag _Tag;

public:
    CK_PROPERTY(_Tag);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_HexCell_RemoveTag, _Tag);
};
