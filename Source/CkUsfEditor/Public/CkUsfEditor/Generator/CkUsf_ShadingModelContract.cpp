#include "CkUsfEditor/Generator/CkUsf_ShadingModelContract.h"

#include "CkCore/Validation/CkIsValid.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionThinTranslucentMaterialOutput.h"
#include "Misc/FileHelper.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"

#if __has_include("Rendering/CkUsfSubstrateSupport.h")
#include "Rendering/CkUsfSubstrateSupport.h"
#endif

namespace ck::usf_editor
{
    auto
        Get_SubstrateShadingModelSupport()
        -> FSubstrateShadingModelSupport
    {
        auto Support = FSubstrateShadingModelSupport{};
        Support.Enabled = Substrate::IsSubstrateEnabled();
        const auto Platform = GMaxRHIShaderPlatform;
        if (Support.Enabled && static_cast<uint32>(Platform) < static_cast<uint32>(SP_NumPlatforms)
            && FDataDrivenShaderPlatformInfo::IsValid(Platform))
        { Support.BlendableGBuffer = Substrate::IsSubstrateBlendableGBufferEnabled(Platform); }
#if defined(UE_CK_USF_SUBSTRATE_BLENDABLE_VERSION) && UE_CK_USF_SUBSTRATE_BLENDABLE_VERSION >= 1
        Support.ToonBridge = true;
#endif
        return Support;
    }

    auto
        Validate_SubstrateShadingModelSupport(
            const FShadingModelContract& InContract,
            const FSubstrateShadingModelSupport& InSupport,
            TArray<FString>& OutErrors)
        -> bool
    {
        if (NOT InSupport.Enabled)
        { return true; }
        const auto InitialErrors = OutErrors.Num();
        if (NOT InSupport.BlendableGBuffer)
        { OutErrors.Add(TEXT("CkUsf source-model selection under Substrate requires Blendable GBuffer (r.Substrate.ProjectGBufferFormat=0); Adaptive is not supported by this prototype.")); }
        if (InContract.Model == ECk_Usf_ShadingModel::Toon && NOT InSupport.ToonBridge)
        { OutErrors.Add(TEXT("Substrate Toon requires the engine's UE_CK_USF_SUBSTRATE_BLENDABLE_VERSION >= 1 bridge; rebuild the matching engine/editor.")); }
        if (NOT InSupport.LegacyRoot)
        { OutErrors.Add(TEXT("CkUsf source-model selection under Substrate requires legacy-root material authoring; explicitly connected FrontMaterial graphs are unsupported.")); }
        if (NOT InSupport.SingleShadingModel)
        { OutErrors.Add(TEXT("CkUsf source-model selection under Substrate requires one static shading model; dynamic or multiple models are unsupported.")); }
        return OutErrors.Num() == InitialErrors;
    }

    namespace shading_model_contract
    {
        struct FModelEntry
        {
            FString Name;
            ECk_Usf_ShadingModel Model;
            EMaterialShadingModel Native;
        };

