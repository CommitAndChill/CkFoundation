#pragma once

#include "CoreMinimal.h"
#include "CkUsfEditor/Generator/CkUsf_ShadingModelContract.h"

class UCkUsf_LookDefinition;

namespace ck::usf_editor
{
    struct FLookValidationResult
    {
        TArray<FString> Errors;
        TArray<FString> Warnings;

        auto Get_IsValid() const -> bool { return Errors.IsEmpty(); }
    };

    // Runs BEFORE any material object is created: the generator wires Custom-node inputs to the look
    // function POSITIONALLY, so an asset-vs-.ush mismatch renders silently wrong when the types happen
    // to coincide. Checks and their error/warning split: CkUsf/CLAUDE.md § The parameter contract.
    CKUSFEDITOR_API auto Validate_LookDefinition(const UCkUsf_LookDefinition* InDef) -> FLookValidationResult;

    enum class ELookGenerationAdmission : uint8
    {
        Compatible,
        OtherRendererProfile,
        Invalid
    };

    struct FLookGenerationAdmissionResult
    {
        ELookGenerationAdmission Admission = ELookGenerationAdmission::Invalid;
        FLookValidationResult Validation;
        ECk_Usf_SurfaceGraph ResolvedGraph = ECk_Usf_SurfaceGraph::Legacy;
        FString RequiredProfile;
        FString Reason;
    };

    // Bulk generation/reload may exclude fully validated opposite-profile content. Single-look validation/generation stays strict.
    // OtherRendererProfile certifies authoring, not rendering or capacity in a future process.
    CKUSFEDITOR_API auto Classify_LookForGeneration(TObjectPtr<const UCkUsf_LookDefinition> InDef)
        -> FLookGenerationAdmissionResult;
    CKUSFEDITOR_API auto Classify_LookForGeneration(TObjectPtr<const UCkUsf_LookDefinition> InDef,
        const FSubstrateShadingModelSupport& InSupport) -> FLookGenerationAdmissionResult;
}
