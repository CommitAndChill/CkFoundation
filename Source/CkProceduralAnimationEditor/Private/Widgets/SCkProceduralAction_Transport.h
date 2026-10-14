#pragma once

#include "Widgets/SCompoundWidget.h"

class FCk_ProceduralActionAuthoringSession;
class FCk_ProceduralActionEditorModel;

// --------------------------------------------------------------------------------------------------------------------

// The engine's scrub control panel over the editor model: play, step, loop and scrub, with the time in seconds, frames and
// the section fraction beside it. Playing and stepping go through the toolkit, which owns the playback sources.
class SCkProceduralAction_Transport : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkProceduralAction_Transport) {}
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionAuthoringSession>, Session)
        SLATE_ARGUMENT(TSharedPtr<FCk_ProceduralActionEditorModel>, Model)
        SLATE_ARGUMENT(TFunction<void()>, OnTogglePlay)
        SLATE_ARGUMENT(TFunction<void(int32)>, OnStep)
        // How far playback reaches; past the clip's end when the solved subject keeps moving after it.
        SLATE_ARGUMENT(TFunction<double()>, Length)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs)
        -> void;

private:
    auto
        Get_Duration() const
        -> double;
    auto
        Get_TimeText() const
        -> FText;

    TWeakPtr<FCk_ProceduralActionAuthoringSession> _Session;
    TWeakPtr<FCk_ProceduralActionEditorModel> _Model;
    TFunction<double()> _Length;
};

// --------------------------------------------------------------------------------------------------------------------
