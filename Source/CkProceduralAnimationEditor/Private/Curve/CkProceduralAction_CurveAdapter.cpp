#include "CkProceduralAnimationEditor/CkProceduralAction_CurveAdapter.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"
#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "CurveEditor.h"
#include "Curves/CurveFloat.h"
#include "RichCurveEditorModel.h"
#include "SCurveEditorPanel.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionCurveAdapter"

namespace ck_procedural_action_curve
{
    using FCompiledPhase = ck::procedural_action::FCompiledPhase;

    auto
        ToSeconds(
            const FCompiledPhase& InPhase,
            double InKeyTime)
        -> double
    {
        return InPhase.Get_Start().Get_Seconds()
            + InPhase.Get_Duration().Get_Seconds() * ck::procedural_action::InvertProgress(InPhase, InKeyTime);
    }

    auto
        ToKeyTime(
            const FCompiledPhase& InPhase,
            double InSeconds)
        -> double
    {
        const auto Raw = (InSeconds - InPhase.Get_Start().Get_Seconds()) / InPhase.Get_Duration().Get_Seconds();
        return ck::procedural_action::EvaluateProgress(InPhase, FMath::Clamp(Raw, 0.0, 1.0));
    }

    auto
        Sample(
            const TArray<FKeySample>& InKeys,
            double InTime)
        -> FKeySample
    {
        if (InKeys.IsEmpty())
        { return FKeySample{}; }
        if (InTime <= InKeys[0].Time)
        { return InKeys[0]; }
        for (auto Index = 1; Index < InKeys.Num(); ++Index)
        {
            if (InTime > InKeys[Index].Time)
            { continue; }
            const auto& Left = InKeys[Index - 1];
            const auto& Right = InKeys[Index];
            const auto Span = Right.Time - Left.Time;
            const auto Alpha = Span > 0.0 ? (InTime - Left.Time) / Span : 0.0;
            auto Result = FKeySample{};
            Result.Time = InTime;
            Result.Value = FMath::Lerp(Left.Value, Right.Value, Alpha);
            Result.Arrive = FMath::Lerp(Left.Arrive, Right.Arrive, Alpha);
            Result.Leave = Result.Arrive;
            Result.Rotation = FQuat::Slerp(Left.Rotation, Right.Rotation, Alpha).GetNormalized();
            return Result;
        }
        return InKeys.Last();
    }

    // The authored keys of one channel in a phase; false when they are not finite and strictly increasing at curve precision.
    auto
        ReadKeys(
            const FCk_ProceduralAction_Phase& InPhase,
            FName InChannel,
            TArray<FKeySample>& OutKeys,
            bool& OutSmooth)
        -> bool
    {
        OutKeys.Reset();
        if (InChannel == ck::procedural_action_editor::Get_BodyChannelName())
        {
            OutSmooth = InPhase.Get_BodyInterpolation() == ECk_ProceduralAction_Interpolation::Smooth;
            for (const auto& Key : InPhase.Get_BodyKeys())
            { OutKeys.Add({Key.Get_Time().Get_Value(), Key.Get_Translation(), Key.Get_ArriveTangent(), Key.Get_LeaveTangent(), Key.Get_Rotation()}); }
        }
        else if (const auto* Track = InPhase.Get_LimbTracks().FindByPredicate([InChannel](const FCk_ProceduralAction_LimbTrack& InTrack)
            { return InTrack.Get_Slot() == InChannel; }))
        {
            OutSmooth = Track->Get_Interpolation() == ECk_ProceduralAction_Interpolation::Smooth;
            for (const auto& Key : Track->Get_PositionKeys())
            { OutKeys.Add({Key.Get_Time().Get_Value(), Key.Get_Value(), Key.Get_ArriveTangent(), Key.Get_LeaveTangent(), FQuat::Identity}); }
        }
        auto Previous = -1.0f;
        for (const auto& Key : OutKeys)
        {
            const auto CurveTime = static_cast<float>(Key.Time);
            if (NOT FMath::IsFinite(Key.Time) || Key.Time < 0.0 || Key.Time > 1.0 || CurveTime <= Previous
                || Key.Value.ContainsNaN() || Key.Arrive.ContainsNaN() || Key.Leave.ContainsNaN() || Key.Rotation.ContainsNaN())
            { return false; }
            Previous = CurveTime;
        }
        return true;
    }

    auto
        AxisColor(
            int32 InAxis)
        -> FLinearColor
    { return InAxis == 0 ? CkStyle::AxisX() : InAxis == 1 ? CkStyle::AxisY() : CkStyle::AxisZ(); }
}

// --------------------------------------------------------------------------------------------------------------------

