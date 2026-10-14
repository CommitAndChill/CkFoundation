#pragma once

#include "Containers/Ticker.h"
#include "Curves/RichCurve.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

class FCk_ProceduralActionAuthoringSession;
class FCk_ProceduralActionEditorModel;
class FCurveEditor;
class SWidget;

// --------------------------------------------------------------------------------------------------------------------

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

    struct FChannelCurves
    {
        FName Channel;
        FRichCurve Axes[3];
        TMap<FKeyHandle, FKeySample> Samples[3];
    };
}

// --------------------------------------------------------------------------------------------------------------------

/** The Curves tab: the selected channel's X/Y/Z (the body's offset or a slot's endpoint) over the selected
 *  section's keys, in seconds of the clip. The clip stays the source of truth: the curves are rebuilt from it, and an
 *  accepted edit is written back through SetBodyKeys / SetPositionKeys, never into the clip directly. In an eased section
 *  the drawn segments between keys approximate the motion; the keys sit where the timeline shows them. */
class CKPROCEDURALANIMATIONEDITOR_API FCkProceduralAction_CurveAdapter : public TSharedFromThis<FCkProceduralAction_CurveAdapter>
{
public:
    FCkProceduralAction_CurveAdapter() = default;
    UE_NONCOPYABLE(FCkProceduralAction_CurveAdapter);
    ~FCkProceduralAction_CurveAdapter();

    auto
        Init(
            TSharedPtr<FCk_ProceduralActionAuthoringSession> InSession,
            TSharedPtr<FCk_ProceduralActionEditorModel> InModel,
            FSimpleDelegate InOnChanged)
        -> TSharedRef<SWidget>;
    // Rebuilds from the clip and the selected section; deferred to the next tick while the curve editor is calling back.
    auto
        Refresh()
        -> void;
    auto
        GetStatusText() const
        -> FText;
    // The curve editor's change callback: writes the channel's keys back to the clip.
    auto
        OnCurveModified(
            FName InChannel,
            int32 InAxis)
        -> void;

    auto
        FindCurve(
            FName InChannel,
            int32 InAxis)
        -> FRichCurve*;
    auto
        GetProxyOwner() const
        -> const UObject*
    { return _ProxyOwner.Get(); }
    auto
        GetHasPendingRebuild() const
        -> bool
    { return _PendingRebuild.IsValid(); }

private:
    auto
        RebuildCurves()
        -> void;
    auto
        AddChannel(
            FName InChannel,
            const TArray<ck_procedural_action_curve::FKeySample>& InKeys,
            bool InSmooth)
        -> void;
    auto
        ScheduleRebuild()
        -> void;
    auto
        Reject(
            const FString& InError)
        -> void;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TWeakPtr<FCk_ProceduralActionEditorModel> _Model;
    FSimpleDelegate _OnChanged;
    TSharedPtr<FCurveEditor> _Editor;
    TStrongObjectPtr<UObject> _ProxyOwner;
    TArray<TUniquePtr<ck_procedural_action_curve::FChannelCurves>> _Channels;
    FTSTicker::FDelegateHandle _PendingRebuild;
    FName _PhaseId = NAME_None;
    FString _Error;
    bool _Rebuilding = false;
    bool _InCallback = false;
};

// --------------------------------------------------------------------------------------------------------------------
