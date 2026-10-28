#include "CkEcs/Pacing/CkSharedPacedWork.h"
#include "CkEcs/Pacing/CkPacedWork.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_SharedPacedWork_Budget,
    "Ck.CkEcs.Pacing.SharedWork.Budget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTest_SharedPacedWork_Budget::RunTest(const FString&) -> bool
{
    auto Now = 0.0;
    auto Budget = ck::FSharedFrameBudget{2, FCk_Time{1.0}, [&Now] { return Now; }};
    TestTrue(TEXT("First explicit frame accepted"), Budget.BeginFrame(0));
    TestTrue(TEXT("First consumer starts"), Budget.BeginPump());
    TestTrue(TEXT("First quota consumed before callback"), Budget.TryConsumeStep());
    Now = 0.4;
    TestEqual(TEXT("First consumer elapsed"), Budget.EndPump().Get_Seconds(), 0.4);
    Now = 100.0;
    TestFalse(TEXT("Repeated frame does not replenish"), Budget.BeginFrame(0));
    TestTrue(TEXT("Another consumer shares frame ledger"), Budget.BeginPump());
    TestTrue(TEXT("Idle wall time is excluded"), Budget.TryConsumeStep());
    TestFalse(TEXT("Step cap shared between consumers"), Budget.TryConsumeStep());
    Budget.EndPump();
    TestEqual(TEXT("Two total steps"), Budget.Get_StepsConsumed(), 2);
    TestTrue(TEXT("New frame replenishes"), Budget.BeginFrame(1));
    TestTrue(TEXT("New frame pump"), Budget.BeginPump());
    TestTrue(TEXT("New frame first step"), Budget.TryConsumeStep());
    Now = 101.25;
    TestFalse(TEXT("An expensive callback stops subsequent work"), Budget.TryConsumeStep());
    TestEqual(TEXT("Overrun remains observable"), Budget.EndPump().Get_Seconds(), 1.25);
    TestFalse(TEXT("Older frame rejected"), Budget.BeginFrame(0));
    TestFalse(TEXT("Negative frame rejected"), Budget.BeginFrame(-1));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_SharedPacedWork_Fairness,
    "Ck.CkEcs.Pacing.SharedWork.Fairness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTest_SharedPacedWork_Fairness::RunTest(const FString&) -> bool
{
    auto Work = ck::FSharedPacedWork{2, FCk_Time{1.0}, [] { return 0.0; }};
    Work.Enqueue(1);
    Work.Enqueue(2);
    auto Visited = TArray<int64>{};
    const auto Discover = TFunction<bool()>{[] { return true; }};
    const auto Step = TFunction<bool(int64)>{[&Visited](int64 InId)
    {
        Visited.Add(InId);
        return false;
    }};
    const auto First = Work.Pump(4, Discover, Step);
    TestEqual(TEXT("Discovery shares quota"), First.DiscoverySteps, 1);
    TestEqual(TEXT("Infinite discovery cannot starve queued work"), First.ExecutedSteps, 1);
    TestFalse(TEXT("Only one pump per frame"), Work.Pump(4, Discover, Step).Pumped);
    Work.Pump(5, Discover, Step);
    Work.Pump(6, Discover, Step);
    TestEqual(TEXT("Three execution visits"), Visited.Num(), 3);
    if (Visited.Num() == 3)
    {
        TestEqual(TEXT("First job"), Visited[0], int64{1});
        TestEqual(TEXT("Next frame resumes round robin"), Visited[1], int64{2});
        TestEqual(TEXT("Wraparound remains fair"), Visited[2], int64{1});
    }
    TestFalse(TEXT("Duplicate ID not admitted"), Work.Enqueue(1));
    TestEqual(TEXT("Duplicate leaves queue unchanged"), Work.Get_NumPending(), 2);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_SharedPacedWork_Mutation,
    "Ck.CkEcs.Pacing.SharedWork.Mutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTest_SharedPacedWork_Mutation::RunTest(const FString&) -> bool
{
    auto Work = ck::FSharedPacedWork{4, FCk_Time{1.0}, [] { return 0.0; }};
    Work.Enqueue(1);
    Work.Enqueue(2);
    Work.Enqueue(3);
    auto Visited = TArray<int64>{};
    const auto Step = TFunction<bool(int64)>{[&](int64 InId)
    {
        Visited.Add(InId);
        if (InId == 1 && Visited.Num() == 1)
        {
            Work.Cancel(1);
            Work.Enqueue(1);
            Work.Cancel(2);
            TestFalse(TEXT("Reentrant pump rejected"), Work.Pump(1, {}, [](int64) { return true; }).Pumped);
        }
        return true;
    }};
    const auto Result = Work.Pump(0, {}, Step);
    TestEqual(TEXT("Cancelled next job skipped; re-enqueued generation retained"), Result.ExecutedSteps, 3);
    if (Visited.Num() == 3)
    {
        TestEqual(TEXT("First original job"), Visited[0], int64{1});
        TestEqual(TEXT("Cancelled next skips to third"), Visited[1], int64{3});
        TestEqual(TEXT("Old completion does not remove new same-ID job"), Visited[2], int64{1});
    }
    TestEqual(TEXT("All surviving jobs complete"), Work.Get_NumPending(), 0);

    Work.Enqueue(7);
    const auto Cleared = Work.Pump(1, {}, [&Work](int64)
    {
        Work.Clear();
        Work.Enqueue(8);
        return true;
    });
    TestEqual(TEXT("Clear terminates active generation"), Cleared.ExecutedSteps, 1);
    TestTrue(TEXT("New generation remains pending"), Work.Get_Contains(8));
    TestFalse(TEXT("Clear cannot refill the same frame"), Work.Pump(1, {}, Step).Pumped);
    TestEqual(TEXT("New generation runs next frame"), Work.Pump(2, {}, Step).ExecutedSteps, 1);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_SharedPacedWork_CancellationAndDiscovery,
    "Ck.CkEcs.Pacing.SharedWork.CancellationAndDiscovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTest_SharedPacedWork_CancellationAndDiscovery::RunTest(const FString&) -> bool
{
    auto Now = 0.0;
    auto Work = ck::FSharedPacedWork{20, FCk_Time{1.0}, [&Now] { return Now; }};
    auto DiscoveryCalls = 0;
    auto WorkCalls = 0;
    const auto Result = Work.Pump(0, [&]
    {
        ++DiscoveryCalls;
        Work.Enqueue(10);
        Now += 1.1;
        return true;
    }, [&](int64)
    {
        ++WorkCalls;
        return true;
    });
    TestEqual(TEXT("Discovery counted"), Result.DiscoverySteps, 1);
    TestEqual(TEXT("Discovery time charged before execution"), WorkCalls, 0);
    TestTrue(TEXT("Discovery overrun reported"), Result.Elapsed.Get_Seconds() >= 1.1);
    auto CanContinue = true;
    const auto Stopped = Work.Pump(1, {}, [&](int64)
    {
        CanContinue = false;
        return false;
    }, [&] { return CanContinue; });
    TestEqual(TEXT("Stop predicate halts subsequent callbacks"), Stopped.ExecutedSteps, 1);
    TestTrue(TEXT("Stop preserves queued job"), Work.Get_Contains(10));
    TestFalse(TEXT("Missing cancel is expected rejection"), Work.Cancel(999));
    const auto CancelledByPredicate = Work.Pump(2, {}, [](int64) { return true; }, [&]
    {
        Work.Cancel(10);
        return true;
    });
    TestEqual(TEXT("Predicate cancellation invokes no work callback"), CancelledByPredicate.ExecutedSteps, 0);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_SharedPacedWork_InvalidInput,
    "Ck.CkEcs.Pacing.SharedWork.InvalidInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTest_SharedPacedWork_InvalidInput::RunTest(const FString&) -> bool
{
    auto Invalid = ck::FSharedPacedWork{};
    auto Calls = 0;
    const auto Step = TFunction<bool(int64)>{[&](int64) { ++Calls; return true; }};
    TestFalse(TEXT("Default invalid"), Invalid.Get_IsValid());
    TestFalse(TEXT("Invalid queue rejects work"), Invalid.Enqueue(1));
    TestFalse(TEXT("Invalid pump rejected"), Invalid.Pump(0, {}, Step).Pumped);
    TestEqual(TEXT("Invalid input invokes no callback"), Calls, 0);
    TestEqual(TEXT("Invalid input publishes no work"), Invalid.Get_NumPending(), 0);
    TestFalse(TEXT("Zero steps rejected"), ck::FSharedPacedWork{0, FCk_Time{1.0}}.Get_IsValid());
    TestFalse(TEXT("Negative duration rejected"), ck::FSharedPacedWork{1, FCk_Time{-1.0}}.Get_IsValid());
    auto Work = ck::FSharedPacedWork{1, FCk_Time{1.0}, [] { return 0.0; }};
    Work.Enqueue(1);
    TestFalse(TEXT("Missing required callback rejected"), Work.Pump(0, {}, {}).Pumped);
    TestTrue(TEXT("Rejected callback preserves queued work"), Work.Get_Contains(1));
    TestTrue(TEXT("Rejected pump does not consume frame"), Work.Pump(0, {}, Step).Pumped);
    auto Now = 2.0;
    auto Clock = ck::FSharedFrameBudget{4, FCk_Time{1.0}, [&Now] { return Now; }};
    Clock.BeginFrame(0);
    Clock.BeginPump();
    Now = 1.0;
    TestFalse(TEXT("Clock reversal admits no callback"), Clock.TryConsumeStep());
    Clock.EndPump();
    TestFalse(TEXT("Failed clock cannot restart within frame"), Clock.BeginPump());
    TestEqual(TEXT("Clock failure consumed no steps"), Clock.Get_StepsConsumed(), 0);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_SharedPacedWork_LegacyCompatibility,
    "Ck.CkEcs.Pacing.SharedWork.LegacyCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTest_SharedPacedWork_LegacyCompatibility::RunTest(const FString&) -> bool
{
    auto Pacer = ck::FFragment_PacedWork{2, 2};
    auto UnusedHandle = FCk_Handle{};
    auto Calls = 0;
    const auto Continue = [&]
    {
        ++Calls;
        return ck::EPacedStepResult::Continue;
    };
    // Draining the exact quota never accesses a marker or registry; only legacy pacing is exercised.
    TestFalse(TEXT("Legacy first pass drains seeded quota"),
        ck::RunPacedSteps(UnusedHandle, Pacer, FCk_Time{}, Continue));
    TestEqual(TEXT("Legacy first-pass count"), Calls, 2);
    ck::RunPacedSteps(UnusedHandle, Pacer, FCk_Time{}, Continue);
    TestEqual(TEXT("Legacy zero delta does not refill"), Calls, 2);
    ck::RunPacedSteps(UnusedHandle, Pacer, FCk_Time{0.016}, Continue);
    TestEqual(TEXT("Legacy positive delta replenishes"), Calls, 4);
    TestEqual(TEXT("Legacy invalid step configuration remains clamped"),
        ck::FFragment_PacedWork{0, -1}.Get_MaxStepsPerFrame(), 1);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
