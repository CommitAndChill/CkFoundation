#include "CkCueSubsystem_Base.h"

#include "UObject/UObjectIterator.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/GameplayTag/CkGameplayTag_Utils.h"
#include "CkCore/Math/Arithmetic/CkArithmetic_Utils.h"
#include "CkCore/Debug/CkDebug_Utils.h"
#include "CkCore/IO/CkIO_Utils.h"
#include "CkCore/Reflection/CkReflection_Utils.h"
#include "CkCore/Validation/CkUntracedStructSafety.h"

#include "CkCue/CkCue_Fragment.h"
#include "CkCue/CkCue_Log.h"
#include "CkCue/Settings/CkCue_Settings.h"
#include "CkEcs/EntityConstructionScript/CkEntity_ConstructionScript.h"

#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkEcs/EntityBridge/CkEntityBridge_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include <AssetRegistry/AssetRegistryModule.h>
#include <Net/UnrealNetwork.h>
#include <Net/Core/PushModel/PushModel.h>
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/PlayerState.h>

#if WITH_EDITOR
#include <Editor.h>
#include "Engine/Blueprint.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand ConsoleCommand_PrintCues(
    TEXT("ck.cue.print"),
    TEXT("Print all discovered cues from all cue subsystems"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        int32 SubsystemCount = 0;
        int32 TotalCues = 0;

        for (TObjectIterator<UCk_CueSubsystem_Base_UE> It; It; ++It)
        {
            auto CueSubsystem = *It;
            if (ck::Is_NOT_Valid(CueSubsystem))
            { continue; }

            SubsystemCount++;
            const auto& DiscoveredCues = CueSubsystem->Get_DiscoveredCues();

            ck::cue::Log(TEXT("=== SUBSYSTEM: [{}] ==="), CueSubsystem->GetClass()->GetName());
            ck::cue::Log(TEXT("Discovered cues: [{}]"), DiscoveredCues.Num());

            for (const auto& [CueName, CueClass] : DiscoveredCues)
            {
                ck::cue::Log(TEXT("  [{}] -> [{}]"), CueName, CueClass->GetName());
                TotalCues++;
            }
        }

        ck::cue::Log(TEXT("=== SUMMARY ==="));
        ck::cue::Log(TEXT("Total subsystems: [{}], Total cues: [{}]"), SubsystemCount, TotalCues);

        if (SubsystemCount == 0)
        {
            ck::cue::Warning(TEXT("No CueSubsystems found!"));
        }
    })
);

