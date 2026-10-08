#include "CkCrowd/Debug/CkCrowd_RemoteDebugRegistry.h"

#include <Engine/LocalPlayer.h>
#include <GameFramework/Actor.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <UObject/UnrealType.h>

namespace ck_crowd_remote_debug_registry
{
    auto LiveWorld(const UWorld* World) -> bool
    {
        return ::IsValid(World) &&
            !World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) &&
            !World->bIsTearingDown;
    }

    auto LiveOwner(const UObject* Owner) -> bool
    {
        if (!::IsValid(Owner) || Owner->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        { return false; }
        const auto* Actor = Cast<AActor>(Owner);
        return Actor == nullptr || !Actor->IsActorBeingDestroyed();
    }

    struct FEntry
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<UObject> Owner;
        FCkCrowdRemoteProviderToken Token;
    };

    auto
        Entries()
        -> TArray<FEntry>&
    {
        static TArray<FEntry> Result;
        return Result;
    }

    auto
        NextToken()
        -> uint64&
    {
        static uint64 Result = 1;
        return Result;
    }

    auto
        Finite(
            const FVector& Value
        )
        -> bool
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    auto
        Finite(
            const FQuat& Value
        )
        -> bool
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z) && FMath::IsFinite(Value.W);
    }

    auto
        Finite(
            const FLinearColor& Value
        )
        -> bool
    {
        return FMath::IsFinite(Value.R) && FMath::IsFinite(Value.G) &&
            FMath::IsFinite(Value.B) && FMath::IsFinite(Value.A);
    }

    // Reject oversized UI text rather than truncating authored agent/path arrays.
    constexpr int32 MaxDisplayTextCodeUnits = 1024;
    constexpr int32 MaxNarrativeTextCodeUnits = 4096;
    auto
        DisplayText(
            const FString& Value
        )
        -> bool
    {
        return Value.Len() <= MaxDisplayTextCodeUnits;
    }
    auto
        NarrativeText(
            const FString& Value
        )
        -> bool
    {
        return Value.Len() <= MaxNarrativeTextCodeUnits;
    }

    template <typename TEnum>
    auto
        ValidEnum(
            TEnum Value
        )
        -> bool
    {
        const UEnum* Enum = StaticEnum<TEnum>();
        if (Enum == nullptr)
        { return false; }
        const int32 Index = Enum->GetIndexByValue(static_cast<int64>(Value));
        if (Index == INDEX_NONE)
        { return false; }
#if WITH_METADATA
        if (Enum->HasMetaData(TEXT("Hidden"), Index))
        { return false; }
#endif
        // IsValidEnumValue includes UHT's reflected but unauthored _MAX sentinel.
        return !Enum->GetNameByIndex(Index).ToString().EndsWith(TEXT("_MAX"));
    }
}

using namespace ck_crowd_remote_debug_registry;

auto FCkCrowdRemoteDebugValidation::IsValid(const FCkCrowdRemoteSelectionReceipt& Receipt) -> bool
{
    const auto IsSerial = [](int64 Value)
    { return Value > 0 && Value < TNumericLimits<int64>::Max(); };
    return IsSerial(Receipt.WorldGeneration) && IsSerial(Receipt.SessionGeneration) &&
        IsSerial(Receipt.DeliverySerial) && IsSerial(Receipt.RequestSerial);
}

