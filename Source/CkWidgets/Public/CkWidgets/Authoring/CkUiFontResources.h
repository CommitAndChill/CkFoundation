#pragma once

#include "CoreMinimal.h"

class FCkUiFontRegistry;
struct FCkUiFontFamilyRegistration;
struct FCkUiLoadResult;

/** Trusted host bridge: validates font files and freezes owned bytes before catalog publication. */
class CKWIDGETS_API FCkUiFontResources final
{
public:
    /** Game-thread-only, whole-family admission; failure preserves the registry. */
    static auto Register(FCkUiFontRegistry& InRegistry, FCkUiFontFamilyRegistration InRegistration) -> FCkUiLoadResult;
};
