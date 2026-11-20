#pragma once

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment_Data.h"
#include "CkEcs/Request/CkRequest_Data.h"

#include "CkHexGridBlocker_Fragment_Data.generated.h"

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKGRID_API FCk_Handle_HexGridBlocker : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_HexGridBlocker);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_HexGridBlocker);

UENUM(BlueprintType)
enum class ECk_HexGridBlocker_Failure : uint8
{
    None, InvalidGrid, InvalidOwner, AlreadyComposed, CompositionFailed, EmptyCells,
    DuplicateCell, OutsideDomain, DuplicateName, NotAuthority, Unavailable
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_HexGridBlocker_Failure);

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridBlocker_Spec
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridBlocker_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Handle_HexGrid _Grid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TArray<FCk_HexCoord> _Cells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGameplayTag _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _InitiallyActive = true;

public:
    CK_PROPERTY(_Grid);
    CK_PROPERTY(_Cells);
    CK_PROPERTY(_Name);
    CK_PROPERTY(_InitiallyActive);
    CK_DEFINE_CONSTRUCTORS(FCk_HexGridBlocker_Spec, _Grid, _Cells, _Name, _InitiallyActive);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_Request_HexGridBlocker_SetActive : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_HexGridBlocker_SetActive);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_HexGridBlocker_SetActive);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _Active = true;

public:
    CK_PROPERTY(_Active);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_HexGridBlocker_SetActive, _Active);
};