auto FCkCrowdRemoteDebugValidation::IsValid(const FCkCrowdRemoteSelectionEnvelope& Envelope) -> bool
{
    const auto IsSerial = [](int64 Value)
    { return Value > 0 && Value < TNumericLimits<int64>::Max(); };
    if (!IsSerial(Envelope.WorldGeneration) || !IsSerial(Envelope.SessionGeneration) ||
        !IsSerial(Envelope.SampleSequence) || Envelope.SelectionRequestSerial < 0 ||
        (Envelope.SelectionRequestSerial != 0 && !IsSerial(Envelope.SelectionRequestSerial)))
    { return false; }
    switch (Envelope.Outcome)
    {
    case ECkCrowdRemoteSelectionOutcome::Accepted:
    case ECkCrowdRemoteSelectionOutcome::Invalid:
    case ECkCrowdRemoteSelectionOutcome::Stale:
    case ECkCrowdRemoteSelectionOutcome::Busy:
    case ECkCrowdRemoteSelectionOutcome::Unauthorized:
    case ECkCrowdRemoteSelectionOutcome::Gone:
    case ECkCrowdRemoteSelectionOutcome::Failed: break;
    default: return false;
    }
    TSet<FCkCrowdRemoteAgentId> Seen;
    const FCkCrowdRemoteAgentRow* Selected = nullptr;
    for (const auto& Row : Envelope.Rows)
    {
        if (!IsValid(Row) || Row.Id.WorldGeneration != Envelope.WorldGeneration ||
            Row.SampleSequence != Envelope.SampleSequence || Seen.Contains(Row.Id)) { return false; }
        Seen.Add(Row.Id);
        if (Row.Id == Envelope.SelectedId) { Selected = &Row; }
    }
    const bool DefaultId = Envelope.SelectedId.WorldGeneration == 0 && Envelope.SelectedId.AgentSerial == 0;
    const FCkCrowdRemoteAgentDetail DefaultDetail;
    const bool DefaultDetailValue = FCkCrowdRemoteAgentDetail::StaticStruct()->CompareScriptStruct(
        &Envelope.Detail, &DefaultDetail, PPF_None);
    if (DefaultId)
    {
        if (Envelope.bHasDetail || !DefaultDetailValue) { return false; }
    }
    else if (!Envelope.SelectedId.IsValid() || !IsSerial(Envelope.SelectionRequestSerial) ||
        !Envelope.bHasDetail || Selected == nullptr || !IsValid(Envelope.Detail) ||
        !(Envelope.Detail.Id == Envelope.SelectedId) ||
        Envelope.Detail.SelectionRequestSerial != Envelope.SelectionRequestSerial ||
        Envelope.Detail.SampleSequence != Envelope.SampleSequence ||
        !FCkCrowdRemoteAgentRow::StaticStruct()->CompareScriptStruct(
            &Envelope.Detail.Sample, Selected, PPF_None)) { return false; }
    if (Envelope.bRetainedDelivery)
    {
        if (!IsValid(Envelope.Receipt) || Envelope.Receipt.WorldGeneration != Envelope.WorldGeneration ||
            Envelope.Receipt.SessionGeneration != Envelope.SessionGeneration) { return false; }
        if (Envelope.Outcome == ECkCrowdRemoteSelectionOutcome::Accepted)
        { return Envelope.SelectionRequestSerial == Envelope.Receipt.RequestSerial; }
        // A rejection cannot be installed as an Accepted selection or detail.
        return DefaultId && Envelope.SelectionRequestSerial == 0 && !Envelope.bHasDetail && DefaultDetailValue;
    }
    const FCkCrowdRemoteSelectionReceipt DefaultReceipt;
    return Envelope.Outcome == ECkCrowdRemoteSelectionOutcome::Accepted &&
        FCkCrowdRemoteSelectionReceipt::StaticStruct()->CompareScriptStruct(
            &Envelope.Receipt, &DefaultReceipt, PPF_None);
}


auto
    FCkCrowdRemoteDebugValidation::
    IsValid(
        const FCkCrowdRemoteAgentRow& Row
    )
    -> bool
{
    const FTransform& Transform = Row.WorldTransform;
    return Row.Id.IsValid() && Row.SampleSequence > 0 && Row.ConfigurationRevision >= 0 &&
        DisplayText(Row.DebugName) && DisplayText(Row.DisplayId) &&
        DisplayText(Row.OwnerName) && DisplayText(Row.PrimaryTag) &&
        NarrativeText(Row.PathTroubleSummary) && DisplayText(Row.QueueDebugName) &&
        DisplayText(Row.QueueCategory) && DisplayText(Row.QueueState) &&
        Row.OwnerSerial >= 0 && Row.NeighborCount >= 0 && Row.PlannedPathPointCount >= 0 &&
        Finite(Transform.GetTranslation()) && Finite(Transform.GetRotation()) &&
        Finite(Transform.GetScale3D()) && Transform.IsValid() &&
        Finite(Row.Position) && Finite(Row.Velocity) &&
        Finite(Row.ActiveGoal) && Finite(Row.BaseColor) && Finite(Row.SeparationForce) &&
        Finite(Row.PathTroubleAgentPosition) && Finite(Row.PathTroubleGoal) &&
        FMath::IsFinite(Row.RadiusCm) && Row.RadiusCm >= 0.0f &&
        FMath::IsFinite(Row.HeightCm) && Row.HeightCm >= 0.0f &&
        FMath::IsFinite(Row.MaxSpeedCmPerSec) && Row.MaxSpeedCmPerSec >= 0.0f &&
        FMath::IsFinite(Row.MaxTurnRateRadPerSec) && Row.MaxTurnRateRadPerSec >= 0.0f &&
        FMath::IsFinite(Row.MaxAccelerationCmPerSecSq) && Row.MaxAccelerationCmPerSecSq >= 0.0f &&
        FMath::IsFinite(Row.ArrivalRadiusCm) && Row.ArrivalRadiusCm >= 0.0f &&
        FMath::IsFinite(Row.ActiveArrivalRadiusCm) && Row.ActiveArrivalRadiusCm >= 0.0f &&
        FMath::IsFinite(Row.SeparationRadiusCm) && Row.SeparationRadiusCm >= 0.0f &&
        FMath::IsFinite(Row.SeparationWeight) && Row.SeparationWeight >= 0.0f &&
        FMath::IsFinite(Row.PathTroubleEventTimeSeconds) && Row.QueueRank >= INDEX_NONE &&
        ValidEnum(Row.Status) && ValidEnum(Row.PathFailReason) &&
        ValidEnum(Row.PathNetworkFailReason) && ValidEnum(Row.TroubleNavigationStatus) &&
        ValidEnum(Row.ActiveProvider) && ValidEnum(Row.TroubleNavigationFailReason);
}

