#include "CkGameSettings_Subsystem.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkCVar/Utils/CkCVar_Utils.h"

#include "CkGameSettings/CkGameSettings_Log.h"
#include "CkGameSettings/Collection/CkGameSettings_Collection.h"
#include "CkGameSettings/Packs/CkGameSettings_AudioPack.h"
#include "CkGameSettings/Packs/CkGameSettings_VideoPack.h"
#include "CkGameSettings/Settings/CkGameSettings_Settings.h"
#include "CkGameSettings/Storage/CkGameSettings_IniStorageProvider.h"

#include <Engine/Engine.h>
#include <Engine/GameInstance.h>
#include <GameFramework/GameUserSettings.h>
#include <Misc/CoreDelegates.h>
#include <Templates/UnrealTemplate.h>
#include <UObject/UObjectGlobals.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_game_settings_cvars
{
    static float DeferredApplyTimeoutSecs = 30.0f;
    static FAutoConsoleVariableRef CVarDeferredApplyTimeoutSecs(
        TEXT("ck.GameSettings.DeferredApplyTimeoutSecs"),
        DeferredApplyTimeoutSecs,
        TEXT("How long a stored CVar-bound GameSettings value may wait for its CVar to register before it is dropped loudly. The stored value itself is never deleted."),
        ECVF_Default);
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_game_settings_subsystem
{
    auto
        TryParse_Bool(
            const FString& InString,
            bool& OutValue)
        -> bool
    {
        if (InString.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
            InString.Equals(TEXT("yes"), ESearchCase::IgnoreCase) ||
            InString.Equals(TEXT("on"), ESearchCase::IgnoreCase) ||
            InString.Equals(TEXT("1")))
        {
            OutValue = true;
            return true;
        }

        if (InString.Equals(TEXT("false"), ESearchCase::IgnoreCase) ||
            InString.Equals(TEXT("no"), ESearchCase::IgnoreCase) ||
            InString.Equals(TEXT("off"), ESearchCase::IgnoreCase) ||
            InString.Equals(TEXT("0")))
        {
            OutValue = false;
            return true;
        }

        return false;
    }

    auto
        TryParse_Int32(
            const FString& InString,
            int32& OutValue)
        -> bool
    {
        return LexTryParseString(OutValue, *InString);
    }

    auto
        TryParse_Float(
            const FString& InString,
            float& OutValue)
        -> bool
    {
        return LexTryParseString(OutValue, *InString);
    }

    auto
        Get_IsParseableAs(
            const FString& InString,
            ECk_GameSettings_ValueType InValueType)
        -> bool
    {
        switch (InValueType)
        {
            case ECk_GameSettings_ValueType::Bool:
            {
                auto Value = false;
                return TryParse_Bool(InString, Value);
            }
            case ECk_GameSettings_ValueType::Int32:
            {
                auto Value = int32{};
                return TryParse_Int32(InString, Value);
            }
            case ECk_GameSettings_ValueType::Float:
            {
                auto Value = float{};
                return TryParse_Float(InString, Value);
            }
            case ECk_GameSettings_ValueType::String:
            {
                return true;
            }
            default:
            {
                CK_INVALID_ENUM(InValueType);
                return false;
            }
        }
    }

    auto
        Get_ValuesEqual(
            ECk_GameSettings_ValueType InValueType,
            const FString& InA,
            const FString& InB)
        -> bool
    {
        switch (InValueType)
        {
            case ECk_GameSettings_ValueType::Bool:
            {
                auto A = false;
                auto B = false;
                if (TryParse_Bool(InA, A) && TryParse_Bool(InB, B))
                { return A == B; }
                return InA.Equals(InB);
            }
            case ECk_GameSettings_ValueType::Int32:
            {
                auto A = int32{};
                auto B = int32{};
                if (TryParse_Int32(InA, A) && TryParse_Int32(InB, B))
                { return A == B; }
                return InA.Equals(InB);
            }
            case ECk_GameSettings_ValueType::Float:
            {
                auto A = float{};
                auto B = float{};
                if (TryParse_Float(InA, A) && TryParse_Float(InB, B))
                { return A == B; }
                return InA.Equals(InB);
            }
            case ECk_GameSettings_ValueType::String:
            {
                return InA.Equals(InB);
            }
            default:
            {
                CK_INVALID_ENUM(InValueType);
                return InA.Equals(InB);
            }
        }
    }

    template <typename T, typename TParseFunc>
    auto
        Get_MatchesAnyOption(
            const TArray<FCk_GameSettings_SettingOption>& InOptions,
            const T& InValue,
            TParseFunc InParse)
        -> bool
    {
        return ck::algo::AnyOf(InOptions, [&](const FCk_GameSettings_SettingOption& InOption)
        {
            auto ParsedOption = T{};
            return InParse(InOption.Get_Value(), ParsedOption) && ParsedOption == InValue;
        });
    }

    auto
        DoWriteCVar(
            const FCk_CVarRef& InCVar,
            ECk_GameSettings_ValueType InValueType,
            const FString& InValueString)
        -> void
    {
        switch (InValueType)
        {
            case ECk_GameSettings_ValueType::Bool:
            {
                auto Value = false;
                if (TryParse_Bool(InValueString, Value))
                { UCk_Utils_CVar_UE::INTERNAL_Set_Bool(InCVar, Value); }
                return;
            }
            case ECk_GameSettings_ValueType::Int32:
            {
                auto Value = int32{};
                if (TryParse_Int32(InValueString, Value))
                { UCk_Utils_CVar_UE::INTERNAL_Set_Int32(InCVar, Value); }
                return;
            }
            case ECk_GameSettings_ValueType::Float:
            {
                auto Value = float{};
                if (TryParse_Float(InValueString, Value))
                { UCk_Utils_CVar_UE::INTERNAL_Set_Float(InCVar, Value); }
                return;
            }
            case ECk_GameSettings_ValueType::String:
            {
                UCk_Utils_CVar_UE::INTERNAL_Set_String(InCVar, InValueString);
                return;
            }
            default:
            {
                CK_INVALID_ENUM(InValueType);
                return;
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Initialize(
        FSubsystemCollectionBase& Collection)
    -> void
{
    _IsPieInstance = DoGet_IsPieWorldContext(GetGameInstance());

    auto* ResolvedProviderClass = UCk_Utils_GameSettings_Settings_UE::Get_StorageProviderClass().LoadSynchronous();
    CK_ENSURE_IF_NOT(ck::IsValid(ResolvedProviderClass) && NOT ResolvedProviderClass->HasAnyClassFlags(CLASS_Abstract), TEXT("GameSettings StorageProviderClass is unset, unloadable, or abstract — falling back to the ini provider"))
    { ResolvedProviderClass = UCk_GameSettings_IniStorageProvider_UE::StaticClass(); }

    _StorageProvider = NewObject<UCk_GameSettings_StorageProvider_UE>(this, ResolvedProviderClass);

    const auto LoadedCount = DoAbsorbStoredValues();
    ck::game_settings::Display(TEXT("GameSettings boot stored-value result [{}] (-1 means rejected)"), LoadedCount);

    if (_IsPieInstance)
    {
        ck::game_settings::Display(TEXT("GameSettings PIE instance: CVar-bound writes are skipped; the store and Get_SettingValue remain authoritative"));
    }

    if (UCk_Utils_GameSettings_Settings_UE::Get_EnableAudioPack())
    { Request_RegisterAudioPack(); }

    if (UCk_Utils_GameSettings_Settings_UE::Get_EnableVideoPack())
    { Request_RegisterVideoPack(); }

    DoStartTicker();

    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddWeakLambda(this, [this]
    {
        DoFlush();
    });
    _AppDeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddWeakLambda(this, [this]
    {
        DoFlush();
    });
    FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisType::DoHandlePreLoadMap);
}

auto
    UCk_GameSettings_Subsystem_UE::
    Deinitialize()
    -> void
{
    if (_PendingSessionActive)
    { Request_RevertPendingChanges(); }

    DoFlush();

    if (_TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_TickerHandle);
        _TickerHandle.Reset();
    }
    FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
    FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(_AppDeactivateHandle);
    FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);

    for (const auto& PackObject : _PackHandlerObjects)
    {
        if (auto* AudioHandler = Cast<UCk_GameSettings_AudioCategoryHandler_UE>(PackObject))
        { AudioHandler->Shutdown_Handler(); }
    }
    _PackHandlerObjects.Empty();
    _ResolutionConfirmActive = false;

    _DeferredCVarApplies.Empty();
    _OrphanValues_Machine.Empty();
    _OrphanValues_Player0.Empty();
    _ApplyHandlers_Bool.Empty();
    _ApplyHandlers_Int32.Empty();
    _ApplyHandlers_Float.Empty();
    _ApplyHandlers_String.Empty();
    _ExternalGetters_Bool.Empty();
    _ExternalGetters_Int32.Empty();
    _ExternalGetters_Float.Empty();
    _ExternalGetters_String.Empty();
    _PendingPriorValues.Empty();
    _ChangeBindings.Empty();
    _Definitions.Empty();
    _CurrentValues.Empty();
    _RegistrationOrder.Empty();
}

auto
    UCk_GameSettings_Subsystem_UE::
    ShouldCreateSubsystem(
        UObject* InOuter) const
    -> bool
{
    return NOT IsRunningDedicatedServer();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterSetting(
        const FCk_GameSettings_SettingDefinition& InDefinition)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    return Request_RegisterSettings(TArray<FCk_GameSettings_SettingDefinition>{InDefinition});
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterSettings(
        const TArray<FCk_GameSettings_SettingDefinition>& InDefinitions)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }
#if NOT CK_DISABLE_ENSURE_CHECKS
    const auto MutationGuard = TGuardValue<bool>{_MutationInProgress, true};
#endif
    const auto Definitions = InDefinitions;
#if NOT CK_DISABLE_ENSURE_CHECKS
    auto Keys = TSet<FName>{};
#endif
    auto InitialValues = TMap<FName, FString>{};
    for (const auto& Definition : Definitions)
    {
#if NOT CK_DISABLE_ENSURE_CHECKS
        const auto ValidationFailure = DoFindValidationFailure(Definition);
        CK_ENSURE_IF_NOT(NOT ValidationFailure.IsSet(), TEXT("Invalid GameSettings definition, nothing was registered: {}"), ValidationFailure.GetValue())
        { return false; }
#endif
        const auto Key = Definition.Get_Key();
        CK_ENSURE_IF_NOT(NOT _Definitions.Contains(Key) && NOT Keys.Contains(Key), TEXT("Setting key [{}] is already registered"), Key)
        { return false; }
#if NOT CK_DISABLE_ENSURE_CHECKS
        Keys.Add(Key);
#endif
        if (Definition.Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider)
        {
            const auto& Orphans = Definition.Get_Scope() == ECk_GameSettings_Scope::Machine ? _OrphanValues_Machine : _OrphanValues_Player0;
            if (const auto* Orphan = Orphans.Find(Key))
            {
                if (NOT DoGet_IsValueValid(Definition, *Orphan))
                {
                    ck::game_settings::Error(TEXT("Invalid stored value for GameSettings key [{}], nothing was registered"), Key);
                    return false;
                }
                InitialValues.Add(Key, *Orphan);
            }
        }
    }
    for (const auto& Definition : Definitions)
    {
        const auto Key = Definition.Get_Key();
        _Definitions.Add(Key, Definition);
        _CurrentValues.Add(Key, InitialValues.Contains(Key) ? InitialValues.FindChecked(Key) : Definition.Get_DefaultValue());
        _RegistrationOrder.Add(Key);
        if (InitialValues.Contains(Key))
        {
            auto& Orphans = Definition.Get_Scope() == ECk_GameSettings_Scope::Machine ? _OrphanValues_Machine : _OrphanValues_Player0;
            Orphans.Remove(Key);
        }
    }
    for (const auto& Definition : Definitions)
    {
        if (const auto* Initial = InitialValues.Find(Definition.Get_Key()))
        { DoRouteApply(Definition.Get_Key(), Definition, *Initial); }
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterCollection(
        const UCk_GameSettings_Collection_PDA* InCollection)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    CK_ENSURE_IF_NOT(ck::IsValid(InCollection), TEXT("Invalid GameSettings Collection, nothing was registered"))
    { return false; }

    return Request_RegisterSettings(InCollection->Get_Settings());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Get_SettingValue_Bool(
        FName InKey,
        bool InFallback) const
    -> bool
{
    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition != nullptr, TEXT("Cannot read GameSettings key [{}], it is not registered"), InKey)
    { return InFallback; }

    CK_ENSURE_IF_NOT(Definition->Get_ValueType() == ECk_GameSettings_ValueType::Bool, TEXT("Cannot read GameSettings key [{}] as Bool, its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return InFallback; }

    if (Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::External)
    {
        const auto* Getter = _ExternalGetters_Bool.Find(InKey);
        CK_ENSURE_IF_NOT(Getter != nullptr && Getter->IsBound(), TEXT("External GameSettings key [{}] has no registered external getter"), InKey)
        { return InFallback; }

        return Getter->Execute();
    }

    auto Value = false;
    const auto StoredValueParses = ck_game_settings_subsystem::TryParse_Bool(_CurrentValues.FindChecked(InKey), Value);
    CK_ENSURE_IF_NOT(StoredValueParses, TEXT("Stored value [{}] of GameSettings key [{}] does not parse as Bool"), _CurrentValues.FindChecked(InKey), InKey)
    { return InFallback; }

    return Value;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Get_SettingValue_Int32(
        FName InKey,
        int32 InFallback) const
    -> int32
{
    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition != nullptr, TEXT("Cannot read GameSettings key [{}], it is not registered"), InKey)
    { return InFallback; }

    CK_ENSURE_IF_NOT(Definition->Get_ValueType() == ECk_GameSettings_ValueType::Int32, TEXT("Cannot read GameSettings key [{}] as Int32, its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return InFallback; }

    if (Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::External)
    {
        const auto* Getter = _ExternalGetters_Int32.Find(InKey);
        CK_ENSURE_IF_NOT(Getter != nullptr && Getter->IsBound(), TEXT("External GameSettings key [{}] has no registered external getter"), InKey)
        { return InFallback; }

        return Getter->Execute();
    }

    auto Value = int32{};
    const auto StoredValueParses = ck_game_settings_subsystem::TryParse_Int32(_CurrentValues.FindChecked(InKey), Value);
    CK_ENSURE_IF_NOT(StoredValueParses, TEXT("Stored value [{}] of GameSettings key [{}] does not parse as Int32"), _CurrentValues.FindChecked(InKey), InKey)
    { return InFallback; }

    return Value;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Get_SettingValue_Float(
        FName InKey,
        float InFallback) const
    -> float
{
    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition != nullptr, TEXT("Cannot read GameSettings key [{}], it is not registered"), InKey)
    { return InFallback; }

    CK_ENSURE_IF_NOT(Definition->Get_ValueType() == ECk_GameSettings_ValueType::Float, TEXT("Cannot read GameSettings key [{}] as Float, its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return InFallback; }

    if (Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::External)
    {
        const auto* Getter = _ExternalGetters_Float.Find(InKey);
        CK_ENSURE_IF_NOT(Getter != nullptr && Getter->IsBound(), TEXT("External GameSettings key [{}] has no registered external getter"), InKey)
        { return InFallback; }

        return Getter->Execute();
    }

    auto Value = float{};
    const auto StoredValueParses = ck_game_settings_subsystem::TryParse_Float(_CurrentValues.FindChecked(InKey), Value);
    CK_ENSURE_IF_NOT(StoredValueParses, TEXT("Stored value [{}] of GameSettings key [{}] does not parse as Float"), _CurrentValues.FindChecked(InKey), InKey)
    { return InFallback; }

    return Value;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Get_SettingValue_String(
        FName InKey,
        const FString& InFallback) const
    -> FString
{
    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition != nullptr, TEXT("Cannot read GameSettings key [{}], it is not registered"), InKey)
    { return InFallback; }

    CK_ENSURE_IF_NOT(Definition->Get_ValueType() == ECk_GameSettings_ValueType::String, TEXT("Cannot read GameSettings key [{}] as String, its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return InFallback; }

    if (Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::External)
    {
        const auto* Getter = _ExternalGetters_String.Find(InKey);
        CK_ENSURE_IF_NOT(Getter != nullptr && Getter->IsBound(), TEXT("External GameSettings key [{}] has no registered external getter"), InKey)
        { return InFallback; }

        return Getter->Execute();
    }

    return _CurrentValues.FindChecked(InKey);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Get_IsSettingRegistered(
        FName InKey) const
    -> bool
{
    return _Definitions.Contains(InKey);
}

auto
    UCk_GameSettings_Subsystem_UE::
    Get_SettingDefinition(
        FName InKey,
        FCk_GameSettings_SettingDefinition& OutDefinition) const
    -> bool
{
    const auto* Definition = _Definitions.Find(InKey);

    if (Definition == nullptr)
    { return false; }

    OutDefinition = *Definition;
    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Get_AllSettingKeys() const
    -> TArray<FName>
{
    return _RegistrationOrder;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Get_SettingKeysByCategory(
        const FGameplayTagQuery& InCategoryQuery) const
    -> TArray<FName>
{
    if (InCategoryQuery.IsEmpty())
    { return _RegistrationOrder; }

    return ck::algo::Filter(_RegistrationOrder, [&](FName InKey)
    {
        return InCategoryQuery.Matches(_Definitions.FindChecked(InKey).Get_CategoryTags());
    });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Request_SetSettingValue_Bool(
        const FCk_Request_GameSettings_SetValue_Bool& InRequest)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto Key = InRequest.Get_Key();
    const auto* Definition = _Definitions.Find(Key);

    CK_ENSURE_IF_NOT(Definition != nullptr, TEXT("Cannot set GameSettings key [{}], it is not registered"), Key)
    { return false; }

    CK_ENSURE_IF_NOT(Definition->Get_ValueType() == ECk_GameSettings_ValueType::Bool, TEXT("Cannot set GameSettings key [{}] as Bool, its value type is [{}]"), Key, Definition->Get_ValueType())
    { return false; }

    CK_ENSURE_IF_NOT(Definition->Get_Options().IsEmpty() ||
        ck_game_settings_subsystem::Get_MatchesAnyOption(Definition->Get_Options(), InRequest.Get_Value(), &ck_game_settings_subsystem::TryParse_Bool), TEXT("Cannot set GameSettings key [{}], value [{}] is not one of the allowed options"), Key, InRequest.Get_Value())
    { return false; }

    return DoCommitValue(Key, *Definition, LexToString(InRequest.Get_Value())) != ECommitResult::Failed;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_SetSettingValue_Int32(
        const FCk_Request_GameSettings_SetValue_Int32& InRequest)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto Key = InRequest.Get_Key();
    const auto* Definition = _Definitions.Find(Key);

    CK_ENSURE_IF_NOT(Definition != nullptr, TEXT("Cannot set GameSettings key [{}], it is not registered"), Key)
    { return false; }

    CK_ENSURE_IF_NOT(Definition->Get_ValueType() == ECk_GameSettings_ValueType::Int32, TEXT("Cannot set GameSettings key [{}] as Int32, its value type is [{}]"), Key, Definition->Get_ValueType())
    { return false; }

    const auto NewValue = InRequest.Get_Value();

#if NOT CK_DISABLE_ENSURE_CHECKS
    auto MinValue = int32{};
    const auto BelowMin = NOT Definition->Get_MinValue().IsEmpty() &&
        ck_game_settings_subsystem::TryParse_Int32(Definition->Get_MinValue(), MinValue) &&
        NewValue < MinValue;

    auto MaxValue = int32{};
    const auto AboveMax = NOT Definition->Get_MaxValue().IsEmpty() &&
        ck_game_settings_subsystem::TryParse_Int32(Definition->Get_MaxValue(), MaxValue) &&
        NewValue > MaxValue;

    const auto InRange = NOT BelowMin && NOT AboveMax;
    CK_ENSURE_IF_NOT(InRange, TEXT("Cannot set GameSettings key [{}], value [{}] is outside the allowed range [{} .. {}]"),
        Key, NewValue, Definition->Get_MinValue(), Definition->Get_MaxValue())
    { return false; }
#endif

    CK_ENSURE_IF_NOT(Definition->Get_Options().IsEmpty() ||
        ck_game_settings_subsystem::Get_MatchesAnyOption(Definition->Get_Options(), NewValue, &ck_game_settings_subsystem::TryParse_Int32), TEXT("Cannot set GameSettings key [{}], value [{}] is not one of the allowed options"), Key, NewValue)
    { return false; }

    return DoCommitValue(Key, *Definition, LexToString(NewValue)) != ECommitResult::Failed;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_SetSettingValue_Float(
        const FCk_Request_GameSettings_SetValue_Float& InRequest)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto Key = InRequest.Get_Key();
    const auto* Definition = _Definitions.Find(Key);

    CK_ENSURE_IF_NOT(Definition != nullptr, TEXT("Cannot set GameSettings key [{}], it is not registered"), Key)
    { return false; }

    CK_ENSURE_IF_NOT(Definition->Get_ValueType() == ECk_GameSettings_ValueType::Float, TEXT("Cannot set GameSettings key [{}] as Float, its value type is [{}]"), Key, Definition->Get_ValueType())
    { return false; }

    const auto NewValue = InRequest.Get_Value();

#if NOT CK_DISABLE_ENSURE_CHECKS
    auto MinValue = float{};
    const auto BelowMin = NOT Definition->Get_MinValue().IsEmpty() &&
        ck_game_settings_subsystem::TryParse_Float(Definition->Get_MinValue(), MinValue) &&
        NewValue < MinValue;

    auto MaxValue = float{};
    const auto AboveMax = NOT Definition->Get_MaxValue().IsEmpty() &&
        ck_game_settings_subsystem::TryParse_Float(Definition->Get_MaxValue(), MaxValue) &&
        NewValue > MaxValue;

    const auto InRange = NOT BelowMin && NOT AboveMax;
    CK_ENSURE_IF_NOT(InRange, TEXT("Cannot set GameSettings key [{}], value [{}] is outside the allowed range [{} .. {}]"),
        Key, NewValue, Definition->Get_MinValue(), Definition->Get_MaxValue())
    { return false; }
#endif

    CK_ENSURE_IF_NOT(Definition->Get_Options().IsEmpty() ||
        ck_game_settings_subsystem::Get_MatchesAnyOption(Definition->Get_Options(), NewValue, &ck_game_settings_subsystem::TryParse_Float), TEXT("Cannot set GameSettings key [{}], value [{}] is not one of the allowed options"), Key, NewValue)
    { return false; }

    return DoCommitValue(Key, *Definition, LexToString(NewValue)) != ECommitResult::Failed;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_SetSettingValue_String(
        const FCk_Request_GameSettings_SetValue_String& InRequest)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto Key = InRequest.Get_Key();
    const auto* Definition = _Definitions.Find(Key);

    CK_ENSURE_IF_NOT(Definition != nullptr, TEXT("Cannot set GameSettings key [{}], it is not registered"), Key)
    { return false; }

    CK_ENSURE_IF_NOT(Definition->Get_ValueType() == ECk_GameSettings_ValueType::String, TEXT("Cannot set GameSettings key [{}] as String, its value type is [{}]"), Key, Definition->Get_ValueType())
    { return false; }

    const auto& NewValue = InRequest.Get_Value();

    CK_ENSURE_IF_NOT(NOT NewValue.Contains(TEXT("\n")) && NOT NewValue.Contains(TEXT("\r")), TEXT("Cannot set GameSettings key [{}], the value contains a newline"), Key)
    { return false; }

    CK_ENSURE_IF_NOT(Definition->Get_Options().IsEmpty() ||
        ck::algo::AnyOf(Definition->Get_Options(), [&](const FCk_GameSettings_SettingOption& InOption)
        {
            return InOption.Get_Value().Equals(NewValue);
        }), TEXT("Cannot set GameSettings key [{}], value [{}] is not one of the allowed options"), Key, NewValue)
    { return false; }

    return DoCommitValue(Key, *Definition, NewValue) != ECommitResult::Failed;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Request_ResetToDefault(
        const FCk_Request_GameSettings_ResetToDefault& InRequest)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto Key = InRequest.Get_Key();

    CK_ENSURE_IF_NOT(_Definitions.Contains(Key), TEXT("Cannot reset GameSettings key [{}], it is not registered"), Key)
    { return false; }

    // External values live in their external store (e.g. GameUserSettings); the schema default is
    // a placeholder, and "resetting" to it would destroy real user configuration.
    CK_ENSURE_IF_NOT(_Definitions.FindChecked(Key).Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider, TEXT("Cannot reset External GameSettings key [{}], its value is owned by the external store"), Key)
    { return false; }

    return DoResetToDefault(Key) != ECommitResult::Failed;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_ResetAllToDefaults()
    -> int32
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return -1; }
    auto Values = TArray<TPair<FName, FString>>{};
    for (const auto& Key : _RegistrationOrder)
    {
        const auto& Definition = _Definitions.FindChecked(Key);
        if (Definition.Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider)
        { Values.Emplace(Key, Definition.Get_DefaultValue()); }
    }
    return DoCommitValues(Values);
}

// --------------------------------------------------------------------------------------------------------------------


auto
    UCk_GameSettings_Subsystem_UE::
    BindTo_OnSettingChanged(
        FName InKey,
        const FCk_Delegate_GameSettings_OnSettingChanged& InDelegate)
    -> void
{
    _ChangeBindings.FindOrAdd(InKey).AddUnique(InDelegate);
}

auto
    UCk_GameSettings_Subsystem_UE::
    UnbindFrom_OnSettingChanged(
        FName InKey,
        const FCk_Delegate_GameSettings_OnSettingChanged& InDelegate)
    -> void
{
    auto* Bindings = _ChangeBindings.Find(InKey);

    if (Bindings == nullptr)
    { return; }

    Bindings->Remove(InDelegate);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    DoFindValidationFailure(
        const FCk_GameSettings_SettingDefinition& InDefinition) const
    -> TOptional<FString>
{
    const auto Key = InDefinition.Get_Key();
    const auto ValueType = InDefinition.Get_ValueType();

    if (Key.IsNone())
    { return ck::Format_UE(TEXT("Setting definition has no Key (None)")); }

    if (NOT ck_game_settings_subsystem::Get_IsParseableAs(InDefinition.Get_DefaultValue(), ValueType))
    { return ck::Format_UE(TEXT("Setting [{}] default value [{}] is not parseable as [{}]"), Key, InDefinition.Get_DefaultValue(), ValueType); }

    if (InDefinition.Get_ApplyBindingType() == ECk_GameSettings_ApplyBindingType::CVar &&
        ck::Is_NOT_Valid(InDefinition.Get_CVar()))
    { return ck::Format_UE(TEXT("Setting [{}] has a CVar apply binding but its CVar name is unset"), Key); }

    if (InDefinition.Get_ApplyBindingType() == ECk_GameSettings_ApplyBindingType::CVar &&
        ck::game_settings::Get_IsCVarOwnedByGameUserSettings(InDefinition.Get_CVar().Get_Name()))
    {
        return ck::Format_UE(
            TEXT("Setting [{}] binds its apply to CVar [{}], which UGameUserSettings mirrors and rewrites from its "
                 "own copy on every ApplyNonResolutionSettings (an F11 fullscreen toggle or any resolution change "
                 "triggers one) — the setting would revert silently. Declare it PersistencePolicy::External over the "
                 "matching UGameUserSettings accessor instead, the way the Video pack does for the keys it covers"),
            Key, InDefinition.Get_CVar().Get_Name());
    }

    const auto HasMinOrMax = NOT InDefinition.Get_MinValue().IsEmpty() || NOT InDefinition.Get_MaxValue().IsEmpty();
    const auto IsNumeric = ValueType == ECk_GameSettings_ValueType::Int32 || ValueType == ECk_GameSettings_ValueType::Float;

    if (HasMinOrMax && NOT IsNumeric)
    { return ck::Format_UE(TEXT("Setting [{}] declares Min/Max but its value type [{}] is not numeric"), Key, ValueType); }

    if (HasMinOrMax && IsNumeric)
    {
        if (NOT InDefinition.Get_MinValue().IsEmpty() &&
            NOT ck_game_settings_subsystem::Get_IsParseableAs(InDefinition.Get_MinValue(), ValueType))
        { return ck::Format_UE(TEXT("Setting [{}] MinValue [{}] is not parseable as [{}]"), Key, InDefinition.Get_MinValue(), ValueType); }

        if (NOT InDefinition.Get_MaxValue().IsEmpty() &&
            NOT ck_game_settings_subsystem::Get_IsParseableAs(InDefinition.Get_MaxValue(), ValueType))
        { return ck::Format_UE(TEXT("Setting [{}] MaxValue [{}] is not parseable as [{}]"), Key, InDefinition.Get_MaxValue(), ValueType); }
    }

    return {};
}

auto
    UCk_GameSettings_Subsystem_UE::
    DoResetToDefault(
        FName InKey)
    -> ECommitResult
{
    const auto& Definition = _Definitions.FindChecked(InKey);
    return DoCommitValue(InKey, Definition, Definition.Get_DefaultValue());
}

auto
    UCk_GameSettings_Subsystem_UE::
    DoFireChanged(
        FName InKey,
        const FString& InNewValue)
    -> void
{
#if NOT CK_DISABLE_ENSURE_CHECKS
    const auto Guard = TGuardValue<bool>{_MutationInProgress, true};
#endif
    const auto PerKeyBindings = _ChangeBindings.FindRef(InKey);
    const auto WildcardBindings = _ChangeBindings.FindRef(NAME_None);
    PerKeyBindings.Broadcast(InKey, InNewValue);
    WildcardBindings.Broadcast(InKey, InNewValue);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterApplyHandler_Bool(
        FName InKey,
        const FCk_Delegate_GameSettings_ApplyHandler_Bool& InHandler)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition == nullptr || Definition->Get_ValueType() == ECk_GameSettings_ValueType::Bool, TEXT("Cannot register a Bool apply handler for GameSettings key [{}], its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return false; }

    _ApplyHandlers_Bool.Add(InKey, InHandler);

    if (Definition != nullptr &&
        Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider &&
        Definition->Get_ApplyBindingType() == ECk_GameSettings_ApplyBindingType::Handler)
    {
        auto Value = false;
        if (ck_game_settings_subsystem::TryParse_Bool(_CurrentValues.FindChecked(InKey), Value))
        {
            auto Callback = InHandler;
#if NOT CK_DISABLE_ENSURE_CHECKS
            const auto Guard = TGuardValue<bool>{_MutationInProgress, true};
#endif
            Callback.ExecuteIfBound(Value);
        }
    }

    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterApplyHandler_Int32(
        FName InKey,
        const FCk_Delegate_GameSettings_ApplyHandler_Int32& InHandler)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition == nullptr || Definition->Get_ValueType() == ECk_GameSettings_ValueType::Int32, TEXT("Cannot register an Int32 apply handler for GameSettings key [{}], its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return false; }

    _ApplyHandlers_Int32.Add(InKey, InHandler);

    if (Definition != nullptr &&
        Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider &&
        Definition->Get_ApplyBindingType() == ECk_GameSettings_ApplyBindingType::Handler)
    {
        auto Value = int32{};
        if (ck_game_settings_subsystem::TryParse_Int32(_CurrentValues.FindChecked(InKey), Value))
        {
            auto Callback = InHandler;
#if NOT CK_DISABLE_ENSURE_CHECKS
            const auto Guard = TGuardValue<bool>{_MutationInProgress, true};
#endif
            Callback.ExecuteIfBound(Value);
        }
    }

    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterApplyHandler_Float(
        FName InKey,
        const FCk_Delegate_GameSettings_ApplyHandler_Float& InHandler)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition == nullptr || Definition->Get_ValueType() == ECk_GameSettings_ValueType::Float, TEXT("Cannot register a Float apply handler for GameSettings key [{}], its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return false; }

    _ApplyHandlers_Float.Add(InKey, InHandler);

    if (Definition != nullptr &&
        Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider &&
        Definition->Get_ApplyBindingType() == ECk_GameSettings_ApplyBindingType::Handler)
    {
        auto Value = float{};
        if (ck_game_settings_subsystem::TryParse_Float(_CurrentValues.FindChecked(InKey), Value))
        {
            auto Callback = InHandler;
#if NOT CK_DISABLE_ENSURE_CHECKS
            const auto Guard = TGuardValue<bool>{_MutationInProgress, true};
#endif
            Callback.ExecuteIfBound(Value);
        }
    }

    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterApplyHandler_String(
        FName InKey,
        const FCk_Delegate_GameSettings_ApplyHandler_String& InHandler)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition == nullptr || Definition->Get_ValueType() == ECk_GameSettings_ValueType::String, TEXT("Cannot register a String apply handler for GameSettings key [{}], its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return false; }

    _ApplyHandlers_String.Add(InKey, InHandler);

    if (Definition != nullptr &&
        Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider &&
        Definition->Get_ApplyBindingType() == ECk_GameSettings_ApplyBindingType::Handler)
    {
        auto Callback = InHandler;
        const auto Value = _CurrentValues.FindChecked(InKey);
#if NOT CK_DISABLE_ENSURE_CHECKS
        const auto Guard = TGuardValue<bool>{_MutationInProgress, true};
#endif
        Callback.ExecuteIfBound(Value);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterExternalAccessors_Bool(
        FName InKey,
        const FCk_Delegate_GameSettings_ExternalGetter_Bool& InGetter,
        const FCk_Delegate_GameSettings_ApplyHandler_Bool& InSetter)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition == nullptr || Definition->Get_ValueType() == ECk_GameSettings_ValueType::Bool, TEXT("Cannot register Bool external accessors for GameSettings key [{}], its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return false; }

    CK_ENSURE_IF_NOT(InGetter.IsBound() && InSetter.IsBound(),
        TEXT("GameSettings external accessors require a bound getter and setter for [{}]"), InKey)
    { return false; }

    _ExternalGetters_Bool.Add(InKey, InGetter);
    _ApplyHandlers_Bool.Add(InKey, InSetter);
    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterExternalAccessors_Int32(
        FName InKey,
        const FCk_Delegate_GameSettings_ExternalGetter_Int32& InGetter,
        const FCk_Delegate_GameSettings_ApplyHandler_Int32& InSetter)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition == nullptr || Definition->Get_ValueType() == ECk_GameSettings_ValueType::Int32, TEXT("Cannot register Int32 external accessors for GameSettings key [{}], its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return false; }

    CK_ENSURE_IF_NOT(InGetter.IsBound() && InSetter.IsBound(),
        TEXT("GameSettings external accessors require a bound getter and setter for [{}]"), InKey)
    { return false; }

    _ExternalGetters_Int32.Add(InKey, InGetter);
    _ApplyHandlers_Int32.Add(InKey, InSetter);
    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterExternalAccessors_Float(
        FName InKey,
        const FCk_Delegate_GameSettings_ExternalGetter_Float& InGetter,
        const FCk_Delegate_GameSettings_ApplyHandler_Float& InSetter)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition == nullptr || Definition->Get_ValueType() == ECk_GameSettings_ValueType::Float, TEXT("Cannot register Float external accessors for GameSettings key [{}], its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return false; }

    CK_ENSURE_IF_NOT(InGetter.IsBound() && InSetter.IsBound(),
        TEXT("GameSettings external accessors require a bound getter and setter for [{}]"), InKey)
    { return false; }

    _ExternalGetters_Float.Add(InKey, InGetter);
    _ApplyHandlers_Float.Add(InKey, InSetter);
    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterExternalAccessors_String(
        FName InKey,
        const FCk_Delegate_GameSettings_ExternalGetter_String& InGetter,
        const FCk_Delegate_GameSettings_ApplyHandler_String& InSetter)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    const auto* Definition = _Definitions.Find(InKey);

    CK_ENSURE_IF_NOT(Definition == nullptr || Definition->Get_ValueType() == ECk_GameSettings_ValueType::String, TEXT("Cannot register String external accessors for GameSettings key [{}], its value type is [{}]"), InKey, Definition->Get_ValueType())
    { return false; }

    CK_ENSURE_IF_NOT(InGetter.IsBound() && InSetter.IsBound(),
        TEXT("GameSettings external accessors require a bound getter and setter for [{}]"), InKey)
    { return false; }

    _ExternalGetters_String.Add(InKey, InGetter);
    _ApplyHandlers_String.Add(InKey, InSetter);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Request_BeginPendingChanges()
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    CK_ENSURE_IF_NOT(NOT _PendingSessionActive, TEXT("A GameSettings pending-changes session is already active"))
    { return false; }

    _PendingSessionActive = true;
    _PendingPriorValues.Reset();
    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_ApplyPendingChanges()
    -> int32
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return -1; }
    CK_ENSURE_IF_NOT(_PendingSessionActive, TEXT("Cannot apply pending GameSettings changes, no session is active"))
    { return -1; }
    if (NOT DoFlush())
    { return -1; }
    const auto Count = _PendingPriorValues.Num();
    _PendingPriorValues.Reset();
    _PendingSessionActive = false;
    return Count;
}


auto
    UCk_GameSettings_Subsystem_UE::
    Request_RevertPendingChanges()
    -> int32
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return -1; }
    CK_ENSURE_IF_NOT(_PendingSessionActive, TEXT("Cannot revert pending GameSettings changes, no session is active"))
    { return -1; }
    auto Values = TArray<TPair<FName, FString>>{};
    for (const auto& Prior : _PendingPriorValues)
    { Values.Emplace(Prior.Key, Prior.Value); }
    const auto Count = DoCommitValues(Values, true, false, nullptr, nullptr, true);
    if (Count < 0)
    { return -1; }
    return Count;
}


auto
    UCk_GameSettings_Subsystem_UE::
    Get_HasPendingChanges() const
    -> bool
{
    return _PendingSessionActive && _PendingPriorValues.Num() > 0;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Get_HasUnappliedChange(
        FName InKey) const
    -> bool
{
    return _PendingSessionActive && _PendingPriorValues.Contains(InKey);
}

auto
    UCk_GameSettings_Subsystem_UE::
    Get_HasPendingCVarApply(
        FName InKey) const
    -> bool
{
    return _DeferredCVarApplies.ContainsByPredicate(
        [InKey](const FDeferredCVarApply& InEntry) { return InEntry._Key == InKey; });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterAudioPack()
    -> int32
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return -1; }

    return ck::game_settings::RegisterAudioPack(
        *this,
        UCk_Utils_GameSettings_Settings_UE::Get_AudioMix(),
        UCk_Utils_GameSettings_Settings_UE::Get_AudioCategories(),
        _PackHandlerObjects);
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RegisterVideoPack()
    -> int32
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return -1; }

    return ck::game_settings::RegisterVideoPack(*this, _PackHandlerObjects);
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_RunHardwareBenchmark()
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    if (ck::game_settings::Get_IsHeadlessPresentation())
    {
        ck::game_settings::Display(TEXT("GameSettings: hardware benchmark skipped (headless presentation)"));
        return false;
    }

    auto* UserSettings = ck::IsValid(GEngine) ? GEngine->GetGameUserSettings() : nullptr;
    CK_ENSURE_IF_NOT(ck::IsValid(UserSettings), TEXT("GEngine->GetGameUserSettings() returned null, cannot run the hardware benchmark"))
    { return false; }

    UserSettings->RunHardwareBenchmark();
    constexpr auto CheckForCommandLineOverrides = false;
    UserSettings->ApplySettings(CheckForCommandLineOverrides);
    UserSettings->SaveSettings();

    for (const auto& VideoKey : ck::game_settings::Get_VideoSettingKeys())
    {
        const auto* Definition = _Definitions.Find(VideoKey);

        if (Definition == nullptr)
        { continue; }

        DoFireChanged(VideoKey, DoGet_CurrentValueString(VideoKey, *Definition));
    }

    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_SetResolutionWithConfirmWindow(
        const FString& InNewResolution,
        float InWindowSeconds)
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    CK_ENSURE_IF_NOT(FMath::IsFinite(InWindowSeconds) && InWindowSeconds > 0.0f,
        TEXT("Resolution confirmation duration must be finite and positive"))
    { return false; }

    auto NewResolution = FIntPoint{};
    const auto ResolutionParses = ck::game_settings::TryParse_Resolution(InNewResolution, NewResolution);
    CK_ENSURE_IF_NOT(ResolutionParses, TEXT("Resolution [{}] is not of the form WIDTHxHEIGHT"), InNewResolution)
    { return false; }

    CK_ENSURE_IF_NOT(NOT _ResolutionConfirmActive, TEXT("A resolution confirm window is already active"))
    { return false; }

    auto* UserSettings = ck::IsValid(GEngine) ? GEngine->GetGameUserSettings() : nullptr;
    CK_ENSURE_IF_NOT(ck::IsValid(UserSettings), TEXT("GEngine->GetGameUserSettings() returned null, cannot change the resolution"))
    { return false; }

    _ResolutionConfirmPriorResolution = UserSettings->GetScreenResolution();
    UserSettings->SetScreenResolution(NewResolution);

    if (NOT ck::game_settings::Get_IsHeadlessPresentation())
    {
        constexpr auto CheckForCommandLineOverrides = false;
        UserSettings->ApplyResolutionSettings(CheckForCommandLineOverrides);
    }

    _ResolutionConfirmActive = true;
    _ResolutionConfirmDeadlineSeconds = FPlatformTime::Seconds() + InWindowSeconds;

    DoFireChanged(ck::game_settings::Key_Video_Resolution, ck::game_settings::Format_Resolution(NewResolution));
    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_ConfirmResolution()
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    CK_ENSURE_IF_NOT(_ResolutionConfirmActive, TEXT("No resolution confirm window is active"))
    { return false; }

    _ResolutionConfirmActive = false;

    auto* UserSettings = ck::IsValid(GEngine) ? GEngine->GetGameUserSettings() : nullptr;

    if (ck::IsValid(UserSettings))
    {
        UserSettings->ConfirmVideoMode();
        UserSettings->SaveSettings();
    }

    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_FlushStorage()
    -> bool
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return false; }

    return DoFlush();
}

auto
    UCk_GameSettings_Subsystem_UE::
    Request_ReloadFromStorage()
    -> int32
{
    CK_ENSURE_IF_NOT(NOT _MutationInProgress, TEXT("Nested GameSettings mutation from a settings callback is rejected"))
    { return -1; }
    return DoAbsorbStoredValues();
}


auto
    UCk_GameSettings_Subsystem_UE::
    Get_StorageProvider() const
    -> UCk_GameSettings_StorageProvider_UE*
{
    return _StorageProvider;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_Subsystem_UE::
    DoGet_CurrentValueString(
        FName InKey,
        const FCk_GameSettings_SettingDefinition& InDefinition) const
    -> FString
{
    if (InDefinition.Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::External)
    {
        switch (InDefinition.Get_ValueType())
        {
            case ECk_GameSettings_ValueType::Bool:
            {
                if (const auto* Getter = _ExternalGetters_Bool.Find(InKey); Getter != nullptr && Getter->IsBound())
                { return LexToString(Getter->Execute()); }
                break;
            }
            case ECk_GameSettings_ValueType::Int32:
            {
                if (const auto* Getter = _ExternalGetters_Int32.Find(InKey); Getter != nullptr && Getter->IsBound())
                { return LexToString(Getter->Execute()); }
                break;
            }
            case ECk_GameSettings_ValueType::Float:
            {
                if (const auto* Getter = _ExternalGetters_Float.Find(InKey); Getter != nullptr && Getter->IsBound())
                { return LexToString(Getter->Execute()); }
                break;
            }
            case ECk_GameSettings_ValueType::String:
            {
                if (const auto* Getter = _ExternalGetters_String.Find(InKey); Getter != nullptr && Getter->IsBound())
                { return Getter->Execute(); }
                break;
            }
            default:
            {
                CK_INVALID_ENUM(InDefinition.Get_ValueType());
                break;
            }
        }
    }

    return _CurrentValues.FindRef(InKey);
}

auto
    UCk_GameSettings_Subsystem_UE::
    DoGet_IsValueValid(
        const FCk_GameSettings_SettingDefinition& InDefinition,
        const FString& InValue) const
    -> bool
{
    const auto Type = InDefinition.Get_ValueType();
    if (NOT ck_game_settings_subsystem::Get_IsParseableAs(InValue, Type))
    { return false; }
    if (NOT InDefinition.Get_Options().IsEmpty() && NOT ck::algo::AnyOf(InDefinition.Get_Options(),
        [&](const FCk_GameSettings_SettingOption& InOption)
        { return ck_game_settings_subsystem::Get_ValuesEqual(Type, InValue, InOption.Get_Value()); }))
    { return false; }
    if (Type == ECk_GameSettings_ValueType::Int32 || Type == ECk_GameSettings_ValueType::Float)
    {
        const auto Value = FCString::Atod(*InValue);
        if (NOT FMath::IsFinite(Value))
        { return false; }
        if (NOT InDefinition.Get_MinValue().IsEmpty() && Value < FCString::Atod(*InDefinition.Get_MinValue()))
        { return false; }
        if (NOT InDefinition.Get_MaxValue().IsEmpty() && Value > FCString::Atod(*InDefinition.Get_MaxValue()))
        { return false; }
    }
    return true;
}

auto
    UCk_GameSettings_Subsystem_UE::
    DoCommitValue(
        FName InKey,
        const FCk_GameSettings_SettingDefinition& InDefinition,
        const FString& InNewValueString)
    -> ECommitResult
{
    const auto Count = DoCommitValues({TPair<FName, FString>{InKey, InNewValueString}});
    return Count < 0 ? ECommitResult::Failed : Count == 0 ? ECommitResult::Unchanged : ECommitResult::Changed;
}

auto
    UCk_GameSettings_Subsystem_UE::
    DoCommitValues(
        const TArray<TPair<FName, FString>>& InValues,
        bool InPersist,
        bool InRecordPending,
        const TMap<FName, FString>* InMachineOrphans,
        const TMap<FName, FString>* InPlayerOrphans,
        bool InClosePendingSession)
    -> int32
{
#if NOT CK_DISABLE_ENSURE_CHECKS
    const auto MutationGuard = TGuardValue<bool>{_MutationInProgress, true};
#endif
    struct FChange
    {
        FName _Key;
        FString _Value;
        FString _Previous;
        FCk_GameSettings_SettingDefinition _Definition;
    };
    auto Changes = TArray<FChange>{};
    auto Writes = TArray<FCk_GameSettings_StoreValue>{};
#if NOT CK_DISABLE_ENSURE_CHECKS
    auto Keys = TSet<FName>{};
#endif
    for (const auto& Entry : InValues)
    {
        CK_ENSURE_IF_NOT(_Definitions.Contains(Entry.Key) && NOT Keys.Contains(Entry.Key),
            TEXT("GameSettings batch contains missing or duplicate key [{}]"), Entry.Key)
        { return -1; }
#if NOT CK_DISABLE_ENSURE_CHECKS
        Keys.Add(Entry.Key);
#endif
        const auto Definition = _Definitions.FindChecked(Entry.Key);
        CK_ENSURE_IF_NOT(DoGet_IsValueValid(Definition, Entry.Value),
            TEXT("GameSettings batch contains invalid value [{}] for [{}]"), Entry.Value, Entry.Key)
        { return -1; }
#if NOT CK_DISABLE_ENSURE_CHECKS
        if (Definition.Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::External)
        {
            auto Bound = false;
            switch (Definition.Get_ValueType())
            {
                case ECk_GameSettings_ValueType::Bool:
                    Bound = _ExternalGetters_Bool.Contains(Entry.Key) && _ExternalGetters_Bool.FindChecked(Entry.Key).IsBound()
                        && _ApplyHandlers_Bool.Contains(Entry.Key) && _ApplyHandlers_Bool.FindChecked(Entry.Key).IsBound();
                    break;
                case ECk_GameSettings_ValueType::Int32:
                    Bound = _ExternalGetters_Int32.Contains(Entry.Key) && _ExternalGetters_Int32.FindChecked(Entry.Key).IsBound()
                        && _ApplyHandlers_Int32.Contains(Entry.Key) && _ApplyHandlers_Int32.FindChecked(Entry.Key).IsBound();
                    break;
                case ECk_GameSettings_ValueType::Float:
                    Bound = _ExternalGetters_Float.Contains(Entry.Key) && _ExternalGetters_Float.FindChecked(Entry.Key).IsBound()
                        && _ApplyHandlers_Float.Contains(Entry.Key) && _ApplyHandlers_Float.FindChecked(Entry.Key).IsBound();
                    break;
                case ECk_GameSettings_ValueType::String:
                    Bound = _ExternalGetters_String.Contains(Entry.Key) && _ExternalGetters_String.FindChecked(Entry.Key).IsBound()
                        && _ApplyHandlers_String.Contains(Entry.Key) && _ApplyHandlers_String.FindChecked(Entry.Key).IsBound();
                    break;
                default: break;
            }
            CK_ENSURE_IF_NOT(Bound, TEXT("External GameSettings key [{}] requires a bound getter and setter"), Entry.Key)
            { return -1; }
        }
#endif
        const auto Previous = DoGet_CurrentValueString(Entry.Key, Definition);
        if (ck_game_settings_subsystem::Get_ValuesEqual(Definition.Get_ValueType(), Previous, Entry.Value))
        { continue; }
        Changes.Add(FChange{Entry.Key, Entry.Value, Previous, Definition});
        if (InPersist && Definition.Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider)
        { Writes.Emplace(Definition.Get_Scope(), 0, Entry.Key, Entry.Value); }
    }
    CK_ENSURE_IF_NOT(Writes.IsEmpty() || ck::IsValid(_StorageProvider), TEXT("GameSettings has no storage provider to accept the batch"))
    { return -1; }
    if (NOT Writes.IsEmpty())
    {
        const auto Accepted = _StorageProvider->Request_StoreValues(Writes);
        if (NOT Accepted)
        { return -1; }
    }
    for (const auto& Change : Changes)
    {
        if (InRecordPending && _PendingSessionActive && NOT _PendingPriorValues.Contains(Change._Key))
        { _PendingPriorValues.Add(Change._Key, Change._Previous); }
        if (Change._Definition.Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider)
        { _CurrentValues.Add(Change._Key, Change._Value); }
    }
    if (InMachineOrphans != nullptr)
    { _OrphanValues_Machine = *InMachineOrphans; }
    if (InPlayerOrphans != nullptr)
    { _OrphanValues_Player0 = *InPlayerOrphans; }
    if (InClosePendingSession)
    {
        _PendingPriorValues.Reset();
        _PendingSessionActive = false;
    }
    if (NOT Writes.IsEmpty() && ck::IsValid(_StorageProvider))
    {
        _FlushPending = true;
        _LastDirtyTimeSeconds = FPlatformTime::Seconds();
    }
    for (const auto& Change : Changes)
    {
        DoRouteApply(Change._Key, Change._Definition, Change._Value);
        DoFireChanged(Change._Key, Change._Value);
    }
    return Changes.Num();
}


auto
    UCk_GameSettings_Subsystem_UE::
    DoRouteApply(
        FName InKey,
        const FCk_GameSettings_SettingDefinition& InDefinition,
        const FString& InValueString)
    -> void
{
    const auto IsExternal = InDefinition.Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::External;
    const auto RouteToHandler = IsExternal || InDefinition.Get_ApplyBindingType() == ECk_GameSettings_ApplyBindingType::Handler;

    if (RouteToHandler)
    {
        switch (InDefinition.Get_ValueType())
        {
            case ECk_GameSettings_ValueType::Bool:
            {
                const auto* Handler = _ApplyHandlers_Bool.Find(InKey);
                const auto HandlerIsBound = Handler != nullptr && Handler->IsBound();
                CK_ENSURE_IF_NOT(NOT IsExternal || HandlerIsBound, TEXT("External GameSettings key [{}] has no registered external setter"), InKey)
                { return; }
                if (NOT IsExternal && NOT HandlerIsBound)
                { return; }

                auto Value = false;
                if (ck_game_settings_subsystem::TryParse_Bool(InValueString, Value))
                { Handler->Execute(Value); }
                return;
            }
            case ECk_GameSettings_ValueType::Int32:
            {
                const auto* Handler = _ApplyHandlers_Int32.Find(InKey);
                const auto HandlerIsBound = Handler != nullptr && Handler->IsBound();
                CK_ENSURE_IF_NOT(NOT IsExternal || HandlerIsBound, TEXT("External GameSettings key [{}] has no registered external setter"), InKey)
                { return; }
                if (NOT IsExternal && NOT HandlerIsBound)
                { return; }

                auto Value = int32{};
                if (ck_game_settings_subsystem::TryParse_Int32(InValueString, Value))
                { Handler->Execute(Value); }
                return;
            }
            case ECk_GameSettings_ValueType::Float:
            {
                const auto* Handler = _ApplyHandlers_Float.Find(InKey);
                const auto HandlerIsBound = Handler != nullptr && Handler->IsBound();
                CK_ENSURE_IF_NOT(NOT IsExternal || HandlerIsBound, TEXT("External GameSettings key [{}] has no registered external setter"), InKey)
                { return; }
                if (NOT IsExternal && NOT HandlerIsBound)
                { return; }

                auto Value = float{};
                if (ck_game_settings_subsystem::TryParse_Float(InValueString, Value))
                { Handler->Execute(Value); }
                return;
            }
            case ECk_GameSettings_ValueType::String:
            {
                const auto* Handler = _ApplyHandlers_String.Find(InKey);
                const auto HandlerIsBound = Handler != nullptr && Handler->IsBound();
                CK_ENSURE_IF_NOT(NOT IsExternal || HandlerIsBound, TEXT("External GameSettings key [{}] has no registered external setter"), InKey)
                { return; }
                if (NOT IsExternal && NOT HandlerIsBound)
                { return; }

                Handler->Execute(InValueString);
                return;
            }
            default:
            {
                CK_INVALID_ENUM(InDefinition.Get_ValueType());
                return;
            }
        }
    }

    if (InDefinition.Get_ApplyBindingType() != ECk_GameSettings_ApplyBindingType::CVar)
    { return; }

    const auto& CVarRef = InDefinition.Get_CVar();

    if (UCk_Utils_CVar_UE::IsRegistered(CVarRef))
    {
        if (_IsPieInstance)
        { return; }

        ck_game_settings_subsystem::DoWriteCVar(CVarRef, InDefinition.Get_ValueType(), InValueString);
        return;
    }

    if (auto* ExistingEntry = _DeferredCVarApplies.FindByPredicate([&](const FDeferredCVarApply& InEntry) { return InEntry._Key == InKey; }))
    {
        ExistingEntry->_Value = InValueString;
        return;
    }

    _DeferredCVarApplies.Add(FDeferredCVarApply{InKey, CVarRef, InDefinition.Get_ValueType(), InValueString, FPlatformTime::Seconds()});
}

auto
    UCk_GameSettings_Subsystem_UE::
    DoAbsorbStoredValues()
    -> int32
{
    CK_ENSURE_IF_NOT(ck::IsValid(_StorageProvider), TEXT("GameSettings has no storage provider"))
    { return -1; }
#if NOT CK_DISABLE_ENSURE_CHECKS
    const auto MutationGuard = TGuardValue<bool>{_MutationInProgress, true};
#endif
    auto Machine = TArray<FCk_GameSettings_StoredValue>{};
    auto Player = TArray<FCk_GameSettings_StoredValue>{};
    const auto MachineResult = _StorageProvider->Get_StoredValues(ECk_GameSettings_Scope::Machine, 0, Machine);
    const auto PlayerResult = _StorageProvider->Get_StoredValues(ECk_GameSettings_Scope::Player, 0, Player);
    if (MachineResult == ECk_GameSettings_StorageReadResult::Failed || PlayerResult == ECk_GameSettings_StorageReadResult::Failed)
    { return -1; }
    auto MachineOrphans = _OrphanValues_Machine;
    auto PlayerOrphans = _OrphanValues_Player0;
    auto Values = TArray<TPair<FName, FString>>{};
    for (const auto Scope : {ECk_GameSettings_Scope::Machine, ECk_GameSettings_Scope::Player})
    {
        const auto& Stored = Scope == ECk_GameSettings_Scope::Machine ? Machine : Player;
        auto& Orphans = Scope == ECk_GameSettings_Scope::Machine ? MachineOrphans : PlayerOrphans;

        auto Keys = TSet<FName>{};
        for (const auto& Value : Stored)
        {
            const auto Key = Value.Get_Key();
            if (Key.IsNone() || Keys.Contains(Key))
            {
                ck::game_settings::Error(TEXT("GameSettings reload contains missing or duplicate key [{}]"), Key);
                return -1;
            }
            Keys.Add(Key);
            const auto* Definition = _Definitions.Find(Key);
            if (Definition != nullptr && Definition->Get_Scope() == Scope && Definition->Get_PersistencePolicy() == ECk_GameSettings_PersistencePolicy::Provider)
            {
                if (NOT DoGet_IsValueValid(*Definition, Value.Get_Value()))
                {
                    ck::game_settings::Error(TEXT("GameSettings reload rejected invalid stored value [{}] for [{}]"), Value.Get_Value(), Key);
                    return -1;
                }
                Values.Emplace(Key, Value.Get_Value());
            }
            else
            { Orphans.Add(Key, Value.Get_Value()); }
        }
    }
    if (DoCommitValues(Values, false, false, &MachineOrphans, &PlayerOrphans) < 0)
    { return -1; }
    return Machine.Num() + Player.Num();
}


auto
    UCk_GameSettings_Subsystem_UE::
    DoStartTicker()
    -> void
{
    constexpr auto TickIntervalSeconds = 1.0f;
    _TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float)
        {
            DoHandleTick();
            constexpr auto KeepTicking = true;
            return KeepTicking;
        }), TickIntervalSeconds);
}


auto
    UCk_GameSettings_Subsystem_UE::
    DoHandleTick()
    -> void
{
    const auto NowSeconds = FPlatformTime::Seconds();
#if NOT CK_DISABLE_ENSURE_CHECKS
    const auto TimeoutSeconds = ck_game_settings_cvars::DeferredApplyTimeoutSecs;
#endif

    for (auto Index = _DeferredCVarApplies.Num() - 1; Index >= 0; --Index)
    {
        const auto& Entry = _DeferredCVarApplies[Index];

        if (UCk_Utils_CVar_UE::IsRegistered(Entry._CVar))
        {
            if (NOT _IsPieInstance)
            { ck_game_settings_subsystem::DoWriteCVar(Entry._CVar, Entry._ValueType, Entry._Value); }

            _DeferredCVarApplies.RemoveAt(Index);
            continue;
        }

#if NOT CK_DISABLE_ENSURE_CHECKS
        const auto TimedOut = NowSeconds - Entry._EnqueuedAtSeconds > TimeoutSeconds;
        CK_ENSURE_IF_NOT(NOT TimedOut, TEXT("GameSettings deferred apply for key [{}] timed out, CVar [{}] never registered within [{}] seconds. The stored value is retained and will apply next boot."),
            Entry._Key, Entry._CVar.Get_Name(), TimeoutSeconds)
        {
            _DeferredCVarApplies.RemoveAt(Index);
            continue;
        }
#endif
    }

    if (_ResolutionConfirmActive && NowSeconds >= _ResolutionConfirmDeadlineSeconds)
    {
        _ResolutionConfirmActive = false;

        auto* UserSettings = ck::IsValid(GEngine) ? GEngine->GetGameUserSettings() : nullptr;

        if (ck::IsValid(UserSettings))
        {
            UserSettings->SetScreenResolution(_ResolutionConfirmPriorResolution);

            if (NOT ck::game_settings::Get_IsHeadlessPresentation())
            {
                constexpr auto CheckForCommandLineOverrides = false;
                UserSettings->ApplyResolutionSettings(CheckForCommandLineOverrides);
            }

            UserSettings->SaveSettings();

            const auto PriorResolutionString = ck::game_settings::Format_Resolution(_ResolutionConfirmPriorResolution);
            ck::game_settings::Display(TEXT("GameSettings: resolution confirm window expired, reverted to [{}]"), PriorResolutionString);
            DoFireChanged(ck::game_settings::Key_Video_Resolution, PriorResolutionString);
        }
    }

    constexpr auto FlushDebounceSeconds = 2.0;
    if (_FlushPending && NowSeconds - _LastDirtyTimeSeconds > FlushDebounceSeconds)
    { DoFlush(); }
}

auto
    UCk_GameSettings_Subsystem_UE::
    DoFlush()
    -> bool
{
#if NOT CK_DISABLE_ENSURE_CHECKS
    const auto MutationGuard = TGuardValue<bool>{_MutationInProgress, true};
#endif
    CK_ENSURE_IF_NOT(ck::IsValid(_StorageProvider), TEXT("GameSettings cannot flush without a storage provider"))
    { return false; }
    const auto Flushed = _StorageProvider->Request_Flush();
    if (NOT Flushed)
    {
        _LastDirtyTimeSeconds = FPlatformTime::Seconds();
        return false;
    }
    _FlushPending = false;
    return true;
}


auto
    UCk_GameSettings_Subsystem_UE::
    DoHandlePreLoadMap(
        const FWorldContext& InWorldContext,
        const FString& InMapName)
    -> void
{
    if (InWorldContext.OwningGameInstance != GetGameInstance())
    { return; }

    if (NOT _PendingSessionActive)
    { return; }

    ck::game_settings::Display(TEXT("GameSettings: map travel with an active pending-changes session, auto-reverting"));
    Request_RevertPendingChanges();
}

auto
    UCk_GameSettings_Subsystem_UE::
    DoGet_IsPieWorldContext(
        const UGameInstance* InGameInstance)
    -> bool
{
    if (ck::Is_NOT_Valid(InGameInstance))
    { return false; }

    const auto* WorldContext = InGameInstance->GetWorldContext();

    if (WorldContext == nullptr)
    { return false; }

    return WorldContext->WorldType == EWorldType::PIE;
}

// --------------------------------------------------------------------------------------------------------------------
