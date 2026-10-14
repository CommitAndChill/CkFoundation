#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"

namespace ck
{
    auto
    FFragment_ProceduralAction::CommitState(
        const FCk_Handle_ProceduralAction& InHandle,
        const FFragment_ProceduralAction_Params& InParams,
        ECk_ProceduralAction_State InState)
        -> void
    {
        if (_State == InState) { return; }
        const auto Terminal = InState == ECk_ProceduralAction_State::Finished
            ? TOptional<FCk_ProceduralAction_Terminal>{FCk_ProceduralAction_Terminal{
                InParams.Get_Identity(), _Generation, _Result, _Context}}
            : TOptional<FCk_ProceduralAction_Terminal>{};
        _State = InState;
        UUtils_Signal_OnProceduralAction_StateChanged::Broadcast(
            InHandle, MakePayload(InHandle, InState));
        if (Terminal.IsSet())
        {
            UUtils_Signal_OnProceduralAction_Finished::Broadcast(InHandle,
                MakePayload(InHandle, Terminal.GetValue()));
        }
    }

    auto
    FFragment_ProceduralAction::BeginFailureContext(
        const FCk_Handle_ProceduralAction& InHandle,
        const FFragment_ProceduralAction_Params& InParams)
        -> FCk_ProceduralAction_FailureContext&
    {
        _Context = {};
        if (NOT InHandle.Has<FFragment_ProceduralAction_Compiled>())
        { return _Context; }

        const auto& Compiled = InHandle.Get<FFragment_ProceduralAction_Compiled>();
        _Context.Set_LimbIds(Compiled.Get_BoundLimbIds());
        auto Evaluated = procedural_action::FEvaluatedPose{};
        if (_HasClaim && Compiled.Get_Clip().IsValid()
            && procedural_action::Evaluate(*Compiled.Get_Clip(), _InitialPose, _AcceptedTime, Evaluated)
            && Compiled.Get_Clip()->Get_Phases().IsValidIndex(Evaluated.Get_PhaseIndex()))
        { _Context.Set_PhaseId(Compiled.Get_Clip()->Get_Phases()[Evaluated.Get_PhaseIndex()].Get_Id()); }
        return _Context;
    }
}
