#include "CkAngelScript_TypeValidation.h"

// --------------------------------------------------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_CK

#include <AngelscriptManager.h>
#include <AngelscriptSettings.h>

#include <UObject/Class.h>
#include <UObject/Package.h>

auto
    ck::angelscript::
    IsEditorOnlyStructHiddenFromAngelScript(
        const UStruct* InStruct)
    -> bool
{
#if WITH_EDITOR
    if (FAngelscriptManager::bUseEditorScripts || InStruct == nullptr)
    { return false; }

    const auto* Package = InStruct->GetOutermost();
    if (Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_UncookedOnly))
    { return true; }

    const auto* Settings = FAngelscriptManager::ConfigSettings;
    return Settings != nullptr && Settings->AdditionalEditorOnlyScriptPackageNames.Contains(Package->GetFName());
#else
    return false;
#endif
}

#endif
