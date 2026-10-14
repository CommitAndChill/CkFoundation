#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#if WITH_EDITOR
#include <Misc/DataValidation.h>
#endif

#if WITH_EDITOR
auto
    UCk_ProceduralAction_Data::
    IsDataValid(FDataValidationContext& InContext) const
    -> EDataValidationResult
{
    auto Result = Super::IsDataValid(InContext);
    auto Compiled = ck::procedural_action::FCompiledClip{};
    auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{};
    if (NOT ck::procedural_action::Compile(*this, Compiled, Diagnostics))
    {
        for (const auto& Diagnostic : Diagnostics)
        {
            InContext.AddError(FText::FromString(FString::Printf(TEXT("%s [%s]: expected %s; got %s"),
                *Diagnostic.Get_FieldPath(), *Diagnostic.Get_Code().ToString(),
                *Diagnostic.Get_Expected(), *Diagnostic.Get_Actual())));
        }
        Result = EDataValidationResult::Invalid;
    }
    return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_ProceduralAction_Data::
    PostLoad()
    -> void
{
    Super::PostLoad();

    for (auto& Slot : _Slots)
    {
        const auto* FirstTrack = static_cast<const FCk_ProceduralAction_LimbTrack*>(nullptr);
        for (const auto& Phase : _Phases)
        {
            FirstTrack = Phase.Get_LimbTracks().FindByPredicate([&Slot](const FCk_ProceduralAction_LimbTrack& InTrack)
                { return InTrack.Get_Slot() == Slot.Get_Id(); });
            if (FirstTrack != nullptr)
            { break; }
        }
        if (FirstTrack == nullptr)
        { continue; }

        if (FirstTrack->_Space_DEPRECATED != ECk_ProceduralAction_TrackSpace::BodyLocal)
        { Slot.Set_Space(FirstTrack->_Space_DEPRECATED); }
    }

    for (auto& Phase : _Phases)
    {
        for (auto& Track : Phase.Get_LimbTracks())
        { Track._Space_DEPRECATED = ECk_ProceduralAction_TrackSpace::BodyLocal; }
    }
}

// --------------------------------------------------------------------------------------------------------------------
