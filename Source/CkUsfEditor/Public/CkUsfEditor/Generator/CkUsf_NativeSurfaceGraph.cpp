#include "CkUsfEditor/Generator/CkUsf_NativeSurfaceGraph.h"

#include "CkCore/Macros/CkMacros.h"
#include "MaterialDomain.h"
#include "Engine/Texture.h"
#include "Engine/ToonProfile.h"
#include "Engine/SpecularProfile.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace ck::usf_editor
{
    namespace native_surface_graph
    {
        const auto SignatureKey = FString{TEXT("CkUsf.NativeSurfaceGraph.v1")};

        // Native pin indices follow the engine expression's public GetInput contract, not GBuffer slots.
        auto Get_Bindings(const FShadingModelContract& InContract) -> TMap<int32, FString>
        {
            auto Bindings = InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSlab
                || Is_OpticalSurfaceGraph(InContract.SurfaceGraph)
                ? TMap<int32, FString>{{0, TEXT("")}, {1, TEXT("F0")}, {3, TEXT("Roughness")},
                    {5, TEXT("Normal")}, {10, TEXT("EmissiveColor")}}
                : TMap<int32, FString>{{0, TEXT("")}, {1, TEXT("Metallic")}, {2, TEXT("Specular")},
                    {3, TEXT("Roughness")}, {4, TEXT("Normal")}, {5, TEXT("EmissiveColor")}, {6, TEXT("PatternUVs")}};
            if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSimpleClearCoat)
            {
                Bindings = {{0, TEXT("")}, {1, TEXT("F0")}, {2, TEXT("Roughness")},
                    {3, TEXT("CkUsf_CoatCoverage")}, {4, TEXT("CkUsf_CoatRoughness")}, {5, TEXT("Normal")},
                    {6, TEXT("EmissiveColor")}};
            }
            if (InContract.SlabFuzz)
            {
                Bindings.Add(14, TEXT("CkUsf_FuzzAmount"));
                Bindings.Add(15, TEXT("CkUsf_FuzzColor"));
            }
            if (InContract.DirectionalShading)
            {
                const auto Slab = InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSlab;
                Bindings.Add(Slab ? 4 : 7, TEXT("CkUsf_Anisotropy"));
                Bindings.Add(Slab ? 6 : 8, TEXT("CkUsf_Tangent"));
            }
            return Bindings;
        }

        auto Get_OutputIndex(const UMaterialExpressionCustom& InCustom, const FString& InName) -> int32
        {
            if (InName.IsEmpty())
            { return 0; }
            const auto Index = InCustom.AdditionalOutputs.IndexOfByPredicate([&](const FCustomOutput& InOutput)
            { return InOutput.OutputName.ToString() == InName; });
            return Index == INDEX_NONE ? INDEX_NONE : Index + 1;
        }

        auto Get_AdaptiveBindings(bool InLayerA, bool InDirectional, bool InBaseGlints) -> TMap<int32, FString>
        {
            const auto Prefix = InLayerA ? FString{TEXT("CkUsf_A")} : FString{TEXT("CkUsf_B")};
            auto Bindings = TMap<int32, FString>{{0, InLayerA ? FString{} : Prefix + TEXT("DiffuseAlbedo")},
                {1, Prefix + TEXT("F0")}, {3, Prefix + TEXT("Roughness")},
                {5, Prefix + TEXT("Normal")}, {10, Prefix + TEXT("EmissiveColor")}};
            if (InDirectional)
            {
                Bindings.Add(4, Prefix + TEXT("Anisotropy"));
                Bindings.Add(6, Prefix + TEXT("Tangent"));
            }
            if (NOT InLayerA && InBaseGlints)
            {
                Bindings.Add(16, TEXT("CkUsf_BGlintValue"));
                Bindings.Add(17, TEXT("CkUsf_BGlintUV"));
            }
            return Bindings;
        }

        auto Connect_Outputs(UMaterialExpression& InNode, UMaterialExpressionCustom& InCustom,
            const TMap<int32, FString>& InBindings) -> bool
        {
            for (const auto& Binding : InBindings)
            {
                const auto OutputIndex = Get_OutputIndex(InCustom, Binding.Value);
                if (OutputIndex == INDEX_NONE || InNode.GetInput(Binding.Key) == nullptr)
                { return false; }
            }
            for (const auto& Binding : InBindings)
            { InNode.GetInput(Binding.Key)->Connect(Get_OutputIndex(InCustom, Binding.Value), &InCustom); }
            return true;
        }

        auto Get_Stack3LayerBindings(const FString& InPrefix, bool InPrimary) -> TMap<int32, FString>
        {
            return {{0, InPrimary ? FString{} : InPrefix + TEXT("DiffuseAlbedo")},
                {1, InPrefix + TEXT("F0")}, {3, InPrefix + TEXT("Roughness")},
                {5, InPrefix + TEXT("Normal")}, {10, InPrefix + TEXT("EmissiveColor")}};
        }

        auto Input_Matches(const FExpressionInput& InInput, UMaterialExpression& InExpression, int32 InOutput) -> bool
        {
            const auto& Outputs = InExpression.GetOutputs();
            if (NOT Outputs.IsValidIndex(InOutput))
            { return false; }
            const auto& Output = Outputs[InOutput];
            return InInput.Expression == &InExpression && InInput.OutputIndex == InOutput
                && InInput.Mask == Output.Mask && InInput.MaskR == Output.MaskR
                && InInput.MaskG == Output.MaskG && InInput.MaskB == Output.MaskB && InInput.MaskA == Output.MaskA;
        }

        auto Get_Signature(const UCkUsf_LookDefinition& InDefinition, UMaterialExpressionCustom& InCustom,
            const FShadingModelContract& InContract) -> FString
        {
            auto Signature = FString::Printf(TEXT("Graph=%d;Directional=%d;TwoSided=%d;Profile=%s;WPO=%s;Pixel=%d;Particle=%d,%d\n"),
                static_cast<int32>(InContract.SurfaceGraph), InContract.DirectionalShading, InContract.TwoSided,
                *GetPathNameSafe(InContract.ToonProfile), *InDefinition._WpoFunctionName.ToString(),
                InDefinition._PixelDataChannels, InDefinition._ParticleColor, InDefinition._ParticleDynamicParameter);
            Signature += InCustom.Code + TEXT("\n");
            if (InContract.SlabFuzz)
            { Signature += TEXT("SlabFuzz=1\n"); }
            for (const auto& Define : InDefinition._Defines)
            { Signature += TEXT("DefinitionDefine=") + Define + TEXT("\n"); }
            for (const auto& Name : InDefinition._ParticleDynamicParameterNames)
            { Signature += TEXT("ParticleName=") + Name + TEXT("\n"); }
            for (const auto& Include : InCustom.IncludeFilePaths)
            { Signature += TEXT("Include=") + Include + TEXT("\n"); }
            for (const auto& Define : InCustom.AdditionalDefines)
            { Signature += Define.DefineName + TEXT("=") + Define.DefineValue + TEXT("\n"); }
            for (const auto& Param : InDefinition._Parameters)
            {
                Signature += FString::Printf(TEXT("Param=%s,%d,%d,%d,%d,%s,%g,%s\n"), *Param._Name.ToString(),
                    static_cast<int32>(Param._Type), Param._PerInstance, Param._CustomPrimitiveData,
                    Param._CustomPrimitiveDataIndex, *Param._DefaultTexturePath, Param._DefaultScalar,
                    *Param._DefaultVector.ToString());
            }
            for (const auto& Input : InCustom.Inputs)
            {
                Signature += FString::Printf(TEXT("Input=%s,%s,%d,%d,%d,%d,%d,%d\n"), *Input.InputName.ToString(),
                    *GetPathNameSafe(Input.Input.Expression), Input.Input.OutputIndex, Input.Input.Mask,
                    Input.Input.MaskR, Input.Input.MaskG, Input.Input.MaskB, Input.Input.MaskA);
                const auto Texture = TObjectPtr<UMaterialExpressionTextureObjectParameter>{
                    Cast<UMaterialExpressionTextureObjectParameter>(Input.Input.Expression)};
                if (Texture != nullptr)
                {
                    Signature += FString::Printf(TEXT("Texture=%s,%s,%d\n"), *Texture->ParameterName.ToString(),
                        *GetPathNameSafe(Texture->Texture), static_cast<int32>(Texture->SamplerType.GetValue()));
                }
                const auto Scalar = TObjectPtr<UMaterialExpressionScalarParameter>{
                    Cast<UMaterialExpressionScalarParameter>(Input.Input.Expression)};
                if (Scalar != nullptr)
                {
                    Signature += FString::Printf(TEXT("Scalar=%s,%d,%d,%g\n"), *Scalar->ParameterName.ToString(),
                        Scalar->bUseCustomPrimitiveData, Scalar->PrimitiveDataIndex, Scalar->DefaultValue);
                }
                const auto Vector = TObjectPtr<UMaterialExpressionVectorParameter>{
                    Cast<UMaterialExpressionVectorParameter>(Input.Input.Expression)};
                if (Vector != nullptr)
                {
                    Signature += FString::Printf(TEXT("Vector=%s,%d,%d,%s\n"), *Vector->ParameterName.ToString(),
                        Vector->bUseCustomPrimitiveData, Vector->PrimitiveDataIndex, *Vector->DefaultValue.ToString());
                }
                const auto PerInstance = TObjectPtr<UMaterialExpressionPerInstanceCustomData>{
                    Cast<UMaterialExpressionPerInstanceCustomData>(Input.Input.Expression)};
                if (PerInstance != nullptr)
                { Signature += FString::Printf(TEXT("PerInstance=%d\n"), PerInstance->DataIndex); }
                const auto PerInstanceVector = TObjectPtr<UMaterialExpressionPerInstanceCustomData3Vector>{
                    Cast<UMaterialExpressionPerInstanceCustomData3Vector>(Input.Input.Expression)};
                if (PerInstanceVector != nullptr)
                { Signature += FString::Printf(TEXT("PerInstanceVector=%d\n"), PerInstanceVector->DataIndex); }
                const auto Coordinates = TObjectPtr<UMaterialExpressionTextureCoordinate>{
                    Cast<UMaterialExpressionTextureCoordinate>(Input.Input.Expression)};
                if (Coordinates != nullptr)
                {
                    Signature += FString::Printf(TEXT("UV=%d,%g,%g,%d,%d\n"), Coordinates->CoordinateIndex,
                        Coordinates->UTiling, Coordinates->VTiling, Coordinates->UnMirrorU, Coordinates->UnMirrorV);
                }
            }
            return Signature;
        }

        auto Get_RecordedSignature(UMaterial& InMaterial, const UCkUsf_LookDefinition& InDefinition,
            UMaterialExpressionCustom& InCustom, const FShadingModelContract& InContract) -> FString
        {
            auto Signature = Get_Signature(InDefinition, InCustom, InContract);
            if (NOT Is_AdaptiveSurfaceGraph(InContract.SurfaceGraph)
                && NOT Is_OpticalSurfaceGraph(InContract.SurfaceGraph)
                && NOT Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph)
                && NOT Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph))
            { return Signature; }
            if (Is_OpticalSurfaceGraph(InContract.SurfaceGraph) || Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph)
                || Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph))
            {
                const auto& Thickness = InMaterial.GetEditorOnlyData()->SurfaceThickness;
                Signature += FString::Printf(TEXT("Optical=%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d\n"),
                    InMaterial.bIsThinSurface, static_cast<int32>(InMaterial.TranslucencyLightingMode.GetValue()),
                    static_cast<int32>(InMaterial.RefractionMethod.GetValue()), static_cast<int32>(InMaterial.TranslucencyPass.GetValue()),
                    *GetPathNameSafe(Thickness.Expression), Thickness.OutputIndex, Thickness.Mask,
                    Thickness.MaskR, Thickness.MaskG, Thickness.MaskB, Thickness.MaskA);
                const auto& Refraction = InMaterial.GetEditorOnlyData()->Refraction;
                Signature += FString::Printf(TEXT("OpticalRefraction=%s,%d,%d,%d,%d,%d,%d\n"),
                    *GetPathNameSafe(Refraction.Expression), Refraction.OutputIndex, Refraction.Mask,
                    Refraction.MaskR, Refraction.MaskG, Refraction.MaskB, Refraction.MaskA);
            }
            Signature += FString::Printf(TEXT("AdaptiveRoughnessTracking=%d\n"), InMaterial.bSubstrateRoughnessTracking);
            // Existing single-BSDF signatures stay byte-for-byte compatible. Fixed Adaptive trees
            // additionally record every expression/edge so added or swapped nodes require regeneration.
            for (const auto& Expression : InMaterial.GetExpressions())
            {
                if (Expression == nullptr)
                { Signature += TEXT("MissingExpression\n"); continue; }
                Signature += FString::Printf(TEXT("Node=%s,%s,%s\n"), *Expression->GetPathName(),
                    *Expression->GetClass()->GetPathName(), *Expression->MaterialExpressionGuid.ToString());
                const auto Inputs = Expression->GetInputsView();
                for (auto Index = 0; Index < Inputs.Num(); ++Index)
                {
                    if (Inputs[Index] == nullptr)
                    { Signature += TEXT("MissingPin\n"); continue; }
                    const auto& Input = *Inputs[Index];
                    Signature += FString::Printf(TEXT("Edge=%d,%s,%d,%d,%d,%d,%d,%d\n"), Index,
                        *GetPathNameSafe(Input.Expression), Input.OutputIndex, Input.Mask,
                        Input.MaskR, Input.MaskG, Input.MaskB, Input.MaskA);
                }
                const auto Slab = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{Cast<UMaterialExpressionSubstrateSlabBSDF>(Expression)};
                if (Slab != nullptr)
                {
                    Signature += FString::Printf(TEXT("Slab=%d,%s,%s\n"), static_cast<int32>(Slab->SubSurfaceType.GetValue()),
                        *GetPathNameSafe(Slab->SubsurfaceProfile), *GetPathNameSafe(Slab->SpecularProfile));
                }
                const auto Vertical = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{Cast<UMaterialExpressionSubstrateVerticalLayering>(Expression)};
                if (Vertical != nullptr)
                { Signature += FString::Printf(TEXT("VerticalParameterBlending=%d\n"), Vertical->bUseParameterBlending); }
                const auto Mix = TObjectPtr<UMaterialExpressionSubstrateHorizontalMixing>{Cast<UMaterialExpressionSubstrateHorizontalMixing>(Expression)};
                if (Mix != nullptr)
                { Signature += FString::Printf(TEXT("MixParameterBlending=%d\n"), Mix->bUseParameterBlending); }
            }
            if (InContract.AdaptiveBaseGlints)
            { Signature += TEXT("AdaptiveBaseGlints=1\n"); }
            return Signature;
        }

        auto Create_Stack3OpticalGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
            const FShadingModelContract& InContract, TArray<FString>& OutErrors) -> bool
        {
            const auto EditorData = TObjectPtr<UMaterialEditorOnlyData>{InMaterial.GetEditorOnlyData()};
            if (EditorData == nullptr)
            { OutErrors.Add(TEXT("Three-layer optical material editor data is missing.")); return false; }
            auto Slabs = TArray<TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>>{};
            auto Nodes = TArray<TObjectPtr<UMaterialExpression>>{};
            const auto Prefixes = TArray<FString>{TEXT("CkUsf_A"), TEXT("CkUsf_B"), TEXT("CkUsf_C")};
            for (auto Index = 0; Index < Prefixes.Num(); ++Index)
            {
                const auto Slab = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{NewObject<UMaterialExpressionSubstrateSlabBSDF>(&InMaterial)};
                const auto Mfp = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{NewObject<UMaterialExpressionSubstrateTransmittanceToMFP>(&InMaterial)};
                if (Slab == nullptr || Mfp == nullptr)
                { OutErrors.Add(TEXT("Cannot create all three transmitting Slabs and optical conversions.")); return false; }
                const auto& Prefix = Prefixes[Index];
                if (NOT Connect_Outputs(*Slab, InCustom, Get_Stack3LayerBindings(Prefix, Index == 0))
                    || NOT Connect_Outputs(*Mfp, InCustom, {{0, Prefix + TEXT("TransmittanceColor")}, {1, Prefix + TEXT("ReferenceThicknessCm")}}))
                { OutErrors.Add(TEXT("Three-layer optical output layout is incomplete.")); return false; }
                Slab->SubSurfaceType = MSS_SimpleVolume;
                Slab->SSSMFP.Connect(0, Mfp.Get());
                Slabs.Add(Slab);
                Nodes.Add(Slab); Nodes.Add(Mfp);
            }
            const auto Outer = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{NewObject<UMaterialExpressionSubstrateVerticalLayering>(&InMaterial)};
            const auto Inner = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{NewObject<UMaterialExpressionSubstrateVerticalLayering>(&InMaterial)};
            if (Outer == nullptr || Inner == nullptr)
            { OutErrors.Add(TEXT("Cannot create both nested optical Vertical operators.")); return false; }
            if (NOT Connect_Outputs(*Outer, InCustom, {{2, TEXT("CkUsf_AThicknessCm")}})
                || NOT Connect_Outputs(*Inner, InCustom, {{2, TEXT("CkUsf_BThicknessCm")}})
                || Get_OutputIndex(InCustom, TEXT("CkUsf_CThicknessCm")) == INDEX_NONE
                || Get_OutputIndex(InCustom, TEXT("CkUsf_RefractionStrength")) == INDEX_NONE)
            { OutErrors.Add(TEXT("Three-layer optical thickness/refraction layout is incomplete.")); return false; }
            Outer->bUseParameterBlending = false;
            Inner->bUseParameterBlending = false;
            Outer->Top.Connect(0, Slabs[0].Get());
            Outer->Base.Connect(0, Inner.Get());
            Inner->Top.Connect(0, Slabs[1].Get());
            Inner->Base.Connect(0, Slabs[2].Get());
            Nodes.Add(Outer); Nodes.Add(Inner);
            for (const auto& Node : Nodes)
            {
                Node->Material = &InMaterial;
                Node->MaterialExpressionGuid = FGuid::NewGuid();
                Node->Desc = TEXT("CkUsf_Stack3OpticalSurfaceGraph");
                InMaterial.GetExpressionCollection().AddExpression(Node);
            }
            EditorData->FrontMaterial.Connect(0, Outer.Get());
            EditorData->SurfaceThickness.Connect(Get_OutputIndex(InCustom, TEXT("CkUsf_CThicknessCm")), &InCustom);
            EditorData->Refraction.Connect(Get_OutputIndex(InCustom, TEXT("CkUsf_RefractionStrength")), &InCustom);
            InMaterial.BlendMode = BLEND_TranslucentColoredTransmittance;
            InMaterial.TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
            InMaterial.bIsThinSurface = true;
            InMaterial.RefractionMethod = RM_PixelNormalOffset;
            InMaterial.TranslucencyPass = MTP_BeforeDOF;
            InMaterial.bTangentSpaceNormal = true;
            InMaterial.TwoSided = InContract.TwoSided;
            return true;
        }

        auto Validate_Stack3OpticalGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
            TArray<FString>& OutErrors) -> bool
        {
            const auto Reject = [&OutErrors]() -> bool
            { OutErrors.Add(TEXT("Three-layer optical topology, calibration, thickness or settings changed. Regenerate the native master before reload.")); return false; };
            const auto EditorData = TObjectPtr<UMaterialEditorOnlyData>{InMaterial.GetEditorOnlyData()};
            if (EditorData == nullptr)
            { return Reject(); }
            const auto Outer = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{Cast<UMaterialExpressionSubstrateVerticalLayering>(EditorData->FrontMaterial.Expression)};
            if (Outer == nullptr || Outer->GetClass() != UMaterialExpressionSubstrateVerticalLayering::StaticClass()
                || Outer->Material != &InMaterial || NOT InMaterial.GetExpressions().Contains(Outer)
                || Outer->bUseParameterBlending || NOT Input_Matches(EditorData->FrontMaterial, *Outer, 0))
            { return Reject(); }
            const auto Inner = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{Cast<UMaterialExpressionSubstrateVerticalLayering>(Outer->Base.Expression)};
            if (Inner == nullptr || Inner == Outer || Inner->GetClass() != UMaterialExpressionSubstrateVerticalLayering::StaticClass()
                || Inner->Material != &InMaterial || NOT InMaterial.GetExpressions().Contains(Inner)
                || Inner->bUseParameterBlending || NOT Input_Matches(Outer->Base, *Inner, 0))
            { return Reject(); }
            const auto Slabs = TArray<TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>>{
                Cast<UMaterialExpressionSubstrateSlabBSDF>(Outer->Top.Expression),
                Cast<UMaterialExpressionSubstrateSlabBSDF>(Inner->Top.Expression),
                Cast<UMaterialExpressionSubstrateSlabBSDF>(Inner->Base.Expression)};
            if (Slabs[0] == nullptr || Slabs[1] == nullptr || Slabs[2] == nullptr
                || Slabs[0] == Slabs[1] || Slabs[0] == Slabs[2] || Slabs[1] == Slabs[2]
                || NOT Input_Matches(Outer->Top, *Slabs[0], 0)
                || NOT Input_Matches(Inner->Top, *Slabs[1], 0) || NOT Input_Matches(Inner->Base, *Slabs[2], 0))
            { return Reject(); }
            if (NOT InMaterial.bIsThinSurface || InMaterial.TranslucencyLightingMode != TLM_SurfacePerPixelLighting
                || InMaterial.RefractionMethod != RM_PixelNormalOffset || InMaterial.TranslucencyPass != MTP_BeforeDOF
                || NOT Input_Matches(Outer->Thickness, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_AThicknessCm")))
                || NOT Input_Matches(Inner->Thickness, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_BThicknessCm")))
                || NOT Input_Matches(EditorData->SurfaceThickness, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_CThicknessCm")))
                || NOT Input_Matches(EditorData->Refraction, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_RefractionStrength"))))
            { return Reject(); }
            auto Conversions = TArray<TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>>{};
            const auto Prefixes = TArray<FString>{TEXT("CkUsf_A"), TEXT("CkUsf_B"), TEXT("CkUsf_C")};
            for (auto Layer = 0; Layer < Slabs.Num(); ++Layer)
            {
                const auto Slab = Slabs[Layer];
                if (Slab->GetClass() != UMaterialExpressionSubstrateSlabBSDF::StaticClass()
                    || Slab->Material != &InMaterial || NOT InMaterial.GetExpressions().Contains(Slab)
                    || Slab->SubSurfaceType != MSS_SimpleVolume || Slab->SubsurfaceProfile != nullptr || Slab->SpecularProfile != nullptr)
                { return Reject(); }
                const auto Mfp = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{Cast<UMaterialExpressionSubstrateTransmittanceToMFP>(Slab->SSSMFP.Expression)};
                if (Mfp == nullptr || Mfp->GetClass() != UMaterialExpressionSubstrateTransmittanceToMFP::StaticClass()
                    || Conversions.Contains(Mfp) || Mfp->Material != &InMaterial || NOT InMaterial.GetExpressions().Contains(Mfp)
                    || NOT Input_Matches(Slab->SSSMFP, *Mfp, 0)
                    || NOT Input_Matches(Mfp->TransmittanceColor, InCustom, Get_OutputIndex(InCustom, Prefixes[Layer] + TEXT("TransmittanceColor")))
                    || NOT Input_Matches(Mfp->Thickness, InCustom, Get_OutputIndex(InCustom, Prefixes[Layer] + TEXT("ReferenceThicknessCm"))))
                { return Reject(); }
                Conversions.Add(Mfp);
                const auto Bindings = Get_Stack3LayerBindings(Prefixes[Layer], Layer == 0);
                const auto Inputs = Slab->GetInputsView();
                for (auto Index = 0; Index < Inputs.Num(); ++Index)
                {
                    if (Inputs[Index] == nullptr)
                    { return Reject(); }
                    if (Bindings.Contains(Index))
                    {
                        if (NOT Input_Matches(*Inputs[Index], InCustom, Get_OutputIndex(InCustom, Bindings[Index])))
                        { return Reject(); }
                    }
                    else if (Index != 7 && Inputs[Index]->Expression != nullptr)
                    { return Reject(); }
                }
            }
            return true;
        }

        auto Create_LayeredOpticalGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
            const FShadingModelContract& InContract, TArray<FString>& OutErrors) -> bool
        {
            const auto EditorData = TObjectPtr<UMaterialEditorOnlyData>{InMaterial.GetEditorOnlyData()};
            if (EditorData == nullptr)
            { OutErrors.Add(TEXT("Layered optical material editor data is missing.")); return false; }
            const auto A = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{NewObject<UMaterialExpressionSubstrateSlabBSDF>(&InMaterial)};
            const auto B = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{NewObject<UMaterialExpressionSubstrateSlabBSDF>(&InMaterial)};
            const auto MfpA = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{NewObject<UMaterialExpressionSubstrateTransmittanceToMFP>(&InMaterial)};
            const auto MfpB = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{NewObject<UMaterialExpressionSubstrateTransmittanceToMFP>(&InMaterial)};
            const auto Vertical = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{NewObject<UMaterialExpressionSubstrateVerticalLayering>(&InMaterial)};
            if (A == nullptr || B == nullptr || MfpA == nullptr || MfpB == nullptr || Vertical == nullptr)
            { OutErrors.Add(TEXT("Cannot create both transmitting Slabs and their optical conversions.")); return false; }
            if (NOT Connect_Outputs(*A, InCustom, Get_AdaptiveBindings(true, false, false))
                || NOT Connect_Outputs(*B, InCustom, Get_AdaptiveBindings(false, false, false))
                || NOT Connect_Outputs(*MfpA, InCustom, {{0, TEXT("CkUsf_ATransmittanceColor")}, {1, TEXT("CkUsf_AReferenceThicknessCm")}})
                || NOT Connect_Outputs(*MfpB, InCustom, {{0, TEXT("CkUsf_BTransmittanceColor")}, {1, TEXT("CkUsf_BReferenceThicknessCm")}})
                || NOT Connect_Outputs(*Vertical, InCustom, {{2, TEXT("CkUsf_AThicknessCm")}})
                || Get_OutputIndex(InCustom, TEXT("CkUsf_BThicknessCm")) == INDEX_NONE
                || Get_OutputIndex(InCustom, TEXT("CkUsf_RefractionStrength")) == INDEX_NONE)
            { OutErrors.Add(TEXT("Layered optical output layout is incomplete.")); return false; }
            A->SubSurfaceType = MSS_SimpleVolume;
            B->SubSurfaceType = MSS_SimpleVolume;
            A->SSSMFP.Connect(0, MfpA.Get());
            B->SSSMFP.Connect(0, MfpB.Get());
            Vertical->bUseParameterBlending = false;
            Vertical->Top.Connect(0, A.Get());
            Vertical->Base.Connect(0, B.Get());
            for (const auto& Node : TArray<TObjectPtr<UMaterialExpression>>{A, B, MfpA, MfpB, Vertical})
            {
                Node->Material = &InMaterial;
                Node->MaterialExpressionGuid = FGuid::NewGuid();
                Node->Desc = TEXT("CkUsf_LayeredOpticalSurfaceGraph");
                InMaterial.GetExpressionCollection().AddExpression(Node);
            }
            EditorData->FrontMaterial.Connect(0, Vertical.Get());
            EditorData->SurfaceThickness.Connect(Get_OutputIndex(InCustom, TEXT("CkUsf_BThicknessCm")), &InCustom);
            EditorData->Refraction.Connect(Get_OutputIndex(InCustom, TEXT("CkUsf_RefractionStrength")), &InCustom);
            InMaterial.BlendMode = BLEND_TranslucentColoredTransmittance;
            InMaterial.TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
            InMaterial.bIsThinSurface = true;
            InMaterial.RefractionMethod = RM_PixelNormalOffset;
            InMaterial.TranslucencyPass = MTP_BeforeDOF;
            InMaterial.bTangentSpaceNormal = true;
            InMaterial.TwoSided = InContract.TwoSided;
            return true;
        }

        auto Validate_LayeredOpticalGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
            TArray<FString>& OutErrors) -> bool
        {
            const auto Reject = [&OutErrors]() -> bool
            { OutErrors.Add(TEXT("Layered optical topology, calibration, thickness or settings changed. Regenerate the native master before reload.")); return false; };
            const auto EditorData = TObjectPtr<UMaterialEditorOnlyData>{InMaterial.GetEditorOnlyData()};
            if (EditorData == nullptr)
            { return Reject(); }
            const auto Vertical = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{Cast<UMaterialExpressionSubstrateVerticalLayering>(EditorData->FrontMaterial.Expression)};
            if (Vertical == nullptr || Vertical->GetClass() != UMaterialExpressionSubstrateVerticalLayering::StaticClass()
                || Vertical->Material != &InMaterial || NOT InMaterial.GetExpressions().Contains(Vertical)
                || Vertical->bUseParameterBlending || NOT Input_Matches(EditorData->FrontMaterial, *Vertical, 0))
            { return Reject(); }
            const auto A = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{Cast<UMaterialExpressionSubstrateSlabBSDF>(Vertical->Top.Expression)};
            const auto B = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{Cast<UMaterialExpressionSubstrateSlabBSDF>(Vertical->Base.Expression)};
            if (A == nullptr || B == nullptr || A == B || NOT Input_Matches(Vertical->Top, *A, 0)
                || NOT Input_Matches(Vertical->Base, *B, 0))
            { return Reject(); }
            const auto MfpA = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{Cast<UMaterialExpressionSubstrateTransmittanceToMFP>(A->SSSMFP.Expression)};
            const auto MfpB = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{Cast<UMaterialExpressionSubstrateTransmittanceToMFP>(B->SSSMFP.Expression)};
            if (MfpA == nullptr || MfpB == nullptr || MfpA == MfpB)
            { return Reject(); }
            if (NOT InMaterial.bIsThinSurface || InMaterial.TranslucencyLightingMode != TLM_SurfacePerPixelLighting
                || InMaterial.RefractionMethod != RM_PixelNormalOffset || InMaterial.TranslucencyPass != MTP_BeforeDOF
                || NOT Input_Matches(Vertical->Thickness, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_AThicknessCm")))
                || NOT Input_Matches(EditorData->SurfaceThickness, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_BThicknessCm")))
                || NOT Input_Matches(EditorData->Refraction, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_RefractionStrength"))))
            { return Reject(); }
            for (const auto LayerA : {true, false})
            {
                const auto Slab = LayerA ? A : B;
                const auto Mfp = LayerA ? MfpA : MfpB;
                const auto Prefix = LayerA ? FString{TEXT("CkUsf_A")} : FString{TEXT("CkUsf_B")};
                if (Slab->GetClass() != UMaterialExpressionSubstrateSlabBSDF::StaticClass()
                    || Slab->Material != &InMaterial || NOT InMaterial.GetExpressions().Contains(Slab)
                    || Slab->SubSurfaceType != MSS_SimpleVolume || Slab->SubsurfaceProfile != nullptr || Slab->SpecularProfile != nullptr
                    || Mfp->GetClass() != UMaterialExpressionSubstrateTransmittanceToMFP::StaticClass()
                    || Mfp->Material != &InMaterial || NOT InMaterial.GetExpressions().Contains(Mfp)
                    || NOT Input_Matches(Slab->SSSMFP, *Mfp, 0)
                    || NOT Input_Matches(Mfp->TransmittanceColor, InCustom, Get_OutputIndex(InCustom, Prefix + TEXT("TransmittanceColor")))
                    || NOT Input_Matches(Mfp->Thickness, InCustom, Get_OutputIndex(InCustom, Prefix + TEXT("ReferenceThicknessCm"))))
                { return Reject(); }
                const auto Bindings = Get_AdaptiveBindings(LayerA, false, false);
                const auto Inputs = Slab->GetInputsView();
                for (auto Index = 0; Index < Inputs.Num(); ++Index)
                {
                    if (Inputs[Index] == nullptr)
                    { return Reject(); }
                    if (Bindings.Contains(Index))
                    {
                        if (NOT Input_Matches(*Inputs[Index], InCustom, Get_OutputIndex(InCustom, Bindings[Index])))
                        { return Reject(); }
                    }
                    else if (Index != 7 && Inputs[Index]->Expression != nullptr)
                    { return Reject(); }
                }
            }
            return true;
        }

        auto Create_AdaptiveGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
            const FShadingModelContract& InContract, TArray<FString>& OutErrors) -> bool
        {
            if (InMaterial.GetEditorOnlyData() == nullptr)
            { OutErrors.Add(TEXT("Adaptive material editor data is missing.")); return false; }
            const auto A = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{NewObject<UMaterialExpressionSubstrateSlabBSDF>(&InMaterial)};
            const auto B = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{NewObject<UMaterialExpressionSubstrateSlabBSDF>(&InMaterial)};
            if (A == nullptr || B == nullptr)
            { OutErrors.Add(TEXT("Cannot create the two Adaptive Slabs.")); return false; }
            if (NOT Connect_Outputs(*A, InCustom, Get_AdaptiveBindings(true, InContract.DirectionalShading, InContract.AdaptiveBaseGlints))
                || NOT Connect_Outputs(*B, InCustom, Get_AdaptiveBindings(false, InContract.DirectionalShading, InContract.AdaptiveBaseGlints)))
            { OutErrors.Add(TEXT("Adaptive Slab output layout is incomplete.")); return false; }
            A->SubSurfaceType = MSS_None;
            B->SubSurfaceType = MSS_None;
            A->SubsurfaceProfile = nullptr;
            B->SubsurfaceProfile = nullptr;
            A->SpecularProfile = nullptr;
            B->SpecularProfile = nullptr;
            auto Nodes = TArray<TObjectPtr<UMaterialExpression>>{A, B};
            auto Root = TObjectPtr<UMaterialExpressionSubstrateBSDF>{};
            if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::AdaptiveVertical)
            {
                const auto Mfp = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{NewObject<UMaterialExpressionSubstrateTransmittanceToMFP>(&InMaterial)};
                const auto Vertical = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{NewObject<UMaterialExpressionSubstrateVerticalLayering>(&InMaterial)};
                if (Mfp == nullptr || Vertical == nullptr
                    || NOT Connect_Outputs(*Mfp, InCustom, {{0, TEXT("CkUsf_ATransmittanceColor")}, {1, TEXT("CkUsf_ThicknessCm")}})
                    || NOT Connect_Outputs(*Vertical, InCustom, {{2, TEXT("CkUsf_ThicknessCm")}}))
                { OutErrors.Add(TEXT("Adaptive vertical output layout is incomplete.")); return false; }
                A->SubSurfaceType = MSS_SimpleVolume;
                A->SSSMFP.Connect(0, Mfp.Get());
                Vertical->bUseParameterBlending = false;
                Vertical->Top.Connect(0, A.Get());
                Vertical->Base.Connect(0, B.Get());
                Nodes.Add(Mfp);
                Nodes.Add(Vertical);
                Root = Vertical;
            }
            else
            {
                const auto Mix = TObjectPtr<UMaterialExpressionSubstrateHorizontalMixing>{NewObject<UMaterialExpressionSubstrateHorizontalMixing>(&InMaterial)};
                if (Mix == nullptr || NOT Connect_Outputs(*Mix, InCustom, {{2, TEXT("CkUsf_MixWeight")}}))
                { OutErrors.Add(TEXT("Adaptive mix output layout is incomplete.")); return false; }
                Mix->bUseParameterBlending = false;
                Mix->Foreground.Connect(0, A.Get());
                Mix->Background.Connect(0, B.Get());
                Nodes.Add(Mix);
                Root = Mix;
            }
            for (const auto& Node : Nodes)
            {
                Node->Material = &InMaterial;
                Node->MaterialExpressionGuid = FGuid::NewGuid();
                Node->Desc = TEXT("CkUsf_AdaptiveSurfaceGraph");
                InMaterial.GetExpressionCollection().AddExpression(Node);
            }
            InMaterial.GetEditorOnlyData()->FrontMaterial.Connect(0, Root.Get());
            InMaterial.bTangentSpaceNormal = true;
            InMaterial.TwoSided = InContract.TwoSided;
            return true;
        }

        auto Validate_AdaptiveGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
            const FShadingModelContract& InContract, TArray<FString>& OutErrors) -> bool
        {
            const auto Reject = [&OutErrors]() -> bool
            { OutErrors.Add(TEXT("Adaptive two-Slab topology, features or connections changed. Regenerate the native master before reload.")); return false; };
            const auto EditorData = TObjectPtr<UMaterialEditorOnlyData>{InMaterial.GetEditorOnlyData()};
            if (EditorData == nullptr)
            { return Reject(); }
            auto A = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{};
            auto B = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{};
            auto Root = TObjectPtr<UMaterialExpressionSubstrateBSDF>{};
            auto Mfp = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{};
            if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::AdaptiveVertical)
            {
                const auto Vertical = TObjectPtr<UMaterialExpressionSubstrateVerticalLayering>{Cast<UMaterialExpressionSubstrateVerticalLayering>(EditorData->FrontMaterial.Expression)};
                if (Vertical == nullptr || Vertical->bUseParameterBlending)
                { return Reject(); }
                A = Cast<UMaterialExpressionSubstrateSlabBSDF>(Vertical->Top.Expression);
                B = Cast<UMaterialExpressionSubstrateSlabBSDF>(Vertical->Base.Expression);
                if (A == nullptr || B == nullptr || A == B
                    || NOT Input_Matches(Vertical->Top, *A, 0) || NOT Input_Matches(Vertical->Base, *B, 0)
                    || NOT Input_Matches(Vertical->Thickness, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_ThicknessCm"))))
                { return Reject(); }
                Mfp = Cast<UMaterialExpressionSubstrateTransmittanceToMFP>(A->SSSMFP.Expression);
                if (Mfp == nullptr || Mfp->GetClass() != UMaterialExpressionSubstrateTransmittanceToMFP::StaticClass()
                    || Mfp->Material != &InMaterial || NOT Input_Matches(A->SSSMFP, *Mfp, 0)
                    || NOT Input_Matches(Mfp->TransmittanceColor, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_ATransmittanceColor")))
                    || NOT Input_Matches(Mfp->Thickness, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_ThicknessCm"))))
                { return Reject(); }
                Root = Vertical;
            }
            else
            {
                const auto Mix = TObjectPtr<UMaterialExpressionSubstrateHorizontalMixing>{Cast<UMaterialExpressionSubstrateHorizontalMixing>(EditorData->FrontMaterial.Expression)};
                if (Mix == nullptr || Mix->bUseParameterBlending)
                { return Reject(); }
                A = Cast<UMaterialExpressionSubstrateSlabBSDF>(Mix->Foreground.Expression);
                B = Cast<UMaterialExpressionSubstrateSlabBSDF>(Mix->Background.Expression);
                if (A == nullptr || B == nullptr || A == B
                    || NOT Input_Matches(Mix->Foreground, *A, 0) || NOT Input_Matches(Mix->Background, *B, 0)
                    || NOT Input_Matches(Mix->Mix, InCustom, Get_OutputIndex(InCustom, TEXT("CkUsf_MixWeight"))))
                { return Reject(); }
                Root = Mix;
            }
            if (Root->Material != &InMaterial || NOT Input_Matches(EditorData->FrontMaterial, *Root, 0)
                || NOT InMaterial.GetExpressions().Contains(Root) || NOT InMaterial.GetExpressions().Contains(A)
                || NOT InMaterial.GetExpressions().Contains(B) || (Mfp != nullptr && NOT InMaterial.GetExpressions().Contains(Mfp)))
            { return Reject(); }
            for (const auto LayerA : {true, false})
            {
                const auto Slab = LayerA ? A : B;
                const auto VerticalTop = LayerA && InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::AdaptiveVertical;
                if (Slab->GetClass() != UMaterialExpressionSubstrateSlabBSDF::StaticClass()
                    || Slab->Material != &InMaterial
                    || Slab->SubSurfaceType != (VerticalTop ? MSS_SimpleVolume : MSS_None)
                    || Slab->SubsurfaceProfile != nullptr || Slab->SpecularProfile != nullptr)
                { return Reject(); }
                const auto Bindings = Get_AdaptiveBindings(LayerA, InContract.DirectionalShading, InContract.AdaptiveBaseGlints);
                const auto Inputs = Slab->GetInputsView();
                for (auto Index = 0; Index < Inputs.Num(); ++Index)
                {
                    if (Inputs[Index] == nullptr)
                    { return Reject(); }
                    if (Bindings.Contains(Index))
                    {
                        if (NOT Input_Matches(*Inputs[Index], InCustom, Get_OutputIndex(InCustom, Bindings[Index])))
                        { return Reject(); }
                    }
                    else if (NOT (VerticalTop && Index == 7) && Inputs[Index]->Expression != nullptr)
                    { return Reject(); }
                }
            }
            return true;
        }
    }

    auto Create_NativeSurfaceGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
        const FShadingModelContract& InContract, TArray<FString>& OutErrors) -> bool
    {
        if (Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph))
        { return native_surface_graph::Create_Stack3OpticalGraph(InMaterial, InCustom, InContract, OutErrors); }
        if (Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph))
        { return native_surface_graph::Create_LayeredOpticalGraph(InMaterial, InCustom, InContract, OutErrors); }
        if (Is_AdaptiveSurfaceGraph(InContract.SurfaceGraph))
        { return native_surface_graph::Create_AdaptiveGraph(InMaterial, InCustom, InContract, OutErrors); }
        auto Node = TObjectPtr<UMaterialExpressionSubstrateBSDF>{};
        if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSlab
            || Is_OpticalSurfaceGraph(InContract.SurfaceGraph))
        { Node = NewObject<UMaterialExpressionSubstrateSlabBSDF>(&InMaterial); }
        else if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSimpleClearCoat)
        { Node = NewObject<UMaterialExpressionSubstrateSimpleClearCoatBSDF>(&InMaterial); }
        else if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeToon)
        {
            const auto Toon = TObjectPtr<UMaterialExpressionSubstrateToonBSDF>{NewObject<UMaterialExpressionSubstrateToonBSDF>(&InMaterial)};
            Toon->ToonProfile = InContract.ToonProfile;
            Node = Toon;
        }
        if (Node == nullptr || InMaterial.GetEditorOnlyData() == nullptr)
        { OutErrors.Add(TEXT("Cannot create the native surface graph.")); return false; }
        Node->Material = &InMaterial;
        Node->MaterialExpressionGuid = FGuid::NewGuid();
        Node->Desc = TEXT("CkUsf_NativeSurfaceGraph");
        const auto Bindings = native_surface_graph::Get_Bindings(InContract);
        for (const auto& Binding : Bindings)
        {
            const auto OutputIndex = native_surface_graph::Get_OutputIndex(InCustom, Binding.Value);
            if (OutputIndex == INDEX_NONE || Node->GetInput(Binding.Key) == nullptr)
            { OutErrors.Add(TEXT("Native surface output layout is incomplete.")); return false; }
            Node->GetInput(Binding.Key)->Connect(OutputIndex, &InCustom);
        }
        InMaterial.GetExpressionCollection().AddExpression(Node);
        InMaterial.GetEditorOnlyData()->FrontMaterial.Connect(0, Node.Get());
        if (Is_OpticalSurfaceGraph(InContract.SurfaceGraph))
        {
            const auto Slab = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{Cast<UMaterialExpressionSubstrateSlabBSDF>(Node)};
            const auto Mfp = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{NewObject<UMaterialExpressionSubstrateTransmittanceToMFP>(&InMaterial)};
            Mfp->Material = &InMaterial;
            Mfp->MaterialExpressionGuid = FGuid::NewGuid();
            Mfp->Desc = TEXT("CkUsf_OpticalTransmittance");
            const auto ThicknessIndex = native_surface_graph::Get_OutputIndex(InCustom, TEXT("CkUsf_ThicknessCm"));
            const auto RefractionIndex = native_surface_graph::Get_OutputIndex(InCustom, TEXT("CkUsf_RefractionStrength"));
            if (Slab == nullptr || ThicknessIndex == INDEX_NONE || RefractionIndex == INDEX_NONE
                || NOT native_surface_graph::Connect_Outputs(*Mfp, InCustom,
                    {{0, TEXT("CkUsf_TransmittanceColor")}, {1, TEXT("CkUsf_ReferenceThicknessCm")}}))
            { OutErrors.Add(TEXT("Native optical surface output layout is incomplete.")); return false; }
            Slab->SubSurfaceType = MSS_SimpleVolume;
            Slab->SSSMFP.Connect(0, Mfp.Get());
            InMaterial.GetExpressionCollection().AddExpression(Mfp);
            InMaterial.GetEditorOnlyData()->SurfaceThickness.Connect(ThicknessIndex, &InCustom);
            const auto Refractive = InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeRefractiveSlab;
            if (Refractive)
            { InMaterial.GetEditorOnlyData()->Refraction = FScalarMaterialInput{}; }
            else
            { InMaterial.GetEditorOnlyData()->Refraction.Connect(RefractionIndex, &InCustom); }
            InMaterial.BlendMode = BLEND_TranslucentColoredTransmittance;
            InMaterial.TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
            InMaterial.bIsThinSurface = true;
            InMaterial.RefractionMethod = Refractive ? RM_IndexOfRefractionFromF0 : RM_PixelNormalOffset;
            InMaterial.TranslucencyPass = MTP_BeforeDOF;
        }
        InMaterial.bTangentSpaceNormal = true;
        InMaterial.TwoSided = InContract.TwoSided;
        return true;
    }

    auto Record_NativeSurfaceGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
        const UCkUsf_LookDefinition& InDefinition, const FShadingModelContract& InContract) -> void
    {
        InMaterial.GetOutermost()->GetMetaData().SetValue(&InMaterial, *native_surface_graph::SignatureKey,
            *native_surface_graph::Get_RecordedSignature(InMaterial, InDefinition, InCustom, InContract));
    }

    auto Validate_NativeSurfaceGraph(UMaterial& InMaterial, UMaterialExpressionCustom& InCustom,
        const UCkUsf_LookDefinition& InDefinition, const FShadingModelContract& InContract,
        TArray<FString>& OutErrors) -> bool
    {
        const auto Reject = [&OutErrors](const FString& InReason) -> bool
        { OutErrors.Add(InReason + TEXT(" Regenerate the native master before reload.")); return false; };
        const auto EditorData = TObjectPtr<UMaterialEditorOnlyData>{InMaterial.GetEditorOnlyData()};
        if (EditorData == nullptr)
        { return Reject(TEXT("Native material editor data is missing.")); }
        const auto Node = TObjectPtr<UMaterialExpressionSubstrateBSDF>{
            Cast<UMaterialExpressionSubstrateBSDF>(EditorData->FrontMaterial.Expression)};
        const auto ExpectedClass = TSubclassOf<UMaterialExpressionSubstrateBSDF>{
            InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSlab
                || Is_OpticalSurfaceGraph(InContract.SurfaceGraph)
                ? UMaterialExpressionSubstrateSlabBSDF::StaticClass()
                : InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeSimpleClearCoat
                    ? UMaterialExpressionSubstrateSimpleClearCoatBSDF::StaticClass()
                    : InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::AdaptiveVertical
                        || Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph)
                        || Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph)
                        ? UMaterialExpressionSubstrateVerticalLayering::StaticClass()
                        : InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::AdaptiveMix
                            ? UMaterialExpressionSubstrateHorizontalMixing::StaticClass()
                            : UMaterialExpressionSubstrateToonBSDF::StaticClass()};
        if (Node == nullptr || Node->GetClass() != ExpectedClass.Get()
            || NOT native_surface_graph::Input_Matches(EditorData->FrontMaterial, *Node, 0))
        { return Reject(TEXT("Native FrontMaterial topology changed.")); }
        const auto Optical = Is_OpticalSurfaceGraph(InContract.SurfaceGraph);
        if (InMaterial.MaterialDomain != MD_Surface || InMaterial.BlendMode != InContract.BlendMode
            || NOT InMaterial.bTangentSpaceNormal || InMaterial.TwoSided != InContract.TwoSided)
        { return Reject(TEXT("Native material settings changed.")); }
        if (Optical)
        {
            const auto Refractive = InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeRefractiveSlab;
            const auto RefractionMethod = Refractive ? RM_IndexOfRefractionFromF0 : RM_PixelNormalOffset;
            const auto Slab = TObjectPtr<UMaterialExpressionSubstrateSlabBSDF>{Cast<UMaterialExpressionSubstrateSlabBSDF>(Node)};
            const auto Mfp = TObjectPtr<UMaterialExpressionSubstrateTransmittanceToMFP>{Cast<UMaterialExpressionSubstrateTransmittanceToMFP>(Slab->SSSMFP.Expression)};
            if (NOT InMaterial.bIsThinSurface || InMaterial.TranslucencyLightingMode != TLM_SurfacePerPixelLighting
                || InMaterial.RefractionMethod != RefractionMethod || InMaterial.TranslucencyPass != MTP_BeforeDOF
                || Slab->SubSurfaceType != MSS_SimpleVolume || Slab->SubsurfaceProfile != nullptr || Slab->SpecularProfile != nullptr)
            { return Reject(TEXT("Native optical material settings changed.")); }
            if (Mfp == nullptr || Mfp->GetClass() != UMaterialExpressionSubstrateTransmittanceToMFP::StaticClass()
                || Mfp->Material != &InMaterial || NOT native_surface_graph::Input_Matches(Slab->SSSMFP, *Mfp, 0)
                || NOT native_surface_graph::Input_Matches(Mfp->TransmittanceColor, InCustom,
                    native_surface_graph::Get_OutputIndex(InCustom, TEXT("CkUsf_TransmittanceColor")))
                || NOT native_surface_graph::Input_Matches(Mfp->Thickness, InCustom,
                    native_surface_graph::Get_OutputIndex(InCustom, TEXT("CkUsf_ReferenceThicknessCm")))
                || NOT native_surface_graph::Input_Matches(EditorData->SurfaceThickness, InCustom,
                    native_surface_graph::Get_OutputIndex(InCustom, TEXT("CkUsf_ThicknessCm"))))
            { return Reject(TEXT("Native optical calibration or thickness wiring changed.")); }
            if (Refractive ? EditorData->Refraction.Expression != nullptr
                : NOT native_surface_graph::Input_Matches(EditorData->Refraction, InCustom,
                    native_surface_graph::Get_OutputIndex(InCustom, TEXT("CkUsf_RefractionStrength"))))
            { return Reject(TEXT("Native optical refraction wiring changed.")); }
        }
        if (InContract.SurfaceGraph == ECk_Usf_SurfaceGraph::NativeToon)
        {
            const auto Toon = TObjectPtr<UMaterialExpressionSubstrateToonBSDF>{Cast<UMaterialExpressionSubstrateToonBSDF>(Node)};
            if (Toon->ToonProfile != InContract.ToonProfile)
            { return Reject(TEXT("Native Toon profile binding changed.")); }
        }
        const auto Outputs = Get_ShadingModelOutputs(InContract);
        if (InCustom.OutputType != CMOT_Float3 || InCustom.AdditionalOutputs.Num() != Outputs.Num())
        { return Reject(TEXT("Native Custom output layout changed.")); }
        for (auto Index = 0; Index < Outputs.Num(); ++Index)
        {
            if (InCustom.AdditionalOutputs[Index].OutputName.ToString() != Outputs[Index].Name
                || InCustom.AdditionalOutputs[Index].OutputType != Outputs[Index].Type)
            { return Reject(TEXT("Native Custom output order or type changed.")); }
        }
        if (Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph))
        {
            if (NOT native_surface_graph::Validate_Stack3OpticalGraph(InMaterial, InCustom, OutErrors))
            { return false; }
        }
        else if (Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph))
        {
            if (NOT native_surface_graph::Validate_LayeredOpticalGraph(InMaterial, InCustom, OutErrors))
            { return false; }
        }
        else if (Is_AdaptiveSurfaceGraph(InContract.SurfaceGraph))
        {
            if (NOT native_surface_graph::Validate_AdaptiveGraph(InMaterial, InCustom, InContract, OutErrors))
            { return false; }
        }
        else
        {
            const auto Bindings = native_surface_graph::Get_Bindings(InContract);
            const auto Inputs = Node->GetInputsView();
            for (auto Index = 0; Index < Inputs.Num(); ++Index)
            {
                if (Inputs[Index] == nullptr)
                { return Reject(TEXT("Native BSDF input is missing.")); }
                const auto& Input = *Inputs[Index];
                if (Optical && Index == 7)
                { continue; } // The exact MFP utility connection was validated above.
                if (Bindings.Contains(Index))
                {
                    const auto OutputIndex = native_surface_graph::Get_OutputIndex(InCustom, Bindings[Index]);
                    if (NOT native_surface_graph::Input_Matches(Input, InCustom, OutputIndex))
                    { return Reject(TEXT("Native BSDF input connection changed.")); }
                }
                else if (Input.Expression != nullptr)
                { return Reject(TEXT("An unsupported native BSDF feature was connected.")); }
            }
        }
        for (const auto Property : {MP_BaseColor, MP_Normal, MP_Metallic, MP_Specular, MP_Roughness,
            MP_AmbientOcclusion, MP_EmissiveColor, MP_Opacity, MP_OpacityMask, MP_Refraction,
            MP_SubsurfaceColor, MP_CustomData0, MP_CustomData1, MP_Anisotropy, MP_Tangent,
            MP_WorldPositionOffset, MP_PixelDepthOffset})
        {
            if ((Optical || Is_LayeredOpticalSurfaceGraph(InContract.SurfaceGraph)
                || Is_Stack3OpticalSurfaceGraph(InContract.SurfaceGraph)) && Property == MP_Refraction)
            { continue; } // Exact optical refraction connection and masks were validated above.
            if (InMaterial.GetExpressionInputForProperty(Property)->Expression != nullptr)
            { return Reject(TEXT("A legacy surface input was connected to the native graph.")); }
        }
        for (const auto& Param : InDefinition._Parameters)
        {
            if (Param._Type != ECk_Usf_ParamType::Texture2D && Param._Type != ECk_Usf_ParamType::TextureCube
                && Param._Type != ECk_Usf_ParamType::Texture2DArray)
            { continue; }
            const auto Index = InCustom.Inputs.IndexOfByPredicate([&](const FCustomInput& InInput)
            { return InInput.InputName == Param._Name; });
            if (Index == INDEX_NONE)
            { return Reject(TEXT("A native texture input is missing.")); }
            const auto Texture = TObjectPtr<UMaterialExpressionTextureObjectParameter>{
                Cast<UMaterialExpressionTextureObjectParameter>(InCustom.Inputs[Index].Input.Expression)};
            const auto ExpectedType = Param._Type == ECk_Usf_ParamType::Texture2D ? MCT_Texture2D
                : Param._Type == ECk_Usf_ParamType::TextureCube ? MCT_TextureCube : MCT_Texture2DArray;
            if (Texture == nullptr || Texture->ParameterName != Param._Name || Texture->Texture == nullptr
                || Texture->Texture->GetMaterialType() != ExpectedType)
            { return Reject(TEXT("A native texture parameter type or dimension changed.")); }
        }
        const auto Recorded = InMaterial.GetOutermost()->GetMetaData().GetValue(&InMaterial, *native_surface_graph::SignatureKey);
        if (Recorded.IsEmpty() || Recorded != native_surface_graph::Get_RecordedSignature(InMaterial, InDefinition, InCustom, InContract))
        { return Reject(TEXT("Native authoring or input binding changed.")); }
        return true;
    }
}
