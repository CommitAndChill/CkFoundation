#include "CkMacros_AngelScript.h"
#include "CkAccessorTypeFormattingPass.h"
#include "CkCore/Format/CkAccessorTypeFormatting.h"

// --------------------------------------------------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_CK

#include "AngelscriptCodeModule.h"
#include "AngelscriptManager.h"
#include "AngelscriptInclude.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "cleantype/cleantype_configuration.hpp"
#include <tuple>
#include <unordered_map>

namespace
{
    auto TypeNameSettings(const cleantype::CleanConfiguration& Config)
    {
        return std::tie(Config.suppress_extra_namespaces_, Config.suppress_extract_struct_class_,
            Config.suppress_custom_, Config.undesirable_type_nodes_, Config.replacements_after_undesirable_node_extractions,
            Config.force_east_const_, Config.indent_depth_limit);
    }

    struct FPropertyTypeNames
    {
        ck::detail::FAccessorTypeFormattingPass Spellings;
        cleantype::CleanConfiguration Settings;
        std::unordered_map<FCkAngelScriptPropertyFunctionRegistration::FTypeNameFunction, FString> Names;
        bool bVerify = FParse::Param(FCommandLine::Get(), TEXT("CkVerifyAccessorTypeNames"));
    };

    thread_local FPropertyTypeNames* ActivePropertyTypeNames = nullptr;
}

auto ck::detail::Get_AccessorTypeFormattingPass() -> std::optional<std::reference_wrapper<FAccessorTypeFormattingPass>>
{
    if (!ActivePropertyTypeNames)
    { return std::nullopt; }
    return std::ref(ActivePropertyTypeNames->Spellings);
}

auto FCkAngelScriptPropertyFunctionRegistration::ResolveTypeName(
    FTypeNameFunction InFormat, const cleantype::CleanConfiguration& InConfig) -> FString
{
    const ck::detail::FAccessorTypeFormattingScope FormattingScope;
    FPropertyTypeNames* Cache = ActivePropertyTypeNames;
    if (Cache)
    {
        if (TypeNameSettings(Cache->Settings) != TypeNameSettings(InConfig))
        {
            Cache->Names.clear();
            Cache->Settings = InConfig;
        }
        if (const auto Existing = Cache->Names.find(InFormat); Existing != Cache->Names.end())
        {
            if (Cache->bVerify)
            {
                checkf(Existing->second.Equals(InFormat(), ESearchCase::CaseSensitive), TEXT("Cached accessor type spelling changed"));
            }
            return Existing->second;
        }
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(CkAccessor_FormatTypeName);
    FString Name = InFormat();
    if (Cache)
    {
        Cache->Names.emplace(InFormat, Name);
    }
    return Name;
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
    TRACE_CPUPROFILER_EVENT_SCOPE(CkPreCompile_RegisterCtors);
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
    if (Get_AllPropertyFunctions().IsEmpty())
    { return; }
    TRACE_CPUPROFILER_EVENT_SCOPE(CkPreCompile_RegisterProperties);
    const double Start = FPlatformTime::Seconds();
    const auto Before = ck::detail::Get_AccessorTypeFormattingStats();
    FPropertyTypeNames Names;
    const bool bUncached = FParse::Param(FCommandLine::Get(), TEXT("CkUncachedAccessorTypeNames"));
    TGuardValue<FPropertyTypeNames*> Scope(ActivePropertyTypeNames, bUncached ? nullptr : &Names);
    for (const auto& RegFunc : Get_AllPropertyFunctions())
    {
        RegFunc();
    }
    UE_LOG(LogTemp, Display, TEXT("CK accessor spelling cache: hits %llu, cold misses %llu, entries %llu"),
        static_cast<uint64>(Names.Spellings.CacheHits), static_cast<uint64>(Names.Spellings.ColdMisses),
        static_cast<uint64>(Names.Spellings.Names.size()));
    const auto Stats = ck::detail::Get_AccessorTypeFormattingStats();
    UE_LOG(LogTemp, Display, TEXT("CK accessor registration: %.3f ms (accepted %llu, fallback %llu, verified %llu)"),
        (FPlatformTime::Seconds() - Start) * 1000.0, static_cast<uint64>(Stats.Accepted - Before.Accepted),
        static_cast<uint64>(Stats.Fallbacks - Before.Fallbacks), static_cast<uint64>(Stats.Verified - Before.Verified));
    if (FParse::Param(FCommandLine::Get(), TEXT("CkDumpAccessorSignatures")))
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(CkAccessor_DumpSignatures);
        const auto Engine = FAngelscriptManager::Get().GetScriptEngine();
        if (Engine == nullptr)
        { return; }
        int32 Count = 0;
        for (asUINT TypeIndex = 0; TypeIndex < Engine->GetObjectTypeCount(); ++TypeIndex)
        {
            const auto& Type = *Engine->GetObjectTypeByIndex(TypeIndex);
            const FString Owner = UTF8_TO_TCHAR(Engine->GetTypeDeclaration(Type.GetTypeId(), true));
            for (asUINT Index = 0; Index < Type.GetMethodCount(); ++Index)
            {
                const auto& Method = *Type.GetMethodByIndex(Index);
                if (FCStringAnsi::Strncmp(Method.GetName(), "Get_", 4) == 0 ||
                    FCStringAnsi::Strncmp(Method.GetName(), "Set_", 4) == 0)
                {
                    UE_LOG(LogTemp, Display, TEXT("[CKAccessorSignature] %s\t%s"),
                        *Owner, UTF8_TO_TCHAR(Method.GetDeclaration(true, true, true, true)));
                    ++Count;
                }
            }
        }
        UE_LOG(LogTemp, Display, TEXT("[CKAccessorSignatureCount] %d"), Count);
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