FCk_ProceduralAction_CurveAdapter::~FCk_ProceduralAction_CurveAdapter()
{
    if (_PendingRebuild.IsValid())
    { FTSTicker::GetCoreTicker().RemoveTicker(_PendingRebuild); }
    if (_Editor.IsValid())
    { _Editor->RemoveAllCurves(); }
    _Editor.Reset();
    _Channels.Reset();
    _ProxyOwner.Reset();
}

auto
    FCk_ProceduralAction_CurveAdapter::
    Init(
        TSharedPtr<FCk_ProceduralActionAuthoringSession> InSession,
        TSharedPtr<FCk_ProceduralActionEditorModel> InModel)
    -> TSharedRef<SWidget>
{
    _Session = InSession;
    _Model = InModel;
    // CurveEditor modifies its model owner before broadcasting. The transient, nontransactional owner keeps a rejected
    // proxy edit out of the clip's undo history; accepted edits Modify the clip through a command.
    _ProxyOwner.Reset(NewObject<UCurveFloat>(GetTransientPackage(), NAME_None, RF_Transient));
    _Editor = MakeShared<FCurveEditor>();
    _Editor->InitCurveEditor(FCurveEditorInitParams{});
    const auto Panel = SNew(SCurveEditorPanel, _Editor.ToSharedRef());
    Panel->SetTag(FName{TEXT("CkActionEditor.Curves")});
    return Panel;
}

auto
    FCk_ProceduralAction_CurveAdapter::
    Refresh()
    -> void
{
    if (_InCallback)
    { ScheduleRebuild(); }
    else if (NOT _Rebuilding)
    {
        _Error.Reset();
        RebuildCurves();
    }
}

auto
    FCk_ProceduralAction_CurveAdapter::
    ScheduleRebuild()
    -> void
{
    if (_PendingRebuild.IsValid())
    { return; }
    const auto WeakAdapter = TWeakPtr<FCk_ProceduralAction_CurveAdapter>{AsShared()};
    _PendingRebuild = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakAdapter](float) -> bool
    {
        if (const auto Adapter = WeakAdapter.Pin())
        {
            Adapter->_PendingRebuild.Reset();
            Adapter->RebuildCurves();
        }
        return false;
    }));
}

auto
    FCk_ProceduralAction_CurveAdapter::
    Reject(
        const FString& InError)
    -> void
{
    _Error = InError;
    ScheduleRebuild();
}

auto
    FCk_ProceduralAction_CurveAdapter::
    GetStatusText() const
    -> FText
{
    if (NOT _Error.IsEmpty())
    { return FText::FromString(_Error); }
    if (_PhaseId.IsNone())
    { return LOCTEXT("SelectSection", "Select a section to edit its curves."); }
    if (_Channels.IsEmpty())
    { return FText::Format(LOCTEXT("NoChannel", "{0}: no keyed channel"), FText::FromName(_PhaseId)); }
    const auto Channel = _Channels[0]->Channel;
    return FText::Format(LOCTEXT("Status", "{0} / {1}: X/Y/Z over seconds of the clip; pick a channel in Channels to switch"),
        FText::FromName(_PhaseId),
        Channel == ck::procedural_action_editor::Get_BodyChannelName() ? LOCTEXT("Body", "Body") : FText::FromName(Channel));
}

auto
    FCk_ProceduralAction_CurveAdapter::
    FindCurve(
        FName InChannel,
        int32 InAxis)
    -> FRichCurve*
{
    for (const auto& Channel : _Channels)
    {
        if (Channel->Channel == InChannel && InAxis >= 0 && InAxis < 3)
        { return &Channel->Axes[InAxis]; }
    }
    return nullptr;
}

auto
    FCk_ProceduralAction_CurveAdapter::
    RebuildCurves()
    -> void
{
    using namespace ck_procedural_action_curve;
    if (NOT _Editor.IsValid())
    { return; }
    if (_PendingRebuild.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_PendingRebuild);
        _PendingRebuild.Reset();
    }
    TGuardValue<bool> RebuildGuard{_Rebuilding, true};
    _Editor->RemoveAllCurves();
    _Channels.Reset();
    const auto Session = _Session.Pin();
    const auto Model = _Model.Pin();
    _PhaseId = Model.IsValid() ? Model->Get_SelectedPhase() : NAME_None;
    const auto* Clip = Session.IsValid() ? Session->Get_Clip() : nullptr;
    if (Clip == nullptr || _PhaseId.IsNone())
    { return; }
    const auto* Phase = Clip->Get_Phases().FindByPredicate([this](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == _PhaseId; });
    if (Phase == nullptr)
    { return; }

    auto Channels = TArray<FName>{};
    if (Clip->Get_BodyChannel().Get_Enabled() || NOT Phase->Get_BodyKeys().IsEmpty())
    { Channels.Add(ck::procedural_action_editor::Get_BodyChannelName()); }
    for (const auto& Track : Phase->Get_LimbTracks())
    { Channels.Add(Track.Get_Slot()); }
    // One channel at a time: the selected one, else the first, so X/Y/Z read unambiguously.
    if (Channels.Num() > 1)
    {
        const auto Selected = Model->Get_SelectedChannel();
        Channels = TArray<FName>{Channels.Contains(Selected) ? Selected : Channels[0]};
    }
    for (const auto Channel : Channels)
    {
        auto Keys = TArray<FKeySample>{};
        auto Smooth = false;
        if (NOT ReadKeys(*Phase, Channel, Keys, Smooth))
        {
            _Error = FString::Printf(TEXT("%s keys need finite values and strictly increasing times; correct this draft in Details."),
                *Channel.ToString());
            continue;
        }
        AddChannel(Channel, Keys, Smooth);
    }
}

