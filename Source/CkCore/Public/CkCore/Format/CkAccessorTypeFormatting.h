#pragma once

#include "cleantype/details/cleantype_clean.hpp"

namespace ck::detail
{
    struct FAccessorTypeFormattingStats
    {
        std::size_t Attempts = 0;
        std::size_t Accepted = 0;
        std::size_t Verified = 0;
        std::size_t Fallbacks = 0;
    };

    class CKCORE_API FAccessorTypeFormattingScope
    {
    public:
        FAccessorTypeFormattingScope();
        ~FAccessorTypeFormattingScope();

        FAccessorTypeFormattingScope(const FAccessorTypeFormattingScope&) = delete;
        auto operator=(const FAccessorTypeFormattingScope&) -> FAccessorTypeFormattingScope& = delete;

    private:
        bool _WasActive;
    };

    CKCORE_API auto Get_AccessorTypeFormattingStats() -> FAccessorTypeFormattingStats;

    CKCORE_API auto FormatAccessorTypeName(
        cleantype::stringliteral InCompilerType,
        std::string (*InOriginal)(),
        const cleantype::CleanConfiguration& InConfig) -> std::string;

    template <typename T>
    auto Get_CleanAccessorTypeName() -> std::string
    {
        return FormatAccessorTypeName(cleantype::internal::impl_typeid<T>(),
            &cleantype::clean<T>, cleantype::CleanConfiguration::GlobalConfig());
    }
}
