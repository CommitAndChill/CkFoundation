#include "CkCore/Clipboard/CkClipboard_Utils.h"

#include <Misc/AutomationTest.h>

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ClipboardCapture,
    "Ck.CkCore.Clipboard.CaptureScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool
    FCkTest_ClipboardCapture::
    RunTest(
        const FString& Parameters)
{
    FCk_ClipboardCapture_Scope Outer;
    TestEqual(TEXT("Fresh scope has no copies"), Outer.Get_CopyCount(), 0);

    ck::clipboard::Copy(TEXT("outer"));
    TestEqual(TEXT("Outer captures the copy"), Outer.Get_Text(), FString{TEXT("outer")});

    {
        FCk_ClipboardCapture_Scope Inner;
        ck::clipboard::Copy(TEXT("inner"));
        TestEqual(TEXT("Innermost scope captures"), Inner.Get_Text(), FString{TEXT("inner")});
        TestEqual(TEXT("Outer is untouched while shadowed"), Outer.Get_Text(), FString{TEXT("outer")});
    }

    ck::clipboard::Copy(TEXT("again"));
    TestEqual(TEXT("Outer resumes after inner ends"), Outer.Get_Text(), FString{TEXT("again")});
    TestEqual(TEXT("Outer counts only its own copies"), Outer.Get_CopyCount(), 2);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