auto
    FCk_ProceduralAction_CurveAdapter::
    AddChannel(
        FName InChannel,
        const TArray<ck_procedural_action_curve::FKeySample>& InKeys,
        bool InSmooth)
    -> void
{
    using namespace ck_procedural_action_curve;
    const auto Session = _Session.Pin();
    const auto* Timing = Session.IsValid() ? Session->Get_Timing().FindByPredicate([this](const FCompiledPhase& InPhase)
        { return InPhase.Get_Id() == _PhaseId; }) : nullptr;
    if (Timing == nullptr)
    { return; }
    const auto Seconds = Timing->Get_Duration().Get_Seconds();
    auto Curves = MakeUnique<FChannelCurves>();
    Curves->Channel = InChannel;
    const auto Label = InChannel == ck::procedural_action_editor::Get_BodyChannelName() ? FString{TEXT("Body")} : InChannel.ToString();
    for (auto Axis = 0; Axis < 3; ++Axis)
    {
        auto& Curve = Curves->Axes[Axis];
        for (const auto& Key : InKeys)
        {
            const auto Handle = Curve.AddKey(static_cast<float>(ToSeconds(*Timing, Key.Time)), static_cast<float>(Key.Value[Axis]));
            Curves->Samples[Axis].Add(Handle, Key);
            Curve.SetKeyInterpMode(Handle, InSmooth ? RCIM_Cubic : RCIM_Linear, false);
            auto& RichKey = Curve.GetKey(Handle);
            RichKey.TangentMode = RCTM_User;
            RichKey.ArriveTangent = static_cast<float>(Key.Arrive[Axis] / Seconds);
            RichKey.LeaveTangent = static_cast<float>(Key.Leave[Axis] / Seconds);
        }
        auto CurveModel = MakeUnique<FRichCurveEditorModelRaw>(&Curve, _ProxyOwner.Get());
        CurveModel->SetShortDisplayName(FText::FromString(FString::Printf(TEXT("%s.%s"), *Label,
            Axis == 0 ? TEXT("X") : Axis == 1 ? TEXT("Y") : TEXT("Z"))));
        CurveModel->SetColor(AxisColor(Axis));
        CurveModel->SetClampInputRange(TRange<double>{Timing->Get_Start().Get_Seconds(), (Timing->Get_Start() + Timing->Get_Duration()).Get_Seconds()});
        CurveModel->OnCurveModified().AddSP(AsShared(), &FCk_ProceduralAction_CurveAdapter::OnCurveModified, InChannel, Axis);
        const auto ModelId = _Editor->AddCurve(MoveTemp(CurveModel));
        _Editor->PinCurve(ModelId);
    }
    _Channels.Add(MoveTemp(Curves));
}

