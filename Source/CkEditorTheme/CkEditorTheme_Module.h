#pragma once

#include "CkEditorTheme_Controller.h"

#include "CkCore/Macros/CkMacros.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkEditorThemeModule : public IModuleInterface
{
public:
    CK_GENERATED_BODY(FCkEditorThemeModule);

public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

private:
    FCk_EditorTheme_Controller _Controller;
};

// --------------------------------------------------------------------------------------------------------------------
