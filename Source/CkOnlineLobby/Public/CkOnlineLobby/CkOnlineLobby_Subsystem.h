#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CkOnlineLobby_Subsystem.generated.h"

class UTexture2D;
struct FCk_OnlineLobby_Native;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCk_OnlineLobby_Invalidated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCk_OnlineLobby_Created, FString, InLobbyId, bool, InSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCk_OnlineLobby_Entered, FString, InLobbyId, bool, InSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCk_OnlineLobby_RosterChanged, FString, InLobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCk_OnlineLobby_DataChanged, FString, InLobbyId, bool, InSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCk_OnlineLobby_InviteAccepted, FString, InLobbyId);

// Steam friends-only lobby membership is independent of the configured transport session.
// IDs cross the reflected boundary as canonical decimal strings; SDK types remain private.
UCLASS()
class CKONLINELOBBY_API UCk_OnlineLobby_Subsystem_UE : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    auto Initialize(FSubsystemCollectionBase& InCollection) -> void override;
    auto Deinitialize() -> void override;
    auto BeginDestroy() -> void override;

    UPROPERTY(BlueprintAssignable, Category = "Ck|Online|Lobby")
    FCk_OnlineLobby_Created OnLobbyCreated;
    UPROPERTY(BlueprintAssignable, Category = "Ck|Online|Lobby")
    FCk_OnlineLobby_Invalidated OnLobbyInvalidated;
    UPROPERTY(BlueprintAssignable, Category = "Ck|Online|Lobby")
    FCk_OnlineLobby_Entered OnLobbyEnter;
    UPROPERTY(BlueprintAssignable, Category = "Ck|Online|Lobby")
    FCk_OnlineLobby_RosterChanged OnLobbyRosterChanged;
    UPROPERTY(BlueprintAssignable, Category = "Ck|Online|Lobby")
    FCk_OnlineLobby_DataChanged OnLobbyDataUpdate;
    UPROPERTY(BlueprintAssignable, Category = "Ck|Online|Lobby")
    FCk_OnlineLobby_InviteAccepted OnGameLobbyJoinRequested;

    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    bool Get_IsAvailable() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    bool Get_IsAdmissionPending() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    bool Get_IsOverlayEnabled() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    bool Get_IsSteamRunningOnSteamDeck() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    FString Get_LocalUserId() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    FString Get_CurrentLobbyId() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    FString Get_LocalPersonaName() const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    FString Get_FriendPersonaName(const FString& InUserId) const;
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Lobby")
    UTexture2D* Get_MediumAvatar(const FString& InUserId);

    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Lobby")
    bool Request_CreateFriendsOnlyLobby(int32 InMaxMembers);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Lobby")
    bool Request_JoinLobby(const FString& InLobbyId);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Lobby")
    void Request_LeaveLobby(const FString& InLobbyId);
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Lobby")
    bool Request_OpenInviteDialog(const FString& InLobbyId);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    FString Get_LobbyOwner(const FString& InLobbyId) const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    int32 Get_NumLobbyMembers(const FString& InLobbyId) const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    FString Get_LobbyMemberByIndex(const FString& InLobbyId, int32 InIndex) const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    FString Get_LobbyData(const FString& InLobbyId, const FString& InKey) const;
    UFUNCTION(BlueprintCallable, Category = "Ck|Online|Lobby")
    bool Request_SetLobbyData(const FString& InLobbyId, const FString& InKey, const FString& InValue);
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    bool Get_IsValidLobbyId(const FString& InLobbyId) const;
    UFUNCTION(BlueprintPure, Category = "Ck|Online|Lobby")
    FString Get_CommandLineLobbyId() const;

private:
    friend struct FCk_OnlineLobby_Native;
    auto DoTick(float InDeltaSeconds) -> bool;
    auto DoTryInitializeNative() -> void;
    auto DoRelease() -> void;

    FCk_OnlineLobby_Native* _Native = nullptr;
    FTSTicker::FDelegateHandle _Ticker;
    bool _Released = false;

    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UTexture2D>> _Avatars;
};