auto
    FCk_ProceduralAction_CurveAdapter::
    OnCurveModified(
        FName InChannel,
        int32 InAxis)
    -> void
{
    using namespace ck_procedural_action_curve;
    if (_Rebuilding || InAxis < 0 || InAxis > 2)
    { return; }
    TGuardValue<bool> CallbackGuard{_InCallback, true};
    const auto Session = _Session.Pin();
    auto* Clip = Session.IsValid() ? Session->Get_Clip() : nullptr;
    const auto* Phase = Clip == nullptr ? nullptr : Clip->Get_Phases().FindByPredicate([this](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == _PhaseId; });
    const auto* Timing = Session.IsValid() ? Session->Get_Timing().FindByPredicate([this](const FCompiledPhase& InPhase)
        { return InPhase.Get_Id() == _PhaseId; }) : nullptr;
    const auto* ChannelCurves = _Channels.FindByPredicate([InChannel](const TUniquePtr<FChannelCurves>& InCurves)
        { return InCurves->Channel == InChannel; });
    if (Phase == nullptr || Timing == nullptr || ChannelCurves == nullptr)
    { return; }
    auto& Curves = **ChannelCurves;
    const auto& Curve = Curves.Axes[InAxis];
    const auto Start = Timing->Get_Start().Get_Seconds();
    const auto Seconds = Timing->Get_Duration().Get_Seconds();
    const auto RichKeys = Curve.GetConstRefOfKeys();
    auto AllCubic = true;
    auto AllLinear = true;
    for (const auto& Key : RichKeys)
    {
        AllCubic &= Key.InterpMode == RCIM_Cubic && Key.TangentWeightMode == RCTWM_WeightedNone;
        AllLinear &= Key.InterpMode == RCIM_Linear;
        if (NOT FMath::IsFinite(Key.Time) || NOT FMath::IsFinite(Key.Value)
            || Key.Time < Start - 1.0e-4 || Key.Time > Start + Seconds + 1.0e-4)
        {
            Reject(TEXT("Curve keys need finite values and must stay inside their section."));
            return;
        }
    }
    if (NOT AllCubic && NOT AllLinear && NOT RichKeys.IsEmpty())
    {
        Reject(TEXT("Mixed or weighted interpolation is unsupported; select Linear or Cubic for all keys."));
        return;
    }

    auto Old = TArray<FKeySample>{};
    auto Smooth = false;
    ReadKeys(*Phase, InChannel, Old, Smooth);
    auto SamplesByIndex = TMap<int32, FKeySample>{};
    auto Retimed = TArray<FCk_ProceduralActionRetimedKey>{};
    for (const auto& Entry : Curves.Samples[InAxis])
    {
        const auto Index = Curve.GetIndexSafe(Entry.Key);
        if (Index == INDEX_NONE)
        { continue; }
        SamplesByIndex.Add(Index, Entry.Value);
        const auto NewTime = ToKeyTime(*Timing, RichKeys[Index].Time);
        if (NOT FMath::IsNearlyEqual(NewTime, Entry.Value.Time, 1.0e-6))
        { Retimed.Add(FCk_ProceduralActionRetimedKey{Entry.Value.Time, NewTime}); }
    }
    auto BodyKeys = TArray<FCk_ProceduralAction_TransformKey>{};
    auto PositionKeys = TArray<FCk_ProceduralAction_VectorKey>{};
    auto NewSamples = TArray<FKeySample>{};
    for (auto Index = 0; Index < RichKeys.Num(); ++Index)
    {
        const auto& RichKey = RichKeys[Index];
        const auto Time = ToKeyTime(*Timing, RichKey.Time);
        auto Key = SamplesByIndex.Contains(Index) ? SamplesByIndex[Index] : Sample(Old, Time);
        Key.Time = Time;
        Key.Value[InAxis] = RichKey.Value;
        Key.Arrive[InAxis] = RichKey.ArriveTangent * Seconds;
        Key.Leave[InAxis] = RichKey.LeaveTangent * Seconds;
        NewSamples.Add(Key);
        if (InChannel == ck::procedural_action_editor::Get_BodyChannelName())
        {
            auto BodyKey = FCk_ProceduralAction_TransformKey{FCk_FloatRange_0to1{Time}, Key.Value, Key.Rotation};
            BodyKey.Set_ArriveTangent(Key.Arrive);
            BodyKey.Set_LeaveTangent(Key.Leave);
            BodyKeys.Add(BodyKey);
        }
        else
        {
            auto PositionKey = FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{Time}, Key.Value};
            PositionKey.Set_ArriveTangent(Key.Arrive);
            PositionKey.Set_LeaveTangent(Key.Leave);
            PositionKeys.Add(PositionKey);
        }
    }

    const auto Interpolation = AllCubic && NOT RichKeys.IsEmpty()
        ? ECk_ProceduralAction_Interpolation::Smooth : ECk_ProceduralAction_Interpolation::Linear;
    auto Commands = FCk_ProceduralActionAuthoringCommands{Clip};
    auto CommandError = FString{};
    const auto Written = InChannel == ck::procedural_action_editor::Get_BodyChannelName()
        ? Commands.SetBodyKeys(_PhaseId, BodyKeys, Interpolation, Retimed, CommandError)
        : Commands.SetPositionKeys(_PhaseId, InChannel, PositionKeys, Interpolation, Retimed, CommandError);
    if (NOT Written)
    {
        Reject(CommandError);
        return;
    }
    Curves.Samples[InAxis].Reset();
    for (auto It = Curve.GetKeyHandleIterator(); It; ++It)
    {
        const auto Index = Curve.GetIndexSafe(*It);
        if (NewSamples.IsValidIndex(Index))
        { Curves.Samples[InAxis].Add(*It, NewSamples[Index]); }
    }
    _Error.Reset();
}

#undef LOCTEXT_NAMESPACE
