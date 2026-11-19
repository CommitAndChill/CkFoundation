#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>

namespace ck::detail
{
    struct FAccessorTypeFormattingPass
    {
        std::map<std::string, std::string, std::less<>> Names;
        std::size_t CacheHits = 0;
        std::size_t ColdMisses = 0;
    };

    auto Get_AccessorTypeFormattingPass() -> std::optional<std::reference_wrapper<FAccessorTypeFormattingPass>>;
}
