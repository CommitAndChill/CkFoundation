#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <UObject/Interface.h>

#include "CkCrowd_RemoteDebugContract.generated.h"

// Wire identifiers are server issued. Signed int64 is supported by the existing BlueprintType
// CkCrowd debug snapshots; zero is reserved so malformed/default messages fail closed.
USTRUCT(BlueprintType)
struct CKCROWD_API FCkCrowdRemoteAgentId
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 WorldGeneration = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 AgentSerial = 0;

    auto
        IsValid() const
        -> bool
    {
        return WorldGeneration > 0 && AgentSerial > 0;
    }
    friend auto
        GetTypeHash(const FCkCrowdRemoteAgentId& Id)
        -> uint32
    {
        return HashCombine(::GetTypeHash(Id.WorldGeneration), ::GetTypeHash(Id.AgentSerial));
    }
    auto
        operator==(
            const FCkCrowdRemoteAgentId& Other
        )
        const -> bool
    {
        return WorldGeneration == Other.WorldGeneration && AgentSerial == Other.AgentSerial;
    }
};

UENUM(BlueprintType)
enum class ECkCrowdRemoteAgentStatus : uint8
{
    None, Idle, Walking, Disabled, Replanning, Failed, PlayerProxy
};

UENUM(BlueprintType)
enum class ECkCrowdRemoteNavPathStatus : uint8
{
    None, Pending, Ready, Failed, Partial
};

UENUM(BlueprintType)
enum class ECkCrowdRemoteNavPathFailReason : uint8
{
    None, NoNavSystem, NoNavData, NoDefaultFilter, StartProjectFailed,
    EndProjectFailed, FindPathError, FindPathNoPath, FindPathInvalid,
    EmptyPath, NotAuthority, BudgetExceeded, PendingTimeout
};

UENUM(BlueprintType)
enum class ECkCrowdRemotePathNetworkFailReason : uint8
{
    None, NoNetwork, NetworkNotBuilt, NoRouteFound, NotAuthority, BudgetExceeded
};

UENUM(BlueprintType)
enum class ECkCrowdRemotePathProvider : uint8
{
    None, Navigation, PathNetwork, VoxelNav, GroundNav
};

USTRUCT(BlueprintType)
struct CKCROWD_API FCkCrowdRemoteNeighbor
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteAgentId Id;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bIdResolved = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float DistanceCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector RelativeOffset = FVector::ZeroVector; // world cm
};

