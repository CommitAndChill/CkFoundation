#pragma once

#include "CoreMinimal.h"

namespace ck::usf_editor
{
    class CKUSFEDITOR_API FShaderReload
    {
    public:
        FShaderReload();
        ~FShaderReload();

        auto Startup() -> void;
        auto Shutdown() -> void;

    private:
        class FState;
        TUniquePtr<FState> _State;
    };
}
