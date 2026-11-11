#pragma once

#include "CoreMinimal.h"

#include "CkAngelscriptGenerator/SelfHeal/CkAngelscriptGenerator_AsErrorParser.h"
#include "CkAngelscriptGenerator/SelfHeal/CkAngelscriptGenerator_AsSourceScanner.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::angelscriptgenerator::self_heal
{
    enum class ECk_RecoveryStrategy : uint8
    {
        SynthesizeStub_EntitySpawnParams,
        KickGenerator_DynamicHandle,
        KickGenerator_AssetRegistry,
        Quarantine_StaleEspCanonical,
        Author_FixupRequired_AdjacentStringLiteral,
        Unrecognized,
    };

    struct CKANGELSCRIPTGENERATOR_API FCk_RecoveryAction
    {
        ECk_RecoveryStrategy Strategy = ECk_RecoveryStrategy::Unrecognized;
        FCk_AsParsedError    Error;

        // For Quarantine_StaleEspCanonical, this is the canonical Classify resolved. For an
        // added field the error is at the caller, so Error.FilePath is not this path.
        FString              QuarantineCanonicalPath;
    };

    class CKANGELSCRIPTGENERATOR_API FCkAsRecoveryDispatcher
    {
    public:
        // Bootstrap-only cap; mid-session bypasses it (the user can intervene).
        static constexpr int32 MaxCycles = 3;

        static constexpr int32 MaxPerSignatureRepeats = 3;

        // The single-argument form probes the live ESP canonicals and Script/ trees,
        // and only for a caller-side `'<Member>' is not a member of 'F<X>_SpawnParams'`.
        // Every other error kind classifies without IO.
        static auto Classify       (const FCk_AsParsedError& InError) -> ECk_RecoveryStrategy;
        static auto Classify(
            const FCk_AsParsedError& InError,
            const TArray<FString>&   InCanonicalCandidates,
            const TArray<FString>&   InScanRoots,
            FCk_AsSourceScanCache*   InScanCache,
            FString*                 OutQuarantineCanonical = nullptr) -> ECk_RecoveryStrategy;
        static auto BuildActionPlan(const TArray<FCk_AsParsedError>& InDedupedRoots) -> TArray<FCk_RecoveryAction>;

        // Added-field twin of the deleted-field quarantine: a caller sets a field
        // that `U<X>`'s source declares ExposeOnSpawn, but the one present
        // `*_EntitySpawnParams.as` canonical defining `F<X>_SpawnParams` predates it.
        // Returns that canonical, or empty when any link of the evidence is missing.
        // An empty result means a genuine authoring error, which must stay Unrecognized.
        // Read-only, so it is safe inside OnReloadHadErrors.
        static auto Find_StaleCanonical_ForAddedField(
            const FCk_AsParsedError& InError,
            const TArray<FString>&   InCanonicalCandidates,
            const TArray<FString>&   InScanRoots,
            FCk_AsSourceScanCache*   InScanCache = nullptr) -> FString;

        // Hook handler for FAngelscriptCodeModule::GetReloadHadErrors. Queues only —
        // applying strategies here is invisible to the hot-reload checker thread
        // (CkAngelscriptGenerator/CLAUDE.md § The modal-tick deferral).
        static auto OnAngelscriptReloadHadErrors() -> void;

        static auto Get_CyclesRun  () -> int32;
        static auto Reset_CyclesRun() -> void;

        // Drops queued-but-undrained actions and disarms the deferred drains. The
        // per-signature convergence trackers and blacklist deliberately survive.
        static auto Clear_PendingRecoveryState() -> void;

        static auto Is_BootstrapMode     () -> bool;
        static auto Mark_BootstrapComplete() -> void;

        static auto Did_SynthesizeJsonStub_ThisSession() -> bool;
        static auto Mark_JsonStubSynthesized          () -> void;

        static auto Did_SynthesizeAssetRegistryStub_ThisSession() -> bool;
        static auto Mark_AssetRegistryStubSynthesized          () -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------
