#include "CkClipboard_Utils.h"

#include "CkCore/Ensure/CkEnsure.h"

#include <HAL/PlatformApplicationMisc.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck::clipboard
{
    auto
    Copy(
        const FString& InText) -> void
    {
#if WITH_DEV_AUTOMATION_TESTS
        CK_ENSURE_IF_NOT(IsInGameThread(), TEXT("ck::clipboard::Copy called off the game thread; bypassing capture"))
        {
            FPlatformApplicationMisc::ClipboardCopy(*InText);
            return;
        }

        if (auto* Capture = FCk_ClipboardCapture_Scope::_Active)
        {
            Capture->_Text = InText;
            ++Capture->_CopyCount;
            return;
        }
#endif

        FPlatformApplicationMisc::ClipboardCopy(*InText);
    }
}

// --------------------------------------------------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

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
    CK_ENSURE_IF_NOT(_Active == this, TEXT("FCk_ClipboardCapture_Scope destroyed out of order; unlinking it from the capture chain"))
    {
        for (auto* Scope = _Active; Scope != nullptr; Scope = Scope->_Previous)
        {
            if (Scope->_Previous == this)
            {
                Scope->_Previous = _Previous;
                break;
            }
        }
        return;
    }

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

#endif

// --------------------------------------------------------------------------------------------------------------------
