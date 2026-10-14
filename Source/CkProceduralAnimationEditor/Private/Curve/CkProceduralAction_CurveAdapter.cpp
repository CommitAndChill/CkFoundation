#include "Curve/CkProceduralAction_CurveAdapter.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"

#include "CurveEditor.h"
#include "Curves/CurveFloat.h"
#include "RichCurveEditorModel.h"
#include "SCurveEditorPanel.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionCurveAdapter"

namespace ck_procedural_action_curve
{
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
}

FCkProceduralAction_CurveAdapter::~FCkProceduralAction_CurveAdapter()
{
    if (_PendingRebuild.IsValid())
    { FTSTicker::GetCoreTicker().RemoveTicker(_PendingRebuild); }
    if (_Editor.IsValid())
    { _Editor->RemoveAllCurves(); }
    _Panel.Reset();
    _Editor.Reset();
    _ProxyOwner.Reset();
}

auto
    FCkProceduralAction_CurveAdapter::
    Init(
        TSharedPtr<FCk_ProceduralActionAuthoringSession> InSession,
        FSimpleDelegate InOnChanged)
    -> TSharedRef<SWidget>
{
    _Session = InSession;
    _OnChanged = MoveTemp(InOnChanged);
    // CurveEditor modifies its model owner before broadcasting. The transient, nontransactional
    // owner keeps a rejected proxy edit out of the clip's undo history; accepted edits Modify the clip.
    _ProxyOwner.Reset(NewObject<UCurveFloat>(GetTransientPackage(), NAME_None, RF_Transient));
    _Editor = MakeShared<FCurveEditor>();
    _Editor->InitCurveEditor(FCurveEditorInitParams{});
    _Panel = SNew(SCurveEditorPanel, _Editor.ToSharedRef());
    return _Panel.ToSharedRef();
}

auto
    FCkProceduralAction_CurveAdapter::
    Select(
        FName InPhaseId,
        FName InSlotId,
        int32 InAxis)
    -> void
{
    _PhaseId = InPhaseId;
    _SlotId = InSlotId;
    _Axis = FMath::Clamp(InAxis, 0, 2);
    _Error.Reset();
    if (_InCallback)
    { ScheduleRebuild(); }
    else
    { RebuildCurve(); }
}

auto
    FCkProceduralAction_CurveAdapter::
    Refresh()
    -> void
{
    if (_InCallback)
    { ScheduleRebuild(); }
    else if (NOT _Rebuilding)
    { _Error.Reset(); RebuildCurve(); }
}

auto
    FCkProceduralAction_CurveAdapter::
    ScheduleRebuild()
    -> void
{
    if (_PendingRebuild.IsValid())
    { return; }
    _PendingRebuild = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateSP(AsShared(), &FCkProceduralAction_CurveAdapter::TickPendingRebuild));
}

auto
    FCkProceduralAction_CurveAdapter::
    TickPendingRebuild(
        float)
    -> bool
{
    _PendingRebuild.Reset();
    RebuildCurve();
    return false;
}

auto
    FCkProceduralAction_CurveAdapter::
    GetStatusText() const
    -> FText
{
    if (NOT _Error.IsEmpty())
    { return FText::FromString(_Error); }
    if (_PhaseId.IsNone())
    { return LOCTEXT("SelectPhase", "Select a phase in the timeline to edit its curve."); }
    const auto AxisName = _Axis == 0 ? TEXT("X") : _Axis == 1 ? TEXT("Y") : TEXT("Z");
    return FText::FromString(FString::Printf(TEXT("%s / %s / %s: normalized key time [0,1]"),
        *_PhaseId.ToString(), _SlotId.IsNone() ? TEXT("Body") : *_SlotId.ToString(), AxisName));
}

