#include "CkMessaging_Utils.h"

#include "CkCore/Validation/CkUntracedStructSafety.h"
#include "CkMessaging/CkMessaging_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "CkMessaging"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Messaging_UE::
    TryGet_MessengerEntity(
        const FCk_Handle& InOwner,
        FGameplayTag InMessageName)
    -> FCk_Handle
{
    if (ck::Is_NOT_Valid(InOwner) ||
        UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            InOwner, ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
        !InMessageName.IsValid() || !RecordOfMessengers_Utils::Has(InOwner))
    { return {}; }

    // Only the owner's own specialized record is authoritative. A generic record
    // lookup also traverses extensions, which cannot own this direct messenger.
    const auto Entry = RecordOfMessengers_Utils::TryGet_DirectEntry_ByTag(InOwner, InMessageName);
    if (ck::Is_NOT_Valid(Entry) ||
        UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(
            Entry, ECk_EntityLifetime_DestructionPhase::BeginDestroy) ||
        !Entry.Has<ck::FFragment_LifetimeOwner>() ||
        UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Entry) != InOwner ||
        !ck::UUtils_Signal_Messaging::Has(Entry) ||
        !UCk_Utils_GameplayLabel_UE::Has(Entry) ||
        UCk_Utils_GameplayLabel_UE::Get_Label(Entry) != InMessageName)
    { return {}; }
    return Entry;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Messaging_UE::
    Get_NumBoundTo_OnBroadcast(
        const FCk_Handle& InOwner,
        FGameplayTag InMessageName,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> int32
{
    if (InPostFireBehavior != ECk_Signal_PostFireBehavior::DoNothing &&
        InPostFireBehavior != ECk_Signal_PostFireBehavior::Unbind) { return 0; }
    const auto Messenger = TryGet_MessengerEntity(InOwner, InMessageName);
    if (ck::Is_NOT_Valid(Messenger)) { return 0; }
    if (InPostFireBehavior == ECk_Signal_PostFireBehavior::Unbind)
    { return ck::UUtils_Signal_Messaging_PostFireUnbind::Get_NumBoundListeners(Messenger); }
    return ck::UUtils_Signal_Messaging::Get_NumBoundListeners(Messenger);
}

auto
    UCk_Utils_Messaging_UE::
    Get_NumUnconditionalBindingsTo_OnBroadcast(
        const FCk_Handle& InOwner,
        FGameplayTag InMessageName,
        const FCk_Delegate_Messaging_OnBroadcast& InDelegate,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> int32
{
    if (NOT InDelegate.IsBound()) { return 0; }
    if (InPostFireBehavior != ECk_Signal_PostFireBehavior::DoNothing &&
        InPostFireBehavior != ECk_Signal_PostFireBehavior::Unbind) { return 0; }
    const auto Messenger = TryGet_MessengerEntity(InOwner, InMessageName);
    if (ck::Is_NOT_Valid(Messenger)) { return 0; }
    if (InPostFireBehavior == ECk_Signal_PostFireBehavior::Unbind)
    { return ck::UUtils_Signal_Messaging_PostFireUnbind::Get_NumUnconditionalBindingsToDelegate(Messenger, InDelegate); }
    return ck::UUtils_Signal_Messaging::Get_NumUnconditionalBindingsToDelegate(Messenger, InDelegate);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Messaging_UE::
    Broadcast(
        FCk_Handle& InHandle,
        FGameplayTag InMessageName,
        FInstancedStruct InPayload)
    -> void
{
    if (InPayload.IsValid())
    {
        const auto* ScriptStruct = InPayload.GetScriptStruct();
        const auto HasScriptStruct = ScriptStruct != nullptr;
        CK_ENSURE_IF_NOT(HasScriptStruct,
            TEXT("Message [{}] rejected an InstancedStruct payload without a reflected struct type"), InMessageName)
        {}
        if (NOT HasScriptStruct) { return; }

        const auto Safety = ck::Analyze_UntracedStructSafety(ScriptStruct);
        const auto IsPayloadSafe = Safety.IsGcIndependent();
        CK_ENSURE_IF_NOT(IsPayloadSafe,
            TEXT("Message [{}] rejected unsafe InstancedStruct payload [{}]; [{}]: {}"),
            InMessageName,
            ScriptStruct->GetName(),
            Safety.FailurePath,
            Safety.FailureReason)
        {}
        if (NOT IsPayloadSafe) { return; }
    }

    RecordOfMessengers_Utils::AddIfMissing(InHandle);
    const auto MessengerEntity = DoGet_MessengerEntity(InHandle, InMessageName);

    ck::UUtils_Signal_Messaging::Broadcast(MessengerEntity, ck::MakePayload(InHandle, InMessageName, InPayload));
}

auto
    UCk_Utils_Messaging_UE::
    BindTo_OnBroadcast(
        FCk_Handle& InHandle,
        FGameplayTag InMessageName,
        const FCk_Delegate_Messaging_OnBroadcast& InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    -> void
{
    RecordOfMessengers_Utils::AddIfMissing(InHandle);
    const auto MessengerEntity = DoGet_MessengerEntity(InHandle, InMessageName);

    CK_SIGNAL_BIND(ck::UUtils_Signal_Messaging, MessengerEntity, InDelegate, InBindingPolicy, InPostFireBehavior);
}

auto
    UCk_Utils_Messaging_UE::
    UnbindFrom_OnBroadcast(
        FCk_Handle& InHandle,
        FGameplayTag InMessageName,
        const FCk_Delegate_Messaging_OnBroadcast& InDelegate)
    -> void
{
    const auto& MessengerEntity = RecordOfMessengers_Utils::Get_ValidEntry_ByTag(InHandle, InMessageName);

    if (ck::Is_NOT_Valid(MessengerEntity))
    { return; }

    CK_SIGNAL_UNBIND(ck::UUtils_Signal_Messaging, MessengerEntity, InDelegate);
}

auto
    UCk_Utils_Messaging_UE::
    DoGet_MessengerEntity(
        FCk_Handle& InHandle,
        FGameplayTag InMessageName)
    -> FCk_Handle
{
    auto MessengerEntity = RecordOfMessengers_Utils::Get_ValidEntry_ByTag(InHandle, InMessageName);

    if (ck::Is_NOT_Valid(MessengerEntity))
    {
        MessengerEntity  = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InHandle);
        UCk_Utils_GameplayLabel_UE::Add(MessengerEntity, InMessageName);
        RecordOfMessengers_Utils::Request_Connect(InHandle, MessengerEntity);
    }

    return MessengerEntity;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_MessageListener_UE::
    Stop(
        FCk_Handle_MessageListener& InMessageListener)
    -> void
{
    auto MessageListener = InMessageListener.Get_MessageListener();

    if (ck::Is_NOT_Valid(MessageListener))
    { return; }

    UCk_Utils_Messaging_UE::UnbindFrom_OnBroadcast(
        MessageListener,
        InMessageListener.Get_MessageName(),
        InMessageListener.Get_MessageDelegate());
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
