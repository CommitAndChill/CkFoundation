#pragma once

#include "CkWidgets/Authoring/CkAuthoredCollection.h"
#include "CkSlateLayout/CkUiCollection.h"

namespace ck_ui_authored_collection
{
    auto TryCreate(const FCk_UI_AuthoredCollection& InDeclaration, TSharedPtr<FCkUiCollection>& OutCollection,
        TArray<FString>& OutErrors) -> bool;
    auto TryConvertRecords(const TArray<FCk_UI_AuthoredRecord>& InRecords, TArray<FCkUiRecordData>& OutRecords,
        TArray<FString>& OutErrors) -> bool;
    auto TryReadRecords(const FCkUiCollection& InCollection, TArray<FCk_UI_AuthoredRecord>& OutRecords) -> bool;
}