        auto Get_Models() -> TArray<FModelEntry>
        {
            return {
                {TEXT("Unlit"), ECk_Usf_ShadingModel::Unlit, MSM_Unlit},
                {TEXT("DefaultLit"), ECk_Usf_ShadingModel::DefaultLit, MSM_DefaultLit},
                {TEXT("Subsurface"), ECk_Usf_ShadingModel::Subsurface, MSM_Subsurface},
                {TEXT("PreintegratedSkin"), ECk_Usf_ShadingModel::PreintegratedSkin, MSM_PreintegratedSkin},
                {TEXT("ClearCoat"), ECk_Usf_ShadingModel::ClearCoat, MSM_ClearCoat},
                {TEXT("SubsurfaceProfile"), ECk_Usf_ShadingModel::SubsurfaceProfile, MSM_SubsurfaceProfile},
                {TEXT("TwoSidedFoliage"), ECk_Usf_ShadingModel::TwoSidedFoliage, MSM_TwoSidedFoliage},
                {TEXT("Hair"), ECk_Usf_ShadingModel::Hair, MSM_Hair},
                {TEXT("Cloth"), ECk_Usf_ShadingModel::Cloth, MSM_Cloth},
                {TEXT("Eye"), ECk_Usf_ShadingModel::Eye, MSM_Eye},
                {TEXT("SingleLayerWater"), ECk_Usf_ShadingModel::SingleLayerWater, MSM_SingleLayerWater},
                {TEXT("ThinTranslucent"), ECk_Usf_ShadingModel::ThinTranslucent, MSM_ThinTranslucent},
#if defined(UE_CK_USF_SHADING_MODEL_VERSION) && UE_CK_USF_SHADING_MODEL_VERSION >= 1
                {TEXT("Toon"), ECk_Usf_ShadingModel::Toon, MSM_CkUsf},
#else
                {TEXT("Toon"), ECk_Usf_ShadingModel::Toon, MSM_NUM},
#endif
            };
        }
    }

    auto Parse_ShadingModelDeclaration(const FString& InSource,
        TOptional<ECk_Usf_ShadingModel>& OutModel, FString& OutError) -> bool
    {
        OutModel.Reset();
        OutError.Reset();
        auto Parsed = TOptional<ECk_Usf_ShadingModel>{};
        auto Header = true;
        auto BlockComment = false;
        auto Lines = TArray<FString>{};
        InSource.ParseIntoArrayLines(Lines, false);
        for (auto Line : Lines)
        {
            Line.TrimStartAndEndInline();
            const auto Marker = Line.Find(TEXT("@ckusf"), ESearchCase::CaseSensitive);
            if (Marker != INDEX_NONE)
            {
                if (BlockComment || NOT Header || NOT Line.StartsWith(TEXT("// @ckusf shading_model ")) || Parsed.IsSet())
                {
                    OutError = TEXT("Shading-model declaration must occur once in the leading top-level // comment header");
                    return false;
                }
                const auto Name = Line.Mid(24).TrimStartAndEnd();
                for (const auto& Entry : shading_model_contract::Get_Models())
                {
                    if (Name == Entry.Name)
                    { Parsed = Entry.Model; break; }
                }
                if (NOT Parsed.IsSet())
                {
                    OutError = FString::Printf(TEXT("Unknown shading-model declaration [%s]; use a symbolic model name"), *Name);
                    return false;
                }
                continue;
            }
            // Only whitespace and comments may precede metadata. Preprocessor branches terminate the header.
            for (auto Index = 0; Index < Line.Len(); ++Index)
            {
                if (BlockComment)
                {
                    if (Line.Mid(Index, 2) == TEXT("*/"))
                    { BlockComment = false; ++Index; }
                    continue;
                }
                if (Line.Mid(Index, 2) == TEXT("//"))
                { break; }
                if (Line.Mid(Index, 2) == TEXT("/*"))
                { BlockComment = true; ++Index; continue; }
                if (NOT FChar::IsWhitespace(Line[Index]))
                { Header = false; }
            }
        }
        OutModel = Parsed;
        return true;
    }

