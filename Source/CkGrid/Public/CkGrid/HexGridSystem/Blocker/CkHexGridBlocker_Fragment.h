#pragma once

#include "CkHexGridBlocker_Fragment_Data.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment_Data.h"
#include "CkRecord/Record/CkRecord_Fragment.h"

namespace ck
{
    CK_DEFINE_RECORD_OF_ENTITIES_TRANSIENT(FFragment_RecordOf_HexGridBlockers, FCk_Handle_HexGridBlocker);
    using FHexBlockerRecord = TUtils_RecordOfEntities<FFragment_RecordOf_HexGridBlockers>;

    namespace hex_grid
    {
        auto InstallBlocker(FCk_Handle Entity, FCk_Handle Board, FGuid Id, FGameplayTag Name,
            TArray<FCk_HexCoord> Coordinates, TArray<FCk_Handle_HexCell> Cells, bool OwnsEntity)
            -> FCk_Handle_HexGridBlocker;
    }

    struct FFragment_HexGridBlocker
    {
        FCk_Handle _Board;
        FGameplayTag _Name;
        TArray<FCk_HexCoord> _Coordinates;
        TArray<FCk_Handle_HexCell> _Cells;
        bool _Active = false;
        FGuid _BlockerId;
        bool _OwnsEntity = false;
    };
}
