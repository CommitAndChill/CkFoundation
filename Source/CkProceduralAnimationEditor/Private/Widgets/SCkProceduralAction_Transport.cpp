#include "Widgets/SCkProceduralAction_Transport.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"

#include "SScrubControlPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionTransport"

auto
    SCkProceduralAction_Transport::
    Construct(
        const FArguments& InArgs)
    -> void
{
    constexpr auto FramesPerSecond = FCk_ProceduralActionAuthoringSession::FramesPerSecond;
    _Session = InArgs._Session;
    _Model = InArgs._Model;
    _Length = InArgs._Length;
    const auto WeakModel = _Model;
    const auto OnTogglePlay = InArgs._OnTogglePlay;
    const auto OnStep = InArgs._OnStep;
    const auto SetTime = [WeakModel](double InSeconds)
    {
        if (const auto Model = WeakModel.Pin())
        { Model->SetTime(FCk_ProceduralActionAuthoringSession::SnapToFrame(FCk_Time{InSeconds})); }
    };
    auto Controls = TArray<FTransportControlWidget>{};
    for (const auto Type : {ETransportControlWidgetType::BackwardEnd, ETransportControlWidgetType::BackwardStep,
        ETransportControlWidgetType::ForwardPlay, ETransportControlWidgetType::ForwardStep,
        ETransportControlWidgetType::ForwardEnd, ETransportControlWidgetType::Loop})
    { Controls.Add(FTransportControlWidget{Type}); }

    ChildSlot
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SNew(SScrubControlPanel)
            .Tag(FName{TEXT("CkActionEditor.Transport")})
            .Value_Lambda([WeakModel]() -> float
            {
                const auto Model = WeakModel.Pin();
                return Model.IsValid() ? static_cast<float>(Model->Get_Time().Get_Seconds()) : 0.0f;
            })
            .NumOfKeys_Lambda([this]() { return static_cast<uint32>(FMath::Max(1.0, Get_Duration() * FramesPerSecond)) + 1; })
            .SequenceLength_Lambda([this]() { return static_cast<float>(Get_Duration()); })
            .ViewInputMin(0.0f)
            .ViewInputMax_Lambda([this]() { return static_cast<float>(Get_Duration()); })
            .bAllowZoom(false)
            .TransportControlWidgetsToCreate(Controls)
            .OnValueChanged_Lambda([SetTime](float InValue) { SetTime(InValue); })
            .OnClickedForwardPlay_Lambda([OnTogglePlay]()
            {
                if (OnTogglePlay)
                { OnTogglePlay(); }
                return FReply::Handled();
            })
            .OnClickedForwardStep_Lambda([OnStep]()
            {
                if (OnStep)
                { OnStep(1); }
                return FReply::Handled();
            })
            .OnClickedBackwardStep_Lambda([OnStep]()
            {
                if (OnStep)
                { OnStep(-1); }
                return FReply::Handled();
            })
            .OnClickedBackwardEnd_Lambda([SetTime]()
            {
                SetTime(0.0);
                return FReply::Handled();
            })
            .OnClickedForwardEnd_Lambda([this, SetTime]()
            {
                SetTime(Get_Duration());
                return FReply::Handled();
            })
            .OnClickedToggleLoop_Lambda([WeakModel]()
            {
                if (const auto Model = WeakModel.Pin())
                { Model->SetLooping(NOT Model->Get_IsLooping()); }
                return FReply::Handled();
            })
            .OnGetLooping_Lambda([WeakModel]()
            {
                const auto Model = WeakModel.Pin();
                return Model.IsValid() && Model->Get_IsLooping();
            })
            .OnGetPlaybackMode_Lambda([WeakModel]()
            {
                const auto Model = WeakModel.Pin();
                return Model.IsValid() && Model->Get_IsPlaying() ? EPlaybackMode::PlayingForward : EPlaybackMode::Stopped;
            })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)
        [
            SNew(STextBlock)
            .Tag(FName{TEXT("CkActionEditor.TransportTime")})
            .Text_Lambda([this]() { return Get_TimeText(); })
        ]
    ];
}

auto
    SCkProceduralAction_Transport::
    Get_Duration() const
    -> double
{
    const auto Session = _Session.Pin();
    const auto Clip = Session.IsValid() ? Session->Get_Duration().Get_Seconds() : 0.0;
    return FMath::Max(Clip, _Length ? _Length() : 0.0);
}

auto
    SCkProceduralAction_Transport::
    Get_TimeText() const
    -> FText
{
    const auto Session = _Session.Pin();
    const auto Model = _Model.Pin();
    if (NOT Session.IsValid() || NOT Model.IsValid())
    { return FText::GetEmpty(); }
    const auto Seconds = Model->Get_Time().Get_Seconds();
    const auto Frame = FMath::RoundToInt(Seconds * FCk_ProceduralActionAuthoringSession::FramesPerSecond);
    auto PhaseId = FName{};
    auto Fraction = 0.0;
    if (Seconds > Session->Get_Duration().Get_Seconds() + 1.0e-6)
    { return FText::FromString(FString::Printf(TEXT("%.2f s | frame %d | after the clip"), Seconds, Frame)); }
    if (NOT Session->ToRawFraction(Model->Get_Time(), PhaseId, Fraction))
    { return FText::FromString(FString::Printf(TEXT("%.2f s | frame %d"), Seconds, Frame)); }
    return FText::FromString(FString::Printf(TEXT("%.2f s | frame %d | %s %.2f"), Seconds, Frame, *PhaseId.ToString(), Fraction));
}

#undef LOCTEXT_NAMESPACE
