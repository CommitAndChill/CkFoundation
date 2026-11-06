// Pins the intrusive lifetime-dependents list through the public UCk_Utils_EntityLifetime_UE surface — no
// world, no scheduler. The list replaced an owner-side array that was never pruned: an owner that churned
// children (an SM spawning tasks, a spawner) walked every stale entry on each new child. The contract here is
// that a dependent is linked in attachment order, leaves the list the moment it is destroyed, moves between
// owners on transfer, and that none of it allocates or walks more than the live dependents.

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
#include "CkEcs/Tag/CkTag.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_entity_lifetime_dependents_spec
{
    CK_DEFINE_ECS_TAG(FTag_Spec_Marker);

    // A live registry is an owned entt registry registered in the slot table (what the EcsWorld subsystems do);
    // the slot is freed before the registry dies, the order FEcsWorld keeps.
    struct FRegistryFixture
    {
        ck::registry_table::EnttRegistryType _EnttRegistry;
        FCk_RegistryHandle _RegistryHandle;
        FCk_Registry _Registry;

        FRegistryFixture()
            : _RegistryHandle(ck::registry_table::Allocate(&_EnttRegistry))
            , _Registry(_RegistryHandle)
        {
        }

        ~FRegistryFixture()
        {
            ck::registry_table::Free(_RegistryHandle);
        }

        auto DestroyRaw(const FCk_Handle& InHandle) -> void
        {
            _EnttRegistry.destroy(InHandle.Get_Entity().Get_ID());
        }

        auto DestroyRawBatch(const TArray<FCk_Handle>& InHandles) -> void
        {
            auto Ids = TArray<FCk_Entity::IdType>{};
            for (const auto& Handle : InHandles)
            { Ids.Emplace(Handle.Get_Entity().Get_ID()); }

            _EnttRegistry.destroy(Ids.begin(), Ids.end());
        }
    };

    auto Get_DependentsInOrder(const FCk_Handle& InOwner) -> TArray<FCk_Handle>
    {
        auto Ret = TArray<FCk_Handle>{};
        UCk_Utils_EntityLifetime_UE::ForEach_LifetimeDependent(InOwner, [&](const FCk_Handle& InDependent)
        {
            Ret.Emplace(InDependent);
        });
        return Ret;
    }

    auto Get_IsSameSequence(const TArray<FCk_Handle>& InActual, const TArray<FCk_Handle>& InExpected) -> bool
    {
        if (InActual.Num() != InExpected.Num())
        { return false; }

        for (auto Index = 0; Index < InActual.Num(); ++Index)
        {
            if (InActual[Index] != InExpected[Index])
            { return false; }
        }

        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EntityLifetime_DependentsLinkInAttachmentOrder,
    "Ck.CkEcs.EntityLifetime.DependentsLinkInAttachmentOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_EntityLifetime_DependentsLinkInAttachmentOrder::RunTest(const FString&)
{
    using namespace ck_entity_lifetime_dependents_spec;

    auto Fixture = FRegistryFixture{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);

    TestFalse(TEXT("an owner without dependents carries no list head"), Owner.Has<ck::FFragment_LifetimeDependents>());
    TestEqual(TEXT("count of a childless owner"), UCk_Utils_EntityLifetime_UE::Get_LifetimeDependentsCount(Owner), 0);

    auto A = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    auto B = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    auto C = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);

    TestTrue(TEXT("ForEach visits dependents in attachment order"), Get_IsSameSequence(Get_DependentsInOrder(Owner), {A, B, C}));
    TestTrue(TEXT("Get_LifetimeDependents matches ForEach"),
        Get_IsSameSequence(UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(Owner), {A, B, C}));
    TestEqual(TEXT("count"), UCk_Utils_EntityLifetime_UE::Get_LifetimeDependentsCount(Owner), 3);

    const auto& Head = Owner.Get<ck::FFragment_LifetimeDependents>();
    TestTrue(TEXT("first link"), Head.Get_FirstDependent() == A);
    TestTrue(TEXT("last link"), Head.Get_LastDependent() == C);
    TestEqual(TEXT("linked count"), Head.Get_LinkedCount(), 3);

    const auto& LinkB = B.Get<ck::FFragment_LifetimeOwner>();
    TestTrue(TEXT("owner back-pointer"), LinkB.Get_Entity() == Owner);
    TestTrue(TEXT("prev sibling"), LinkB.Get_PrevSibling() == A);
    TestTrue(TEXT("next sibling"), LinkB.Get_NextSibling() == C);
    TestTrue(TEXT("reverse-linked"), LinkB.Get_IsReverseLinked());
    TestTrue(TEXT("Get_LifetimeOwner"), UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(B) == Owner);

    // Re-setup with the same owner is idempotent and must not re-link
    UCk_Utils_EntityLifetime_UE::Request_SetupEntityWithLifetimeOwner(B, Owner);
    TestEqual(TEXT("idempotent setup keeps the count"), Owner.Get<ck::FFragment_LifetimeDependents>().Get_LinkedCount(), 3);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EntityLifetime_DependentUnlinksWhenDestroyed,
    "Ck.CkEcs.EntityLifetime.DependentUnlinksWhenDestroyed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_EntityLifetime_DependentUnlinksWhenDestroyed::RunTest(const FString&)
{
    using namespace ck_entity_lifetime_dependents_spec;

    auto Fixture = FRegistryFixture{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
    auto A = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    auto B = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    auto C = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);

    // middle
    Fixture.DestroyRaw(B);
    TestFalse(TEXT("destroyed dependent is gone"), ck::IsValid(B, ck::IsValid_Policy_IncludePendingKill{}));
    TestTrue(TEXT("middle removal stitches A -> C"), Get_IsSameSequence(Get_DependentsInOrder(Owner), {A, C}));
    TestTrue(TEXT("A.next is C"), A.Get<ck::FFragment_LifetimeOwner>().Get_NextSibling() == C);
    TestTrue(TEXT("C.prev is A"), C.Get<ck::FFragment_LifetimeOwner>().Get_PrevSibling() == A);
    TestEqual(TEXT("linked count after middle removal"), Owner.Get<ck::FFragment_LifetimeDependents>().Get_LinkedCount(), 2);

    // first
    Fixture.DestroyRaw(A);
    TestTrue(TEXT("first removal advances the head"), Get_IsSameSequence(Get_DependentsInOrder(Owner), {C}));
    TestTrue(TEXT("head is C"), Owner.Get<ck::FFragment_LifetimeDependents>().Get_FirstDependent() == C);
    TestTrue(TEXT("C has no prev"), C.Get<ck::FFragment_LifetimeOwner>().Get_PrevSibling().Get_Entity().Get_IsTombstone());

    // last
    Fixture.DestroyRaw(C);
    TestEqual(TEXT("empty after last removal"), UCk_Utils_EntityLifetime_UE::Get_LifetimeDependentsCount(Owner), 0);
    TestTrue(TEXT("head cleared"), Owner.Get<ck::FFragment_LifetimeDependents>().Get_FirstDependent().Get_Entity().Get_IsTombstone());
    TestTrue(TEXT("tail cleared"), Owner.Get<ck::FFragment_LifetimeDependents>().Get_LastDependent().Get_Entity().Get_IsTombstone());
    TestEqual(TEXT("linked count zero"), Owner.Get<ck::FFragment_LifetimeDependents>().Get_LinkedCount(), 0);

    // the list is reusable after draining
    auto D = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    TestTrue(TEXT("re-attach after drain"), Get_IsSameSequence(Get_DependentsInOrder(Owner), {D}));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EntityLifetime_DependentChurnStaysBounded,
    "Ck.CkEcs.EntityLifetime.DependentChurnStaysBounded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_EntityLifetime_DependentChurnStaysBounded::RunTest(const FString&)
{
    using namespace ck_entity_lifetime_dependents_spec;

    auto Fixture = FRegistryFixture{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
    auto Survivor = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);

    constexpr auto ChurnCount = 2000;
    for (auto Index = 0; Index < ChurnCount; ++Index)
    {
        auto Scratch = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
        Fixture.DestroyRaw(Scratch);
    }

    TestEqual(TEXT("churn leaves nothing behind"), Owner.Get<ck::FFragment_LifetimeDependents>().Get_LinkedCount(), 1);
    TestTrue(TEXT("only the survivor is linked"), Get_IsSameSequence(Get_DependentsInOrder(Owner), {Survivor}));
    TestTrue(TEXT("survivor is both head and tail"),
        Owner.Get<ck::FFragment_LifetimeDependents>().Get_FirstDependent() == Survivor &&
        Owner.Get<ck::FFragment_LifetimeDependents>().Get_LastDependent() == Survivor);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EntityLifetime_TransferMovesDependentBetweenOwners,
    "Ck.CkEcs.EntityLifetime.TransferMovesDependentBetweenOwners",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_EntityLifetime_TransferMovesDependentBetweenOwners::RunTest(const FString&)
{
    using namespace ck_entity_lifetime_dependents_spec;

    auto Fixture = FRegistryFixture{};
    auto OwnerOne = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
    auto OwnerTwo = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
    auto A = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OwnerOne);
    auto B = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OwnerOne);
    auto C = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OwnerOne);
    auto X = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OwnerTwo);

    UCk_Utils_EntityLifetime_UE::Request_TransferLifetimeOwner(B, OwnerTwo);

    TestTrue(TEXT("old owner no longer lists the transferred dependent"), Get_IsSameSequence(Get_DependentsInOrder(OwnerOne), {A, C}));
    TestTrue(TEXT("new owner appends it"), Get_IsSameSequence(Get_DependentsInOrder(OwnerTwo), {X, B}));
    TestTrue(TEXT("forward link follows"), UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(B) == OwnerTwo);
    TestEqual(TEXT("old count"), OwnerOne.Get<ck::FFragment_LifetimeDependents>().Get_LinkedCount(), 2);
    TestEqual(TEXT("new count"), OwnerTwo.Get<ck::FFragment_LifetimeDependents>().Get_LinkedCount(), 2);

    // transfer to the current owner is a no-op
    UCk_Utils_EntityLifetime_UE::Request_TransferLifetimeOwner(B, OwnerTwo);
    TestTrue(TEXT("self-transfer keeps the list"), Get_IsSameSequence(Get_DependentsInOrder(OwnerTwo), {X, B}));

    // a transferred dependent still unlinks from its NEW owner when destroyed
    Fixture.DestroyRaw(B);
    TestTrue(TEXT("destroy after transfer unlinks from the new owner"), Get_IsSameSequence(Get_DependentsInOrder(OwnerTwo), {X}));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EntityLifetime_BatchDestroySurvivesAnyPoolOrder,
    "Ck.CkEcs.EntityLifetime.BatchDestroySurvivesAnyPoolOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_EntityLifetime_BatchDestroySurvivesAnyPoolOrder::RunTest(const FString&)
{
    using namespace ck_entity_lifetime_dependents_spec;

    auto Fixture = FRegistryFixture{};

    // owner and every dependent in ONE batch — the shape the destruction pipeline's finalize pass produces
    {
        auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
        auto A = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
        auto B = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
        auto A1 = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(A);

        Fixture.DestroyRawBatch({Owner, A, B, A1});
        TestFalse(TEXT("batch: owner gone"), ck::IsValid(Owner, ck::IsValid_Policy_IncludePendingKill{}));
        TestFalse(TEXT("batch: grandchild gone"), ck::IsValid(A1, ck::IsValid_Policy_IncludePendingKill{}));
    }

    // owner destroyed BEFORE its dependents — the dependents must unlink without an owner to patch
    {
        auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
        auto A = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
        auto B = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);

        Fixture.DestroyRaw(Owner);
        TestTrue(TEXT("orphaned dependent keeps its sibling link until it goes"), A.Get<ck::FFragment_LifetimeOwner>().Get_NextSibling() == B);

        Fixture.DestroyRaw(A);
        TestTrue(TEXT("orphaned sibling is stitched"), B.Get<ck::FFragment_LifetimeOwner>().Get_PrevSibling().Get_Entity().Get_IsTombstone());

        Fixture.DestroyRaw(B);
    }

    // a dependent whose siblings are already gone
    {
        auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
        auto A = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
        auto B = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
        auto C = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);

        Fixture.DestroyRawBatch({A, C});
        TestTrue(TEXT("surviving middle dependent is the whole list"), Get_IsSameSequence(Get_DependentsInOrder(Owner), {B}));
        TestEqual(TEXT("count after batch"), Owner.Get<ck::FFragment_LifetimeDependents>().Get_LinkedCount(), 1);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EntityLifetime_CrossRegistryDependentIsNotLinked,
    "Ck.CkEcs.EntityLifetime.CrossRegistryDependentIsNotLinked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_EntityLifetime_CrossRegistryDependentIsNotLinked::RunTest(const FString&)
{
    using namespace ck_entity_lifetime_dependents_spec;

    auto Main = FRegistryFixture{};
    auto Nested = FRegistryFixture{};

    auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Main._Registry);
    auto Foreign = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Nested._Registry);
    UCk_Utils_EntityLifetime_UE::Request_SetupEntityWithLifetimeOwner(Foreign, Owner);

    TestTrue(TEXT("forward link crosses registries"), UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Foreign) == Owner);
    TestFalse(TEXT("no reverse link across registries"), Foreign.Get<ck::FFragment_LifetimeOwner>().Get_IsReverseLinked());
    TestEqual(TEXT("owner lists nothing"), UCk_Utils_EntityLifetime_UE::Get_LifetimeDependentsCount(Owner), 0);

    // idempotent re-setup and destruction must both tolerate the missing reverse link
    UCk_Utils_EntityLifetime_UE::Request_SetupEntityWithLifetimeOwner(Foreign, Owner);
    Nested.DestroyRaw(Foreign);
    TestEqual(TEXT("owner still lists nothing"), UCk_Utils_EntityLifetime_UE::Get_LifetimeDependentsCount(Owner), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EntityLifetime_DescendantWalksAndTypedQueries,
    "Ck.CkEcs.EntityLifetime.DescendantWalksAndTypedQueries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_EntityLifetime_DescendantWalksAndTypedQueries::RunTest(const FString&)
{
    using namespace ck_entity_lifetime_dependents_spec;
    using FMarker = FTag_Spec_Marker;

    auto Fixture = FRegistryFixture{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
    auto A = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    auto A1 = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(A);
    auto A2 = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(A);
    auto B = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);

    auto PostOrder = TArray<FCk_Handle>{};
    UCk_Utils_EntityLifetime_UE::ForEach_LifetimeDescendant(Owner, [&](const FCk_Handle& InDescendant)
    {
        PostOrder.Emplace(InDescendant);
    });
    TestTrue(TEXT("descendants in post-order (leaf-to-root, siblings in attachment order)"),
        Get_IsSameSequence(PostOrder, {A1, A2, A, B}));

    TestTrue(TEXT("first direct dependent matching"),
        UCk_Utils_EntityLifetime_UE::Get_LifetimeDependent_If(Owner, [&](const FCk_Handle& InDependent) { return InDependent == B; }) == B);
    TestFalse(TEXT("no direct dependent matching a grandchild"),
        ck::IsValid(UCk_Utils_EntityLifetime_UE::Get_LifetimeDependent_If(Owner, [&](const FCk_Handle& InDependent) { return InDependent == A1; })));

    TestFalse(TEXT("typed descendant query misses before the tag exists"),
        ck::IsValid(UCk_Utils_EntityLifetime_UE::Get_LifetimeDescendant_With<FMarker>(Owner)));

    A2.Add<FMarker>();
    B.Add<FMarker>();
    TestTrue(TEXT("typed descendant query is pre-order: A's subtree before B"),
        UCk_Utils_EntityLifetime_UE::Get_LifetimeDescendant_With<FMarker>(Owner) == A2);

    TestFalse(TEXT("typed ownership-chain query misses before the tag exists on the chain"),
        ck::IsValid(UCk_Utils_EntityLifetime_UE::Get_EntityInOwnershipChain_With<FMarker>(A1)));
    Owner.Add<FMarker>();
    TestTrue(TEXT("typed ownership-chain query climbs to the nearest match"),
        UCk_Utils_EntityLifetime_UE::Get_EntityInOwnershipChain_With<FMarker>(A1) == Owner);
    TestTrue(TEXT("typed ownership-chain query starts at the entity itself"),
        UCk_Utils_EntityLifetime_UE::Get_EntityInOwnershipChain_With<FMarker>(B) == B);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_EntityLifetime_DestroyCascadeReachesEveryDescendant,
    "Ck.CkEcs.EntityLifetime.DestroyCascadeReachesEveryDescendant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_EntityLifetime_DestroyCascadeReachesEveryDescendant::RunTest(const FString&)
{
    using namespace ck_entity_lifetime_dependents_spec;

    auto Fixture = FRegistryFixture{};
    auto Owner = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
    auto A = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);
    auto A1 = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(A);
    auto B = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Owner);

    UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Owner);

    for (const auto& Handle : {Owner, A, A1, B})
    {
        TestTrue(TEXT("cascade marks the whole subtree for destruction"), Handle.Has<ck::FTag_DestroyEntity_Initiate>());
    }

    // destruction is deferred: the list still names the pending-kill dependents until they are finally destroyed
    TestTrue(TEXT("pending-kill dependents remain linked"), Get_IsSameSequence(Get_DependentsInOrder(Owner), {A, B}));

    // a child attached to an owner already being destroyed inherits the cascade immediately
    auto Late = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Fixture._Registry);
    UCk_Utils_EntityLifetime_UE::Request_SetupEntityWithLifetimeOwner(Late, Owner);
    TestTrue(TEXT("late dependent is linked"), Get_IsSameSequence(Get_DependentsInOrder(Owner), {A, B, Late}));
    TestTrue(TEXT("late dependent inherits destruction"), Late.Has<ck::FTag_DestroyEntity_Initiate>());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
