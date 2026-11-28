#include "CkUsfEditor/Generator/CkUsf_ShadingModelContract.h"

#include "CkCore/Validation/CkIsValid.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionThinTranslucentMaterialOutput.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
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
        {
            Support.BlendableGBuffer = Substrate::IsSubstrateBlendableGBufferEnabled(Platform);
            Support.GlintsEnabled = Substrate::IsGlintEnabled(Platform);
            Support.MaxClosuresPerPixel = Substrate::GetClosurePerPixel(Platform, GetCurrentMaterialQualityLevelChecked());
        }
#if defined(UE_CK_USF_SUBSTRATE_BLENDABLE_VERSION) && UE_CK_USF_SUBSTRATE_BLENDABLE_VERSION >= 1
        Support.ToonBridge = true;
#endif
        return Support;
    }

    auto Is_AdaptiveSurfaceGraph(ECk_Usf_SurfaceGraph InGraph) -> bool
    {
        return InGraph == ECk_Usf_SurfaceGraph::AdaptiveVertical
            || InGraph == ECk_Usf_SurfaceGraph::AdaptiveMix;
    }

    auto Is_OpticalSurfaceGraph(ECk_Usf_SurfaceGraph InGraph) -> bool
    {
        return InGraph == ECk_Usf_SurfaceGraph::NativeTranslucentSlab
            || InGraph == ECk_Usf_SurfaceGraph::NativeRefractiveSlab;
    }

    auto Requires_AdaptiveGBuffer(ECk_Usf_SurfaceGraph InGraph) -> bool
    {
        return Is_AdaptiveSurfaceGraph(InGraph) || Is_OpticalSurfaceGraph(InGraph)
            || Is_LayeredOpticalSurfaceGraph(InGraph) || Is_Stack3OpticalSurfaceGraph(InGraph);
    }

    auto Is_LayeredOpticalSurfaceGraph(ECk_Usf_SurfaceGraph InGraph) -> bool
    {
        return InGraph == ECk_Usf_SurfaceGraph::NativeTranslucentVertical;
    }

    auto Is_Stack3OpticalSurfaceGraph(ECk_Usf_SurfaceGraph InGraph) -> bool
    {
        return InGraph == ECk_Usf_SurfaceGraph::NativeTranslucentStack3;
    }

    auto Get_RequiredSubstrateClosureCount(ECk_Usf_SurfaceGraph InGraph) -> uint32
    {
        if (Is_Stack3OpticalSurfaceGraph(InGraph))
        { return 3u; }
        return Is_AdaptiveSurfaceGraph(InGraph) || Is_LayeredOpticalSurfaceGraph(InGraph) ? 2u : 1u;
    }

    auto
        Validate_SubstrateShadingModelSupport(
            const FShadingModelContract& InContract,
            const FSubstrateShadingModelSupport& InSupport,
            TArray<FString>& OutErrors)
        -> bool
    {
        if (InContract.AdaptiveBaseGlints && InContract.SurfaceGraph != ECk_Usf_SurfaceGraph::AdaptiveVertical)
        {
            OutErrors.Add(TEXT("_AdaptiveBaseGlints requires AdaptiveVertical"));
            return false;
        }
        if (InContract.SurfaceGraph != ECk_Usf_SurfaceGraph::Legacy)
        {
            if (Requires_AdaptiveGBuffer(InContract.SurfaceGraph))
            {
                const auto RequiredClosures = Get_RequiredSubstrateClosureCount(InContract.SurfaceGraph);
                if (NOT InSupport.Enabled || InSupport.BlendableGBuffer || InSupport.MaxClosuresPerPixel < RequiredClosures)
                {
                    OutErrors.Add(RequiredClosures == 3u
                        ? TEXT("Native translucent stack requires enabled Substrate with Adaptive GBuffer and at least three supported closures per pixel")
                        : RequiredClosures == 2u
                            ? TEXT("Adaptive CkUsf graphs require enabled Substrate with Adaptive GBuffer and at least two supported closures per pixel")
                            : TEXT("Native translucent Slab requires enabled Substrate with Adaptive GBuffer and at least one supported closure per pixel"));
                    return false;
                }
                if (InContract.AdaptiveBaseGlints && NOT InSupport.GlintsEnabled)
                {
                    OutErrors.Add(TEXT("_AdaptiveBaseGlints requires native Substrate glints enabled for the active shader platform (r.Substrate.Glints)."));
                    return false;
                }
                return true;
            }
            if (NOT InSupport.Enabled || NOT InSupport.BlendableGBuffer)
            {
                OutErrors.Add(TEXT("Native CkUsf BSDF graphs require enabled Substrate with Blendable GBuffer"));
                return false;
            }
            return true;
        }
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

    static auto Parse_SurfaceDeclarations(const FString& InSource,
        TOptional<ECk_Usf_ShadingModel>& OutModel, TOptional<ECk_Usf_SurfaceGraph>& OutGraph,
        FString& OutError) -> bool
    {
        OutModel.Reset();
        OutGraph.Reset();
        OutError.Reset();
        auto Parsed = TOptional<ECk_Usf_ShadingModel>{};
        auto ParsedGraph = TOptional<ECk_Usf_SurfaceGraph>{};
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
                const auto ModelPrefix = FString{TEXT("// @ckusf shading_model ")};
                const auto GraphPrefix = FString{TEXT("// @ckusf surface_graph ")};
                if (BlockComment || NOT Header || Parsed.IsSet() || ParsedGraph.IsSet()
                    || (NOT Line.StartsWith(ModelPrefix) && NOT Line.StartsWith(GraphPrefix)))
                {
                    OutError = TEXT("Surface declaration must occur once in the leading top-level // comment header; shading_model and surface_graph cannot be mixed");
                    return false;
                }
                if (Line.StartsWith(GraphPrefix))
                {
                    const auto Name = Line.Mid(GraphPrefix.Len()).TrimStartAndEnd();
                    if (Name == TEXT("Slab"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::NativeSlab; }
                    else if (Name == TEXT("Toon"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::NativeToon; }
                    else if (Name == TEXT("SimpleClearCoat"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::NativeSimpleClearCoat; }
                    else if (Name == TEXT("AdaptiveVertical"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::AdaptiveVertical; }
                    else if (Name == TEXT("AdaptiveMix"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::AdaptiveMix; }
                    else if (Name == TEXT("NativeTranslucentSlab"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::NativeTranslucentSlab; }
                    else if (Name == TEXT("NativeRefractiveSlab"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::NativeRefractiveSlab; }
                    else if (Name == TEXT("NativeTranslucentVertical"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::NativeTranslucentVertical; }
                    else if (Name == TEXT("NativeTranslucentStack3"))
                    { ParsedGraph = ECk_Usf_SurfaceGraph::NativeTranslucentStack3; }
                    else
                    {
                        OutError = TEXT("Unknown surface-graph declaration; use Slab, Toon, SimpleClearCoat, AdaptiveVertical, AdaptiveMix, NativeTranslucentSlab, NativeRefractiveSlab, NativeTranslucentVertical or NativeTranslucentStack3");
                        return false;
                    }
                }
                else
                {
                    const auto Name = Line.Mid(ModelPrefix.Len()).TrimStartAndEnd();
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
        OutGraph = ParsedGraph;
        return true;
    }

    auto Parse_ShadingModelDeclaration(const FString& InSource,
        TOptional<ECk_Usf_ShadingModel>& OutModel, FString& OutError) -> bool
    {
        auto Graph = TOptional<ECk_Usf_SurfaceGraph>{};
        return Parse_SurfaceDeclarations(InSource, OutModel, Graph, OutError);
    }

    auto Parse_SurfaceGraphDeclaration(const FString& InSource,
        TOptional<ECk_Usf_SurfaceGraph>& OutGraph, FString& OutError) -> bool
    {
        auto Model = TOptional<ECk_Usf_ShadingModel>{};
        return Parse_SurfaceDeclarations(InSource, Model, OutGraph, OutError);
    }

    auto Get_SurfaceOutputType(const FShadingModelContract& InContract) -> FString
    {
        switch (InContract.SurfaceGraph)
        {
            case ECk_Usf_SurfaceGraph::NativeSlab: return TEXT("FCkUsf_SlabOutput");
            case ECk_Usf_SurfaceGraph::NativeSimpleClearCoat: return TEXT("FCkUsf_CoatedOutput");
            case ECk_Usf_SurfaceGraph::NativeToon: return TEXT("FCkUsf_ToonOutput");
            case ECk_Usf_SurfaceGraph::AdaptiveVertical: return TEXT("FCkUsf_AdaptiveVerticalOutput");
            case ECk_Usf_SurfaceGraph::AdaptiveMix: return TEXT("FCkUsf_AdaptiveMixOutput");
            case ECk_Usf_SurfaceGraph::NativeTranslucentSlab: return TEXT("FCkUsf_TranslucentSlabOutput");
            case ECk_Usf_SurfaceGraph::NativeRefractiveSlab: return TEXT("FCkUsf_TranslucentSlabOutput");
            case ECk_Usf_SurfaceGraph::NativeTranslucentVertical: return TEXT("FCkUsf_TranslucentVerticalOutput");
            case ECk_Usf_SurfaceGraph::NativeTranslucentStack3: return TEXT("FCkUsf_TranslucentStack3Output");
            default: return TEXT("FCkUsf_SurfaceOutput");
        }
    }

    auto Get_SurfacePrimaryField(const FShadingModelContract& InContract) -> FString
    {
        if (Is_AdaptiveSurfaceGraph(InContract.SurfaceGraph) || Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph)
            || Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph))
        { return TEXT("ADiffuseAlbedo"); }
        return InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSlab
            || InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSimpleClearCoat
            || Is_OpticalSurfaceGraph(InContract.SurfaceGraph)
            ? TEXT("DiffuseAlbedo") : TEXT("BaseColor");
    }

    auto detail::Resolve_ShadingModelAuthoringContract(const UCkUsf_LookDefinition& InDefinition,
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
        auto DeclaredGraph = TOptional<ECk_Usf_SurfaceGraph>{};
        auto ParseError = FString{};
        if (NOT Parse_SurfaceDeclarations(Source, Declared, DeclaredGraph, ParseError))
        {
            OutErrors.Add(ParseError);
            return false;
        }
        Candidate.SurfaceGraph = DeclaredGraph.Get(InDefinition._SurfaceGraph);
        if (Candidate.SurfaceGraph != ECk_Usf_SurfaceGraph::Legacy)
        {
            Candidate.SourceOverride = DeclaredGraph.IsSet();
            Candidate.DirectionalShading = InDefinition._DirectionalShading;
            Candidate.SlabFuzz = InDefinition._SlabFuzz;
            Candidate.AdaptiveBaseGlints = InDefinition._AdaptiveBaseGlints;
            Candidate.TwoSided = InDefinition._TwoSided;
            const auto Optical = Is_OpticalSurfaceGraph(Candidate.SurfaceGraph)
                || Is_LayeredOpticalSurfaceGraph(Candidate.SurfaceGraph) || Is_Stack3OpticalSurfaceGraph(Candidate.SurfaceGraph);
            if (Candidate.SurfaceGraph != ECk_Usf_SurfaceGraph::NativeSlab
                && Candidate.SurfaceGraph != ECk_Usf_SurfaceGraph::NativeSimpleClearCoat
                && Candidate.SurfaceGraph != ECk_Usf_SurfaceGraph::NativeToon
                && NOT Optical
                && NOT Is_AdaptiveSurfaceGraph(Candidate.SurfaceGraph))
            { OutErrors.Add(TEXT("Unsupported native surface-graph value")); }
            if (Candidate.AdaptiveBaseGlints && Candidate.SurfaceGraph != ECk_Usf_SurfaceGraph::AdaptiveVertical)
            { OutErrors.Add(TEXT("_AdaptiveBaseGlints requires AdaptiveVertical")); }
            if (Candidate.SlabFuzz && Candidate.SurfaceGraph != ECk_Usf_SurfaceGraph::NativeSlab)
            { OutErrors.Add(TEXT("_SlabFuzz requires NativeSlab")); }
            if (Candidate.SlabFuzz && Candidate.DirectionalShading)
            { OutErrors.Add(TEXT("Blendable native fuzz does not preserve anisotropy")); }
            if (Candidate.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSimpleClearCoat && Candidate.DirectionalShading)
            { OutErrors.Add(TEXT("Native Simple Clear Coat does not support directional shading")); }
            if (Optical && Candidate.DirectionalShading)
            { OutErrors.Add(TEXT("Native translucent Slab does not support directional shading in this contract")); }
            if (InDefinition._Domain != ECk_Usf_Domain::SurfaceLit)
            { OutErrors.Add(TEXT("Native BSDF graphs require SurfaceLit")); }
            if (Declared.IsSet() || InDefinition._ShadingModel != ECk_Usf_ShadingModel::Inherit)
            { OutErrors.Add(TEXT("Native BSDF graphs require shading-model Inherit and cannot use shading_model declarations")); }
            if (InDefinition._BlendMode != ECk_Usf_BlendMode::Inherit
                && (Optical || InDefinition._BlendMode != ECk_Usf_BlendMode::Opaque))
            { OutErrors.Add(Optical ? TEXT("Native translucent Slab requires blend-mode Inherit; colored transmittance is fixed by its graph")
                : TEXT("Native BSDF graphs currently require Opaque blend")); }
            if (InDefinition._SubsurfaceProfile || InDefinition._SurfaceSceneColor
                || InDefinition._SceneColorReplacesBackground
                || InDefinition._TranslucencyLighting != ECk_Usf_TranslucencyLighting::Inherit)
            { OutErrors.Add(TEXT("Native BSDF graphs do not support legacy profiles, scene-color or translucency settings")); }
            if (NOT InDefinition._WpoFunctionName.IsNone())
            { OutErrors.Add(TEXT("Native BSDF graphs do not yet support a separate WPO function")); }
            if (Candidate.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeToon)
            {
                Candidate.ToonProfile = InDefinition._ToonProfile;
                if (ck::Is_NOT_Valid(Candidate.ToonProfile))
                { OutErrors.Add(TEXT("Native Toon requires a valid persistent _ToonProfile asset")); }
                else if (Candidate.ToonProfile->HasAnyFlags(RF_Transient) || NOT Candidate.ToonProfile->IsAsset()
                    || Candidate.ToonProfile->GetPackage()->HasAnyFlags(RF_Transient)
                    || NOT FPackageName::DoesPackageExist(Candidate.ToonProfile->GetPackage()->GetName()))
                { OutErrors.Add(TEXT("Native Toon requires a non-transient _ToonProfile in a saved asset package")); }
            }
            else if (InDefinition._ToonProfile)
            { OutErrors.Add(TEXT("Native Slab does not consume _ToonProfile")); }
            if (OutErrors.Num() != InitialErrors)
            { return false; }
            if (Optical)
            {
                Candidate.BlendMode = BLEND_TranslucentColoredTransmittance;
                Candidate.TranslucencyLighting = TLM_SurfacePerPixelLighting;
            }
            OutContract = Candidate;
            return true;
        }
        if (InDefinition._AdaptiveBaseGlints)
        {
            OutErrors.Add(TEXT("_AdaptiveBaseGlints requires AdaptiveVertical"));
            return false;
        }
        if (InDefinition._SlabFuzz)
        {
            OutErrors.Add(TEXT("_SlabFuzz requires NativeSlab"));
            return false;
        }
        if (InDefinition._ToonProfile)
        {
            OutErrors.Add(TEXT("Legacy graphs do not consume _ToonProfile; select NativeToon"));
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
        Candidate.DirectionalShading = InDefinition._DirectionalShading;
        if (Candidate.DirectionalShading && (InDefinition._Domain != ECk_Usf_Domain::SurfaceLit
            || (Candidate.Model != ECk_Usf_ShadingModel::DefaultLit && Candidate.Model != ECk_Usf_ShadingModel::ClearCoat)
            || NOT OpaqueOrMasked))
        { OutErrors.Add(TEXT("Directional shading requires Opaque or Masked SurfaceLit DefaultLit or ClearCoat")); }
        if (InDefinition._SurfaceSceneColor && (InDefinition._Domain != ECk_Usf_Domain::SurfaceLit
            || Candidate.BlendMode != BLEND_Translucent
            || (Candidate.Model != ECk_Usf_ShadingModel::DefaultLit && Candidate.Model != ECk_Usf_ShadingModel::ThinTranslucent)))
        { OutErrors.Add(TEXT("Surface scene color requires Translucent SurfaceLit DefaultLit or ThinTranslucent")); }
        if (InDefinition._SceneColorReplacesBackground && NOT InDefinition._SurfaceSceneColor)
        { OutErrors.Add(TEXT("Scene-color background replacement requires surface scene color")); }
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

    auto Resolve_ShadingModelContract(const UCkUsf_LookDefinition& InDefinition,
        FShadingModelContract& OutContract, TArray<FString>& OutErrors) -> bool
    {
        auto Candidate = FShadingModelContract{};
        if (NOT detail::Resolve_ShadingModelAuthoringContract(InDefinition, Candidate, OutErrors))
        { return false; }
        if (Candidate.SurfaceGraph != ECk_Usf_SurfaceGraph::Legacy
            && NOT Validate_SubstrateShadingModelSupport(Candidate, Get_SubstrateShadingModelSupport(), OutErrors))
        { return false; }
        OutContract = Candidate;
        return true;
    }

    auto Get_ShadingModelOutputs(const FShadingModelContract& InContract) -> TArray<FShadingModelOutput>
    {
        if (Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph))
        {
            return {
                {TEXT("CkUsf_AF0"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_ARoughness"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_ANormal"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_AEmissiveColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BDiffuseAlbedo"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BF0"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BRoughness"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_BNormal"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BEmissiveColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_CDiffuseAlbedo"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_CF0"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_CRoughness"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_CNormal"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_CEmissiveColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_ATransmittanceColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_AReferenceThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_AThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_BTransmittanceColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BReferenceThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_BThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_CTransmittanceColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_CReferenceThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_CThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_RefractionStrength"), CMOT_Float1, MP_MAX}};
        }
        if (Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph))
        {
            return {
                {TEXT("CkUsf_AF0"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_ARoughness"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_ANormal"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_AEmissiveColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BDiffuseAlbedo"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BF0"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BRoughness"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_BNormal"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BEmissiveColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_ATransmittanceColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_AReferenceThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_AThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_BTransmittanceColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BReferenceThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_BThicknessCm"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_RefractionStrength"), CMOT_Float1, MP_MAX}};
        }
        if (Is_AdaptiveSurfaceGraph(InContract.SurfaceGraph))
        {
            auto Outputs = TArray<FShadingModelOutput>{
                {TEXT("CkUsf_AF0"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_ARoughness"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_ANormal"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_AEmissiveColor"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BDiffuseAlbedo"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BF0"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BRoughness"), CMOT_Float1, MP_MAX},
                {TEXT("CkUsf_BNormal"), CMOT_Float3, MP_MAX},
                {TEXT("CkUsf_BEmissiveColor"), CMOT_Float3, MP_MAX}};
            if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::AdaptiveVertical)
            {
                Outputs.Add({TEXT("CkUsf_ATransmittanceColor"), CMOT_Float3, MP_MAX});
                Outputs.Add({TEXT("CkUsf_ThicknessCm"), CMOT_Float1, MP_MAX});
            }
            else
            { Outputs.Add({TEXT("CkUsf_MixWeight"), CMOT_Float1, MP_MAX}); }
            if (InContract.DirectionalShading)
            {
                Outputs.Add({TEXT("CkUsf_AAnisotropy"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_ATangent"), CMOT_Float3, MP_MAX});
                Outputs.Add({TEXT("CkUsf_BAnisotropy"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_BTangent"), CMOT_Float3, MP_MAX});
            }
            if (InContract.AdaptiveBaseGlints)
            {
                Outputs.Add({TEXT("CkUsf_BGlintValue"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_BGlintUV"), CMOT_Float2, MP_MAX});
            }
            return Outputs;
        }
        if (InContract.SurfaceGraph != ECk_Usf_SurfaceGraph::Legacy)
        {
            auto Outputs = InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSlab
                || InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSimpleClearCoat
                || Is_OpticalSurfaceGraph(InContract.SurfaceGraph)
                ? TArray<FShadingModelOutput>{
                    {TEXT("EmissiveColor"), CMOT_Float3, MP_MAX},
                    {TEXT("Roughness"), CMOT_Float1, MP_MAX},
                    {TEXT("Normal"), CMOT_Float3, MP_MAX},
                    {TEXT("F0"), CMOT_Float3, MP_MAX}}
                : TArray<FShadingModelOutput>{
                    {TEXT("Metallic"), CMOT_Float1, MP_MAX},
                    {TEXT("Specular"), CMOT_Float1, MP_MAX},
                    {TEXT("Roughness"), CMOT_Float1, MP_MAX},
                    {TEXT("Normal"), CMOT_Float3, MP_MAX},
                    {TEXT("EmissiveColor"), CMOT_Float3, MP_MAX},
                    {TEXT("PatternUVs"), CMOT_Float2, MP_MAX}};
            if (Is_OpticalSurfaceGraph(InContract.SurfaceGraph))
            {
                Outputs.Add({TEXT("CkUsf_TransmittanceColor"), CMOT_Float3, MP_MAX});
                Outputs.Add({TEXT("CkUsf_ReferenceThicknessCm"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_ThicknessCm"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_RefractionStrength"), CMOT_Float1, MP_MAX});
            }
            if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSimpleClearCoat)
            {
                Outputs.Add({TEXT("CkUsf_CoatCoverage"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_CoatRoughness"), CMOT_Float1, MP_MAX});
            }
            if (InContract.SlabFuzz)
            {
                Outputs.Add({TEXT("CkUsf_FuzzAmount"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_FuzzColor"), CMOT_Float3, MP_MAX});
            }
            if (InContract.DirectionalShading)
            {
                Outputs.Add({TEXT("CkUsf_Anisotropy"), CMOT_Float1, MP_MAX});
                Outputs.Add({TEXT("CkUsf_Tangent"), CMOT_Float3, MP_MAX});
            }
            return Outputs;
        }
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
        if (InContract.DirectionalShading)
        {
            Outputs.Add({TEXT("CkUsf_Anisotropy"), CMOT_Float1, MP_Anisotropy});
            Outputs.Add({TEXT("CkUsf_Tangent"), CMOT_Float3, MP_Tangent});
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
            else if (Output.Name == TEXT("CkUsf_ATransmittanceColor") || Output.Name == TEXT("CkUsf_TransmittanceColor")
                || Output.Name == TEXT("CkUsf_BTransmittanceColor") || Output.Name == TEXT("CkUsf_CTransmittanceColor"))
            { Code += FString::Printf(TEXT("%s = clamp(O.%s, 0.0001, 0.9999);\n"), *Output.Name, *Field); }
            else if (Output.Name == TEXT("CkUsf_ThicknessCm") || Output.Name == TEXT("CkUsf_ReferenceThicknessCm")
                || ((Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph) || Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph))
                    && Output.Name.EndsWith(TEXT("ThicknessCm"))))
            { Code += FString::Printf(TEXT("%s = max(O.%s, 0.0001);\n"), *Output.Name, *Field); }
            else if (Output.Name == TEXT("CkUsf_RefractionStrength"))
            { Code += FString::Printf(TEXT("%s = clamp(O.%s, 1.0, 1.1);\n"), *Output.Name, *Field); }
            else if (Output.Name == TEXT("CkUsf_MixWeight"))
            { Code += FString::Printf(TEXT("%s = saturate(O.%s);\n"), *Output.Name, *Field); }
            else
            { Code += FString::Printf(TEXT("%s = O.%s;\n"), *Output.Name, *Field); }
        }
        return Code;
    }

    auto Apply_ShadingModelContract(UMaterial& InMaterial,
        UMaterialExpressionCustom& InCustom, const FShadingModelContract& InContract) -> void
    {
        if (InContract.SurfaceGraph != ECk_Usf_SurfaceGraph::Legacy)
        { return; }
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
            MP_SubsurfaceColor, MP_CustomData0, MP_CustomData1, MP_Anisotropy, MP_Tangent})
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
