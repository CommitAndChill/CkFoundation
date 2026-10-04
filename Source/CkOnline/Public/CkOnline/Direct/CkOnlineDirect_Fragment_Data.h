#pragma once

#include "CkOnline/CkOnline_Fragment_Data.h"

#include "CkOnlineDirect_Fragment_Data.generated.h"

UENUM(BlueprintType)
enum class ECk_OnlineDirect_State : uint8 { Offline, Hosting, Joining, Joined, Failed };
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineDirect_State);

UENUM(BlueprintType)
enum class ECk_OnlineDirect_Action : uint8 { Host, Join, Leave, SetReady, SendText };
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineDirect_Action);

UENUM(BlueprintType)
enum class ECk_OnlineDirect_Result : uint8
{ Succeeded, Cancelled, TimedOut, InvalidInput, InvalidState, TransportError, Unauthorized, Expired, Full, ProtocolError };
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineDirect_Result);

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKONLINE_API FCk_Handle_OnlineDirect : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_OnlineDirect);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_OnlineDirect);

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineDirect_Spec
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineDirect_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FString _Name = TEXT("Player");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FString _AdvertisedAddress = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _Port = 7788;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _Capacity = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    double _InviteLifetimeSeconds = 300.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    double _OperationTimeoutSeconds = 5.0;

public:
    CK_PROPERTY(_Name);
    CK_PROPERTY(_AdvertisedAddress);
    CK_PROPERTY(_Port);
    CK_PROPERTY(_Capacity);
    CK_PROPERTY(_InviteLifetimeSeconds);
    CK_PROPERTY(_OperationTimeoutSeconds);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineDirect_Member
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineDirect_Member);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _Id;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Name;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    bool _Ready = false;

public:
    CK_PROPERTY(_Id);
    CK_PROPERTY(_Name);
    CK_PROPERTY(_Ready);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineDirect_Message
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineDirect_Message);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Sequence = 0;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _SenderId;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _SenderName;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Text;

public:
    CK_PROPERTY(_Sequence);
    CK_PROPERTY(_SenderId);
    CK_PROPERTY(_SenderName);
    CK_PROPERTY(_Text);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineDirect_Snapshot
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineDirect_Snapshot);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineDirect_State _State = ECk_OnlineDirect_State::Offline;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _ServiceGeneration;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _LobbyEpoch;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _LocalMemberId;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_OnlineDirect_Member> _Members;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_OnlineDirect_Message> _Messages;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _JoinDescriptor;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _LastError;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Revision = 0;

public:
    CK_PROPERTY(_State);
    CK_PROPERTY(_ServiceGeneration);
    CK_PROPERTY(_LobbyEpoch);
    CK_PROPERTY(_LocalMemberId);
    CK_PROPERTY(_Members);
    CK_PROPERTY(_Messages);
    CK_PROPERTY(_JoinDescriptor);
    CK_PROPERTY(_LastError);
    CK_PROPERTY(_Revision);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_Request_OnlineDirect : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_OnlineDirect);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_OnlineDirect);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGuid _OperationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_OnlineDirect_Action _Action = ECk_OnlineDirect_Action::Host;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FString _Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _Ready = false;

public:
    CK_PROPERTY(_OperationId);
    CK_PROPERTY(_Action);
    CK_PROPERTY(_Text);
    CK_PROPERTY(_Ready);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineDirect_Completion
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineDirect_Completion);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _OperationId;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineDirect_Result _Result = ECk_OnlineDirect_Result::InvalidState;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Detail;

public:
    CK_PROPERTY(_OperationId);
    CK_PROPERTY(_Result);
    CK_PROPERTY(_Detail);
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineDirect_Completed,
    FCk_Handle_OnlineDirect, InService, FCk_OnlineDirect_Completion, InCompletion);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineDirect_Changed,
    FCk_Handle_OnlineDirect, InService, FCk_OnlineDirect_Snapshot, InSnapshot);
