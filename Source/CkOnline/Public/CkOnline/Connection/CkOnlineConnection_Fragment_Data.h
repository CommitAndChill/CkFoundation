#pragma once

#include "CkOnline/Direct/CkOnlineDirect_Fragment_Data.h"
#include "CkOnline/Direct/CkOnlineDirect_ReadyLobbyBinding.h"
#include "GameFramework/GameModeBase.h"

#include "CkOnlineConnection_Fragment_Data.generated.h"

UENUM(BlueprintType)
enum class ECk_OnlineConnection_Action : uint8 { Invalid, Host, Join, ConfirmGameReady, Return, Cancel };
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineConnection_Action);

UENUM(BlueprintType)
enum class ECk_OnlineConnection_State : uint8 { Idle, Entering, AwaitingGameReady, Active, AwaitingReturn, Returning, Failed, Stopped };
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineConnection_State);

UENUM(BlueprintType)
enum class ECk_OnlineConnection_Result : uint8 { InvalidState, Succeeded, Cancelled, TimedOut, ControlLost, TransportError, ReturnFailed };
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineConnection_Result);

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKONLINE_API FCk_Handle_OnlineConnection : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_OnlineConnection);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_OnlineConnection);

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineConnection_Operation
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineConnection_Operation);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _Id;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _Generation;

public:
    CK_PROPERTY(_Id);
    CK_PROPERTY(_Generation);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineConnection_Spec
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineConnection_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Handle_OnlineDirect _DirectService;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_OnlineDirect_ReadyLobbyBinding _Binding;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _ExpectedMembers = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UWorld> _GameplayMap;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSubclassOf<AGameModeBase> _GameplayMode;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FString _Endpoint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _ListenPort = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UWorld> _ReturnMap;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSubclassOf<AGameModeBase> _ReturnMode;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    double _EntryTimeoutSeconds = 15.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    double _ReturnTimeoutSeconds = 15.0;

public:
    CK_PROPERTY(_DirectService);
    CK_PROPERTY(_Binding);
    CK_PROPERTY(_ExpectedMembers);
    CK_PROPERTY(_GameplayMap);
    CK_PROPERTY(_GameplayMode);
    CK_PROPERTY(_Endpoint);
    CK_PROPERTY(_ListenPort);
    CK_PROPERTY(_ReturnMap);
    CK_PROPERTY(_ReturnMode);
    CK_PROPERTY(_EntryTimeoutSeconds);
    CK_PROPERTY(_ReturnTimeoutSeconds);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_Request_OnlineConnection : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_OnlineConnection);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_OnlineConnection);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_OnlineConnection_Action _Action = ECk_OnlineConnection_Action::Invalid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_OnlineConnection_Spec _Spec;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_OnlineConnection_Operation _Target;

public:
    CK_PROPERTY(_Action);
    CK_PROPERTY(_Spec);
    CK_PROPERTY(_Target);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineConnection_Snapshot
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineConnection_Snapshot);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineConnection_State _State = ECk_OnlineConnection_State::Stopped;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_OnlineConnection_Operation _Operation;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_OnlineConnection_Operation _Session;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    bool _Hosting = false;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Endpoint;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _LastError;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineConnection_Result _RecoveryCause = ECk_OnlineConnection_Result::InvalidState;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Revision = 0;

public:
    CK_PROPERTY(_State);
    CK_PROPERTY(_Operation);
    CK_PROPERTY(_Session);
    CK_PROPERTY(_Hosting);
    CK_PROPERTY(_Endpoint);
    CK_PROPERTY(_LastError);
    CK_PROPERTY(_RecoveryCause);
    CK_PROPERTY(_Revision);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineConnection_Completion
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineConnection_Completion);

private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_OnlineConnection_Operation _Operation;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_OnlineConnection_Operation _Target;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineConnection_Action _Action = ECk_OnlineConnection_Action::Invalid;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineConnection_Result _Result = ECk_OnlineConnection_Result::InvalidState;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineConnection_Result _RecoveryCause = ECk_OnlineConnection_Result::InvalidState;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Detail;

public:
    CK_PROPERTY(_Operation);
    CK_PROPERTY(_Target);
    CK_PROPERTY(_Action);
    CK_PROPERTY(_Result);
    CK_PROPERTY(_RecoveryCause);
    CK_PROPERTY(_Detail);
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineConnection_Completed,
    FCk_Handle_OnlineConnection, InService, FCk_OnlineConnection_Completion, InCompletion);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineConnection_Changed,
    FCk_Handle_OnlineConnection, InService, FCk_OnlineConnection_Snapshot, InSnapshot);
