#pragma once

#include "CkUsfEditor/Generator/CkUsf_ShadingModelContract.h"

namespace ck::usf_editor
{
    auto Create_NativeSurfaceGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
        const FShadingModelContract& InContract, TArray<FString>& OutErrors) -> bool;
    auto Record_NativeSurfaceGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
        const UCkUsf_LookDefinition& InDefinition, const FShadingModelContract& InContract) -> void;
    auto Validate_NativeSurfaceGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
        const UCkUsf_LookDefinition& InDefinition, const FShadingModelContract& InContract,
        TArray<FString>& OutErrors) -> bool;
}