static FAutoConsoleCommand ConsoleCommand_RediscoverCues(
    TEXT("ck.cue.rediscover"),
    TEXT("Force re-discovery of all cues in all cue subsystems"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        int32 SubsystemCount = 0;

        for (TObjectIterator<UCk_CueSubsystem_Base_UE> It; It; ++It)
        {
            auto CueSubsystem = *It;
            if (ck::Is_NOT_Valid(CueSubsystem))
            { continue; }

            SubsystemCount++;
            ck::cue::Log(TEXT("Re-discovering cues for subsystem: [{}]"), CueSubsystem->GetClass()->GetName());
            CueSubsystem->Request_PopulateAllCues();
            ck::cue::Log(TEXT("  Found [{}] cues"), CueSubsystem->Get_DiscoveredCues().Num());
        }

        if (SubsystemCount == 0)
        {
            ck::cue::Warning(TEXT("No CueSubsystems found!"));
        }
        else
        {
            ck::cue::Log(TEXT("Re-discovery complete for [{}] subsystems"), SubsystemCount);
        }
    })
);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_cue_subsystem_base
{
    auto Get_CueSubsystemFromClass(TSubclassOf<UCk_CueSubsystem_Base_UE> InCueSubsystemClass) -> UCk_CueSubsystem_Base_UE*;
    auto ValidateRetainedSpawnParams(const FGameplayTag& InCueName, const FInstancedStruct& InSpawnParams) -> bool;

    auto
        ExecuteCueEntityScript_Internal(
            FCk_Handle InOwnerEntity,
            const FGameplayTag& InCueName,
            TSubclassOf<UCk_CueBase_EntityScript> InCueClass,
            const FInstancedStruct& InSpawnParams,
            bool InAuthenticatedPlayerOwner)
        -> FCk_Handle_PendingEntityScript
    {
        CK_ENSURE_IF_NOT(ck::IsValid(InCueClass),
            TEXT("CueClass was INVALID when trying to execute Cue [{}]"), InCueName)
        { return {}; }

        auto CueDefaultObject = InCueClass->GetDefaultObject<UCk_CueBase_EntityScript>();

        if (CueDefaultObject->Get_RequireAuthenticatedPlayerOwner() != InAuthenticatedPlayerOwner)
        {
            ck::cue::Warning(TEXT("Rejected cue [{}]: authenticated player ingress policy mismatch"), InCueName);
            return {};
        }

        if (ck::Is_NOT_Valid(InOwnerEntity))
        {
            if (ck::IsValid(CueDefaultObject) &&
                CueDefaultObject->Get_OwnerValidationPolicy() == ECk_Cue_OwnerValidationPolicy::RequireValid)
            {
                CK_ENSURE_IF_NOT(false,
                    TEXT("OwnerEntity is invalid when trying to execute Cue [{}] (OwnerValidationPolicy = RequireValid)"), InCueName)
                { return {}; }
            }

            ck::cue::Verbose(TEXT("Skipping cue [{}] - OwnerEntity is invalid (OwnerValidationPolicy = SkipIfInvalid)"), InCueName);
            return {};
        }

        if (ck::IsValid(CueDefaultObject) &&
            CueDefaultObject->Get_ConcurrencyPolicy() == ECk_Cue_ConcurrencyPolicy::RestartExisting)
        {
            if (auto ExistingCue = ck::ActiveCues_Utils::Get_ValidEntry_ByTag(InOwnerEntity, InCueName);
                ck::IsValid(ExistingCue))
            {
                if (const auto CueScript = Cast<UCk_CueBase_EntityScript>(ExistingCue.Get<ck::FFragment_EntityScript>().Get_Script().Get());
                    ck::IsValid(CueScript))
                {
                    ck::cue::Verbose(TEXT("Restarting existing cue [{}] on entity [{}]"), InCueName, InOwnerEntity);
                    UCk_Utils_EntityScript_UE::TryInjectEntityScriptSpawnParams(CueScript, InSpawnParams);
                    CueScript->Restart();
                    return FCk_Handle_PendingEntityScript{ExistingCue};
                }
            }
        }

        ck::cue::Verbose(TEXT("Executing cue [{}] on entity [{}]"), InCueName, InOwnerEntity);
        auto PendingEntityScript = UCk_Utils_EntityScript_UE::Request_SpawnEntity(InOwnerEntity, InCueClass, InSpawnParams, {});

        return PendingEntityScript;
    }

    auto
        ExecuteCueEntityScript(
            FCk_Handle InOwnerEntity,
            const FGameplayTag& InCueName,
            TSubclassOf<UCk_CueBase_EntityScript> InCueClass,
            const FInstancedStruct& InSpawnParams)
        -> FCk_Handle_PendingEntityScript
    {
        return ExecuteCueEntityScript_Internal(InOwnerEntity, InCueName, InCueClass, InSpawnParams, false);
    }

    auto
        ExecuteCueForPlayer(
            UCk_CueExecutor_Subsystem_Base_UE* InExecutor,
            APlayerState* InPlayerState,
            const FGameplayTag& InCueName,
            const FInstancedStruct& InSpawnParams)
        -> FCk_Handle_PendingEntityScript
    {
        if (ck::Is_NOT_Valid(InExecutor) || ck::Is_NOT_Valid(InPlayerState) || NOT InCueName.IsValid())
        { return {}; }

        if (InSpawnParams.IsValid())
        {
            const auto* ParamsType = InSpawnParams.GetScriptStruct();
            if (ParamsType == nullptr || InSpawnParams.GetMemory() == nullptr ||
                NOT ck::Analyze_UntracedStructSafety(ParamsType).IsGcIndependent())
            {
                ck::cue::Warning(TEXT("Rejected player cue [{}]: unsafe retained command payload"), InCueName);
                return {};
            }
        }

        auto Pawn = InPlayerState->GetPawn();
        if (ck::Is_NOT_Valid(Pawn) || Pawn->GetWorld() != InExecutor->GetWorld() ||
            NOT Pawn->HasAuthority() || ck::Is_NOT_Valid(Pawn->GetController()) ||
            Pawn->GetPlayerState() != InPlayerState)
        {
            ck::cue::Warning(TEXT("Rejected player cue [{}]: player has no current authoritative pawn"), InCueName);
            return {};
        }
        const auto* Controller = Cast<APlayerController>(Pawn->GetController());
        if (ck::Is_NOT_Valid(Controller) || Controller->PlayerState != InPlayerState)
        {
            ck::cue::Warning(TEXT("Rejected player cue [{}]: pawn is no longer possessed by the relay player"), InCueName);
            return {};
        }

        const auto Owner = UCk_Utils_EntityBridge_UE::TryGet_Entity(Pawn);
        if (ck::Is_NOT_Valid(Owner))
        {
            ck::cue::Warning(TEXT("Rejected player cue [{}]: current pawn has no ready entity"), InCueName);
            return {};
        }

        auto CueSubsystem = Get_CueSubsystemFromClass(InExecutor->Get_CueSubsystemClass());
        if (ck::Is_NOT_Valid(CueSubsystem))
        { return {}; }

        const auto* CueClass = CueSubsystem->Get_DiscoveredCues().Find(InCueName);
        if (CueClass == nullptr || ck::Is_NOT_Valid(*CueClass))
        {
            ck::cue::Warning(TEXT("Rejected unknown player cue [{}]"), InCueName);
            return {};
        }

        auto* Defaults = (*CueClass)->GetDefaultObject<UCk_CueBase_EntityScript>();
        if (InSpawnParams.IsValid())
        {
            for (TFieldIterator<FProperty> It(InSpawnParams.GetScriptStruct()); It; ++It)
            {
                const auto* Source = *It;
                const auto Name = UCk_Utils_Reflection_UE::Get_SanitizedUserDefinedPropertyName(Source);
                const auto* Target = UCk_Utils_Reflection_UE::Get_PropertyBySanitizedName(Defaults, Name);
                if (Target == nullptr || NOT Target->HasAnyPropertyFlags(CPF_ExposeOnSpawn) ||
                    NOT Source->SameType(Target))
                {
                    ck::cue::Warning(TEXT("Rejected player cue [{}]: field [{}] is not a matching exposed command parameter"), InCueName, Name);
                    return {};
                }
            }
        }

        return ExecuteCueEntityScript_Internal(Owner, InCueName, *CueClass, InSpawnParams, true);
    }

    auto
        Get_CueSubsystemFromClass(
            TSubclassOf<UCk_CueSubsystem_Base_UE> InCueSubsystemClass)
        -> UCk_CueSubsystem_Base_UE*
    {
        CK_ENSURE_IF_NOT(ck::IsValid(GEngine),
            TEXT("GEngine is invalid when trying to get CueSubsystem"))
        { return {}; }

        CK_ENSURE_IF_NOT(ck::IsValid(InCueSubsystemClass),
            TEXT("CueSubsystemClass is invalid"))
        { return {}; }

        return Cast<UCk_CueSubsystem_Base_UE>(GEngine->GetEngineSubsystemBase(InCueSubsystemClass));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GenericCueExecutor_Subsystem_UE::
    Get_GroupTag() const
    -> FGameplayTag
{
    return UCk_Utils_GameplayTag_UE::ResolveGameplayTag(TEXT("ActorRelay.Cue.Generic"));
}

auto
    UCk_GenericCueExecutor_Subsystem_UE::
    Get_CueSubsystemClass() const
    -> TSubclassOf<UCk_CueSubsystem_Base_UE>
{
    return UCk_GenericCueSubsystem_UE::StaticClass();
}

auto
    UCk_GenericCueExecutor_Subsystem_UE::
    Get_DedicatedServerPolicy() const
    -> ECk_Cue_DedicatedServerPolicy
{
    return ECk_Cue_DedicatedServerPolicy::GameplayRelevant;
}

auto
    UCk_GenericCueSubsystem_UE::
    Get_CueBaseClass() const
    -> TSubclassOf<UCk_CueBase_EntityScript>
{
    return UCk_GenericCue_EntityScript::StaticClass();
}