auto
    FCkProceduralAction_CurveAdapter::
    RebuildCurve()
    -> void
{
    if (NOT _Editor.IsValid())
    { return; }
    if (_PendingRebuild.IsValid())
    { FTSTicker::GetCoreTicker().RemoveTicker(_PendingRebuild); _PendingRebuild.Reset(); }
    _Rebuilding = true;
    _Editor->RemoveAllCurves();
    _Curve.Reset();
    _SamplesByHandle.Reset();
    const auto Session = _Session.Pin();
    const auto* Clip = Session.IsValid() ? Session->GetClip() : nullptr;
    if (Clip == nullptr || _PhaseId.IsNone())
    { _Rebuilding = false; return; }
    const auto* Phase = Clip->Get_Phases().FindByPredicate([this](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == _PhaseId; });
    if (Phase == nullptr)
    { _Rebuilding = false; return; }

    auto Smooth = false;
    if (_SlotId.IsNone())
    {
        Smooth = Phase->Get_BodyInterpolation() == ECk_ProceduralAction_Interpolation::Smooth;
        auto PreviousTime = -1.0f;
        for (const auto& Key : Phase->Get_BodyKeys())
        {
            const auto Time = Key.Get_Time().Get_Value();
            const auto CurveTime = static_cast<float>(Time);
            if (NOT FMath::IsFinite(Time) || NOT FMath::IsFinite(CurveTime)
                || Time < 0.0 || Time > 1.0 || CurveTime <= PreviousTime
                || Key.Get_Translation().ContainsNaN() || Key.Get_Rotation().ContainsNaN()
                || Key.Get_ArriveTangent().ContainsNaN() || Key.Get_LeaveTangent().ContainsNaN())
            {
                _Error = TEXT("Authored body keys require finite values and strictly increasing normalized times at curve precision; correct this draft in Details.");
                _Rebuilding = false;
                return;
            }
            PreviousTime = CurveTime;
        }
        for (const auto& Key : Phase->Get_BodyKeys())
        {
            const auto Handle = _Curve.AddKey(static_cast<float>(Key.Get_Time().Get_Value()),
                static_cast<float>(Key.Get_Translation()[_Axis]));
            _SamplesByHandle.Add(Handle, {Key.Get_Time().Get_Value(), Key.Get_Translation(),
                Key.Get_ArriveTangent(), Key.Get_LeaveTangent(), Key.Get_Rotation()});
            _Curve.SetKeyInterpMode(Handle, Smooth ? RCIM_Cubic : RCIM_Linear, false);
            auto& RichKey = _Curve.GetKey(Handle);
            RichKey.TangentMode = RCTM_User;
            RichKey.ArriveTangent = static_cast<float>(Key.Get_ArriveTangent()[_Axis]);
            RichKey.LeaveTangent = static_cast<float>(Key.Get_LeaveTangent()[_Axis]);
        }
    }
    else
    {
        const auto* Track = Phase->Get_LimbTracks().FindByPredicate([this](const FCk_ProceduralAction_LimbTrack& InTrack)
            { return InTrack.Get_Slot() == _SlotId; });
        if (Track == nullptr)
        { _Rebuilding = false; return; }
        Smooth = Track->Get_Interpolation() == ECk_ProceduralAction_Interpolation::Smooth;
        auto PreviousTime = -1.0f;
        for (const auto& Key : Track->Get_PositionKeys())
        {
            const auto Time = Key.Get_Time().Get_Value();
            const auto CurveTime = static_cast<float>(Time);
            if (NOT FMath::IsFinite(Time) || NOT FMath::IsFinite(CurveTime)
                || Time < 0.0 || Time > 1.0 || CurveTime <= PreviousTime
                || Key.Get_Value().ContainsNaN() || Key.Get_ArriveTangent().ContainsNaN()
                || Key.Get_LeaveTangent().ContainsNaN())
            {
                _Error = TEXT("Authored limb keys require finite values and strictly increasing normalized times at curve precision; correct this draft in Details.");
                _Rebuilding = false;
                return;
            }
            PreviousTime = CurveTime;
        }
        for (const auto& Key : Track->Get_PositionKeys())
        {
            const auto Handle = _Curve.AddKey(static_cast<float>(Key.Get_Time().Get_Value()),
                static_cast<float>(Key.Get_Value()[_Axis]));
            _SamplesByHandle.Add(Handle, {Key.Get_Time().Get_Value(), Key.Get_Value(),
                Key.Get_ArriveTangent(), Key.Get_LeaveTangent(), FQuat::Identity});
            _Curve.SetKeyInterpMode(Handle, Smooth ? RCIM_Cubic : RCIM_Linear, false);
            auto& RichKey = _Curve.GetKey(Handle);
            RichKey.TangentMode = RCTM_User;
            RichKey.ArriveTangent = static_cast<float>(Key.Get_ArriveTangent()[_Axis]);
            RichKey.LeaveTangent = static_cast<float>(Key.Get_LeaveTangent()[_Axis]);
        }
    }
    auto Model = MakeUnique<FRichCurveEditorModelRaw>(&_Curve, _ProxyOwner.Get());
    Model->SetShortDisplayName(GetStatusText());
    Model->SetColor(_Axis == 0 ? FLinearColor::Red : _Axis == 1 ? FLinearColor::Green : FLinearColor::Blue);
    Model->SetClampInputRange(TRange<double>{0.0, 1.0});
    Model->OnCurveModified().AddSP(AsShared(), &FCkProceduralAction_CurveAdapter::OnCurveModified);
    const auto ModelId = _Editor->AddCurve(MoveTemp(Model));
    _Editor->PinCurve(ModelId);
    _Rebuilding = false;
}

