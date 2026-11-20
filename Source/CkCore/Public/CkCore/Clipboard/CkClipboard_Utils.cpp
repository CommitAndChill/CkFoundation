#include "CkClipboard_Utils.h"

#include <HAL/PlatformApplicationMisc.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck::clipboard
{
    auto
    Copy(
        const FString& InText) -> void
    {
        if (auto* Capture = FCk_ClipboardCapture_Scope::_Active)
        {
            Capture->_Text = InText;
            ++Capture->_CopyCount;
            return;
        }

        FPlatformApplicationMisc::ClipboardCopy(*InText);
    }
}

// --------------------------------------------------------------------------------------------------------------------

FCk_ClipboardCapture_Scope* FCk_ClipboardCapture_Scope::_Active = nullptr;

FCk_ClipboardCapture_Scope::
    FCk_ClipboardCapture_Scope()
    : _Previous(_Active)
{
    _Active = this;
}

FCk_ClipboardCapture_Scope::
    ~FCk_ClipboardCapture_Scope()
{
    _Active = _Previous;
}

auto
    FCk_ClipboardCapture_Scope::
    Get_Text() const
    -> const FString&
{
    return _Text;
}

auto
    FCk_ClipboardCapture_Scope::
    Get_CopyCount() const
    -> int32
{
    return _CopyCount;
}

// --------------------------------------------------------------------------------------------------------------------
