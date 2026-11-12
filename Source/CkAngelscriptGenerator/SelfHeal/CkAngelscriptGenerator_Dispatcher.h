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

        // The canonical Classify resolved; for an added field, Error.FilePath is the caller instead.
        FString              QuarantineCanonicalPath;
    };

    class CKANGELSCRIPTGENERATOR_API FCkAsRecoveryDispatcher
    {
    public:
        // Bootstrap-only cap; mid-session bypasses it (the user can intervene).
        static constexpr int32 MaxCycles = 3;

        static constexpr int32 MaxPerSignatureRepeats = 3;

        // The single-argument form reads the live canonicals and Script/ trees, only for a caller-side
        // `F<X>_SpawnParams` member error; every other kind classifies without IO.
        static auto Classify       (const FCk_AsParsedError& InError) -> ECk_RecoveryStrategy;
        static auto Classify(
            const FCk_AsParsedError& InError,
            const TArray<FString>&   InCanonicalCandidates,
            const TArray<FString>&   InScanRoots,
            FCk_AsSourceScanCache*   InScanCache,
            FString*                 OutQuarantineCanonical = nullptr) -> ECk_RecoveryStrategy;
        static auto BuildActionPlan(const TArray<FCk_AsParsedError>& InDedupedRoots) -> TArray<FCk_RecoveryAction>;

        // The one canonical defining `F<X>_SpawnParams` that predates a member `U<X>`'s source declares
        // ExposeOnSpawn, or empty (a genuine authoring error). Read-only, so safe inside OnReloadHadErrors.
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
