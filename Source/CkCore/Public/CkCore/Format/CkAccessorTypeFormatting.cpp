#include "CkAccessorTypeFormatting.h"
#if WITH_ANGELSCRIPT_CK
#include "CkAccessorTypeFormattingPass.h"

#include "CkCore/Macros/CkMacros.h"
#include "CoreMinimal.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include <string_view>
#include <tuple>

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    thread_local bool AccessorTypeFormattingActive = false;

    thread_local ck::detail::FAccessorTypeFormattingStats AccessorTypeFormattingStats;

    auto ConfigurationSettings(const cleantype::CleanConfiguration& InConfig)
    {
        return std::tie(InConfig.suppress_extra_namespaces_, InConfig.suppress_extract_struct_class_,
            InConfig.suppress_custom_, InConfig.undesirable_type_nodes_,
            InConfig.replacements_after_undesirable_node_extractions,
            InConfig.force_east_const_, InConfig.indent_depth_limit);
    }

    auto IsIdentifierStart(char InCharacter) -> bool
    {
        return (InCharacter >= 'a' && InCharacter <= 'z') ||
            (InCharacter >= 'A' && InCharacter <= 'Z') || InCharacter == '_';
    }

    auto IsSimpleIdentifier(std::string_view InName) -> bool
    {
        if (InName.empty() || NOT IsIdentifierStart(InName.front()))
        { return false; }

        for (const auto Character : InName)
        {
            if (NOT IsIdentifierStart(Character) && NOT (Character >= '0' && Character <= '9'))
            { return false; }
        }
        return true;
    }

    auto TryGetSimpleTypeName(cleantype::stringliteral InCompilerType, std::string_view& OutName) -> bool
    {
        if (InCompilerType.ptr == nullptr || InCompilerType.length == 0)
        { return false; }

        auto Name = std::string_view{InCompilerType.ptr, InCompilerType.length};
        constexpr auto MsvcHolder = std::string_view{"struct cleantype::internal::TupleTypeHolder<"};
        constexpr auto OtherHolder = std::string_view{"cleantype::internal::TupleTypeHolder<"};
        if (Name.starts_with(MsvcHolder))
        { Name.remove_prefix(MsvcHolder.size()); }
        else if (Name.starts_with(OtherHolder))
        { Name.remove_prefix(OtherHolder.size()); }
        else
        { return false; }

        if (Name.empty() || Name.back() != '>')
        { return false; }
        Name.remove_suffix(1);

        if (Name.starts_with("struct "))
        { Name.remove_prefix(7); }
        else if (Name.starts_with("class "))
        { Name.remove_prefix(6); }

        const auto Identifier = Name.starts_with("enum ") ? Name.substr(5) : Name;
        // cleantype's default suppressions strip the compiler's __ptr64 token.
        if (NOT IsSimpleIdentifier(Identifier) || Identifier.starts_with("__ptr64"))
        { return false; }

        OutName = Name;
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto ck::detail::Get_AccessorTypeFormattingStats() -> FAccessorTypeFormattingStats
{
    return AccessorTypeFormattingStats;
}

ck::detail::FAccessorTypeFormattingScope::FAccessorTypeFormattingScope()
    : _WasActive(AccessorTypeFormattingActive)
{
    AccessorTypeFormattingActive = true;
}

ck::detail::FAccessorTypeFormattingScope::~FAccessorTypeFormattingScope()
{
    AccessorTypeFormattingActive = _WasActive;
}

// --------------------------------------------------------------------------------------------------------------------

auto ck::detail::FormatAccessorTypeName(
    cleantype::stringliteral InCompilerType,
    std::string (*InOriginal)(),
    const cleantype::CleanConfiguration& InConfig) -> std::string
{
    if (NOT AccessorTypeFormattingActive)
    { return InOriginal(); }

    ++AccessorTypeFormattingStats.Attempts;
    static const auto Legacy = FParse::Param(FCommandLine::Get(), TEXT("CkLegacyAccessorFormatting"));
    static const auto Uncached = FParse::Param(FCommandLine::Get(), TEXT("CkUncachedAccessorTypeNames"));
    static const auto Verify = FParse::Param(FCommandLine::Get(), TEXT("CkVerifyAccessorTypeNames"));
    static const cleantype::CleanConfiguration DefaultConfiguration;
    const auto Pass = Get_AccessorTypeFormattingPass();
    if (Legacy || Uncached || !Pass ||
        ConfigurationSettings(InConfig) != ConfigurationSettings(DefaultConfiguration))
    {
        ++AccessorTypeFormattingStats.Fallbacks;
        return InOriginal();
    }

    std::string_view Name;
    std::string Result;
    if (TryGetSimpleTypeName(InCompilerType, Name))
    {
        Result.assign(Name);
    }
    else if (InCompilerType.ptr != nullptr && InCompilerType.length != 0)
    {
        const auto Key = std::string_view{InCompilerType.ptr, InCompilerType.length};
        auto& Cache = Pass->get();
        const auto Existing = Cache.Names.find(Key);
        if (Existing == Cache.Names.end())
        {
            ++Cache.ColdMisses;
            ++AccessorTypeFormattingStats.Fallbacks;
            auto Original = InOriginal();
            Cache.Names.emplace(std::string{Key}, Original);
            return Original;
        }
        ++Cache.CacheHits;
        Result = Existing->second;
    }
    else
    {
        ++AccessorTypeFormattingStats.Fallbacks;
        return InOriginal();
    }

    ++AccessorTypeFormattingStats.Accepted;
    if (Verify)
    {
        const auto Original = InOriginal();
        ++AccessorTypeFormattingStats.Verified;
        if (Result != Original)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Accessor type formatting oracle mismatch: candidate [%hs], original [%hs]"),
                Result.c_str(), Original.c_str());
        }
    }
    return Result;
}

#endif
