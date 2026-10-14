#pragma once

#include "Curves/RichCurve.h"
#include "Containers/Ticker.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

class FCk_ProceduralActionAuthoringSession;
class FCurveEditor;
class SCurveEditorPanel;
class SWidget;

namespace ck_procedural_action_curve
{
    struct FKeySample
    {
        double Time = 0.0;
        FVector Value = FVector::ZeroVector;
        FVector Arrive = FVector::ZeroVector;
        FVector Leave = FVector::ZeroVector;
        FQuat Rotation = FQuat::Identity;
    };
}

// One editable scalar view of an authored vector key lane. The clip remains the source of truth;
// the FRichCurve is a short-lived CurveEditor bridge and never becomes runtime content.
class FCkProceduralAction_CurveAdapter : public TSharedFromThis<FCkProceduralAction_CurveAdapter>
{
public:
    ~FCkProceduralAction_CurveAdapter();

    auto
        Init(
            TSharedPtr<FCk_ProceduralActionAuthoringSession> InSession,
            FSimpleDelegate InOnChanged)
        -> TSharedRef<SWidget>;
    auto
        Select(
            FName InPhaseId,
            FName InSlotId,
            int32 InAxis)
        -> void;
    auto
        Refresh()
        -> void;
    auto
        GetStatusText() const
        -> FText;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCkTest_ProceduralAction_CurveAdapter_KeyIdentityAndUndo;
    friend class FCkTest_ProceduralAction_CurveAdapter_DeferredRejectClose;
    friend class FCkTest_ProceduralAction_CurveAdapter_MalformedDraftLane;
#endif
    auto
        RebuildCurve()
        -> void;
    auto
        ScheduleRebuild()
        -> void;
    auto
        TickPendingRebuild(
            float InDeltaTime)
        -> bool;
    auto
        OnCurveModified()
        -> void;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    FSimpleDelegate _OnChanged;
    TSharedPtr<FCurveEditor> _Editor;
    TSharedPtr<SCurveEditorPanel> _Panel;
    TStrongObjectPtr<UObject> _ProxyOwner;
    FRichCurve _Curve;
    TMap<FKeyHandle, ck_procedural_action_curve::FKeySample> _SamplesByHandle;
    FTSTicker::FDelegateHandle _PendingRebuild;
    FName _PhaseId = NAME_None;
    FName _SlotId = NAME_None;
    int32 _Axis = 0;
    FString _Error;
    bool _Rebuilding = false;
    bool _InCallback = false;
};
