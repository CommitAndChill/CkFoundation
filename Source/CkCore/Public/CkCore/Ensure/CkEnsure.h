#pragma once

#include "CkCore/Build/CkBuild_Macros.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Format/CkFormat.h"

#include <HAL/PlatformMisc.h> // for PLATFORM_BREAK() + FPlatformMisc::IsDebuggerPresent()

struct FCk_Utils_EditorOnly_PushNewEditorMessage_Params;

// --------------------------------------------------------------------------------------------------------------------
namespace ck::ensure
{
    CKCORE_API auto Ensure_Impl(
        const FString& InMessage,
        const FString& InExpressionText,
        const FName& InFile,
        int32 InLine,
        bool& OutBreakInCode,
        bool& OutBreakInScript) -> void;

    // Returns true when the CALLER must trigger the debug break itself; a script break, if requested,
    // is handled internally before returning.
    CKCORE_API auto Do_HandleFail(
        const FString& InMessage,
        const FString& InExpressionText,
        const FName& InFile,
        int32 InLine) -> bool;

    CKCORE_API auto Do_BreakInScript() -> void;
    CKCORE_API auto Get_IsEnsureFromScript() -> bool;
    CKCORE_API auto Do_Push_EnsureIsFromScript() -> void;
    CKCORE_API auto Do_Pop_EnsureIsFromScript() -> void;

#if WITH_DEV_AUTOMATION_TESTS
    // True for the ck::Ensure wrappers the site-identity walk steps past. Exposed only to pin the list.
    CKCORE_API auto Get_IsEnsurePlumbingFunction_ForTesting(
        const FString& InNamespace,
        const FString& InFunctionName) -> bool;

    // The site every log record carries: "[File:<path>] [Line: <n>]" natively, "[Script:<identity>]" from script.
    CKCORE_API auto Get_EnsureSiteText_ForTesting(
        const FName& InFile,
        int32 InLine,
        const FString& InScriptSiteIdentity) -> FString;

    CKCORE_API auto Request_TrimEngineBoilerplateFrames_ForTesting(
        const FString& InStackTrace) -> FString;

    CKCORE_API auto Ensure_Impl_ForTesting(
        const FString& InMessage,
        const FString& InExpressionText,
        const FName& InFile,
        int32 InLine,
        bool& OutBreakInCode,
        bool& OutBreakInScript,
        TFunctionRef<void(const FString&)> InWorkerReportEmitter,
        TFunctionRef<void(const FCk_Utils_EditorOnly_PushNewEditorMessage_Params&)> InEditorMessageEmitter) -> void;
#endif
}

// PLATFORM_BREAK expands textually here rather than inside a lambda or an ensureAlwaysMsgf wrapper,
// so the debugger lands directly in the user's function.
#define CK_ENSURE(InExpression, InString, ...)                                                                                             \
(LIKELY(static_cast<bool>(InExpression))                                                                                                   \
    ? true                                                                                                                                 \
    : (ck::ensure::Do_HandleFail(ck::Format_UE(InString, ##__VA_ARGS__), TEXT(#InExpression), __FILE__, __LINE__)                          \
        && FPlatformMisc::IsDebuggerPresent()                                                                                              \
            ? (PLATFORM_BREAK(), false)                                                                                                    \
            : false))

// CK_ENSURE_IF_NOT owns the failure branch. Keep its predicate total and side-effect-safe, with rejection,
// rollback, failure completion, and termination in the body; never repeat that guard in a separate if.
// Shipping intentionally omits predicate evaluation and the entire body for performance. Validation-only work
// must stay inside the predicate or under the same compilation policy. Unfixed invariant violations may crash
// when checks are omitted. Retaining Shipping checks requires an explicit delivery exception and does not change
// this call-site contract; current retained flags are not authorization to preserve recovery unconditionally.
#if CK_DISABLE_ENSURE_CHECKS
#define CK_ENSURE_IF_NOT(InExpression, InFormat, ...)\
if constexpr(false)
#elif CK_DISABLE_ENSURE_DEBUGGING
#define CK_ENSURE_IF_NOT(InExpression, InFormat, ...)\
if (NOT (InExpression))
#else
#define CK_ENSURE_IF_NOT(InExpression, InFormat, ...)\
if (NOT CK_ENSURE(InExpression, InFormat, ##__VA_ARGS__))
#endif

#define CK_ENSURE_VALID_UNREAL_WORLD_IF_NOT(InWorldContextObject)\
if(\
NOT [InWorldContextObject]()\
{\
    CK_ENSURE_IF_NOT(ck::IsValid(InWorldContextObject), TEXT("Invalid World Context object used to validate UWorld"))\
    { return false; }\
\
    CK_ENSURE_IF_NOT(ck::IsValid(InWorldContextObject->GetWorld()), TEXT("Invalid UWorld"))\
    { return false; }\
\
    return true;\
}())

#define CK_TRIGGER_ENSURE(InString, ...)\
CK_ENSURE(false, InString, ##__VA_ARGS__)

// Same expansion as CK_ENSURE, but semantically the trigger expression is NOT part of the ensure itself.
#define CK_TRIGGER_ENSURE_IF(InExpression, InString, ...)\
if(InExpression) { CK_TRIGGER_ENSURE(InString, ##__VA_ARGS__); }

#define CK_INVALID_ENUM(InEnsure)\
CK_TRIGGER_ENSURE(TEXT("Encountered an invalid value for Enum [{}]"), InEnsure)

// --------------------------------------------------------------------------------------------------------------------

#define CK_ENSURE_VALID_IF_NOT(_ToValidate_)                    \
CK_ENSURE_IF_NOT(ck::IsValid(_ToValidate_), TEXT(#_ToValidate_" [{}] is INVALID"), _ToValidate_)

#define CK_ENSURE_VALID_IF_NOT_MSG(_ToValidate_, InFormat, ...) \
CK_ENSURE_IF_NOT(ck::IsValid(_ToValidate_), TEXT(#_ToValidate_ " [{}] is INVALID. {}"), _ToValidate_, ck::Format_UE(InFormat, ##__VA_ARGS__))

// --------------------------------------------------------------------------------------------------------------------
