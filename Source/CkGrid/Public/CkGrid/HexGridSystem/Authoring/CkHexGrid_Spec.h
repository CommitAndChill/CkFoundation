#pragma once

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridPersistence_Fragment_Data.h"

#include <Engine/DataAsset.h>

#include "CkHexGrid_Spec.generated.h"

UCLASS(BlueprintType)
class CKGRID_API UCk_HexGrid_AuthoringSpec : public UDataAsset
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_HexGrid_AuthoringSpec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Grid", meta = (AllowPrivateAccess = true))
    FCk_HexGrid_Spec _Spec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Compatibility", meta = (AllowPrivateAccess = true, ClampMin = "1"))
    int32 _RecipeRevision = 1;

public:
    CK_PROPERTY(_Spec);
    CK_PROPERTY(_RecipeRevision);

    UFUNCTION(BlueprintCallable, Category = "Ck|HexGrid")
    bool TryResolve(FCk_HexGrid_Spec& OutSpec, ECk_HexBoardFailure& OutFailure) const;

    auto TryResolveWithIdentity(FCk_HexGrid_Spec& OutSpec, FCk_HexGridRecipeIdentity& OutIdentity,
        ECk_HexBoardFailure& OutFailure) const -> bool;

#if WITH_EDITOR
    // The editor stages a full recipe before touching the serialized asset.
    auto EditorOnly_TryReplaceSpec(const FCk_HexGrid_Spec& InSpec, ECk_HexBoardFailure& OutFailure) -> bool;
#endif
};

namespace ck::hex_grid
{
    // Canonical v1 bytes for a validated authored recipe. Asset path and revision are separate compatibility fields.
    CKGRID_API auto TryEncodeRecipe(const FCk_HexGrid_Spec& InSpec, TArray<uint8>& OutBytes,
        ECk_HexBoardFailure& OutFailure) -> bool;
}
