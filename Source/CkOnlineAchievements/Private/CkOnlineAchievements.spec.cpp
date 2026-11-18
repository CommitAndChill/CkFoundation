#include "CkOnlineAchievements/CkOnlineAchievements_Subsystem.h"
#include "CkOnlineAchievements_OperationOwner.h"

#include <Engine/GameInstance.h>
#include <Misc/App.h>
#include <Misc/AutomationTest.h>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOnlineAchievementsOperationOwnership,
    "Ck.Online.Achievements.NativeOperationOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkOnlineAchievementsOperationOwnership::RunTest(const FString&) -> bool
{
    using namespace ck::online_achievements;
    FCallbackIngress Ingress;
    FOperationOwner Owner(Ingress);
    const FIdentity Identity{11, 22, 33, 44};
    int32 NativeCalls = 0;
    TArray<FCompletion> Completions;
    const auto ExecuteNative = [&] { ++NativeCalls; return true; };

    TestTrue(TEXT("Progress is its own native operation"), Owner.TryBegin(EOperation::Progress, Identity, 0.0, ExecuteNative));
    TestFalse(TEXT("Store cannot overlap progress"), Owner.TryBegin(EOperation::Store, Identity, 1.0, ExecuteNative));
    Ingress.Receive(44, 1);
    Ingress.Receive(44, 1);
    Owner.Pump(Identity, 2.0, [&](FCompletion Completion)
    {
        Completions.Add(Completion);
        TestTrue(TEXT("Native progress callback retains its operation kind"), Completion.Operation == EOperation::Progress);
        TestFalse(TEXT("Notification reentry cannot begin Store"), Owner.TryBegin(EOperation::Store, Identity, 2.0, ExecuteNative));
    });
    TestEqual(TEXT("Duplicate queued progress callbacks complete once"), Completions.Num(), 1);
    TestEqual(TEXT("Blocked/reentrant Store did not reach native provider"), NativeCalls, 1);
    TestTrue(TEXT("Store starts after progress dispatch has returned"), Owner.TryBegin(EOperation::Store, Identity, 3.0, ExecuteNative));
    Owner.Pump(Identity, 4.0, [&](FCompletion Completion) { Completions.Add(Completion); });
    TestTrue(TEXT("Old progress payload cannot acknowledge a new Store"), Owner.IsPending());
    TestEqual(TEXT("No fake Store completion"), Completions.Num(), 1);
    Ingress.Receive(44, 1);
    Owner.Pump(Identity, 5.0, [&](FCompletion Completion) { Completions.Add(Completion); });
    TestFalse(TEXT("Own Store callback settles"), Owner.IsPending());

    TestTrue(TEXT("Reset is admitted without an explicit Store"), Owner.TryBegin(EOperation::Reset, Identity, 6.0, ExecuteNative));
    TestFalse(TEXT("Store cannot overlap internally storing Reset"), Owner.TryBegin(EOperation::Store, Identity, 7.0, ExecuteNative));
    Ingress.Receive(44, 1);
    Owner.Pump(Identity, 8.0, [&](FCompletion Completion)
    {
        TestTrue(TEXT("Reset completion is not a Store completion"), Completion.Operation == EOperation::Reset);
        Completions.Add(Completion);
    });
    TestEqual(TEXT("Only progress, Store and Reset reached native provider"), NativeCalls, 3);

    TestTrue(TEXT("New Store starts"), Owner.TryBegin(EOperation::Store, Identity, 10.0, ExecuteNative));
    Ingress.Receive(99, 1);
    Owner.Pump(Identity, 11.0, [&](FCompletion Completion) { Completions.Add(Completion); });
    TestTrue(TEXT("Foreign app callback is ignored"), Owner.IsPending());
    Owner.Pump(Identity, 41.0, [&](FCompletion Completion)
    {
        TestEqual(TEXT("Deadline reports unconfirmed outcome"), Completion.Result, -1);
        Completions.Add(Completion);
    });
    TestTrue(TEXT("Timeout retains native lease"), Owner.IsUnconfirmed());
    TestFalse(TEXT("No operation can follow an unconfirmed outcome"), Owner.TryBegin(EOperation::Reset, Identity, 42.0, ExecuteNative));
    Owner.Detach();
    const FIdentity NewOwner{12, 22, 33, 44};
    TestFalse(TEXT("Owner replacement cannot steal unresolved native callback"), Owner.Acquire(NewOwner));
    const auto BeforeLate = Completions.Num();
    Ingress.Receive(44, 1);
    Owner.Pump(NewOwner, 43.0, [&](FCompletion Completion) { Completions.Add(Completion); });
    TestEqual(TEXT("Retired owner receives no late callback"), Completions.Num(), BeforeLate);
    TestTrue(TEXT("New owner acquires only after late settlement"), Owner.Acquire(NewOwner));

    TestTrue(TEXT("New owner can start Store"), Owner.TryBegin(EOperation::Store, NewOwner, 44.0, ExecuteNative));
    const FIdentity ChangedAccount{12, 23, 33, 44};
    TestFalse(TEXT("Account switch rejects admission"), Owner.Acquire(ChangedAccount));
    Ingress.Receive(44, 1);
    Owner.Pump(ChangedAccount, 45.0, [&](FCompletion Completion)
    {
        TestEqual(TEXT("Old account success cannot acknowledge changed account"), Completion.Result, -3);
    });
    TestFalse(TEXT("Returning to original account does not replay retained writes"), Owner.TryBegin(EOperation::Store, NewOwner, 46.0, ExecuteNative));
    TestEqual(TEXT("Account change caused no additional native call"), NativeCalls, 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkOnlineAchievementsUnavailable,
    "Ck.Online.Achievements.UnattendedCannotWriteSteamAccount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkOnlineAchievementsUnavailable::RunTest(const FString&) -> bool
{
    if (!FApp::IsUnattended())
    {
        AddInfo(TEXT("Requires -unattended; live account access is intentionally not tested"));
        return true;
    }
    auto* Game = NewObject<UGameInstance>();
    auto* Service = NewObject<UCk_OnlineAchievements_Subsystem_UE>(Game);
    TestFalse(TEXT("Automation cannot acquire the Steam provider"), Service->Get_IsAvailable());
    TestFalse(TEXT("Automation cannot report live readiness"), Service->Get_IsReady());
    bool Unlocked = true;
    int32 Value = 123;
    TestFalse(TEXT("Achievement read rejects"), Service->TryGetAchievement(TEXT("TEST"), Unlocked));
    TestFalse(TEXT("Rejected read clears unlocked output"), Unlocked);
    TestFalse(TEXT("Integer stat read rejects"), Service->TryGetStatInt(TEXT("test"), Value));
    TestEqual(TEXT("Rejected read clears integer output"), Value, 0);
    TestFalse(TEXT("Integer stat write rejects"), Service->SetStatInt(TEXT("test"), 5));
    TestFalse(TEXT("Achievement write rejects"), Service->SetAchievement(TEXT("TEST")));
    TestFalse(TEXT("Progress rejects"), Service->IndicateAchievementProgress(TEXT("TEST"), 1, 10));
    TestFalse(TEXT("Reset rejects"), Service->ResetAllStats(true));
    TestFalse(TEXT("Store rejects"), Service->StoreStats());
    TestFalse(TEXT("Rejected store does not publish pending state"), Service->Get_IsStorePending());
    Service->Deinitialize();
    return true;
}
#endif
