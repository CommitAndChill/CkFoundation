#pragma once

#include "CoreMinimal.h"

namespace ck::usf_editor
{
    class CKUSFEDITOR_API FShaderReload
    {
    public:
        FShaderReload();
        ~FShaderReload();

        // Reload requires the engine's complete shader dependency ownership API.
        static auto Get_IsSupported() -> bool;

        auto Startup() -> void;
        auto Shutdown() -> void;

    private:
        class FState;
        TUniquePtr<FState> _State;
    };
}
