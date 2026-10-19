#pragma once

#include "CoreMinimal.h"
#include "CkUsf/LookDefinition/CkUsf_LookDefinition.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialExpressionCustom.h"

class UMaterial;

namespace ck::usf_editor
{
    struct CKUSFEDITOR_API FShadingModelContract
    {
        ECk_Usf_ShadingModel Model = ECk_Usf_ShadingModel::DefaultLit;
        EMaterialShadingModel NativeModel = MSM_DefaultLit;
        EBlendMode BlendMode = BLEND_Opaque;
        TEnumAsByte<ETranslucencyLightingMode> TranslucencyLighting = TLM_VolumetricNonDirectional;
        bool TwoSided = false;
        bool TangentSpaceNormal = true;
        TObjectPtr<USubsurfaceProfile> Profile;
        bool SourceOverride = false;
    };

    struct CKUSFEDITOR_API FShadingModelOutput
    {
        FString Name;
        ECustomMaterialOutputType Type;
        EMaterialProperty Property;
    };

    struct CKUSFEDITOR_API FSubstrateShadingModelSupport
    {
        bool Enabled = false;
        bool BlendableGBuffer = false;
        bool ToonBridge = false;
        bool LegacyRoot = true;
        bool SingleShadingModel = true;
    };

    CKUSFEDITOR_API auto Validate_SubstrateShadingModelSupport(const FShadingModelContract& InContract,
        const FSubstrateShadingModelSupport& InSupport, TArray<FString>& OutErrors) -> bool;
    CKUSFEDITOR_API auto Get_SubstrateShadingModelSupport() -> FSubstrateShadingModelSupport;

    CKUSFEDITOR_API auto Parse_ShadingModelDeclaration(const FString& InSource,
        TOptional<ECk_Usf_ShadingModel>& OutModel, FString& OutError) -> bool;
    CKUSFEDITOR_API auto Resolve_ShadingModelContract(const UCkUsf_LookDefinition& InDefinition,
        FShadingModelContract& OutContract, TArray<FString>& OutErrors) -> bool;
    CKUSFEDITOR_API auto Get_ShadingModelOutputs(const FShadingModelContract& InContract)
        -> TArray<FShadingModelOutput>;
    CKUSFEDITOR_API auto Build_ShadingModelOutputCode(const FShadingModelContract& InContract) -> FString;
    CKUSFEDITOR_API auto Apply_ShadingModelContract(UMaterial& InMaterial,
        UMaterialExpressionCustom& InCustom, const FShadingModelContract& InContract) -> void;
}
