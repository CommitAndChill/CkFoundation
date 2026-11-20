#pragma once

#include "CkHexGridPlacement_Result_Data.h"
#include "CkEcs/Request/CkRequest_Data.h"
#include "CkHexGridPlacement_Fragment_Data.generated.h"

USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridPlacement_Spec
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridPlacement_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_Handle_HexGridObject _Object;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_HexCoord _Anchor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    ECk_HexRotation _Rotation = ECk_HexRotation::Turn0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    ECk_HexGridConnectivity _Connectivity = ECk_HexGridConnectivity::Ignore;

public:
    CK_PROPERTY(_Object);
    CK_PROPERTY(_Anchor);
    CK_PROPERTY(_Rotation);
    CK_PROPERTY(_Connectivity);
    CK_DEFINE_CONSTRUCTORS(FCk_HexGridPlacement_Spec, _Object, _Anchor, _Rotation, _Connectivity);
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FCk_Delegate_HexGridPlacement_OnResult, const FCk_HexGridPlacement_Result&, InResult);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FCk_Delegate_HexGrid_OccupantBindingFailed, FCk_Handle_HexGrid, InGrid, FGuid, InPlacementId, ECk_HexGridFailure, InFailure);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_HexGridPlacement_BindingConflict, FCk_Handle_HexGrid, InGrid, const FCk_HexGridPlacement_Result&, InResult);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_HexGridPlacement_ObjectPlaced, FCk_Handle_HexGrid, InGrid, const FCk_HexGridPlacement_View&, InPlacement);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_HexGridPlacement_ObjectMoved, FCk_Handle_HexGrid, InGrid, const FCk_HexGridPlacement_View&, InPlacement);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_HexGridPlacement_ObjectRemoved, FCk_Handle_HexGrid, InGrid, const FCk_HexGridPlacement_View&, InPlacement);

USTRUCT(BlueprintType)
struct CKGRID_API FCk_Request_HexGridPlacement_Place : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_HexGridPlacement_Place);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_HexGridPlacement_Place);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_HexGridPlacement_Spec _Spec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_Delegate_HexGridPlacement_OnResult _OnResult;

public:
    CK_PROPERTY(_Spec);
    CK_PROPERTY(_OnResult);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_HexGridPlacement_Place, _Spec);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_Request_HexGridPlacement_Move : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_HexGridPlacement_Move);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_HexGridPlacement_Move);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FGuid _PlacementId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_HexCoord _Anchor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    ECk_HexRotation _Rotation = ECk_HexRotation::Turn0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_Delegate_HexGridPlacement_OnResult _OnResult;

public:
    CK_PROPERTY(_PlacementId);
    CK_PROPERTY(_Anchor);
    CK_PROPERTY(_Rotation);
    CK_PROPERTY(_OnResult);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_HexGridPlacement_Move, _PlacementId, _Anchor, _Rotation);
};

USTRUCT(BlueprintType)
struct CKGRID_API FCk_Request_HexGridPlacement_Remove : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_HexGridPlacement_Remove);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_HexGridPlacement_Remove);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FGuid _PlacementId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_Delegate_HexGridPlacement_OnResult _OnResult;

public:
    CK_PROPERTY(_PlacementId);
    CK_PROPERTY(_OnResult);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_HexGridPlacement_Remove, _PlacementId);
};
