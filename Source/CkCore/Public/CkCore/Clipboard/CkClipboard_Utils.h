#pragma once

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::clipboard
{
    CKCORE_API auto
    Copy(
        const FString& InText) -> void;
}

// --------------------------------------------------------------------------------------------------------------------

// While alive, ck::clipboard::Copy writes here instead of the OS clipboard; the innermost scope wins.
class CKCORE_API FCk_ClipboardCapture_Scope
{
public:
    FCk_ClipboardCapture_Scope();
    ~FCk_ClipboardCapture_Scope();

    FCk_ClipboardCapture_Scope(const FCk_ClipboardCapture_Scope&) = delete;
    auto operator=(const FCk_ClipboardCapture_Scope&) -> FCk_ClipboardCapture_Scope& = delete;

    auto
    Get_Text() const -> const FString&;

    auto
    Get_CopyCount() const -> int32;

private:
    friend auto ck::clipboard::Copy(const FString&) -> void;

    static FCk_ClipboardCapture_Scope* _Active;

    FCk_ClipboardCapture_Scope* _Previous = nullptr;
    FString _Text;
    int32 _CopyCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------
