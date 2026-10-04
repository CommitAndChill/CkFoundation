#pragma once

#include "CkSlateLayout/CkUiWidgetRegistry.h"

/** Registers the retained native action-card control. */
class CKSLATELAYOUT_API FCkUiActionCard final
{
public:
    static auto Register(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult;
};
