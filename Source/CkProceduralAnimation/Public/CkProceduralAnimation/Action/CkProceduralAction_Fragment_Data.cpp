#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include <String/LexFromString.h>
#include <UObject/Class.h>
#include <UObject/PropertyPortFlags.h>

auto
    FCk_ProceduralAction_PlayOptions::
    Get_HasConsistentTuningSelection() const
    -> bool
{
    if (_TuningMode == ECk_ProceduralAction_TuningMode::Override)
    { return true; }
    if (_TuningMode != ECk_ProceduralAction_TuningMode::PlayerDefault)
    { return false; }

    const auto Default = FCk_ProceduralAction_Tuning{};
    const auto* TuningStruct = FCk_ProceduralAction_Tuning::StaticStruct();
    if (TuningStruct->CompareScriptStruct(&_Tuning, &Default, PPF_None))
    { return true; }

    // MakeStruct exports the native double default to text and imports it again.
    // Only that exact engine roundtrip may differ in PlayerDefault mode.
    const auto DefaultSeconds = Default.Get_Sampling().Get_MaxAdvance().Get_Seconds();
    const auto DefaultText = LexToString(DefaultSeconds);
    auto BlueprintSeconds = double{};
    LexFromString(BlueprintSeconds, FStringView{*DefaultText, DefaultText.Len()});
    if (_Tuning.Get_Sampling().Get_MaxAdvance().Get_Seconds() != BlueprintSeconds)
    { return false; }

    auto Normalized = _Tuning;
    auto Sampling = Normalized.Get_Sampling();
    Sampling.Set_MaxAdvance(Default.Get_Sampling().Get_MaxAdvance());
    Normalized.Set_Sampling(Sampling);
    return TuningStruct->CompareScriptStruct(&Normalized, &Default, PPF_None);
}
