#pragma once

#include "CkOnline/CkOnline_Fragment_Data.h"

#include "CkOnlineSteam_Fragment_Data.generated.h"

UENUM(BlueprintType)
enum class ECk_OnlineSteam_State : uint8
{
    Unavailable,
    LoggedOut,
    Ready,
    Creating,
    Searching,
    Joining,
    Joined,
    Leaving,
    CleaningUp,
    CleanupUnknown,
    Failed,
    Stopped
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineSteam_State);

UENUM(BlueprintType)
enum class ECk_OnlineSteam_Action : uint8 { Invalid, Login, CreateLobby, FindLobbies, JoinLobby, LeaveLobby, Cancel };
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineSteam_Action);

UENUM(BlueprintType)
enum class ECk_OnlineSteam_Result : uint8
{ InvalidState, Succeeded, Cancelled, TimedOut, ProviderUnavailable, NotLoggedIn, IdentityChanged, Rejected, Failed };
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_OnlineSteam_Result);

/** Explicit local-user selection; Steam currently supports local user zero only. */
USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineSteam_Spec
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineSteam_Spec);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _LocalUserIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _MaxLobbyMembers = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _MaxSearchResults = 16;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FString _LobbyLabel = TEXT("Ck Steam Lobby");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    double _CleanupGraceSeconds = 30.0;
public:
    CK_PROPERTY(_LocalUserIndex);
    CK_PROPERTY(_MaxLobbyMembers);
    CK_PROPERTY(_MaxSearchResults);
    CK_PROPERTY(_LobbyLabel);
    CK_PROPERTY(_CleanupGraceSeconds);
};

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKONLINE_API FCk_Handle_OnlineSteam : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_OnlineSteam);
};
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_OnlineSteam);

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineSteam_Lobby
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineSteam_Lobby);
private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Id;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _OwnerName;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Label;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _OpenSlots = 0;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _MaxMembers = 0;
public:
    CK_PROPERTY(_Id);
    CK_PROPERTY(_OwnerName);
    CK_PROPERTY(_Label);
    CK_PROPERTY(_OpenSlots);
    CK_PROPERTY(_MaxMembers);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineSteam_Snapshot
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineSteam_Snapshot);
private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineSteam_State _State = ECk_OnlineSteam_State::Unavailable;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _ServiceGeneration;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _AccountGeneration;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _AccountId;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _DisplayName;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _LobbyId;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_OnlineSteam_Lobby> _SearchResults;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _SearchGeneration;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _LastError;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Revision = 0;
public:
    CK_PROPERTY(_State);
    CK_PROPERTY(_ServiceGeneration);
    CK_PROPERTY(_AccountGeneration);
    CK_PROPERTY(_AccountId);
    CK_PROPERTY(_DisplayName);
    CK_PROPERTY(_LobbyId);
    CK_PROPERTY(_SearchResults);
    CK_PROPERTY(_SearchGeneration);
    CK_PROPERTY(_LastError);
    CK_PROPERTY(_Revision);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_Request_OnlineSteam : public FCk_Request_Base
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_OnlineSteam);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_OnlineSteam);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGuid _OperationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_OnlineSteam_Action _Action = ECk_OnlineSteam_Action::Invalid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int32 _LobbyIndex = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGuid _SearchGeneration;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FGuid _TargetOperationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    double _TimeoutSeconds = 15.0;
public:
    CK_PROPERTY(_OperationId);
    CK_PROPERTY(_Action);
    CK_PROPERTY(_LobbyIndex);
    CK_PROPERTY(_SearchGeneration);
    CK_PROPERTY(_TargetOperationId);
    CK_PROPERTY(_TimeoutSeconds);
};

USTRUCT(BlueprintType)
struct CKONLINE_API FCk_OnlineSteam_Completion
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_OnlineSteam_Completion);
private:
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGuid _OperationId;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineSteam_Action _Action = ECk_OnlineSteam_Action::Invalid;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_OnlineSteam_Result _Result = ECk_OnlineSteam_Result::InvalidState;
    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Detail;
public:
    CK_PROPERTY(_OperationId);
    CK_PROPERTY(_Action);
    CK_PROPERTY(_Result);
    CK_PROPERTY(_Detail);
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineSteam_Completed,
    FCk_Handle_OnlineSteam, InService, FCk_OnlineSteam_Completion, InCompletion);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCk_Delegate_OnlineSteam_Changed,
    FCk_Handle_OnlineSteam, InService, FCk_OnlineSteam_Snapshot, InSnapshot);
