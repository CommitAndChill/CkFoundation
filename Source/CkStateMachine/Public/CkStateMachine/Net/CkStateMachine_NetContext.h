#pragma once
#include "CkCore/Format/CkFormat.h"


#include "CkCore/Macros/CkMacros.h"

#include "GameplayTagContainer.h"

#include "CkStateMachine_NetContext.generated.h"

class UCk_SmState_EntityScript;

// --------------------------------------------------------------------------------------------------------------------

// Per-machine role context passed into every State / Task / Condition lifecycle method.
// Users switch on this to branch behavior by role.
UENUM(BlueprintType)
enum class ECk_Sm_NetContext : uint8
{
    Standalone,        // Single-player / no net session
    Server,            // Authority on a networked server
    OwningClient,      // Client owning this SM's actor (player on their own machine)
    NonOwningClient    // Client observing someone else's SM
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Sm_NetContext);

// --------------------------------------------------------------------------------------------------------------------

// Per-SM choice of who drives transitions. Immutable for the SM's lifetime; an SM is either
// ServerAuth or OwningClientAuth, never mixed. AutoDetect (the default) resolves from the SM host's
// net ownership at use-time — always resolve via UCk_Utils_StateMachine_UE::Get_EffectiveAuthorityModel,
// which never returns AutoDetect.
UENUM(BlueprintType)
enum class ECk_Sm_AuthorityModel : uint8
{
    AutoDetect,                  // Default. Resolved from host ownership at use-time (see Get_EffectiveAuthorityModel).
    ServerAuthoritative,         // Server evaluates conditions, drives transitions.
    OwningClientAuthoritative    // Owning client evaluates locally, pushes to server via RPC.
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Sm_AuthorityModel);

// --------------------------------------------------------------------------------------------------------------------

// Per-SM choice of replication payload shape. Immutable for the SM's lifetime.
UENUM(BlueprintType)
enum class ECk_Sm_ReplicationModel : uint8
{
    WithHistory,       // Default. Rolling window of transition events; replays A→B→C in order.
    WithoutHistory     // Latest state only. Snap-to-current. For high-frequency / cosmetic SMs.
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Sm_ReplicationModel);

// --------------------------------------------------------------------------------------------------------------------

// One transition event in the replicated history. Carries previous → new + monotonic seq +
// the new state's structural fingerprint (for spec §9 determinism check).
USTRUCT(BlueprintType)
struct CKSTATEMACHINE_API FCk_Sm_TransitionEvent
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Sm_TransitionEvent);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        meta = (AllowPrivateAccess = true))
    TSubclassOf<UCk_SmState_EntityScript> _PreviousStateClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        meta = (AllowPrivateAccess = true))
    TSubclassOf<UCk_SmState_EntityScript> _NewStateClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        meta = (AllowPrivateAccess = true))
    int32 _Seq = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        meta = (AllowPrivateAccess = true))
    int32 _NewStateFingerprint = 0;

    // Cross-machine address of the sub-SM this event belongs to (ck::statemachine::FSubSmAddress),
    // as two parallel arrays. EMPTY means a root-level event. Non-empty means the event was relayed
    // through the root SM's transport on behalf of a non-replicated sub-SM, and the receiver must
    // dispatch it to the local sub-SM at this address (UCk_Utils_StateMachine_UE::TryFind_ActiveSubSm_ByAddress).
    //   _SubSmIdentity - the hosting state's tag path, root -> leaf (FFragment_Sm_ParentHierarchy).
    //   _SubSmOrdinals - per level, the sub-SM task's index among its hosting state's sub-SM tasks. EMPTY
    //                    with a non-empty identity is the shape written before ordinals existed; it reads
    //                    as ordinal 0 per level.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        meta = (AllowPrivateAccess = true))
    TArray<FGameplayTag> _SubSmIdentity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        meta = (AllowPrivateAccess = true))
    TArray<uint8> _SubSmOrdinals;

public:
    CK_PROPERTY(_PreviousStateClass);
    CK_PROPERTY(_NewStateClass);
    CK_PROPERTY(_Seq);
    CK_PROPERTY(_NewStateFingerprint);
    CK_PROPERTY(_SubSmIdentity);
    CK_PROPERTY(_SubSmOrdinals);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Sm_TransitionEvent, _PreviousStateClass, _NewStateClass, _Seq, _NewStateFingerprint);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::statemachine
{
    // A sub-SM's cross-machine address. One entry per nesting level, root -> leaf:
    //   _Hierarchy - the hosting state's tag (the hosting state's hierarchy, FFragment_Sm_ParentHierarchy)
    //   _Ordinals  - which of that state's sub-SM tasks hosts the level below: the task's index among the
    //                state's UCk_SmTask_SubStateMachine tasks, in definition order
    // Unique among the live sub-SMs under one root: two sub-SM tasks on one state differ by ordinal, and
    // nested levels carry every level's ordinal. Identical on every machine without extra replication,
    // because DefineState output is (fingerprint-enforced). Empty for a root SM.
    struct CKSTATEMACHINE_API FSubSmAddress
    {
    public:
        CK_GENERATED_BODY(FSubSmAddress);

    public:
        // Reads the shape written before ordinals existed (a tag path, no ordinals) as ordinal 0 per level.
        static auto
        Make(
            const TArray<FGameplayTag>& InHierarchy,
            const TArray<uint8>& InOrdinals) -> FSubSmAddress;

        static auto
        Make(
            const FCk_Sm_TransitionEvent& InEvent) -> FSubSmAddress;

        auto
        Get_IsRoot() const -> bool;

        auto
        operator==(
            const FSubSmAddress& InOther) const -> bool;

        // "(A#0 > B#1)"; "(root)" for an empty address.
        auto
        ToString() const -> FString;

        auto
        Request_WriteTo(
            FCk_Sm_TransitionEvent& InEvent) const -> void;

    private:
        TArray<FGameplayTag> _Hierarchy;
        TArray<uint8> _Ordinals;

    public:
        CK_PROPERTY_GET(_Hierarchy);
        CK_PROPERTY_GET(_Ordinals);
    };
}

// --------------------------------------------------------------------------------------------------------------------
