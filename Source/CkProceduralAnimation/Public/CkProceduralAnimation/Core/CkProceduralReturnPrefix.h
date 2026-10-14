#pragma once

#include "CoreMinimal.h"

namespace ck::procedural_action
{
    enum class EReturnPrefixProbe : uint8
    {
        Qualified,
        Rejected,
        Interrupted
    };

    // The bracket only guides the next local probe. Qualification must be checked at every
    // sampled fraction because IK and clearance need not be monotone over the whole path.
    template<typename FProbe>
    auto
        FindReturnPrefix(
            FProbe&& InProbe,
            int32& OutProbeCount,
            bool& OutInterrupted)
        -> TOptional<double>
    {
        OutProbeCount = 0;
        OutInterrupted = false;
        auto UnsafeFraction = 1.0;
        auto SafeFraction = 0.0;
        auto HasSafeFraction = false;
        while (OutProbeCount < 31)
        {
            const auto Fraction = UnsafeFraction * 0.5;
            const auto Result = InProbe(Fraction);
            ++OutProbeCount;
            if (Result == EReturnPrefixProbe::Interrupted)
            {
                OutInterrupted = true;
                return {};
            }
            if (Result == EReturnPrefixProbe::Qualified)
            {
                SafeFraction = Fraction;
                HasSafeFraction = true;
                break;
            }
            UnsafeFraction = Fraction;
        }
        if (NOT HasSafeFraction) { return {}; }
        for (auto Improvement = 0; Improvement < 4 && OutProbeCount < 31; ++Improvement)
        {
            const auto Fraction = (SafeFraction + UnsafeFraction) * 0.5;
            const auto Result = InProbe(Fraction);
            ++OutProbeCount;
            if (Result == EReturnPrefixProbe::Interrupted)
            {
                OutInterrupted = true;
                return {};
            }
            if (Result == EReturnPrefixProbe::Qualified)
            { SafeFraction = Fraction; }
            else { UnsafeFraction = Fraction; }
        }
        // Re-evaluate the exact retained prefix so trial-time candidate state cannot be staged.
        const auto FinalResult = InProbe(SafeFraction);
        ++OutProbeCount;
        if (FinalResult != EReturnPrefixProbe::Qualified)
        {
            OutInterrupted = true;
            return {};
        }
        return SafeFraction;
    }
}