// Detached server sample. All current FCkCrowdDebugger_AgentSnapshot scalars have typed
// counterparts; the selected-only arrays are carried by FCkCrowdRemoteAgentDetail.
USTRUCT(BlueprintType)
struct CKCROWD_API FCkCrowdRemoteAgentRow
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteAgentId Id;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 SampleSequence = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 ConfigurationRevision = 0;

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString DebugName;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString DisplayId;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString OwnerName;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 OwnerSerial = 0; // zero when owner focus is unavailable
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") TArray<FGameplayTag> Tags;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString PrimaryTag;

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FTransform WorldTransform = FTransform::Identity;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector Position = FVector::ZeroVector; // world cm
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector Velocity = FVector::ZeroVector; // desired cm/s
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector ActiveGoal = FVector::ZeroVector; // world cm
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FLinearColor BaseColor = FLinearColor::White;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float RadiusCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float HeightCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemoteAgentStatus Status = ECkCrowdRemoteAgentStatus::None;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemoteNavPathFailReason PathFailReason =
        ECkCrowdRemoteNavPathFailReason::None;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bIsWalking = false;
    // Body tint uses independent tags; display-status precedence can mask either tag.
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bIsPathPending = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bIsDisabled = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bHasDebugOverride = false; // authority CrowdAgent override, not a client guess
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int32 NeighborCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int32 PlannedPathPointCount = 0;

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float MaxSpeedCmPerSec = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float MaxTurnRateRadPerSec = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float MaxAccelerationCmPerSecSq = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float ArrivalRadiusCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float ActiveArrivalRadiusCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float SeparationRadiusCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float SeparationWeight = 0.0f; // dimensionless
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector SeparationForce = FVector::ZeroVector; // cm/s

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bHasPathTroubleEvent = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bHadPathNetworkFailure = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bUsedNavigationFallback = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemotePathNetworkFailReason PathNetworkFailReason =
        ECkCrowdRemotePathNetworkFailReason::None;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemoteNavPathStatus TroubleNavigationStatus =
        ECkCrowdRemoteNavPathStatus::None;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemotePathProvider ActiveProvider =
        ECkCrowdRemotePathProvider::None;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemoteNavPathFailReason TroubleNavigationFailReason =
        ECkCrowdRemoteNavPathFailReason::None;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector PathTroubleAgentPosition = FVector::ZeroVector; // world cm
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector PathTroubleGoal = FVector::ZeroVector; // world cm
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") double PathTroubleEventTimeSeconds = -1.0; // server clock
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString PathTroubleSummary;

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString QueueDebugName;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString QueueCategory;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString QueueState;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int32 QueueRank = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct CKCROWD_API FCkCrowdRemoteAgentDetail
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteAgentId Id;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 SampleSequence = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 ConfigurationRevision = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 SelectionRequestSerial = 0; // per local player
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteAgentRow Sample; // same frozen sample as arrays
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") TArray<FCkCrowdRemoteNeighbor> Neighbors;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") TArray<FVector> PlannedPath; // selected agent only, world cm
};


// Identifies one retained Selection delivery on one actual owning session.
USTRUCT(BlueprintType)
struct CKCROWD_API FCkCrowdRemoteSelectionReceipt
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 WorldGeneration = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 SessionGeneration = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 DeliverySerial = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 RequestSerial = 0;
};

UENUM(BlueprintType)
enum class ECkCrowdRemoteSelectionOutcome : uint8
{
    Accepted, Invalid, Stale, Busy, Unauthorized, Gone, Failed
};

// Whole immutable owner epoch. Never combine its rows/detail with another copy.
// Retained Selection dominates continuous view until explicitly consumed and receipted.
USTRUCT(BlueprintType)
struct CKCROWD_API FCkCrowdRemoteSelectionEnvelope
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 WorldGeneration = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 SessionGeneration = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 SampleSequence = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") TArray<FCkCrowdRemoteAgentRow> Rows;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteAgentId SelectedId;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 SelectionRequestSerial = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bHasDetail = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteAgentDetail Detail;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bRetainedDelivery = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteSelectionReceipt Receipt;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemoteSelectionOutcome Outcome =
        ECkCrowdRemoteSelectionOutcome::Accepted;
};

UENUM(BlueprintType)
enum class ECkCrowdRemoteCommand : uint8
{
    SetTunables, SetDebugOverride, ManualMove
};

UENUM(BlueprintType)
enum class ECkCrowdRemoteCommandOutcome : uint8
{
    Accepted, Stale, Unauthorized, Invalid, Gone, Failed
};

USTRUCT(BlueprintType)
struct CKCROWD_API FCkCrowdRemoteCommandRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteAgentId Id;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 ClientSerial = 0; // monotonic per owning PC
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 ObservedConfigurationRevision = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemoteCommand Command = ECkCrowdRemoteCommand::SetTunables;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector Destination = FVector::ZeroVector; // world cm
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bOverride = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float MaxSpeedCmPerSec = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float MaxTurnRateRadPerSec = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float MaxAccelerationCmPerSecSq = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float ArrivalRadiusCm = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") float SeparationRadiusCm = 0.0f;
};

