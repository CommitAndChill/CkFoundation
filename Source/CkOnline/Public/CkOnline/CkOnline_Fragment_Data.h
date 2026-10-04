#pragma once

#include "CkCore/Time/CkTime.h"
#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkEcs/Request/CkRequest_Data.h"

#include "CkOnline_Fragment_Data.generated.h"

UENUM(BlueprintType)
enum class ECk_OnlineService_State : uint8
{
    Stopped,
    Running,
    Stopping
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineService_State);

UENUM(BlueprintType)
enum class ECk_OnlineService_Result : uint8
{
    Succeeded,
    Failed,
    Cancelled,
    TimedOut
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineService_Result);

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKONLINE_API FCk_Handle_OnlineService : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_OnlineService);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_OnlineService);

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_Request_OnlineService_Execute : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_OnlineService_Execute);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_OnlineService_Execute);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGuid _OperationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Time _Timeout = FCk_Time{5.0};

public:
    CK_PROPERTY_GET(_OperationId);
    CK_PROPERTY(_Timeout);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_OnlineService_Execute, _OperationId);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_Request_OnlineService_Cancel : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_OnlineService_Cancel);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_OnlineService_Cancel);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGuid _OperationId;

public:
    CK_PROPERTY_GET(_OperationId);
    CK_DEFINE_CONSTRUCTORS(FCk_Request_OnlineService_Cancel, _OperationId);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineService_Completion
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineService_Completion);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _OperationId;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineService_Result _Result = ECk_OnlineService_Result::Failed;

public:
    CK_PROPERTY_GET(_OperationId);
    CK_PROPERTY_GET(_Result);
    CK_DEFINE_CONSTRUCTORS(FCk_OnlineService_Completion, _OperationId, _Result);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineService_Diagnostics
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineService_Diagnostics);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _Generation;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineService_State _State = ECk_OnlineService_State::Stopped;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Queued = 0;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Pending = 0;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Completed = 0;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Cancelled = 0;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _TimedOut = 0;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _IgnoredCallbacks = 0;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _TickCount = 0;

public:
    CK_PROPERTY(_Generation);
    CK_PROPERTY(_State);
    CK_PROPERTY(_Queued);
    CK_PROPERTY(_Pending);
    CK_PROPERTY(_Completed);
    CK_PROPERTY(_Cancelled);
    CK_PROPERTY(_TimedOut);
    CK_PROPERTY(_IgnoredCallbacks);
    CK_PROPERTY(_TickCount);
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineService_Completed,
    FCk_Handle_OnlineService, InService, FCk_OnlineService_Completion, InCompletion);