    auto Resolve_ShadingModelContract(const UCkUsf_LookDefinition& InDefinition,
        FShadingModelContract& OutContract, TArray<FString>& OutErrors) -> bool
    {
        const auto InitialErrors = OutErrors.Num();
        auto Candidate = FShadingModelContract{};
        auto Source = FString{};
        auto PathErrors = TArray<FShaderCompilerError>{};
        const auto DiskPath = GetShaderSourceFilePath(InDefinition._UshIncludePath, &PathErrors);
        if (DiskPath.IsEmpty() || NOT FFileHelper::LoadFileToString(Source, *DiskPath))
        {
            OutErrors.Add(TEXT("Shading-model contract: entrypoint source is not readable"));
            return false;
        }
        auto Declared = TOptional<ECk_Usf_ShadingModel>{};
        auto ParseError = FString{};
        if (NOT Parse_ShadingModelDeclaration(Source, Declared, ParseError))
        {
            OutErrors.Add(ParseError);
            return false;
        }
        const auto Surface = InDefinition._Domain == ECk_Usf_Domain::SurfaceLit
            || InDefinition._Domain == ECk_Usf_Domain::SurfaceUnlit;
        if (NOT Surface && Declared.IsSet())
        {
            OutErrors.Add(TEXT("Shading-model source declarations require a surface domain"));
            return false;
        }
        Candidate.SourceOverride = Declared.IsSet();
        Candidate.Model = Declared.Get(InDefinition._ShadingModel);
        if (NOT Surface || Candidate.Model == ECk_Usf_ShadingModel::Inherit)
        {
            Candidate.Model = InDefinition._Domain == ECk_Usf_Domain::SurfaceLit
                ? ECk_Usf_ShadingModel::DefaultLit : ECk_Usf_ShadingModel::Unlit;
        }
        auto Found = false;
        for (const auto& Entry : shading_model_contract::Get_Models())
        {
            if (Entry.Model == Candidate.Model)
            { Candidate.NativeModel = Entry.Native; Found = true; break; }
        }
        if (NOT Found || Candidate.NativeModel == MSM_NUM)
        { OutErrors.Add(TEXT("Unsupported shading model for this engine")); }
        Candidate.BlendMode = InDefinition._Domain == ECk_Usf_Domain::SurfaceLit
            || InDefinition._Domain == ECk_Usf_Domain::PostProcess ? BLEND_Opaque : BLEND_Translucent;
        if (Surface)
        {
            if (InDefinition._BlendMode == ECk_Usf_BlendMode::Inherit)
            {
                if (Candidate.Model == ECk_Usf_ShadingModel::ThinTranslucent)
                { Candidate.BlendMode = BLEND_Translucent; }
                else if (Candidate.SourceOverride || static_cast<uint8>(Candidate.Model) > static_cast<uint8>(ECk_Usf_ShadingModel::Toon))
                { Candidate.BlendMode = BLEND_Opaque; }
            }
            else
            {
                switch (InDefinition._BlendMode)
                {
                    case ECk_Usf_BlendMode::Opaque: Candidate.BlendMode = BLEND_Opaque; break;
                    case ECk_Usf_BlendMode::Masked: Candidate.BlendMode = BLEND_Masked; break;
                    case ECk_Usf_BlendMode::Translucent: Candidate.BlendMode = BLEND_Translucent; break;
                    case ECk_Usf_BlendMode::Additive: Candidate.BlendMode = BLEND_Additive; break;
                    case ECk_Usf_BlendMode::Modulate: Candidate.BlendMode = BLEND_Modulate; break;
                    default: OutErrors.Add(TEXT("Unsupported blend-mode value")); break;
                }
            }
        }
        const auto Opaque = Candidate.BlendMode == BLEND_Opaque;
        const auto OpaqueOrMasked = Opaque || Candidate.BlendMode == BLEND_Masked;
        if (Surface && Candidate.Model == ECk_Usf_ShadingModel::ThinTranslucent && Candidate.BlendMode != BLEND_Translucent)
        { OutErrors.Add(TEXT("ThinTranslucent requires Translucent blend")); }
        else if (Surface && (Candidate.Model == ECk_Usf_ShadingModel::Toon || Candidate.Model == ECk_Usf_ShadingModel::SingleLayerWater) && NOT Opaque)
        { OutErrors.Add(TEXT("Toon and SingleLayerWater require Opaque blend")); }
        else if (Surface && Candidate.Model != ECk_Usf_ShadingModel::Unlit
            && Candidate.Model != ECk_Usf_ShadingModel::DefaultLit
            && Candidate.Model != ECk_Usf_ShadingModel::ThinTranslucent && NOT OpaqueOrMasked)
        { OutErrors.Add(TEXT("Selected shading model requires Opaque or Masked blend")); }
        Candidate.TwoSided = InDefinition._TwoSided || Candidate.Model == ECk_Usf_ShadingModel::TwoSidedFoliage;
        Candidate.TangentSpaceNormal = Candidate.Model != ECk_Usf_ShadingModel::Hair;
        if (Candidate.Model == ECk_Usf_ShadingModel::Eye || Candidate.Model == ECk_Usf_ShadingModel::SubsurfaceProfile)
        {
            Candidate.Profile = InDefinition._SubsurfaceProfile;
            if (ck::Is_NOT_Valid(Candidate.Profile))
            { OutErrors.Add(TEXT("Eye and SubsurfaceProfile require a valid _SubsurfaceProfile asset")); }
        }
        switch (InDefinition._TranslucencyLighting)
        {
            case ECk_Usf_TranslucencyLighting::Inherit:
            case ECk_Usf_TranslucencyLighting::VolumetricNonDirectional: break;
            case ECk_Usf_TranslucencyLighting::VolumetricDirectional: Candidate.TranslucencyLighting = TLM_VolumetricDirectional; break;
            case ECk_Usf_TranslucencyLighting::Surface: Candidate.TranslucencyLighting = TLM_Surface; break;
            case ECk_Usf_TranslucencyLighting::SurfacePerPixel: Candidate.TranslucencyLighting = TLM_SurfacePerPixelLighting; break;
            default: OutErrors.Add(TEXT("Unsupported translucency-lighting value")); break;
        }
        if (Candidate.Model == ECk_Usf_ShadingModel::ThinTranslucent)
        {
            if (InDefinition._TranslucencyLighting != ECk_Usf_TranslucencyLighting::Inherit
                && InDefinition._TranslucencyLighting != ECk_Usf_TranslucencyLighting::SurfacePerPixel)
            { OutErrors.Add(TEXT("ThinTranslucent requires SurfacePerPixel translucency lighting")); }
            Candidate.TranslucencyLighting = TLM_SurfacePerPixelLighting;
        }
        if (OutErrors.Num() != InitialErrors)
        { return false; }
        OutContract = Candidate;
        return true;
    }

