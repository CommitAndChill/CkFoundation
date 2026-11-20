#include "CkEcs/World/CkEcsWorld.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EcsWorld_FallibleCreationCapacity,
    "CkEcs.UnitTests.World.FallibleCreationCapacity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_EcsWorld_FallibleCreationCapacity::
    RunTest(const FString&)
    -> bool
{
    auto ExistingWorld = ck::FEcsWorld{};
    const auto ExistingSlot = ExistingWorld.Get_Registry().Get_RegistryHandle();
    const auto* ExistingRegistry = ck::registry_table::TryResolve(ExistingSlot);
    if (NOT TestNotNull(TEXT("Legacy constructor retains a valid registry"), ExistingRegistry))
    { return false; }

    using EResult = ck::FEcsWorld::ECreateResult;
    auto Result = EResult::InvalidState;
    auto ExpiredSlot = FCk_RegistryHandle{};
    {
        const auto Capacity = ck::registry_table::FScopedAllocationCapacityForTest{1};
        auto First = ck::FEcsWorld::TryCreate(Result);
        if (NOT TestNotNull(TEXT("One permitted world is created"), First.Get()))
        { return false; }
        TestTrue(TEXT("Successful creation is classified"), Result == EResult::Success);
        const auto FirstSlot = First->Get_Registry().Get_RegistryHandle();
        TestTrue(TEXT("Private world has a distinct registry slot"), FirstSlot != ExistingSlot);
        TestNotNull(TEXT("Transient context exists after success"),
            First->Get_Registry().TryGetContext<ck::FCtx_TransientEntity>());

        for (auto Attempt = 0; Attempt < 2; ++Attempt)
        {
            auto Rejected = ck::FEcsWorld::TryCreate(Result);
            TestNull(TEXT("Capacity failure publishes no world"), Rejected.Get());
            TestTrue(TEXT("Capacity failure is explicit"), Result == EResult::CapacityExceeded);
            TestTrue(TEXT("Existing world remains untouched"),
                ck::registry_table::TryResolve(ExistingSlot) == ExistingRegistry);
            TestNotNull(TEXT("Accepted private world remains live"), ck::registry_table::TryResolve(FirstSlot));
        }

        First.Reset();
        TestNull(TEXT("Released private slot no longer resolves"), ck::registry_table::TryResolve(FirstSlot));
        auto Replacement = ck::FEcsWorld::TryCreate(Result);
        if (NOT TestNotNull(TEXT("Capacity recovers after release"), Replacement.Get()))
        { return false; }
        TestTrue(TEXT("Recovered admission succeeds"), Result == EResult::Success);
        ExpiredSlot = Replacement->Get_Registry().Get_RegistryHandle();
        TestEqual(TEXT("Released slot is reused"), ExpiredSlot.SlotIndex, FirstSlot.SlotIndex);
        TestNotEqual(TEXT("Reused slot advances generation"), ExpiredSlot.Generation, FirstSlot.Generation);
    }

    TestNull(TEXT("Scope destruction releases its private world"), ck::registry_table::TryResolve(ExpiredSlot));
    auto AfterScope = ck::FEcsWorld::TryCreate(Result);
    TestNotNull(TEXT("Capacity fixture restores ordinary admission"), AfterScope.Get());
    TestTrue(TEXT("Restored admission succeeds"), Result == EResult::Success);
    TestTrue(TEXT("Pre-existing registry survives fixture and world teardown"),
        ck::registry_table::TryResolve(ExistingSlot) == ExistingRegistry);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Registry_FallibleAllocationCapacity,
    "CkEcs.UnitTests.Registry.FallibleAllocationCapacity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_Registry_FallibleAllocationCapacity::
    RunTest(const FString&)
    -> bool
{
    using EResult = ck::registry_table::EAllocationResult;
    auto Registry = ck::registry_table::EnttRegistryType{};
    auto Result = EResult::Success;
    {
        const auto Capacity = ck::registry_table::FScopedAllocationCapacityForTest{0};
        const auto Rejected = ck::registry_table::TryAllocate(&Registry, Result);
        TestFalse(TEXT("Rejected allocation publishes no handle"), Rejected.IsSet());
        TestTrue(TEXT("Rejected allocation reports capacity"), Result == EResult::CapacityExceeded);
        TestNull(TEXT("Rejected handle cannot resolve"), ck::registry_table::TryResolve(Rejected));
        TestFalse(TEXT("Rejected allocation does not install transient context"),
            Registry.ctx().contains<ck::FCtx_TransientEntity>());
        const auto LegacyRejected = ck::registry_table::Allocate(&Registry);
        TestFalse(TEXT("Legacy allocation also rejects capacity without an ensure"), LegacyRejected.IsSet());
    }

    const auto Accepted = ck::registry_table::TryAllocate(&Registry, Result);
    TestTrue(TEXT("Result is reset to success on later admission"), Result == EResult::Success);
    TestTrue(TEXT("Accepted handle resolves to the supplied registry"),
        ck::registry_table::TryResolve(Accepted) == &Registry);
    ck::registry_table::Free(Accepted);
    TestNull(TEXT("Released allocation cannot resolve"), ck::registry_table::TryResolve(Accepted));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