auto
    FCkCrowdRemoteDebugValidation::
    IsValid(
        const FCkCrowdRemoteAgentDetail& Detail
    )
    -> bool
{
    if (!Detail.Id.IsValid() || Detail.SelectionRequestSerial <= 0 ||
        Detail.SampleSequence <= 0 || Detail.ConfigurationRevision < 0 ||
        !IsValid(Detail.Sample) || !(Detail.Sample.Id == Detail.Id) ||
        Detail.Sample.SampleSequence != Detail.SampleSequence ||
        Detail.Sample.ConfigurationRevision != Detail.ConfigurationRevision ||
        Detail.Neighbors.Num() != Detail.Sample.NeighborCount ||
        Detail.PlannedPath.Num() != Detail.Sample.PlannedPathPointCount)
    {
        return false;
    }

    for (const FCkCrowdRemoteNeighbor& Neighbor : Detail.Neighbors)
    {
        if (Neighbor.bIdResolved != Neighbor.Id.IsValid() ||
            (Neighbor.bIdResolved && Neighbor.Id.WorldGeneration != Detail.Id.WorldGeneration) ||
            !FMath::IsFinite(Neighbor.DistanceCm) || Neighbor.DistanceCm < 0.0f ||
            !Finite(Neighbor.RelativeOffset))
        {
            return false;
        }
    }

    for (const FVector& Point : Detail.PlannedPath)
    {
        if (!Finite(Point))
        {
            return false;
        }
    }
    return true;
}

auto
    FCkCrowdRemoteDebugValidation::
    IsValid(
        const FCkCrowdRemoteCommandRequest& Request
    )
    -> bool
{
    if (!Request.Id.IsValid() || Request.ClientSerial <= 0 ||
        Request.ObservedConfigurationRevision < 0 || !ValidEnum(Request.Command))
    {
        return false;
    }
    if (!Finite(Request.Destination) || !FMath::IsFinite(Request.MaxSpeedCmPerSec) ||
        !FMath::IsFinite(Request.MaxTurnRateRadPerSec) ||
        !FMath::IsFinite(Request.MaxAccelerationCmPerSecSq) ||
        !FMath::IsFinite(Request.ArrivalRadiusCm) ||
        !FMath::IsFinite(Request.SeparationRadiusCm))
    {
        return false;
    }
    if (Request.Command == ECkCrowdRemoteCommand::ManualMove)
    {
        return Finite(Request.Destination);
    }
    if (Request.Command == ECkCrowdRemoteCommand::SetDebugOverride)
    {
        return true;
    }
    return Request.MaxSpeedCmPerSec >= 60.0f && Request.MaxSpeedCmPerSec <= 600.0f &&
        Request.MaxTurnRateRadPerSec >= 0.5f && Request.MaxTurnRateRadPerSec <= 20.0f &&
        Request.MaxAccelerationCmPerSecSq >= 120.0f &&
        Request.MaxAccelerationCmPerSecSq <= 2000.0f &&
        Request.ArrivalRadiusCm >= 10.0f && Request.ArrivalRadiusCm <= 120.0f &&
        Request.SeparationRadiusCm >= 40.0f && Request.SeparationRadiusCm <= 200.0f;
}

