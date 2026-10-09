#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"

/** An image cell from a regular atlas. A negative index renders an empty slot. */
CKSLATELAYOUT_API auto RegisterAtlasImageUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