auto
    FCkProceduralAction_CurveAdapter::
    OnCurveModified()
    -> void
{
    if (_Rebuilding)
    { return; }
    TGuardValue<bool> CallbackGuard{_InCallback, true};
    const auto Session = _Session.Pin();
    auto* Clip = Session.IsValid() ? Session->GetClip() : nullptr;
    if (Clip == nullptr)
    { return; }
    auto Phases = Clip->Get_Phases();
    auto* Phase = Phases.FindByPredicate([this](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == _PhaseId; });
    if (Phase == nullptr)
    { return; }
    auto Tracks = Phase->Get_LimbTracks();
    auto* Track = _SlotId.IsNone() ? nullptr : Tracks.FindByPredicate(
        [this](const FCk_ProceduralAction_LimbTrack& InTrack)
        { return InTrack.Get_Slot() == _SlotId; });
    if (NOT _SlotId.IsNone() && Track == nullptr)
    { return; }

    const auto RichKeys = _Curve.GetConstRefOfKeys();
    auto AllCubic = true;
    auto AllLinear = true;
    for (const auto& Key : RichKeys)
    {
        AllCubic &= Key.InterpMode == RCIM_Cubic && Key.TangentWeightMode == RCTWM_WeightedNone;
        AllLinear &= Key.InterpMode == RCIM_Linear;
        if (NOT FMath::IsFinite(Key.Time) || Key.Time < 0.0f || Key.Time > 1.0f || NOT FMath::IsFinite(Key.Value))
        { _Error = TEXT("Curve keys require finite normalized time and value."); ScheduleRebuild(); return; }
    }
    if (NOT AllCubic && NOT AllLinear && NOT RichKeys.IsEmpty())
    { _Error = TEXT("Mixed or weighted interpolation is unsupported; select Linear or Cubic for all keys."); ScheduleRebuild(); return; }

    auto Old = TArray<ck_procedural_action_curve::FKeySample>{};
    if (_SlotId.IsNone())
    {
        for (const auto& Key : Phase->Get_BodyKeys())
        { Old.Add({Key.Get_Time().Get_Value(), Key.Get_Translation(), Key.Get_ArriveTangent(),
            Key.Get_LeaveTangent(), Key.Get_Rotation()}); }
    }
    else
    {
        for (const auto& Key : Track->Get_PositionKeys())
        { Old.Add({Key.Get_Time().Get_Value(), Key.Get_Value(), Key.Get_ArriveTangent(),
            Key.Get_LeaveTangent(), FQuat::Identity}); }
    }
    auto BodyKeys = TArray<FCk_ProceduralAction_TransformKey>{};
    auto PositionKeys = TArray<FCk_ProceduralAction_VectorKey>{};
    auto SamplesByIndex = TMap<int32, ck_procedural_action_curve::FKeySample>{};
    for (const auto& Entry : _SamplesByHandle)
    {
        const auto Index = _Curve.GetIndexSafe(Entry.Key);
        if (Index != INDEX_NONE)
        { SamplesByIndex.Add(Index, Entry.Value); }
    }
    auto NewSamples = TArray<ck_procedural_action_curve::FKeySample>{};
    for (auto Index = 0; Index < RichKeys.Num(); ++Index)
    {
        const auto& RichKey = RichKeys[Index];
        const auto Time = static_cast<double>(RichKey.Time);
        auto Sample = SamplesByIndex.Contains(Index)
            ? SamplesByIndex[Index] : ck_procedural_action_curve::Sample(Old, Time);
        Sample.Time = Time;
        Sample.Value[_Axis] = RichKey.Value;
        Sample.Arrive[_Axis] = RichKey.ArriveTangent;
        Sample.Leave[_Axis] = RichKey.LeaveTangent;
        NewSamples.Add(Sample);
        if (_SlotId.IsNone())
        {
            auto Key = FCk_ProceduralAction_TransformKey{FCk_FloatRange_0to1{Time},
                Sample.Value, Sample.Rotation};
            Key.Set_ArriveTangent(Sample.Arrive);
            Key.Set_LeaveTangent(Sample.Leave);
            BodyKeys.Add(Key);
        }
        else
        {
            auto Key = FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{Time}, Sample.Value};
            Key.Set_ArriveTangent(Sample.Arrive);
            Key.Set_LeaveTangent(Sample.Leave);
            PositionKeys.Add(Key);
        }
    }

    Clip->Modify();
    if (_SlotId.IsNone())
    {
        Phase->Set_BodyKeys(BodyKeys);
        Phase->Set_BodyInterpolation(AllCubic
            ? ECk_ProceduralAction_Interpolation::Smooth : ECk_ProceduralAction_Interpolation::Linear);
    }
    else
    {
        Track->Set_PositionKeys(PositionKeys);
        Track->Set_Interpolation(AllCubic
            ? ECk_ProceduralAction_Interpolation::Smooth : ECk_ProceduralAction_Interpolation::Linear);
        Phase->Set_LimbTracks(Tracks);
    }
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    _SamplesByHandle.Reset();
    for (auto It = _Curve.GetKeyHandleIterator(); It; ++It)
    {
        const auto Index = _Curve.GetIndexSafe(*It);
        if (NewSamples.IsValidIndex(Index))
        { _SamplesByHandle.Add(*It, NewSamples[Index]); }
    }
    _Error.Reset();
    _OnChanged.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