auto
    FCkCrowdRemoteDebugValidation::
    IsValid(
        const FCkCrowdRemoteCommandResult& Result
    )
    -> bool
{
    return Result.Id.IsValid() && Result.ClientSerial > 0 &&
        Result.AppliedConfigurationRevision >= 0 && Result.ResultSampleSequence >= 0 &&
        (Result.Outcome != ECkCrowdRemoteCommandOutcome::Accepted ||
            Result.ResultSampleSequence > 0) &&
        ValidEnum(Result.Outcome) &&
        Finite(Result.AppliedDestination) && NarrativeText(Result.Diagnostic);
}

auto
    FCkCrowdRemoteDebugRegistry::
    TryRegister(
        UWorld& World, UObject& ProviderOwner, FCkCrowdRemoteProviderToken& OutToken
    )
    -> bool
{
    if (!IsInGameThread() || !LiveWorld(&World) || !LiveOwner(&ProviderOwner) ||
        ProviderOwner.GetWorld() != &World)
    {
        return false;
    }
    const TWeakObjectPtr<UWorld> ExpectedWorld(&World);
    const TWeakObjectPtr<UObject> ExpectedOwner(&ProviderOwner);
    auto* Provider = Cast<ICkCrowdRemoteDebugProvider>(&ProviderOwner);
    if (Provider == nullptr || !Provider->IsForWorld(World)) { return false; }
    // IsForWorld is virtual: do not publish a claim for an owner changed by it.
    if (!LiveWorld(ExpectedWorld.Get()) || !LiveOwner(ExpectedOwner.Get()) ||
        ExpectedWorld.Get() != &World || ExpectedOwner.Get() != &ProviderOwner ||
        ExpectedOwner.Get()->GetWorld() != &World ||
        Cast<ICkCrowdRemoteDebugProvider>(ExpectedOwner.Get()) != Provider)
    { return false; }

    TArray<FEntry>& Registry = Entries();
    for (int32 Index = Registry.Num() - 1; Index >= 0; --Index)
    {
        const FEntry& Entry = Registry[Index];
        if (!LiveWorld(Entry.World.Get()) || !LiveOwner(Entry.Owner.Get()) ||
            Entry.Owner.Get()->GetWorld() != Entry.World.Get())
        {
            Registry.RemoveAtSwap(Index);
            continue;
        }
        if (Entry.World.Get() == &World)
        {
            return false; // exact live duplicate, preserving first registration
        }
    }

    uint64& Serial = NextToken();
    if (Serial == 0 || Serial == MAX_uint64)
    {
        return false;
    }
    FCkCrowdRemoteProviderToken NewToken;
    NewToken.Value = Serial++;
    FEntry Entry;
    Entry.World = &World;
    Entry.Owner = &ProviderOwner;
    Entry.Token = NewToken;
    Registry.Add(MoveTemp(Entry));
    OutToken = NewToken; // publish only after the registration exists
    return true;
}

auto
    FCkCrowdRemoteDebugRegistry::
    TryUnregister(
        const UWorld& World, const UObject& ProviderOwner, FCkCrowdRemoteProviderToken Token
    )
    -> bool
{
    if (!IsInGameThread() || !Token.IsValid())
    {
        return false;
    }
    TArray<FEntry>& Registry = Entries();
    for (int32 Index = Registry.Num() - 1; Index >= 0; --Index)
    {
        const FEntry& Entry = Registry[Index];
        if (Entry.World.Get() == &World && Entry.Owner.Get() == &ProviderOwner &&
            Entry.Token.Value == Token.Value)
        {
            Registry.RemoveAtSwap(Index); // invalidate token before caller teardown
            return true;
        }
    }
    return false;
}