    auto Get_ShadingModelOutputs(const FShadingModelContract& InContract) -> TArray<FShadingModelOutput>
    {
        auto Outputs = TArray<FShadingModelOutput>{
            {TEXT("EmissiveColor"), CMOT_Float3, MP_EmissiveColor},
            {TEXT("Roughness"), CMOT_Float1, MP_Roughness},
            {TEXT("Metallic"), CMOT_Float1, MP_Metallic},
            {TEXT("Specular"), CMOT_Float1, MP_Specular},
            {TEXT("AmbientOcclusion"), CMOT_Float1, MP_AmbientOcclusion},
            {TEXT("Normal"), CMOT_Float3, MP_Normal},
            {TEXT("Opacity"), CMOT_Float1, MP_Opacity},
            {TEXT("OpacityMask"), CMOT_Float1, MP_OpacityMask},
            {TEXT("Refraction"), CMOT_Float1, MP_Refraction},
            {TEXT("SubsurfaceColor"), CMOT_Float3, MP_SubsurfaceColor},
            {TEXT("ClearCoat"), CMOT_Float1, MP_CustomData0},
            {TEXT("ClearCoatRoughness"), CMOT_Float1, MP_CustomData1}
        };
        switch (InContract.Model)
        {
            case ECk_Usf_ShadingModel::Toon:
                Outputs.Add({TEXT("ToonThreshold"), CMOT_Float1, MP_CustomData0});
                Outputs.Add({TEXT("ToonSoftness"), CMOT_Float1, MP_CustomData1});
                break;
            case ECk_Usf_ShadingModel::Hair:
                Outputs.Add({TEXT("CkUsf_HairTangent"), CMOT_Float3, MP_Normal});
                Outputs.Add({TEXT("CkUsf_HairScatter"), CMOT_Float1, MP_Metallic});
                Outputs.Add({TEXT("CkUsf_HairBacklit"), CMOT_Float1, MP_CustomData0});
                break;
            case ECk_Usf_ShadingModel::Cloth:
                Outputs.Add({TEXT("CkUsf_ClothMask"), CMOT_Float1, MP_CustomData0});
                break;
            case ECk_Usf_ShadingModel::Eye:
                Outputs.Add({TEXT("CkUsf_EyeIrisMask"), CMOT_Float1, MP_CustomData0});
                Outputs.Add({TEXT("CkUsf_EyeIrisDistance"), CMOT_Float1, MP_CustomData1});
                break;
            case ECk_Usf_ShadingModel::SingleLayerWater:
                Outputs.Add({TEXT("CkUsf_WaterScattering"), CMOT_Float3, MP_MAX});
                Outputs.Add({TEXT("CkUsf_WaterAbsorption"), CMOT_Float3, MP_MAX});
                Outputs.Add({TEXT("CkUsf_WaterPhaseG"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_WaterColorScaleBehindWater"), CMOT_Float3, MP_MAX});
                break;
            case ECk_Usf_ShadingModel::ThinTranslucent:
                Outputs.Add({TEXT("CkUsf_ThinTransmittance"), CMOT_Float3, MP_MAX});
                Outputs.Add({TEXT("CkUsf_ThinSurfaceCoverage"), CMOT_Float1, MP_MAX});
                break;
            default: break;
        }
        return Outputs;
    }

    auto Build_ShadingModelOutputCode(const FShadingModelContract& InContract) -> FString
    {
        auto Code = FString{};
        for (const auto& Output : Get_ShadingModelOutputs(InContract))
        {
            const auto Field = Output.Name.StartsWith(TEXT("CkUsf_")) ? Output.Name.Mid(6) : Output.Name;
            if (Output.Name == TEXT("ToonThreshold") || Output.Name == TEXT("ToonSoftness"))
            { Code += FString::Printf(TEXT("%s = saturate(O.%s);\n"), *Output.Name, *Field); }
            else
            { Code += FString::Printf(TEXT("%s = O.%s;\n"), *Output.Name, *Field); }
        }
        return Code;
    }

    auto Apply_ShadingModelContract(UMaterial& InMaterial,
        UMaterialExpressionCustom& InCustom, const FShadingModelContract& InContract) -> void
    {
        InMaterial.SetShadingModel(InContract.NativeModel);
        InMaterial.BlendMode = InContract.BlendMode;
        InMaterial.TranslucencyLightingMode = InContract.TranslucencyLighting;
        InMaterial.TranslucencyPass = InContract.Model == ECk_Usf_ShadingModel::ThinTranslucent ? MTP_BeforeDOF : MTP_AfterDOF;
        InMaterial.TwoSided = InContract.TwoSided;
        InMaterial.bTangentSpaceNormal = InContract.TangentSpaceNormal;
        InMaterial.SubsurfaceProfile = InContract.Profile;

        auto& Expressions = InMaterial.GetExpressionCollection();
        const auto Existing = Expressions.Expressions;
        for (const auto& Expression : Existing)
        {
            if (Expression && Expression->Desc == TEXT("CkUsf_ShadingModelContract")
                && (Expression->IsA<UMaterialExpressionSingleLayerWaterMaterialOutput>()
                    || Expression->IsA<UMaterialExpressionThinTranslucentMaterialOutput>()))
            { Expressions.RemoveExpression(Expression); }
        }
        const auto Translucent = InContract.BlendMode == BLEND_Translucent
            || InContract.BlendMode == BLEND_Additive || InContract.BlendMode == BLEND_Modulate
            || InContract.BlendMode == BLEND_AlphaComposite;
        InMaterial.RefractionMethod = Translucent && InContract.NativeModel != MSM_Unlit
            ? RM_IndexOfRefraction : GetDefault<UMaterial>()->RefractionMethod.GetValue();
        const auto Subsurface = InContract.Model == ECk_Usf_ShadingModel::Subsurface
            || InContract.Model == ECk_Usf_ShadingModel::PreintegratedSkin
            || InContract.Model == ECk_Usf_ShadingModel::TwoSidedFoliage
            || InContract.Model == ECk_Usf_ShadingModel::SubsurfaceProfile
            || InContract.Model == ECk_Usf_ShadingModel::Eye;
        const auto Outputs = Get_ShadingModelOutputs(InContract);
        for (const auto Property : {MP_BaseColor, MP_Normal, MP_Metallic, MP_Specular, MP_Roughness,
            MP_AmbientOcclusion, MP_EmissiveColor, MP_Opacity, MP_OpacityMask, MP_Refraction,
            MP_SubsurfaceColor, MP_CustomData0, MP_CustomData1})
        {
            InMaterial.GetExpressionInputForProperty(Property)->Expression = nullptr;
        }
        InMaterial.GetExpressionInputForProperty(MP_BaseColor)->Connect(0, &InCustom);
        for (auto Index = 0; Index < Outputs.Num(); ++Index)
        {
            const auto& Output = Outputs[Index];
            if (Output.Property == MP_MAX)
            { continue; }
            if (Output.Name == TEXT("Opacity") && NOT Translucent && NOT Subsurface
                && InContract.Model != ECk_Usf_ShadingModel::SingleLayerWater)
            { continue; }
            if (Output.Name == TEXT("OpacityMask") && InContract.BlendMode != BLEND_Masked)
            { continue; }
            if (Output.Name == TEXT("Refraction") && (NOT Translucent || InContract.NativeModel == MSM_Unlit))
            { continue; }
            if (Output.Name == TEXT("SubsurfaceColor") && InContract.Model != ECk_Usf_ShadingModel::Subsurface
                && InContract.Model != ECk_Usf_ShadingModel::PreintegratedSkin
                && InContract.Model != ECk_Usf_ShadingModel::TwoSidedFoliage && InContract.Model != ECk_Usf_ShadingModel::Cloth)
            { continue; }
            if ((Output.Name == TEXT("ClearCoat") || Output.Name == TEXT("ClearCoatRoughness"))
                && InContract.Model != ECk_Usf_ShadingModel::ClearCoat)
            { continue; }
            InMaterial.GetExpressionInputForProperty(Output.Property)->Connect(Index + 1, &InCustom);
        }
        if (InContract.Model == ECk_Usf_ShadingModel::SingleLayerWater)
        {
            const auto Node = TObjectPtr<UMaterialExpressionSingleLayerWaterMaterialOutput>{
                NewObject<UMaterialExpressionSingleLayerWaterMaterialOutput>(&InMaterial)};
            Node->Material = &InMaterial;
            Node->MaterialExpressionGuid = FGuid::NewGuid();
            Node->Desc = TEXT("CkUsf_ShadingModelContract");
            Expressions.AddExpression(Node);
            Node->ScatteringCoefficients.Connect(13, &InCustom);
            Node->AbsorptionCoefficients.Connect(14, &InCustom);
            Node->PhaseG.Connect(15, &InCustom);
            Node->ColorScaleBehindWater.Connect(16, &InCustom);
        }
        else if (InContract.Model == ECk_Usf_ShadingModel::ThinTranslucent)
        {
            const auto Node = TObjectPtr<UMaterialExpressionThinTranslucentMaterialOutput>{
                NewObject<UMaterialExpressionThinTranslucentMaterialOutput>(&InMaterial)};
            Node->Material = &InMaterial;
            Node->MaterialExpressionGuid = FGuid::NewGuid();
            Node->Desc = TEXT("CkUsf_ShadingModelContract");
            Expressions.AddExpression(Node);
            Node->TransmittanceColor.Connect(13, &InCustom);
            Node->SurfaceCoverage.Connect(14, &InCustom);
        }
    }
}
