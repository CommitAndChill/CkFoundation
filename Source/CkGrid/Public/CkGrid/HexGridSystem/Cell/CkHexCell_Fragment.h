#pragma once

#include "CkHexCell_Fragment_Data.h"
#include "CkEcs/Tag/CkTag.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Fragment_Data.h"

namespace ck
{
    struct CKGRID_API FFragment_HexCell_Identity
    {
        CK_GENERATED_BODY(FFragment_HexCell_Identity);

    private:
        FCk_HexCoord _Coordinate;
        FCk_Handle _Grid;

    public:
        CK_PROPERTY_GET(_Coordinate);
        CK_PROPERTY_GET(_Grid);
        CK_DEFINE_CONSTRUCTORS(FFragment_HexCell_Identity, _Coordinate, _Grid);
    };

    struct CKGRID_API FFragment_HexCell_Tags
    {
        CK_GENERATED_BODY(FFragment_HexCell_Tags);

    private:
        FGameplayTagContainer _Tags;

    public:
        CK_PROPERTY_GET(_Tags);
        CK_PROPERTY_GET_NON_CONST(_Tags);
        CK_DEFINE_CONSTRUCTORS(FFragment_HexCell_Tags, _Tags);
    };

    CK_DEFINE_ECS_TAG_COUNTED(FTag_HexCell_Disabled);
    CK_DEFINE_ECS_TAG(FTag_HexCell_ManualBlock);
}
