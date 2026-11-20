#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkEcs/Handle/CkHandle.h"

#include <Templates/Function.h>

class UWorld;
class UScriptStruct;

// Transport-neutral construction disposition. These declarations carry no saved payload or feature schema.
namespace ck::persistence
{
    enum class EInitializationDisposition : uint8
    {
        Undeclared,
        Fresh,
        IdentityUnresolved,
        BaselineExpected,
        BaselineCommitted,
        Released,
        Unavailable
    };

    enum class EInitializationFailure : uint8
    {
        None,
        MissingBaseline,
        Rejected,
        TimedOut,
        Cancelled,
        Dropped
    };

    struct CKECS_API FInitializationStatus
    {
        CK_GENERATED_BODY(FInitializationStatus);

    private:
        uint64 _Epoch = 0;
        EInitializationDisposition _Disposition = EInitializationDisposition::Undeclared;
        EInitializationFailure _Failure = EInitializationFailure::None;
        bool _IsReleased = false;

    public:
        CK_PROPERTY_GET(_Epoch);
        CK_PROPERTY_GET(_Disposition);
        CK_PROPERTY_GET(_Failure);
        CK_PROPERTY_GET(_IsReleased);
        CK_DEFINE_CONSTRUCTORS(FInitializationStatus, _Epoch, _Disposition, _Failure);
        CK_DEFINE_CONSTRUCTOR(FInitializationStatus, _Epoch, _Disposition, _Failure, _IsReleased);
    };

    enum class EInitializationIdentity : uint8
    {
        Automatic,
        Fresh,
        SaveKey,
        OwnedLabel,
        Player
    };

    // Existing rendezvous identities may be supplied before the feature becomes public. Fresh explicitly
    // excludes this entity/type from adoption for the active epoch. Automatic never asserts freshness.
    struct CKECS_API FInitializationIdentity
    {
        EInitializationIdentity _Kind = EInitializationIdentity::Automatic;
        FGuid _SaveKey;
        FCk_Handle _Owner;
        FString _Label;
        FString _PlayerId;
        int32 _Occurrence = 0;
    };

    using FInitializationResolver = TFunction<void(FCk_Handle&, const UScriptStruct*, const FInitializationIdentity&)>;
    using FInitializationChanged = TFunction<void(FCk_Handle, const FInitializationStatus&)>;

    CKECS_API auto Query(const FCk_Handle& InEntity, const UScriptStruct* InType) -> FInitializationStatus;
    CKECS_API auto GetWorldTransient(const FCk_Handle& InEntity) -> FCk_Handle;

    // Only Fresh, IdentityUnresolved and BaselineExpected can be declared. Epoch zero is reserved for Fresh
    // outside a load. A newer epoch replaces a prior ticket; stale or contradictory deliveries never mutate it.
    CKECS_API auto TryDeclare(FCk_Handle& InEntity, const UScriptStruct* InType, uint64 InEpoch,
        EInitializationDisposition InDisposition) -> bool;
    CKECS_API auto TryAccept(FCk_Handle& InEntity, const UScriptStruct* InType, uint64 InEpoch) -> bool;
    CKECS_API auto TryRelease(FCk_Handle& InEntity, const UScriptStruct* InType, uint64 InEpoch) -> bool;
    CKECS_API auto TryDiscard(FCk_Handle& InEntity, const UScriptStruct* InType, uint64 InEpoch,
        EInitializationFailure InFailure) -> bool;

    // Named subscriptions belong to the entity generation. Duplicate names and rebinding a currently executing
    // name reject unchanged; unsubscribe before replacement. Callbacks must retain external UObjects weakly.
    // A new subscription reports its current state immediately (or at batch exit), then observes transitions.
    CKECS_API auto Subscribe(FCk_Handle& InEntity, const UScriptStruct* InType, FName InSubscriber,
        FInitializationChanged InCallback) -> bool;
    CKECS_API auto Unsubscribe(FCk_Handle& InEntity, const UScriptStruct* InType, FName InSubscriber) -> bool;

    CKECS_API auto InstallResolver(FCk_Handle& InWorldTransient, uint64 InEpoch,
        FInitializationResolver InResolver) -> bool;
    CKECS_API auto RemoveResolver(FCk_Handle& InWorldTransient, uint64 InEpoch) -> bool;

    // Module-owned bootstrap installs the destination world's resolver even before OnWorldBeginPlay.
    // This callback contains no load state; each installed resolver belongs to one world's transient entity.
    CKECS_API auto SetResolverBootstrap(TFunction<void(UWorld&, FCk_Handle&)> InBootstrap) -> void;
    CKECS_API auto RequestInitialization(FCk_Handle& InEntity, const UScriptStruct* InType,
        const FInitializationIdentity& InIdentity = {}) -> FInitializationStatus;

    // Complete the entire world's state transition before delivering any observer edge. Nested scopes share
    // the same queue; each entity/type is delivered once with its final state when the outer scope leaves.
    class CKECS_API FScopedInitializationNotifications
    {
    public:
        explicit FScopedInitializationNotifications(const FCk_Handle& InWorldTransient);
        ~FScopedInitializationNotifications();
        FScopedInitializationNotifications(const FScopedInitializationNotifications&) = delete;
        auto operator=(const FScopedInitializationNotifications&) -> FScopedInitializationNotifications& = delete;

    private:
        FCk_Handle _Transient;
    };
}