auto
    FCkCrowdRemoteDebugRegistry::
    TryFind(
        const UWorld& World
    )
    -> ICkCrowdRemoteDebugProvider*
{
    if (!IsInGameThread() || !LiveWorld(&World))
    {
        return nullptr;
    }
    for (int32 Index = Entries().Num() - 1; Index >= 0; --Index)
    {
        const FEntry Entry = Entries()[Index];
        if (!LiveWorld(Entry.World.Get()) || !LiveOwner(Entry.Owner.Get()) ||
            Entry.Owner.Get()->GetWorld() != Entry.World.Get())
        {
            Entries().RemoveAtSwap(Index);
            continue;
        }
        if (Entry.World.Get() == &World)
        {
            UObject* Owner = Entry.Owner.Get();
            ICkCrowdRemoteDebugProvider* Provider = Cast<ICkCrowdRemoteDebugProvider>(Owner);
            if (Provider == nullptr || !Provider->IsForWorld(World)) { return nullptr; }
            // The callback can unregister or replace this entry. Never return a
            // provider unless the exact original token and owner still own it.
            if (!LiveWorld(Entry.World.Get()) || !LiveOwner(Entry.Owner.Get()) ||
                Entry.World.Get() != &World || Entry.Owner.Get() != Owner ||
                Owner->GetWorld() != &World ||
                Cast<ICkCrowdRemoteDebugProvider>(Owner) != Provider)
            { return nullptr; }
            for (const FEntry& Current : Entries())
            {
                if (Current.Token.Value == Entry.Token.Value &&
                    Current.World.Get() == &World && Current.Owner.Get() == Owner)
                { return Provider; }
            }
            return nullptr;
        }
    }
    return nullptr;
}

auto
    FCkCrowdRemoteDebugRegistry::
    HasLocalContext(
        const UWorld& World, const APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer
    )
    -> bool
{
    return IsInGameThread() && LiveWorld(&World) && LiveOwner(&RequestingPC) &&
        LiveOwner(&RequestingLocalPlayer) &&
        RequestingPC.GetWorld() == &World &&
        RequestingLocalPlayer.GetWorld() == &World &&
        RequestingPC.GetLocalPlayer() == &RequestingLocalPlayer &&
        RequestingLocalPlayer.GetPlayerController(&World) == &RequestingPC &&
        RequestingPC.IsLocalController();
}

auto
    FCkCrowdRemoteDebugRegistry::
    TryCopyRows(
        const UWorld& World, const APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer, TArray<FCkCrowdRemoteAgentRow>& Out
    )
    -> bool
{
    Out.Reset();
    if (!HasLocalContext(World, RequestingPC, RequestingLocalPlayer))
    {
        return false;
    }
    ICkCrowdRemoteDebugProvider* Provider = TryFind(World);
    if (Provider == nullptr || !Provider->CopyRows(World, RequestingPC, RequestingLocalPlayer, Out))
    {
        Out.Reset();
        return false;
    }
    TSet<FCkCrowdRemoteAgentId> Seen;
    int64 WorldGeneration = 0;
    int64 SampleSequence = 0;
    for (const FCkCrowdRemoteAgentRow& Row : Out)
    {
        if (!FCkCrowdRemoteDebugValidation::IsValid(Row) || Seen.Contains(Row.Id) ||
            (WorldGeneration != 0 && Row.Id.WorldGeneration != WorldGeneration) ||
            (SampleSequence != 0 && Row.SampleSequence != SampleSequence))
        {
            Out.Reset();
            return false;
        }
        Seen.Add(Row.Id);
        WorldGeneration = Row.Id.WorldGeneration;
        SampleSequence = Row.SampleSequence;
    }
    return true;
}

auto
    FCkCrowdRemoteDebugRegistry::
    TryRequestSelection(
        const UWorld& World, APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteAgentId& Id,
        int64 SelectionRequestSerial, bool bClear)
    -> bool
{
    const bool IsDefaultId = Id.WorldGeneration == 0 && Id.AgentSerial == 0;
    if (!HasLocalContext(World, RequestingPC, RequestingLocalPlayer) ||
        SelectionRequestSerial <= 0 || (bClear ? !IsDefaultId : !Id.IsValid()))
    { return false; }
    ICkCrowdRemoteDebugProvider* Provider = TryFind(World);
    return Provider != nullptr && Provider->RequestSelectionFromOwningLocalController(
        World, RequestingPC, RequestingLocalPlayer, Id, SelectionRequestSerial, bClear);
}


auto FCkCrowdRemoteDebugRegistry::TryCopySelectionEnvelope(const UWorld& World,
    const APlayerController& RequestingPC, const ULocalPlayer& RequestingLocalPlayer,
    FCkCrowdRemoteSelectionEnvelope& Out) -> bool
{
    Out = {};
    if (!HasLocalContext(World, RequestingPC, RequestingLocalPlayer)) { return false; }
    auto* Provider = TryFind(World);
    FCkCrowdRemoteSelectionEnvelope Candidate;
    if (Provider == nullptr || !Provider->CopySelectionEnvelope(
        World, RequestingPC, RequestingLocalPlayer, Candidate) ||
        !FCkCrowdRemoteDebugValidation::IsValid(Candidate)) { return false; }
    Out = MoveTemp(Candidate);
    return true;
}

