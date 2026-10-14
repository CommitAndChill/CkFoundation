#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "Widgets/SCompoundWidget.h"

class FCk_ProceduralActionAuthoringSession;
class FCk_ProceduralActionEditorModel;
class SCkProceduralAction_TimelineLanes;

// --------------------------------------------------------------------------------------------------------------------

/** The clip on a time ruler in seconds: a Sections row (a bar per section with its easing drawn inside and a hollow start
 *  when it starts from the carried pose; drag a boundary to change a duration), a lane per channel with its keys (drag a
 *  key within its section), an Events lane (drag a marker, a window or either window edge) and a Poses row. Right-click
 *  an item to delete it. Selection and the playhead are the model's; every edit is one command. */
class SCkProceduralAction_Timeline : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkProceduralAction_Timeline) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionAuthoringSession>, Session)
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionEditorModel>, Model)
        SLATE_ARGUMENT(TFunction<void(bool, const FString&)>, OnResult)
        SLATE_ARGUMENT(TFunction<void(bool)>, OnAddEvent)
        SLATE_ARGUMENT(TFunction<void()>, OnDelete)
        // How far playback reaches; past the clip's end when the solved subject keeps moving after it.
        SLATE_ARGUMENT(TFunction<double()>, Length)
    SLATE_END_ARGS()

    struct FSection
    {
        FName Id;
        double Start = 0.0;
        double Duration = 0.0;
        ECk_ProceduralAction_Easing Easing = ECk_ProceduralAction_Easing::Linear;
        TArray<FVector2f> EasingShape;
        bool CarriedStart = true;
    };
    struct FKey
    {
        FName PhaseId;
        FName Channel;
        double KeyTime = 0.0;
        double Seconds = 0.0;
        int32 Row = 0;
    };
    struct FEvent
    {
        FName Id;
        bool IsWindow = false;
        double Start = 0.0;
        double End = 0.0;
        int32 Lane = 0;
    };
    struct FPose
    {
        FGuid Id;
        FName PhaseId;
        FString Label;
        double Seconds = 0.0;
    };

    auto
        Construct(
            const FArguments& InArgs)
        -> void;
    // Re-reads the clip; call when its content changed.
    auto
        Refresh()
        -> void;
    auto
        SupportsKeyboardFocus() const
        -> bool override
    { return true; }

private:
    friend class SCkProceduralAction_TimelineLanes;

    auto
        Get_Total() const
        -> double;
    auto
        Get_Extent() const
        -> double;
    auto
        Get_ViewRange() const
        -> TRange<double>;
    auto
        Snap(
            double InSeconds) const
        -> double;
    // The section a raw clip time falls in and its fraction there; a boundary belongs to the section that ends there.
    auto
        Get_SectionAt(
            double InSeconds,
            FName& OutPhaseId,
            double& OutFraction) const
        -> bool;
    auto
        Report(
            bool InSucceeded,
            const FString& InSuccess,
            const FString& InError)
        -> void;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TWeakPtr<FCk_ProceduralActionEditorModel> _Model;
    TFunction<void(bool, const FString&)> _OnResult;
    TFunction<void(bool)> _OnAddEvent;
    TFunction<void()> _OnDelete;
    TFunction<double()> _Length;
    TSharedPtr<SCkProceduralAction_TimelineLanes> _Lanes;
    TRange<double> _ViewRange{0.0, 1.0};
    // The view frames the whole playback length until the user zooms or pans.
    bool _ViewFollowsExtent = true;
    int32 _EventLanes = 1;
    TArray<FSection> _Sections;
    TArray<FName> _Channels;
    TArray<FKey> _Keys;
    TArray<FEvent> _Events;
    TArray<FPose> _Poses;
};

// --------------------------------------------------------------------------------------------------------------------
