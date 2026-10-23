#include "CkMacros_AngelScript.h"

// --------------------------------------------------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_CK

#include "AngelscriptCodeModule.h"
#include "AngelscriptSettings.h"

auto
    ck::angelscript::
    Get_IsEditorOnlyStructHiddenFromScript(
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

auto
    FCkAngelScriptCtorFunctionRegistration::
    EnsureCallbackRegistered()
    -> void
{
    static auto CallbackRegistered = false;
    if (CallbackRegistered)
    { return; }

    _PreCompileDelegateHandle = FAngelscriptCodeModule::GetPreCompile().AddStatic([]
    {
        RegisterAllCtorFunctions();
        Get_AllCtorFunctions().Reset();
    });
    CallbackRegistered = true;
}

auto
    FCkAngelScriptCtorFunctionRegistration::
    RegisterAllCtorFunctions()
    -> void
{
    for (const auto& RegFunc : Get_AllCtorFunctions())
    {
        RegFunc();
    }
}

auto
    FCkAngelScriptCtorFunctionRegistration::
    Get_AllCtorFunctions()
    -> TArray<FCtorFunction>&
{
    static TArray<FCtorFunction> CtorFunctions;
    return CtorFunctions;
}

auto
    FCkAngelScriptCtorFunctionRegistration::
    RegisterCtorFunction(
        const FCtorFunction& InCtorFunction)
    -> void
{
    Get_AllCtorFunctions().Add(InCtorFunction);

    // If AngelScript is already initialized, register immediately
    if (FAngelscriptManager::IsInitialized())
    {
        InCtorFunction();
    }
    else
    {
        // Otherwise, ensure we have a callback set up
        EnsureCallbackRegistered();
    }
}

auto
    FCkAngelScriptPropertyFunctionRegistration::
    EnsureCallbackRegistered()
    -> void
{
    static auto CallbackRegistered = false;
    if (CallbackRegistered)
    { return; }

    _PreCompileDelegateHandle = FAngelscriptCodeModule::GetPreCompile().AddStatic([]
    {
        RegisterAllPropertyFunctions();
        Get_AllPropertyFunctions().Reset();
    });
    CallbackRegistered = true;
}

auto
    FCkAngelScriptPropertyFunctionRegistration::
    RegisterAllPropertyFunctions()
    -> void
{
    for (const auto& RegFunc : Get_AllPropertyFunctions())
    {
        RegFunc();
    }
}

auto
    FCkAngelScriptPropertyFunctionRegistration::
    Get_AllPropertyFunctions()
    -> TArray<FPropertyFunction>&
{
    static TArray<FPropertyFunction> PropertyFunctions;
    return PropertyFunctions;
}

auto
    FCkAngelScriptPropertyFunctionRegistration::
    RegisterPropertyFunction(
        const FPropertyFunction& InPropertyFunction)
    -> void
{
    Get_AllPropertyFunctions().Add(InPropertyFunction);

    // If AngelScript is already initialized, register immediately
    if (FAngelscriptManager::IsInitialized())
    {
        InPropertyFunction();
    }
    else
    {
        // Otherwise, ensure we have a callback set up
        EnsureCallbackRegistered();
    }
}

#endif

// --------------------------------------------------------------------------------------------------------------------