USTRUCT(BlueprintType)
struct CKCROWD_API FCkCrowdRemoteCommandResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FCkCrowdRemoteAgentId Id;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 ClientSerial = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 AppliedConfigurationRevision = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") int64 ResultSampleSequence = 0;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") ECkCrowdRemoteCommandOutcome Outcome =
        ECkCrowdRemoteCommandOutcome::Failed;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") bool bHasAppliedDestination = false;
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FVector AppliedDestination = FVector::ZeroVector; // world cm
    UPROPERTY(BlueprintReadOnly, Category="Crowd Remote Debug") FString Diagnostic;
};

class UWorld;
class APlayerController;
class ULocalPlayer;

UINTERFACE()
class CKCROWD_API UCkCrowdRemoteDebugProvider : public UInterface
{
    GENERATED_BODY()
};

// Implement on a world-owned UObject. The registry holds only a weak UObject and re-casts
// this interface on every call; no raw provider pointer or ECS handle is retained.
class CKCROWD_API ICkCrowdRemoteDebugProvider
{
    GENERATED_BODY()

public:
    virtual auto
        IsForWorld(
            const UWorld& World
        )
        const -> bool = 0;
    virtual auto
        CopyRows(
            const UWorld& World, const APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, TArray<FCkCrowdRemoteAgentRow>& Out
        )
        const -> bool = 0;
    // Local request admission only. Completion is published later by the owning session.
    // Existing providers remain source-compatible and reject selection until implemented.
    virtual auto
        RequestSelectionFromOwningLocalController(
            const UWorld& World, APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteAgentId& Id,
            int64 SelectionRequestSerial, bool bClear)
        -> bool
    { return false; }


    // Pure complete copy; existing providers remain source-compatible.
    virtual auto CopySelectionEnvelope(const UWorld& World, const APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer, FCkCrowdRemoteSelectionEnvelope& Out) const -> bool
    { Out = {}; return false; }
    // Explicit receipt after complete installation, never called by getters.
    virtual auto AcknowledgeSelectionFromOwningLocalController(const UWorld& World,
        APlayerController& RequestingPC, const ULocalPlayer& RequestingLocalPlayer,
        const FCkCrowdRemoteSelectionReceipt& Receipt) -> bool
    { return false; }
    // Actual per-PC session issues a fresh serial, not a window-local counter/clock.
    virtual auto RequestNextSelectionFromOwningLocalController(const UWorld& World,
        APlayerController& RequestingPC, const ULocalPlayer& RequestingLocalPlayer,
        const FCkCrowdRemoteAgentId& Id, bool bClear, int64& OutIssuedSerial) -> bool
    { OutIssuedSerial = 0; return false; }

    virtual auto
        CopySelectedDetail(
            const UWorld& World, const APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteAgentId& Id,
            int64 SelectionRequestSerial, FCkCrowdRemoteAgentDetail& Out
        )
        const -> bool = 0;
    virtual auto
        SubmitFromOwningLocalController(
            const UWorld& World, APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer,
            const FCkCrowdRemoteCommandRequest& Request
        )
        -> bool = 0;
    virtual auto
        CopyCommandResult(
            const UWorld& World, const APlayerController& RequestingPC,
            const ULocalPlayer& RequestingLocalPlayer, int64 ClientSerial,
            FCkCrowdRemoteCommandResult& Out
        )
        const -> bool = 0;
};

// Validates detached wire data before any presentation or mutation. No authored agent/path cap.
class CKCROWD_API FCkCrowdRemoteDebugValidation
{
public:
    static auto IsValid(const FCkCrowdRemoteSelectionReceipt& Receipt) -> bool;
    static auto IsValid(const FCkCrowdRemoteSelectionEnvelope& Envelope) -> bool;
    static auto
        IsValid(
            const FCkCrowdRemoteAgentRow& Row
        )
        -> bool;
    static auto
        IsValid(
            const FCkCrowdRemoteAgentDetail& Detail
        )
        -> bool;
    static auto
        IsValid(
            const FCkCrowdRemoteCommandRequest& Request
        )
        -> bool;
    static auto
        IsValid(
            const FCkCrowdRemoteCommandResult& Result
        )
        -> bool;
};
