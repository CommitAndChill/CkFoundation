#include "Curve/CkProceduralAction_CurveAdapter.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"

#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"

namespace ck_procedural_action_curve_test
{
    constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    auto
        MakeClip()
        -> TStrongObjectPtr<UCk_ProceduralAction_Data>
    {
        auto Clip = TStrongObjectPtr<UCk_ProceduralAction_Data>{
            NewObject<UCk_ProceduralAction_Data>(GetTransientPackage(), NAME_None,
                RF_Transient | RF_Transactional)};
        auto Phase = FCk_ProceduralAction_Phase{FName(TEXT("Move")), FCk_Time{1.0}};
        auto First = FCk_ProceduralAction_TransformKey{FCk_FloatRange_0to1{0.25},
            FVector{10.0, 100.0, 0.0}, FQuat::Identity};
        auto Second = FCk_ProceduralAction_TransformKey{FCk_FloatRange_0to1{0.75},
            FVector{20.0, 200.0, 0.0}, FQuat::Identity};
        auto Keys = TArray<FCk_ProceduralAction_TransformKey>{};
        Keys.Add(First);
        Keys.Add(Second);
        Phase.Set_BodyKeys(Keys);
        auto Phases = TArray<FCk_ProceduralAction_Phase>{};
        Phases.Add(Phase);
        Clip->Set_Phases(Phases);
        return Clip;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_ProceduralAction_CurveAdapter_KeyIdentityAndUndo,
    "Ck.ProceduralAnimation.Action.Editor.CurveAdapter.KeyIdentityAndUndo",
    ck_procedural_action_curve_test::Flags)

auto
    FCkTest_ProceduralAction_CurveAdapter_KeyIdentityAndUndo::
    RunTest(
        const FString&)
    -> bool
{
    if (NOT TestNotNull(TEXT("editor transaction system"), GEditor))
    { return false; }
    auto Clip = ck_procedural_action_curve_test::MakeClip();
    auto Session = MakeShared<FCk_ProceduralActionAuthoringSession>(Clip.Get());
    auto Adapter = MakeShared<FCkProceduralAction_CurveAdapter>();
    Adapter->Init(Session, FSimpleDelegate{});
    Adapter->Select(FName(TEXT("Move")), NAME_None, 0);
    TestFalse(TEXT("proxy model owner is nontransactional"),
        Adapter->_ProxyOwner->HasAnyFlags(RF_Transactional));

    auto FirstHandle = FKeyHandle::Invalid();
    for (auto It = Adapter->_Curve.GetKeyHandleIterator(); It; ++It)
    {
        if (FMath::IsNearlyEqual(Adapter->_Curve.GetKeyTime(*It), 0.25f))
        { FirstHandle = *It; break; }
    }
    if (NOT TestTrue(TEXT("first key has stable handle"), FirstHandle != FKeyHandle::Invalid()))
    { return false; }
    {
        const auto Transaction = FScopedTransaction{FText::FromString(TEXT("Cross authored curve keys"))};
        Adapter->_Curve.SetKeyTime(FirstHandle, 0.9f);
        Adapter->OnCurveModified();
    }

    const auto& Keys = Clip->Get_Phases()[0].Get_BodyKeys();
    if (NOT TestEqual(TEXT("crossing keeps two keys"), Keys.Num(), 2))
    { return false; }
    TestEqual(TEXT("earlier key retains its Y coordinate"), Keys[0].Get_Translation().Y, 200.0);
    TestEqual(TEXT("moved key retains its Y coordinate"), Keys[1].Get_Translation().Y, 100.0);
    TestTrue(TEXT("moved key has new time"),
        FMath::IsNearlyEqual(Keys[1].Get_Time().Get_Value(), 0.9, 1e-5));
    if (NOT TestTrue(TEXT("undo curve edit"), GEditor->UndoTransaction()))
    { return false; }
    TestEqual(TEXT("undo restores first Y coordinate"),
        Clip->Get_Phases()[0].Get_BodyKeys()[0].Get_Translation().Y, 100.0);
    TestEqual(TEXT("undo restores first time"),
        Clip->Get_Phases()[0].Get_BodyKeys()[0].Get_Time().Get_Value(), 0.25);
    Adapter->Refresh();
    return NOT HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_ProceduralAction_CurveAdapter_DeferredRejectClose,
    "Ck.ProceduralAnimation.Action.Editor.CurveAdapter.DeferredRejectClose",
    ck_procedural_action_curve_test::Flags)

auto
    FCkTest_ProceduralAction_CurveAdapter_DeferredRejectClose::
    RunTest(
        const FString&)
    -> bool
{
    auto Clip = ck_procedural_action_curve_test::MakeClip();
    auto Session = MakeShared<FCk_ProceduralActionAuthoringSession>(Clip.Get());
    TSharedPtr<FCkProceduralAction_CurveAdapter> Adapter = MakeShared<FCkProceduralAction_CurveAdapter>();
    Adapter->Init(Session, FSimpleDelegate{});
    Adapter->Select(FName(TEXT("Move")), NAME_None, 0);
    const auto MixedHandle = *Adapter->_Curve.GetKeyHandleIterator();
    Adapter->_Curve.SetKeyInterpMode(MixedHandle, RCIM_Cubic, false);
    Adapter->OnCurveModified();
    TestTrue(TEXT("mixed interpolation is diagnosed"),
        Adapter->GetStatusText().ToString().Contains(TEXT("Mixed or weighted")));
    TestEqual(TEXT("mixed interpolation leaves authored mode unchanged"),
        Clip->Get_Phases()[0].Get_BodyInterpolation(), ECk_ProceduralAction_Interpolation::Linear);
    Adapter.Reset();

    Adapter = MakeShared<FCkProceduralAction_CurveAdapter>();
    Adapter->Init(Session, FSimpleDelegate{});
    Adapter->Select(FName(TEXT("Move")), NAME_None, 0);
    const auto Handle = *Adapter->_Curve.GetKeyHandleIterator();
    Adapter->_Curve.SetKeyTime(Handle, 1.5f);
    Adapter->OnCurveModified();
    TestTrue(TEXT("out-of-range edit reports normalized-time diagnostic"),
        Adapter->GetStatusText().ToString().Contains(TEXT("normalized time")));
    TestTrue(TEXT("rebuild is deferred outside model callback"), Adapter->_PendingRebuild.IsValid());
    TestEqual(TEXT("rejected edit leaves authored key unchanged"),
        Clip->Get_Phases()[0].Get_BodyKeys()[0].Get_Time().Get_Value(), 0.25);
    const auto WeakAdapter = TWeakPtr<FCkProceduralAction_CurveAdapter>{Adapter};
    Adapter.Reset();
    FTSTicker::GetCoreTicker().Tick(0.0f);
    TestFalse(TEXT("closing with pending rebuild releases adapter"), WeakAdapter.IsValid());
    return NOT HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_ProceduralAction_CurveAdapter_MalformedDraftLane,
    "Ck.ProceduralAnimation.Action.Editor.CurveAdapter.MalformedDraftLane",
    ck_procedural_action_curve_test::Flags)

auto
    FCkTest_ProceduralAction_CurveAdapter_MalformedDraftLane::
    RunTest(
        const FString&)
    -> bool
{
    auto Clip = ck_procedural_action_curve_test::MakeClip();
    auto Phases = Clip->Get_Phases();
    auto Keys = Phases[0].Get_BodyKeys();
    Keys.Swap(0, 1);
    Phases[0].Set_BodyKeys(Keys);
    Clip->Set_Phases(Phases);

    auto Session = MakeShared<FCk_ProceduralActionAuthoringSession>(Clip.Get());
    auto Adapter = MakeShared<FCkProceduralAction_CurveAdapter>();
    Adapter->Init(Session, FSimpleDelegate{});
    Adapter->Select(FName(TEXT("Move")), NAME_None, 0);
    TestTrue(TEXT("out-of-order draft displays a Details correction"),
        Adapter->GetStatusText().ToString().Contains(TEXT("correct this draft in Details")));
    TestEqual(TEXT("invalid draft has no editable proxy keys"), Adapter->_Curve.GetNumKeys(), 0);
    TestEqual(TEXT("draft key order remains untouched"),
        Clip->Get_Phases()[0].Get_BodyKeys()[0].Get_Time().Get_Value(), 0.75);

    Keys.Swap(0, 1);
    Phases[0].Set_BodyKeys(Keys);
    Clip->Set_Phases(Phases);
    Adapter->Refresh();
    TestEqual(TEXT("corrected draft obtains editable curve"), Adapter->_Curve.GetNumKeys(), 2);
    TestFalse(TEXT("diagnostic clears after Details correction"),
        Adapter->GetStatusText().ToString().Contains(TEXT("correct this draft")));

    Keys[1].Set_Time(FCk_FloatRange_0to1{0.25});
    Phases[0].Set_BodyKeys(Keys);
    Clip->Set_Phases(Phases);
    Adapter->Refresh();
    TestEqual(TEXT("duplicate-time draft has no editable proxy keys"), Adapter->_Curve.GetNumKeys(), 0);
    TestEqual(TEXT("duplicate-time draft remains authored"),
        Clip->Get_Phases()[0].Get_BodyKeys()[1].Get_Time().Get_Value(), 0.25);
    return NOT HasAnyErrors();
}

#endif