auto FCkCrowdRemoteDebugRegistry::TryAcknowledgeSelection(const UWorld& World,
    APlayerController& RequestingPC, const ULocalPlayer& RequestingLocalPlayer,
    const FCkCrowdRemoteSelectionReceipt& Receipt) -> bool
{
    if (!HasLocalContext(World, RequestingPC, RequestingLocalPlayer) ||
        !FCkCrowdRemoteDebugValidation::IsValid(Receipt)) { return false; }
    auto* Provider = TryFind(World);
    return Provider != nullptr && Provider->AcknowledgeSelectionFromOwningLocalController(
        World, RequestingPC, RequestingLocalPlayer, Receipt);
}

auto FCkCrowdRemoteDebugRegistry::TryRequestNextSelection(const UWorld& World,
    APlayerController& RequestingPC, const ULocalPlayer& RequestingLocalPlayer,
    const FCkCrowdRemoteAgentId& Id, bool bClear, int64& OutIssuedSerial) -> bool
{
    OutIssuedSerial = 0;
    const bool DefaultId = Id.WorldGeneration == 0 && Id.AgentSerial == 0;
    if (!HasLocalContext(World, RequestingPC, RequestingLocalPlayer) ||
        (bClear ? !DefaultId : !Id.IsValid())) { return false; }
    auto* Provider = TryFind(World);
    int64 CandidateSerial = 0;
    if (Provider == nullptr || !Provider->RequestNextSelectionFromOwningLocalController(
        World, RequestingPC, RequestingLocalPlayer, Id, bClear, CandidateSerial) ||
        CandidateSerial <= 0 || CandidateSerial >= TNumericLimits<int64>::Max()) { return false; }
    OutIssuedSerial = CandidateSerial;
    return true;
}

auto
    FCkCrowdRemoteDebugRegistry::
    TryCopySelectedDetail(
        const UWorld& World, const APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer, const FCkCrowdRemoteAgentId& Id,
        int64 SelectionRequestSerial, FCkCrowdRemoteAgentDetail& Out
    )
    -> bool
{
    Out = {};
    if (!HasLocalContext(World, RequestingPC, RequestingLocalPlayer) ||
        !Id.IsValid() || SelectionRequestSerial <= 0)
    {
        return false;
    }
    ICkCrowdRemoteDebugProvider* Provider = TryFind(World);
    if (Provider == nullptr || !Provider->CopySelectedDetail(
            World, RequestingPC, RequestingLocalPlayer, Id, SelectionRequestSerial, Out) ||
        !FCkCrowdRemoteDebugValidation::IsValid(Out) ||
        !(Out.Id == Id) || Out.SelectionRequestSerial != SelectionRequestSerial)
    {
        Out = {};
        return false;
    }
    return true;
}

auto
    FCkCrowdRemoteDebugRegistry::
    TrySubmit(
        const UWorld& World, APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer,
        const FCkCrowdRemoteCommandRequest& Request
    )
    -> bool
{
    if (!HasLocalContext(World, RequestingPC, RequestingLocalPlayer) ||
        !FCkCrowdRemoteDebugValidation::IsValid(Request))
    {
        return false;
    }
    ICkCrowdRemoteDebugProvider* Provider = TryFind(World);
    return Provider != nullptr && Provider->SubmitFromOwningLocalController(
        World, RequestingPC, RequestingLocalPlayer, Request);
}

auto
    FCkCrowdRemoteDebugRegistry::
    TryCopyCommandResult(
        const UWorld& World, const APlayerController& RequestingPC,
        const ULocalPlayer& RequestingLocalPlayer, int64 ClientSerial,
        FCkCrowdRemoteCommandResult& Out
    )
    -> bool
{
    Out = {};
    if (!HasLocalContext(World, RequestingPC, RequestingLocalPlayer) || ClientSerial <= 0)
    {
        return false;
    }
    ICkCrowdRemoteDebugProvider* Provider = TryFind(World);
    if (Provider == nullptr || !Provider->CopyCommandResult(
            World, RequestingPC, RequestingLocalPlayer, ClientSerial, Out) ||
        !FCkCrowdRemoteDebugValidation::IsValid(Out) || Out.ClientSerial != ClientSerial)
    {
        Out = {};
        return false;
    }
    return true;
}
