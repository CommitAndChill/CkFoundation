#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"

/** Registers the shared retained key-hint control for authored UI documents. */
CKSLATELAYOUT_API auto RegisterKeyHintUiControl(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
