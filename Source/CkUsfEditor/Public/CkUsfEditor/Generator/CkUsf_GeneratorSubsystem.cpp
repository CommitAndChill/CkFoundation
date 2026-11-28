#include "CkUsfEditor/Generator/CkUsf_GeneratorSubsystem.h"

#include "CkUsfEditor/Generator/CkUsf_Generator.h"
#include "CkCore/Macros/CkMacros.h"

#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Rendering/SubstrateMaterialShared.h"
#include "RHI.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkUsf_GeneratorSubsystem::
    Get_CompiledSubstrateStats(
        TSoftObjectPtr<UMaterialInterface> InMaterial) const
    -> FCk_Usf_CompiledSubstrateStats
{
    auto Result = FCk_Usf_CompiledSubstrateStats{};
    const auto Interface = TObjectPtr<UMaterialInterface>{InMaterial.Get()};
    if (NOT IsValid(Interface.Get()))
    { Result.CompileErrors.Add(TEXT("The material interface is not loaded or valid.")); return Result; }
    if (Interface->GetMaterialResource(GMaxRHIShaderPlatform) == nullptr)
    { Result.CompileErrors.Add(TEXT("The active material resource is unavailable.")); return Result; }
    const auto& Resource = *Interface->GetMaterialResource(GMaxRHIShaderPlatform);
    Result.CompileErrors = Resource.GetCompileErrors();
    if (NOT Resource.IsCompilationFinished() || NOT Resource.IsGameThreadShaderMapComplete()
        || Resource.GetGameThreadShaderMap() == nullptr)
    { Result.CompileErrors.Add(TEXT("The active game-thread shader map is not complete.")); return Result; }
    Result.ClosureCount = Resource.MaterialGetSubstrateClosureCount_GameThread();
    Result.UintPerPixel = Resource.MaterialGetSubstrateUintPerPixel_GameThread();
    Result.MaterialType = Resource.MaterialGetSubstrateMaterialType_GameThread();
    const auto Features = Resource.MaterialGetSubstrateMaterialBsdfFeatures_GameThread();
    Result.BsdfFeatures = static_cast<int64>(Features);
    Result.HasGlint = EnumHasAnyFlags(Features, ESubstrateBsdfFeature::Glint);
    Result.ComplexSpecial = Result.MaterialType == SUBSTRATE_MATERIAL_TYPE_COMPLEX_SPECIAL;
    Result.Valid = Result.CompileErrors.IsEmpty();
    return Result;
}

void
    UCkUsf_GeneratorSubsystem::
    Generate_LookMaterials()
{
    ck::usf_editor::Generate_AllLookMaterials();
}

void
    UCkUsf_GeneratorSubsystem::
    Generate_LookMaterial_For(
        UCkUsf_LookDefinition* InLook)
{
    ck::usf_editor::Generate_LookMaterial(InLook);
}

// --------------------------------------------------------------------------------------------------------------------
