#pragma once

#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Fragment_Data.h"
#include <Engine/DataAsset.h>

#include "CkHexGridObject_Spec.generated.h"

UCLASS(BlueprintType)
class CKGRID_API UCk_HexGridObject_AuthoringSpec : public UDataAsset
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_HexGridObject_AuthoringSpec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid Object", meta = (AllowPrivateAccess = true))
    FCk_HexGridObject_Spec _ObjectSpec;

public:
    CK_PROPERTY(_ObjectSpec);
};
