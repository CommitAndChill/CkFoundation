#include "CkGameSettings/CkGameSettings_Common.h"
#include "CkGameSettings/Subsystem/CkGameSettings_Subsystem.h"
#include "CkGameSettings/Subsystem/CkGameSettings_Utils.h"

#include "CkGameSettings_SpecSupport.h"

#include "Misc/AutomationTest.h"

#include <Engine/Engine.h>
#include <Engine/GameInstance.h>
#include <GameFramework/GameUserSettings.h>
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_game_settings_registry_spec
{
    auto Make_Subsystem() -> UCk_GameSettings_Subsystem_UE*
    {
        auto* GameInstance = NewObject<UGameInstance>(GEngine);
        auto* Subsystem = NewObject<UCk_GameSettings_Subsystem_UE>(GameInstance);
        FCk_GameSettings_SpecAccess::Request_SetProvider(Subsystem, NewObject<UCk_GameSettings_SpecStorageProvider_UE>(Subsystem));
        return Subsystem;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Registry_RegisterAndQuery,
    "Ck.CkGameSettings.Registry.RegisterAndQuery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Registry_RegisterAndQuery::RunTest(const FString&)
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();

    TestTrue(TEXT("register Bool"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("test.bool"), ECk_GameSettings_ValueType::Bool, TEXT("true")}));
    TestTrue(TEXT("register Int32"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("test.int"), ECk_GameSettings_ValueType::Int32, TEXT("5")}));
    TestTrue(TEXT("register Float"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("test.float"), ECk_GameSettings_ValueType::Float, TEXT("0.5")}));
    TestTrue(TEXT("register String"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("test.string"), ECk_GameSettings_ValueType::String, TEXT("hello")}));

    TestTrue(TEXT("registered key is registered"), Subsystem->Get_IsSettingRegistered(TEXT("test.bool")));
    TestFalse(TEXT("unknown key is not registered"), Subsystem->Get_IsSettingRegistered(TEXT("test.missing")));

    TestTrue(TEXT("Bool default readable"), Subsystem->Get_SettingValue_Bool(TEXT("test.bool"), false));
    TestEqual(TEXT("Int32 default readable"), Subsystem->Get_SettingValue_Int32(TEXT("test.int"), 0), 5);
    TestEqual(TEXT("Float default readable"), Subsystem->Get_SettingValue_Float(TEXT("test.float"), 0.0f), 0.5f);
    TestEqual(TEXT("String default readable"), Subsystem->Get_SettingValue_String(TEXT("test.string")), TEXT("hello"));

    const auto AllKeys = Subsystem->Get_AllSettingKeys();
    TestEqual(TEXT("four keys registered"), AllKeys.Num(), 4);
    if (AllKeys.Num() == 4)
    {
        TestEqual(TEXT("registration order preserved [0]"), AllKeys[0], FName{TEXT("test.bool")});
        TestEqual(TEXT("registration order preserved [1]"), AllKeys[1], FName{TEXT("test.int")});
        TestEqual(TEXT("registration order preserved [2]"), AllKeys[2], FName{TEXT("test.float")});
        TestEqual(TEXT("registration order preserved [3]"), AllKeys[3], FName{TEXT("test.string")});
    }

    auto Definition = FCk_GameSettings_SettingDefinition{};
    TestTrue(TEXT("definition retrievable"), Subsystem->Get_SettingDefinition(TEXT("test.int"), Definition));
    TestEqual(TEXT("retrieved definition has the right value type"), Definition.Get_ValueType(), ECk_GameSettings_ValueType::Int32);
    TestFalse(TEXT("unknown definition not retrievable"), Subsystem->Get_SettingDefinition(TEXT("test.missing"), Definition));

    TestEqual(TEXT("empty category query matches every registered setting"),
        Subsystem->Get_SettingKeysByCategory(FGameplayTagQuery{}).Num(), 4);

    const auto BatchDefinitions = TArray<FCk_GameSettings_SettingDefinition>
    {
        FCk_GameSettings_SettingDefinition{TEXT("batch.a"), ECk_GameSettings_ValueType::Bool, TEXT("false")},
        FCk_GameSettings_SettingDefinition{TEXT("batch.b"), ECk_GameSettings_ValueType::Int32, TEXT("0")},
    };
    TestTrue(TEXT("valid batch registers"), Subsystem->Request_RegisterSettings(BatchDefinitions));
    TestEqual(TEXT("six keys after batch"), Subsystem->Get_AllSettingKeys().Num(), 6);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Registry_DuplicateKeyRejected,
    "Ck.CkGameSettings.Registry.DuplicateKeyRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Registry_DuplicateKeyRejected::RunTest(const FString&)
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();

    TestTrue(TEXT("first registration succeeds"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("dup.key"), ECk_GameSettings_ValueType::Int32, TEXT("1")}));

    AddExpectedError(TEXT("already registered"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("duplicate registration rejected"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("dup.key"), ECk_GameSettings_ValueType::Int32, TEXT("999")}));

    TestEqual(TEXT("still exactly one key"), Subsystem->Get_AllSettingKeys().Num(), 1);
    TestEqual(TEXT("original default untouched"), Subsystem->Get_SettingValue_Int32(TEXT("dup.key"), -1), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Registry_AtomicBatchRejectsAll,
    "Ck.CkGameSettings.Registry.AtomicBatchRejectsAll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Registry_AtomicBatchRejectsAll::RunTest(const FString&)
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();

    AddExpectedErrorPlain(TEXT("Invalid GameSettings definition, nothing was registered"), EAutomationExpectedErrorFlags::Contains, 0);

    const auto BatchWithUnparseableDefault = TArray<FCk_GameSettings_SettingDefinition>
    {
        FCk_GameSettings_SettingDefinition{TEXT("atomic.a"), ECk_GameSettings_ValueType::Bool, TEXT("true")},
        FCk_GameSettings_SettingDefinition{TEXT("atomic.b"), ECk_GameSettings_ValueType::Int32, TEXT("7")},
        FCk_GameSettings_SettingDefinition{TEXT("atomic.bad"), ECk_GameSettings_ValueType::Int32, TEXT("abc")},
    };
    TestFalse(TEXT("batch with unparseable default rejected"), Subsystem->Request_RegisterSettings(BatchWithUnparseableDefault));
    TestEqual(TEXT("nothing registered from rejected batch"), Subsystem->Get_AllSettingKeys().Num(), 0);
    TestFalse(TEXT("valid member of rejected batch not registered"), Subsystem->Get_IsSettingRegistered(TEXT("atomic.a")));

    const auto BatchWithUnsetCVarName = TArray<FCk_GameSettings_SettingDefinition>
    {
        FCk_GameSettings_SettingDefinition{TEXT("atomic.c"), ECk_GameSettings_ValueType::Float, TEXT("1.0")},
        FCk_GameSettings_SettingDefinition{TEXT("atomic.cvar"), ECk_GameSettings_ValueType::Float, TEXT("1.0")}
            .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::CVar),
    };
    TestFalse(TEXT("batch with unset CVar name rejected"), Subsystem->Request_RegisterSettings(BatchWithUnsetCVarName));
    TestEqual(TEXT("nothing registered from CVar batch"), Subsystem->Get_AllSettingKeys().Num(), 0);

    const auto BatchWithRangeOnBool = TArray<FCk_GameSettings_SettingDefinition>
    {
        FCk_GameSettings_SettingDefinition{TEXT("atomic.d"), ECk_GameSettings_ValueType::String, TEXT("x")},
        FCk_GameSettings_SettingDefinition{TEXT("atomic.rangedbool"), ECk_GameSettings_ValueType::Bool, TEXT("true")}
            .Set_MinValue(TEXT("0")),
    };
    TestFalse(TEXT("batch with Min/Max on non-numeric type rejected"), Subsystem->Request_RegisterSettings(BatchWithRangeOnBool));
    TestEqual(TEXT("nothing registered from ranged-bool batch"), Subsystem->Get_AllSettingKeys().Num(), 0);

    const auto BatchWithNoneKey = TArray<FCk_GameSettings_SettingDefinition>
    {
        FCk_GameSettings_SettingDefinition{TEXT("atomic.e"), ECk_GameSettings_ValueType::Bool, TEXT("false")},
        FCk_GameSettings_SettingDefinition{FName{}, ECk_GameSettings_ValueType::Bool, TEXT("false")},
    };
    TestFalse(TEXT("batch with None key rejected"), Subsystem->Request_RegisterSettings(BatchWithNoneKey));
    TestEqual(TEXT("nothing registered from None-key batch"), Subsystem->Get_AllSettingKeys().Num(), 0);

    const auto BatchWithUnparseableMin = TArray<FCk_GameSettings_SettingDefinition>
    {
        FCk_GameSettings_SettingDefinition{TEXT("atomic.f"), ECk_GameSettings_ValueType::Int32, TEXT("1")},
        FCk_GameSettings_SettingDefinition{TEXT("atomic.badmin"), ECk_GameSettings_ValueType::Int32, TEXT("1")}
            .Set_MinValue(TEXT("abc")),
    };
    TestFalse(TEXT("batch with unparseable Min rejected"), Subsystem->Request_RegisterSettings(BatchWithUnparseableMin));
    TestEqual(TEXT("nothing registered from bad-min batch"), Subsystem->Get_AllSettingKeys().Num(), 0);

    AddExpectedError(TEXT("Invalid GameSettings Collection"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("null collection rejected"), Subsystem->Request_RegisterCollection(nullptr));
    TestEqual(TEXT("nothing registered from null collection"), Subsystem->Get_AllSettingKeys().Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Registry_TypedAccessAndMismatchRejected,
    "Ck.CkGameSettings.Registry.TypedAccessAndMismatchRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Registry_TypedAccessAndMismatchRejected::RunTest(const FString&)
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();

    TestTrue(TEXT("register Int32"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("typed.int"), ECk_GameSettings_ValueType::Int32, TEXT("7")}));

    TestEqual(TEXT("typed read succeeds"), Subsystem->Get_SettingValue_Int32(TEXT("typed.int"), 0), 7);

    AddExpectedError(TEXT("it is not registered"), EAutomationExpectedErrorFlags::Contains, 0);
    TestTrue(TEXT("unknown key returns the supplied fallback"), Subsystem->Get_SettingValue_Bool(TEXT("typed.missing"), true));

    AddExpectedError(TEXT("its value type is"), EAutomationExpectedErrorFlags::Contains, 0);
    TestEqual(TEXT("type-mismatched read returns the supplied fallback"),
        Subsystem->Get_SettingValue_Float(TEXT("typed.int"), 1.5f), 1.5f);

    TestFalse(TEXT("type-mismatched set rejected"), Subsystem->Request_SetSettingValue_Bool(
        FCk_Request_GameSettings_SetValue_Bool{TEXT("typed.int"), true}));
    TestEqual(TEXT("value unchanged after rejected set"), Subsystem->Get_SettingValue_Int32(TEXT("typed.int"), 0), 7);

    TestFalse(TEXT("unknown-key set rejected"), Subsystem->Request_SetSettingValue_Int32(
        FCk_Request_GameSettings_SetValue_Int32{TEXT("typed.missing"), 1}));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Registry_SetValueFiresChangeOnce,
    "Ck.CkGameSettings.Registry.SetValueFiresChangeOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Registry_SetValueFiresChangeOnce::RunTest(const FString&)
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();

    auto* Listener = NewObject<UCk_GameSettings_SpecListener_UE>();
    auto ListenerDelegate = FCk_Delegate_GameSettings_OnSettingChanged{};
    ListenerDelegate.BindDynamic(Listener, &UCk_GameSettings_SpecListener_UE::OnSettingChanged);
    Subsystem->BindTo_OnSettingChanged(TEXT("chg.bool"), ListenerDelegate);

    TestTrue(TEXT("register after bind"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("chg.bool"), ECk_GameSettings_ValueType::Bool, TEXT("false")}));
    TestEqual(TEXT("registration does not fire the change delegate"), Listener->Get_FireCount(), 0);

    TestTrue(TEXT("set true holds"), Subsystem->Request_SetSettingValue_Bool(
        FCk_Request_GameSettings_SetValue_Bool{TEXT("chg.bool"), true}));
    TestEqual(TEXT("change fired once"), Listener->Get_FireCount(), 1);
    TestEqual(TEXT("change carries the key"), Listener->Get_LastKey(), FName{TEXT("chg.bool")});
    TestEqual(TEXT("change carries the new value"), Listener->Get_LastValue(), TEXT("true"));

    TestTrue(TEXT("same-value set still holds"), Subsystem->Request_SetSettingValue_Bool(
        FCk_Request_GameSettings_SetValue_Bool{TEXT("chg.bool"), true}));
    TestEqual(TEXT("same-value set does NOT fire"), Listener->Get_FireCount(), 1);

    TestTrue(TEXT("set false holds"), Subsystem->Request_SetSettingValue_Bool(
        FCk_Request_GameSettings_SetValue_Bool{TEXT("chg.bool"), false}));
    TestEqual(TEXT("change fired again"), Listener->Get_FireCount(), 2);

    auto* WildcardListener = NewObject<UCk_GameSettings_SpecListener_UE>();
    auto WildcardDelegate = FCk_Delegate_GameSettings_OnSettingChanged{};
    WildcardDelegate.BindDynamic(WildcardListener, &UCk_GameSettings_SpecListener_UE::OnSettingChanged);
    Subsystem->BindTo_OnSettingChanged(NAME_None, WildcardDelegate);

    TestTrue(TEXT("set true fires both listeners"), Subsystem->Request_SetSettingValue_Bool(
        FCk_Request_GameSettings_SetValue_Bool{TEXT("chg.bool"), true}));
    TestEqual(TEXT("per-key listener fired"), Listener->Get_FireCount(), 3);
    TestEqual(TEXT("wildcard listener fired"), WildcardListener->Get_FireCount(), 1);
    TestEqual(TEXT("wildcard carries the concrete key"), WildcardListener->Get_LastKey(), FName{TEXT("chg.bool")});

    Subsystem->UnbindFrom_OnSettingChanged(TEXT("chg.bool"), ListenerDelegate);

    TestTrue(TEXT("set false after unbind holds"), Subsystem->Request_SetSettingValue_Bool(
        FCk_Request_GameSettings_SetValue_Bool{TEXT("chg.bool"), false}));
    TestEqual(TEXT("unbound listener no longer fires"), Listener->Get_FireCount(), 3);
    TestEqual(TEXT("wildcard listener still fires"), WildcardListener->Get_FireCount(), 2);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Registry_ResetAllRestoresDefaults,
    "Ck.CkGameSettings.Registry.ResetAllRestoresDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Registry_ResetAllRestoresDefaults::RunTest(const FString&)
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();

    TestTrue(TEXT("register r.a"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("r.a"), ECk_GameSettings_ValueType::Bool, TEXT("false")}));
    TestTrue(TEXT("register r.b"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("r.b"), ECk_GameSettings_ValueType::Int32, TEXT("10")}));
    TestTrue(TEXT("register r.c"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("r.c"), ECk_GameSettings_ValueType::String, TEXT("abc")}));

    TestTrue(TEXT("change r.a"), Subsystem->Request_SetSettingValue_Bool(
        FCk_Request_GameSettings_SetValue_Bool{TEXT("r.a"), true}));
    TestTrue(TEXT("change r.b"), Subsystem->Request_SetSettingValue_Int32(
        FCk_Request_GameSettings_SetValue_Int32{TEXT("r.b"), 20}));

    TestEqual(TEXT("reset-all resets exactly the two changed settings"), Subsystem->Request_ResetAllToDefaults(), 2);
    TestFalse(TEXT("r.a restored"), Subsystem->Get_SettingValue_Bool(TEXT("r.a"), true));
    TestEqual(TEXT("r.b restored"), Subsystem->Get_SettingValue_Int32(TEXT("r.b"), 0), 10);
    TestEqual(TEXT("r.c untouched"), Subsystem->Get_SettingValue_String(TEXT("r.c")), TEXT("abc"));

    TestEqual(TEXT("second reset-all is a no-op"), Subsystem->Request_ResetAllToDefaults(), 0);

    TestTrue(TEXT("change r.b again"), Subsystem->Request_SetSettingValue_Int32(
        FCk_Request_GameSettings_SetValue_Int32{TEXT("r.b"), 30}));
    TestTrue(TEXT("single reset succeeds"), Subsystem->Request_ResetToDefault(
        FCk_Request_GameSettings_ResetToDefault{TEXT("r.b")}));
    TestEqual(TEXT("r.b restored by single reset"), Subsystem->Get_SettingValue_Int32(TEXT("r.b"), 0), 10);

    AddExpectedError(TEXT("it is not registered"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("resetting an unknown key is rejected"), Subsystem->Request_ResetToDefault(
        FCk_Request_GameSettings_ResetToDefault{TEXT("r.missing")}));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Registry_RangeViolationRejected,
    "Ck.CkGameSettings.Registry.RangeViolationRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Registry_RangeViolationRejected::RunTest(const FString&)
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();

    TestTrue(TEXT("register ranged Int32"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("rng.int"), ECk_GameSettings_ValueType::Int32, TEXT("5")}
            .Set_MinValue(TEXT("0"))
            .Set_MaxValue(TEXT("10"))));

    AddExpectedError(TEXT("outside the allowed range"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("above-max set rejected"), Subsystem->Request_SetSettingValue_Int32(
        FCk_Request_GameSettings_SetValue_Int32{TEXT("rng.int"), 11}));
    TestEqual(TEXT("value unchanged after above-max"), Subsystem->Get_SettingValue_Int32(TEXT("rng.int"), -1), 5);

    TestFalse(TEXT("below-min set rejected"), Subsystem->Request_SetSettingValue_Int32(
        FCk_Request_GameSettings_SetValue_Int32{TEXT("rng.int"), -1}));
    TestEqual(TEXT("value unchanged after below-min"), Subsystem->Get_SettingValue_Int32(TEXT("rng.int"), -1), 5);

    TestTrue(TEXT("max boundary is inclusive"), Subsystem->Request_SetSettingValue_Int32(
        FCk_Request_GameSettings_SetValue_Int32{TEXT("rng.int"), 10}));
    TestEqual(TEXT("boundary value stored"), Subsystem->Get_SettingValue_Int32(TEXT("rng.int"), -1), 10);
    TestTrue(TEXT("min boundary is inclusive"), Subsystem->Request_SetSettingValue_Int32(
        FCk_Request_GameSettings_SetValue_Int32{TEXT("rng.int"), 0}));

    TestTrue(TEXT("register ranged Float"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("rng.float"), ECk_GameSettings_ValueType::Float, TEXT("0.5")}
            .Set_MinValue(TEXT("0"))
            .Set_MaxValue(TEXT("1"))));

    TestFalse(TEXT("out-of-range float set rejected"), Subsystem->Request_SetSettingValue_Float(
        FCk_Request_GameSettings_SetValue_Float{TEXT("rng.float"), 1.5f}));
    TestEqual(TEXT("float value unchanged"), Subsystem->Get_SettingValue_Float(TEXT("rng.float"), -1.0f), 0.5f);
    TestTrue(TEXT("in-range float set holds"), Subsystem->Request_SetSettingValue_Float(
        FCk_Request_GameSettings_SetValue_Float{TEXT("rng.float"), 1.0f}));

    TestTrue(TEXT("register Select-style String"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("opt.str"), ECk_GameSettings_ValueType::String, TEXT("low")}
            .Set_Options(TArray<FCk_GameSettings_SettingOption>
            {
                FCk_GameSettings_SettingOption{FText::FromString(TEXT("Low")), TEXT("low")},
                FCk_GameSettings_SettingOption{FText::FromString(TEXT("High")), TEXT("high")},
            })));

    AddExpectedError(TEXT("not one of the allowed options"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("non-option value rejected"), Subsystem->Request_SetSettingValue_String(
        FCk_Request_GameSettings_SetValue_String{TEXT("opt.str"), TEXT("medium")}));
    TestEqual(TEXT("value unchanged after rejected option"), Subsystem->Get_SettingValue_String(TEXT("opt.str")), TEXT("low"));

    TestTrue(TEXT("allowed option accepted"), Subsystem->Request_SetSettingValue_String(
        FCk_Request_GameSettings_SetValue_String{TEXT("opt.str"), TEXT("high")}));
    TestEqual(TEXT("option value stored"), Subsystem->Get_SettingValue_String(TEXT("opt.str")), TEXT("high"));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Registry_GameUserSettingsCVarBindingRejected,
    "Ck.CkGameSettings.Registry.GameUserSettingsCVarBindingRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Registry_GameUserSettingsCVarBindingRejected::RunTest(const FString&)
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();

    AddExpectedError(TEXT("which UGameUserSettings mirrors"), EAutomationExpectedErrorFlags::Contains, 0);

    TestFalse(TEXT("scalability-group CVar binding rejected"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("gus.shadow"), ECk_GameSettings_ValueType::Int32, TEXT("2")}
            .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::CVar)
            .Set_CVar(FCk_CVarRef{TEXT("sg.ShadowQuality"), ECk_CVarType::Int32})));

    TestFalse(TEXT("r.VSync binding rejected"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("gus.vsync"), ECk_GameSettings_ValueType::Bool, TEXT("true")}
            .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::CVar)
            .Set_CVar(FCk_CVarRef{TEXT("r.VSync"), ECk_CVarType::Bool})));

    TestFalse(TEXT("t.MaxFPS binding rejected"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("gus.fps"), ECk_GameSettings_ValueType::Float, TEXT("60")}
            .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::CVar)
            .Set_CVar(FCk_CVarRef{TEXT("t.MaxFPS"), ECk_CVarType::Float})));

    // FName comparison is case-insensitive, so a differently-cased spelling must not slip past the set.
    TestFalse(TEXT("differently-cased scalability CVar still rejected"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("gus.texture"), ECk_GameSettings_ValueType::Int32, TEXT("3")}
            .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::CVar)
            .Set_CVar(FCk_CVarRef{TEXT("SG.TEXTUREQUALITY"), ECk_CVarType::Int32})));

    TestEqual(TEXT("nothing registered from any rejected binding"), Subsystem->Get_AllSettingKeys().Num(), 0);

    // A game-owned CVar with a colliding-looking name is NOT owned by UGameUserSettings and must register.
    TestTrue(TEXT("unrelated CVar binding still registers"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("game.shadow_style"), ECk_GameSettings_ValueType::Int32, TEXT("1")}
            .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::CVar)
            .Set_CVar(FCk_CVarRef{TEXT("bb.ShadowQuality"), ECk_CVarType::Int32})));

    // The same value reached the sanctioned way — External, over the UGameUserSettings accessor.
    TestTrue(TEXT("External declaration of the same value accepted"), Subsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("video.sg.shadow"), ECk_GameSettings_ValueType::Int32, TEXT("2")}
            .Set_PersistencePolicy(ECk_GameSettings_PersistencePolicy::External)
            .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::Handler)));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Utils_UnbindIsTeardownSafe,
    "Ck.CkGameSettings.Utils.UnbindIsTeardownSafe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Utils_UnbindIsTeardownSafe::RunTest(const FString&)
{
    const auto Delegate = FCk_Delegate_GameSettings_OnSettingChanged{};

    // A valid UObject with no world models the context chain after world teardown. Cleanup must
    // quietly become a no-op rather than report a missing GameSettings subsystem.
    UCk_Utils_GameSettings_UE::UnbindFrom_OnSettingChanged(GetTransientPackage(), NAME_None, Delegate);

    TestTrue(TEXT("unbind returns cleanly after its world context is gone"), true);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_StorageRejectionIsAtomic,
    "Ck.CkGameSettings.TenetIntegrity.StorageRejectionIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_StorageRejectionIsAtomic::
    RunTest(
        const FString&)
    -> bool
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();
    auto* Provider = CastChecked<UCk_GameSettings_SpecStorageProvider_UE>(Subsystem->Get_StorageProvider());
    TestTrue(TEXT("two scopes registered"), Subsystem->Request_RegisterSettings({
        FCk_GameSettings_SettingDefinition{TEXT("atomic.a"), ECk_GameSettings_ValueType::String, TEXT("default.a")},
        FCk_GameSettings_SettingDefinition{TEXT("atomic.b"), ECk_GameSettings_ValueType::String, TEXT("default.b")}
            .Set_Scope(ECk_GameSettings_Scope::Player)}));
    TestTrue(TEXT("first change accepted"), Subsystem->Request_SetSettingValue_String({TEXT("atomic.a"), TEXT("changed.a")}));
    TestTrue(TEXT("second change accepted"), Subsystem->Request_SetSettingValue_String({TEXT("atomic.b"), TEXT("changed.b")}));
    TestTrue(TEXT("session begins"), Subsystem->Request_BeginPendingChanges());
    auto* Listener = NewObject<UCk_GameSettings_SpecListener_UE>();
    auto Changed = FCk_Delegate_GameSettings_OnSettingChanged{};
    Changed.BindDynamic(Listener, &UCk_GameSettings_SpecListener_UE::OnSettingChanged);
    Subsystem->BindTo_OnSettingChanged(NAME_None, Changed);
    const auto StoredBefore = Provider->Get_Values();
    Provider->Set_RejectWrites(true);
    TestFalse(TEXT("single rejected write reports failure"), Subsystem->Request_SetSettingValue_String({TEXT("atomic.a"), TEXT("rejected")}));
    TestEqual(TEXT("reset-all rejects entire two-scope batch"), Subsystem->Request_ResetAllToDefaults(), -1);
    TestEqual(TEXT("first live value preserved"), Subsystem->Get_SettingValue_String(TEXT("atomic.a")), TEXT("changed.a"));
    TestEqual(TEXT("second live value preserved"), Subsystem->Get_SettingValue_String(TEXT("atomic.b")), TEXT("changed.b"));
    TestEqual(TEXT("provider entry count preserved"), Provider->Get_Values().Num(), StoredBefore.Num());
    TestEqual(TEXT("no rejected callback"), Listener->Get_FireCount(), 0);
    TestFalse(TEXT("no rejected pending prior"), Subsystem->Get_HasPendingChanges());
    Provider->Set_RejectWrites(false);
    const auto BeforeBatch = Provider->Get_BatchCount();
    TestEqual(TEXT("reset-all recovery changes both"), Subsystem->Request_ResetAllToDefaults(), 2);
    TestEqual(TEXT("one provider admission for both scopes"), Provider->Get_BatchCount(), BeforeBatch + 1);
    Provider->Set_RejectWrites(true);
    TestEqual(TEXT("revert rejection reports failure"), Subsystem->Request_RevertPendingChanges(), -1);
    TestTrue(TEXT("revert rejection retains session priors"), Subsystem->Get_HasPendingChanges());
    TestEqual(TEXT("failed revert preserves first current value"), Subsystem->Get_SettingValue_String(TEXT("atomic.a")), TEXT("default.a"));
    Provider->Set_RejectWrites(false);
    TestEqual(TEXT("retry reverts both"), Subsystem->Request_RevertPendingChanges(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_ReloadAndFlushFailurePreserveState,
    "Ck.CkGameSettings.TenetIntegrity.ReloadAndFlushFailurePreserveState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_ReloadAndFlushFailurePreserveState::
    RunTest(
        const FString&)
    -> bool
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();
    auto* Provider = CastChecked<UCk_GameSettings_SpecStorageProvider_UE>(Subsystem->Get_StorageProvider());
    Subsystem->Request_RegisterSetting({TEXT("reload.a"), ECk_GameSettings_ValueType::Int32, TEXT("1")});
    Subsystem->Request_BeginPendingChanges();
    TestTrue(TEXT("live change accepted"), Subsystem->Request_SetSettingValue_Int32({TEXT("reload.a"), 2}));
    Provider->Set_RejectFlush(true);
    TestFalse(TEXT("flush propagates provider rejection"), Subsystem->Request_FlushStorage());
    TestEqual(TEXT("apply reports flush failure"), Subsystem->Request_ApplyPendingChanges(), -1);
    TestTrue(TEXT("apply failure retains session priors"), Subsystem->Get_HasPendingChanges());
    Provider->Set_Values({
        FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Machine, 0, TEXT("reload.a"), TEXT("7")},
        FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Player, 0, TEXT("future.orphan"), TEXT("later")}});
    Provider->Set_RejectPlayerRead(true);
    TestEqual(TEXT("one failed scope rejects both"), Subsystem->Request_ReloadFromStorage(), -1);
    TestEqual(TEXT("machine live value preserved"), Subsystem->Get_SettingValue_Int32(TEXT("reload.a"), 0), 2);
    Provider->Set_RejectPlayerRead(false);
    auto FixtureValues = Provider->Get_Values();
    FixtureValues[0] = FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Machine, 0, TEXT("reload.a"), TEXT("not-an-int")};
    Provider->Set_Values(FixtureValues);
    AddExpectedError(TEXT("GameSettings reload rejected invalid stored value"), EAutomationExpectedErrorFlags::Contains, 0);
    TestEqual(TEXT("invalid known value rejects whole reload"), Subsystem->Request_ReloadFromStorage(), -1);
    TestEqual(TEXT("invalid reload preserves current value"), Subsystem->Get_SettingValue_Int32(TEXT("reload.a"), 0), 2);
    FixtureValues[0] = FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Machine, 0, TEXT("reload.a"), TEXT("7")};
    Provider->Set_Values(FixtureValues);
    FixtureValues.Add(FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Machine, 0, TEXT("reload.a"), TEXT("8")});
    Provider->Set_Values(FixtureValues);
    AddExpectedError(TEXT("GameSettings reload contains missing or duplicate key"), EAutomationExpectedErrorFlags::Contains, 0);
    TestEqual(TEXT("duplicate persisted key rejects whole reload"), Subsystem->Request_ReloadFromStorage(), -1);
    TestEqual(TEXT("duplicate reload preserves current value"), Subsystem->Get_SettingValue_Int32(TEXT("reload.a"), 0), 2);
    TestTrue(TEXT("duplicate reload preserves pending priors"), Subsystem->Get_HasPendingChanges());
    FixtureValues.Pop();
    Provider->Set_Values(FixtureValues);
    FixtureValues.Add(FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Machine, 0, TEXT("reload.bad"), TEXT("orphan")});
    Provider->Set_Values(FixtureValues);
    const auto WritesBefore = Provider->Get_BatchCount();
    TestEqual(TEXT("successful reload reports all values"), Subsystem->Request_ReloadFromStorage(), 3);
    TestEqual(TEXT("reload never writes back to provider"), Provider->Get_BatchCount(), WritesBefore);
    TestEqual(TEXT("reloaded value applied"), Subsystem->Get_SettingValue_Int32(TEXT("reload.a"), 0), 7);
    AddExpectedError(TEXT("Invalid stored value for GameSettings key"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("invalid saved orphan rejects whole registration batch"), Subsystem->Request_RegisterSettings({
        FCk_GameSettings_SettingDefinition{TEXT("admission.peer"), ECk_GameSettings_ValueType::String, TEXT("default")},
        FCk_GameSettings_SettingDefinition{TEXT("reload.bad"), ECk_GameSettings_ValueType::Int32, TEXT("0")}}));
    TestFalse(TEXT("valid definition prefix was not published"), Subsystem->Get_IsSettingRegistered(TEXT("admission.peer")));
    TestFalse(TEXT("invalid orphan definition was not published"), Subsystem->Get_IsSettingRegistered(TEXT("reload.bad")));
    TestTrue(TEXT("rejected admission retains saved orphan for corrected schema"), Subsystem->Request_RegisterSetting(
        {TEXT("reload.bad"), ECk_GameSettings_ValueType::String, TEXT("default")}));
    TestEqual(TEXT("retained orphan still applies"), Subsystem->Get_SettingValue_String(TEXT("reload.bad")), TEXT("orphan"));
    Provider->Set_RejectFlush(false);
    TestEqual(TEXT("apply retry commits retained session"), Subsystem->Request_ApplyPendingChanges(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_CallbackCannotMutateAdmittedBatch,
    "Ck.CkGameSettings.TenetIntegrity.CallbackCannotMutateAdmittedBatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_CallbackCannotMutateAdmittedBatch::
    RunTest(
        const FString&)
    -> bool
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();
    Subsystem->Request_RegisterSettings({
        FCk_GameSettings_SettingDefinition{TEXT("atomic.a"), ECk_GameSettings_ValueType::String, TEXT("default.a")}
            .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::Handler),
        FCk_GameSettings_SettingDefinition{TEXT("atomic.b"), ECk_GameSettings_ValueType::String, TEXT("default.b")}});
    Subsystem->Request_SetSettingValue_String({TEXT("atomic.a"), TEXT("changed.a")});
    Subsystem->Request_SetSettingValue_String({TEXT("atomic.b"), TEXT("changed.b")});
    auto* Callback = NewObject<UCk_GameSettings_SpecCallback_UE>();
    Callback->Set_Subsystem(Subsystem);
    auto Handler = FCk_Delegate_GameSettings_ApplyHandler_String{};
    Handler.BindDynamic(Callback, &UCk_GameSettings_SpecCallback_UE::Request_ApplyAndReenter);
    Subsystem->Request_RegisterApplyHandler_String(TEXT("atomic.a"), Handler);
    TestTrue(TEXT("callback batch session begins"), Subsystem->Request_BeginPendingChanges());
    Callback->Set_Reenter(true);
    AddExpectedError(TEXT("Nested GameSettings mutation from a settings callback is rejected"), EAutomationExpectedErrorFlags::Contains, 0);
    TestEqual(TEXT("outer atomic batch completes"), Subsystem->Request_ResetAllToDefaults(), 2);
    TestFalse(TEXT("nested write explicitly rejected"), Callback->Get_NestedAccepted());
    TestEqual(TEXT("callback sees whole batch admitted"), Callback->Get_ObservedB(), TEXT("default.b"));
    TestEqual(TEXT("nested write cannot diverge live state"), Subsystem->Get_SettingValue_String(TEXT("atomic.b")), TEXT("default.b"));
    TestEqual(TEXT("revert accepts both prior values"), Subsystem->Request_RevertPendingChanges(), 2);
    TestFalse(TEXT("revert callback observes no pending session changes"), Callback->Get_ObservedPending());
    TestFalse(TEXT("revert callback observes no pending key prior"), Callback->Get_ObservedUnapplied());
    TestEqual(TEXT("revert callback sees complete restored batch"), Callback->Get_ObservedB(), TEXT("changed.b"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_ExternalSetterRejectionIsAtomic,
    "Ck.CkGameSettings.TenetIntegrity.ExternalSetterRejectionIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_ExternalSetterRejectionIsAtomic::
    RunTest(
        const FString&)
    -> bool
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();
    Subsystem->Request_RegisterSettings({
        FCk_GameSettings_SettingDefinition{TEXT("external.bool"), ECk_GameSettings_ValueType::Bool, TEXT("false")}
            .Set_PersistencePolicy(ECk_GameSettings_PersistencePolicy::External),
        FCk_GameSettings_SettingDefinition{TEXT("external.int"), ECk_GameSettings_ValueType::Int32, TEXT("0")}
            .Set_PersistencePolicy(ECk_GameSettings_PersistencePolicy::External),
        FCk_GameSettings_SettingDefinition{TEXT("external.float"), ECk_GameSettings_ValueType::Float, TEXT("0")}
            .Set_PersistencePolicy(ECk_GameSettings_PersistencePolicy::External),
        FCk_GameSettings_SettingDefinition{TEXT("external.string"), ECk_GameSettings_ValueType::String, TEXT("default")}
            .Set_PersistencePolicy(ECk_GameSettings_PersistencePolicy::External)});
    auto* Listener = NewObject<UCk_GameSettings_SpecListener_UE>();
    auto Changed = FCk_Delegate_GameSettings_OnSettingChanged{};
    Changed.BindDynamic(Listener, &UCk_GameSettings_SpecListener_UE::OnSettingChanged);
    Subsystem->BindTo_OnSettingChanged(NAME_None, Changed);
    Subsystem->Request_BeginPendingChanges();
    AddExpectedError(TEXT("requires a bound getter and setter"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("missing external pair rejects write"), Subsystem->Request_SetSettingValue_Bool({TEXT("external.bool"), true}));
    TestFalse(TEXT("missing int pair rejects write"), Subsystem->Request_SetSettingValue_Int32({TEXT("external.int"), 1}));
    TestFalse(TEXT("missing float pair rejects write"), Subsystem->Request_SetSettingValue_Float({TEXT("external.float"), 1.0f}));
    TestFalse(TEXT("missing string pair rejects write"), Subsystem->Request_SetSettingValue_String({TEXT("external.string"), TEXT("new")}));
    TestFalse(TEXT("rejected write records no session prior"), Subsystem->Get_HasPendingChanges());
    TestEqual(TEXT("rejected write emits no notification"), Listener->Get_FireCount(), 0);
    auto* Callback = NewObject<UCk_GameSettings_SpecCallback_UE>();
    auto Getter = FCk_Delegate_GameSettings_ExternalGetter_Bool{};
    Getter.BindDynamic(Callback, &UCk_GameSettings_SpecCallback_UE::Get_ExternalBool_ForDelegate);
    auto Setter = FCk_Delegate_GameSettings_ApplyHandler_Bool{};
    Setter.BindDynamic(Callback, &UCk_GameSettings_SpecCallback_UE::Request_SetBool);
    TestTrue(TEXT("complete pair accepted"), Subsystem->Request_RegisterExternalAccessors_Bool(TEXT("external.bool"), Getter, Setter));
    AddExpectedError(TEXT("external accessors require a bound getter and setter"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("incomplete replacement rejected"), Subsystem->Request_RegisterExternalAccessors_Bool(TEXT("external.bool"), Getter, {}));
    TestTrue(TEXT("prior pair remains usable"), Subsystem->Request_SetSettingValue_Bool({TEXT("external.bool"), true}));
    TestTrue(TEXT("external store receives accepted value"), Callback->Get_ExternalBool());
    TestTrue(TEXT("idempotent external write succeeds"), Subsystem->Request_SetSettingValue_Bool({TEXT("external.bool"), true}));
    TestEqual(TEXT("one external write applied"), Callback->Get_ApplyCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_ResolutionConfirmRejectsInvalidWindow,
    "Ck.CkGameSettings.TenetIntegrity.ResolutionConfirmRejectsInvalidWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_ResolutionConfirmRejectsInvalidWindow::
    RunTest(
        const FString&)
    -> bool
{
    auto* Subsystem = ck_game_settings_registry_spec::Make_Subsystem();
    const auto* UserSettings = GEngine->GetGameUserSettings();
    const auto Prior = UserSettings != nullptr ? UserSettings->GetScreenResolution() : FIntPoint::ZeroValue;
    auto* Listener = NewObject<UCk_GameSettings_SpecListener_UE>();
    auto Changed = FCk_Delegate_GameSettings_OnSettingChanged{};
    Changed.BindDynamic(Listener, &UCk_GameSettings_SpecListener_UE::OnSettingChanged);
    Subsystem->BindTo_OnSettingChanged(NAME_None, Changed);
    AddExpectedError(TEXT("Resolution confirmation duration must be finite and positive"), EAutomationExpectedErrorFlags::Contains, 0);
    for (const auto Duration : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.0f, -1.0f})
    {
        TestFalse(TEXT("invalid duration is rejected"), Subsystem->Request_SetResolutionWithConfirmWindow(TEXT("1280x720"), Duration));
    }
    TestEqual(TEXT("no rejected resolution notification"), Listener->Get_FireCount(), 0);
    if (UserSettings != nullptr)
    { TestTrue(TEXT("actual user resolution unchanged"), UserSettings->GetScreenResolution() == Prior); }
    AddExpectedError(TEXT("No resolution confirm window is active"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("rejected requests never arm a confirmation window"), Subsystem->Request_ConfirmResolution());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
#include <HAL/IConsoleManager.h>
#include <Misc/ScopeExit.h>
#include <UObject/StrongObjectPtr.h>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_GameSettings_TimeoutFixture_StartReleaseIsIdempotent,
    "Ck.CkGameSettings.Registry.TimeoutFixture.StartReleaseIsIdempotent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TimeoutFixture_StartReleaseIsIdempotent::
    RunTest(
        const FString&)
    -> bool
{
    auto Fixture = TStrongObjectPtr<UCk_GameSettings_TimeoutSpecFixture_UE>{
        NewObject<UCk_GameSettings_TimeoutSpecFixture_UE>()};
    ON_SCOPE_EXIT { Fixture->Request_Release(); };
    auto* Started = Fixture->Request_Start();
    if (NOT TestNotNull(TEXT("actual isolated fixture starts"), Started))
    { return false; }
    const auto KeptSubsystem = TStrongObjectPtr<UCk_GameSettings_Subsystem_UE>{Started};
    TestTrue(TEXT("repeated start returns the same live subsystem"), Fixture->Request_Start() == Started);
    TestEqual(TEXT("new isolated registry starts empty"), Started->Get_AllSettingKeys().Num(), 0);
    TestTrue(TEXT("release reports ticker, deferred queue and registry cleared by the existing fixture helper"),
        Fixture->Request_Release());
    TestTrue(TEXT("release is safely repeatable"), Fixture->Request_Release());
    TestNull(TEXT("a released started fixture cannot start a second ticker"), Fixture->Request_Start());
    TestEqual(TEXT("kept subsystem confirms released registry is empty"), Started->Get_AllSettingKeys().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_GameSettings_TimeoutFixture_EarlyReleaseCancelsPendingAndIsolates,
    "Ck.CkGameSettings.Registry.TimeoutFixture.EarlyReleaseCancelsPendingAndIsolates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TimeoutFixture_EarlyReleaseCancelsPendingAndIsolates::
    RunTest(
        const FString&)
    -> bool
{
    const auto MissingName = TEXT("ck.tests.registry.timeoutfixture.cleanup.missing");
    if (NOT TestNull(TEXT("control CVar is actually absent; the test never registers it"),
            IConsoleManager::Get().FindConsoleVariable(MissingName)))
    { return false; }
    auto First = TStrongObjectPtr<UCk_GameSettings_TimeoutSpecFixture_UE>{
        NewObject<UCk_GameSettings_TimeoutSpecFixture_UE>()};
    auto Other = TStrongObjectPtr<UCk_GameSettings_TimeoutSpecFixture_UE>{
        NewObject<UCk_GameSettings_TimeoutSpecFixture_UE>()};
    ON_SCOPE_EXIT
    {
        First->Request_Release();
        Other->Request_Release();
    };
    auto* FirstSubsystem = First->Request_Start();
    auto* OtherSubsystem = Other->Request_Start();
    if (NOT TestNotNull(TEXT("first fixture starts"), FirstSubsystem)
        || NOT TestNotNull(TEXT("other fixture starts"), OtherSubsystem))
    { return false; }
    const auto KeptFirst = TStrongObjectPtr<UCk_GameSettings_Subsystem_UE>{FirstSubsystem};
    TestTrue(TEXT("fixtures own distinct registries"), FirstSubsystem != OtherSubsystem);
    TestTrue(TEXT("fixtures own distinct in-memory providers"),
        FirstSubsystem->Get_StorageProvider() != OtherSubsystem->Get_StorageProvider());
    const auto Definition = FCk_GameSettings_SettingDefinition{
        TEXT("cleanup.pending"), ECk_GameSettings_ValueType::Float, TEXT("0.25")}
        .Set_ApplyBindingType(ECk_GameSettings_ApplyBindingType::CVar)
        .Set_CVar(FCk_CVarRef{MissingName, ECk_CVarType::Float});
    TestTrue(TEXT("actual missing-CVar definition registers"), FirstSubsystem->Request_RegisterSetting(Definition));
    TestTrue(TEXT("actual public write queues deferred application"), FirstSubsystem->Request_SetSettingValue_Float(
        FCk_Request_GameSettings_SetValue_Float{TEXT("cleanup.pending"), 0.75f}));
    if (NOT TestTrue(TEXT("actual apply is pending before any ticker is pumped"),
            FirstSubsystem->Get_HasPendingCVarApply(TEXT("cleanup.pending"))))
    { return false; }
    TestFalse(TEXT("other registry never acquired the first definition"),
        OtherSubsystem->Get_IsSettingRegistered(TEXT("cleanup.pending")));
    TestFalse(TEXT("other registry never acquired the first pending apply"),
        OtherSubsystem->Get_HasPendingCVarApply(TEXT("cleanup.pending")));
    TestTrue(TEXT("other registry accepts an independent ordinary value"), OtherSubsystem->Request_RegisterSetting(
        FCk_GameSettings_SettingDefinition{TEXT("cleanup.other"), ECk_GameSettings_ValueType::Int32, TEXT("7")}));
    // Synchronous test: no FTSTicker pump, sleep, manual timeout or extraction/replay of ticker logic.
    TestTrue(TEXT("early release reports actual ticker, queue and registry cleanup"), First->Request_Release());
    TestFalse(TEXT("kept first subsystem has no deferred apply after release"),
        FirstSubsystem->Get_HasPendingCVarApply(TEXT("cleanup.pending")));
    TestFalse(TEXT("kept first subsystem has no definition after release"),
        FirstSubsystem->Get_IsSettingRegistered(TEXT("cleanup.pending")));
    TestEqual(TEXT("first registry cardinality is zero after release"), FirstSubsystem->Get_AllSettingKeys().Num(), 0);
    TestTrue(TEXT("repeat early release is safe"), First->Request_Release());
    TestNull(TEXT("released first fixture stays stopped"), First->Request_Start());
    TestTrue(TEXT("first release does not release the other fixture"), Other->Request_Start() == OtherSubsystem);
    TestEqual(TEXT("other registry retains its independent value"),
        OtherSubsystem->Get_SettingValue_Int32(TEXT("cleanup.other"), -1), 7);
    TestTrue(TEXT("other fixture releases independently"), Other->Request_Release());
    TestTrue(TEXT("other release is repeatable"), Other->Request_Release());
    TestNull(TEXT("other started fixture also remains released"), Other->Request_Start());
    TestNull(TEXT("control CVar remains unregistered"), IConsoleManager::Get().FindConsoleVariable(MissingName));
    return true;
}
#endif
