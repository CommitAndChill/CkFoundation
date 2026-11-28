#include "CkUsfEditor/Generator/CkUsf_LookValidator.h"
#include "CkUsfEditor/Generator/CkUsf_ShadingModelContract.h"

#include "CkUsf/LookDefinition/CkUsf_LookDefinition.h"

#include "CkCore/Macros/CkMacros.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/EngineTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SceneTypes.h"
#include "RenderUtils.h"
#include "ShaderCore.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_usf_look_validator
{
    // KEEP IN SYNC with Build_CustomCode / Get_ExtraOutputs in CkUsf_Generator.cpp.
    static const TCHAR* kReservedParamNames[] =
    {
        // generated locals
        TEXT("In"), TEXT("O"),
        TEXT("Time"), TEXT("UV"), TEXT("UV1"), TEXT("UV2"), TEXT("LocalPosition"),
        TEXT("WorldPosition"), TEXT("CameraVector"), TEXT("VertexNormal"),
        TEXT("VertexTangent"), TEXT("PixelDepth"), TEXT("VertexColor"),
        TEXT("SurfaceScreenUV"),
        TEXT("SceneColor"), TEXT("SceneDepth"), TEXT("SceneNormal"), TEXT("CustomDepth"), TEXT("CustomStencil"),
        TEXT("SceneBaseColor"), TEXT("SceneMetallic"), TEXT("SceneRoughness"), TEXT("SceneSpecular"),
        TEXT("ParticleColor"), TEXT("ParticleAlpha"),
        TEXT("DynParam0"), TEXT("DynParam1"), TEXT("DynParam2"), TEXT("DynParam3"),
        TEXT("F0"), TEXT("PatternUVs"), TEXT("DiffuseAlbedo"),
        TEXT("CkUsf_FuzzAmount"), TEXT("CkUsf_FuzzColor"), TEXT("CkUsf_CoatCoverage"), TEXT("CkUsf_CoatRoughness"),
        TEXT("CkUsf_AF0"), TEXT("CkUsf_ARoughness"), TEXT("CkUsf_ANormal"), TEXT("CkUsf_AEmissiveColor"),
        TEXT("CkUsf_BDiffuseAlbedo"), TEXT("CkUsf_BF0"), TEXT("CkUsf_BRoughness"), TEXT("CkUsf_BNormal"), TEXT("CkUsf_BEmissiveColor"),
        TEXT("CkUsf_AAnisotropy"), TEXT("CkUsf_ATangent"), TEXT("CkUsf_BAnisotropy"), TEXT("CkUsf_BTangent"),
        TEXT("CkUsf_ATransmittanceColor"), TEXT("CkUsf_ThicknessCm"), TEXT("CkUsf_MixWeight"),
        TEXT("EmissiveColor"), TEXT("Roughness"), TEXT("Metallic"), TEXT("Specular"),
        TEXT("AmbientOcclusion"), TEXT("Normal"), TEXT("Opacity"), TEXT("OpacityMask"),
        TEXT("Refraction"), TEXT("SubsurfaceColor"), TEXT("ClearCoat"), TEXT("ClearCoatRoughness"),
    };

    // Lowercase set â€” HLSL is case-sensitive, and so is the comparison against it.
    static const TCHAR* kHlslKeywords[] =
    {
        TEXT("bool"), TEXT("break"), TEXT("case"), TEXT("centroid"), TEXT("const"), TEXT("continue"),
        TEXT("default"), TEXT("discard"), TEXT("do"), TEXT("double"), TEXT("else"), TEXT("false"),
        TEXT("float"), TEXT("float2"), TEXT("float3"), TEXT("float4"), TEXT("for"), TEXT("half"),
        TEXT("if"), TEXT("in"), TEXT("inline"), TEXT("inout"), TEXT("int"), TEXT("linear"),
        TEXT("matrix"), TEXT("out"), TEXT("register"), TEXT("return"), TEXT("sampler"),
        TEXT("static"), TEXT("struct"), TEXT("switch"), TEXT("true"), TEXT("uint"),
        TEXT("uniform"), TEXT("vector"), TEXT("void"), TEXT("while"),
    };

    static auto Get_IsValidHlslIdentifier(const FString& InName) -> bool
    {
        if (InName.IsEmpty())
        { return false; }

        const auto IsIdentChar = [](TCHAR C, bool InFirst) -> bool
        {
            return FChar::IsAlpha(C) || C == TEXT('_') || (NOT InFirst && FChar::IsDigit(C));
        };

        for (auto Index = 0; Index < InName.Len(); ++Index)
        {
            if (NOT IsIdentChar(InName[Index], Index == 0))
            { return false; }
        }
        return true;
    }

    struct FHlslParam
    {
        FString Type;
        FString Name;
    };

    // KEEP IN SYNC with the positional argument assembly in CkUsf_Generator.cpp.
    static auto Build_ExpectedParams(const UCkUsf_LookDefinition* InDef) -> TArray<FHlslParam>
    {
        TArray<FHlslParam> Expected;
        for (const auto& Param : InDef->_Parameters)
        {
            const auto Name = Param._Name.ToString();
            switch (Param._Type)
            {
                case ECk_Usf_ParamType::Scalar:
                    Expected.Add({TEXT("float"), Name});
                    break;
                case ECk_Usf_ParamType::Vector:
                    Expected.Add({TEXT("float3"), Name});
                    break;
                case ECk_Usf_ParamType::Texture2D:
                    Expected.Add({TEXT("Texture2D"), Name});
                    Expected.Add({TEXT("SamplerState"), Name + TEXT("Sampler")});
                    break;
                case ECk_Usf_ParamType::TextureCube:
                    Expected.Add({TEXT("TextureCube"), Name});
                    Expected.Add({TEXT("SamplerState"), Name + TEXT("Sampler")});
                    break;
                case ECk_Usf_ParamType::Texture2DArray:
                    Expected.Add({TEXT("Texture2DArray"), Name});
                    Expected.Add({TEXT("SamplerState"), Name + TEXT("Sampler")});
                    break;
            }
        }
        return Expected;
    }

    static auto Render_Signature(const FString& InReturnType, const FString& InFnName,
        const FString& InFirstParam, const TArray<FHlslParam>& InParams) -> FString
    {
        FString Rendered = FString::Printf(TEXT("%s %s(%s"), *InReturnType, *InFnName, *InFirstParam);
        for (const auto& Param : InParams)
        { Rendered += FString::Printf(TEXT(", %s %s"), *Param.Type, *Param.Name); }
        return Rendered + TEXT(")");
    }

    static auto Strip_Comments(const FString& InSource) -> FString
    {
        FString Out;
        Out.Reserve(InSource.Len());
        auto InLine = false;
        auto InBlock = false;
        for (auto Index = 0; Index < InSource.Len(); ++Index)
        {
            const auto C = InSource[Index];
            const auto Next = Index + 1 < InSource.Len() ? InSource[Index + 1] : TEXT('\0');
            if (InLine)
            {
                if (C == TEXT('\n'))
                { InLine = false; Out.AppendChar(C); }
                continue;
            }
            if (InBlock)
            {
                if (C == TEXT('*') && Next == TEXT('/'))
                { InBlock = false; ++Index; }
                continue;
            }
            if (C == TEXT('/') && Next == TEXT('/'))
            { InLine = true; ++Index; continue; }
            if (C == TEXT('/') && Next == TEXT('*'))
            { InBlock = true; ++Index; continue; }
            Out.AppendChar(C);
        }
        return Out;
    }

    // OutNameAppears separates "wrong signature" from "not found" for the caller's error wording.
    static auto Find_FunctionParamList(const FString& InStripped, const FString& InFnName,
        const FString& InExpectedReturnType, FString& OutParamList, bool& OutNameAppears) -> bool
    {
        OutNameAppears = false;

        auto SearchFrom = 0;
        while (true)
        {
            const auto NameAt = InStripped.Find(InFnName, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
            if (NameAt == INDEX_NONE)
            { return false; }
            SearchFrom = NameAt + InFnName.Len();

            const auto Before = NameAt > 0 ? InStripped[NameAt - 1] : TEXT(' ');
            const auto After = NameAt + InFnName.Len() < InStripped.Len() ? InStripped[NameAt + InFnName.Len()] : TEXT(' ');
            const auto IsPartOfALongerIdentifier =
                FChar::IsAlnum(Before) || Before == TEXT('_') || FChar::IsAlnum(After) || After == TEXT('_');
            if (IsPartOfALongerIdentifier)
            { continue; }

            OutNameAppears = true;

            auto OpenAt = NameAt + InFnName.Len();
            while (OpenAt < InStripped.Len() && FChar::IsWhitespace(InStripped[OpenAt])) { ++OpenAt; }
            if (OpenAt >= InStripped.Len() || InStripped[OpenAt] != TEXT('('))
            { continue; }

            // Rejects call sites: only a declaration/definition has the return type as its previous token.
            auto TokenEnd = NameAt;
            while (TokenEnd > 0 && FChar::IsWhitespace(InStripped[TokenEnd - 1])) { --TokenEnd; }
            auto TokenStart = TokenEnd;
            while (TokenStart > 0 && (FChar::IsAlnum(InStripped[TokenStart - 1]) || InStripped[TokenStart - 1] == TEXT('_'))) { --TokenStart; }
            if (InStripped.Mid(TokenStart, TokenEnd - TokenStart) != InExpectedReturnType)
            { continue; }

            auto Depth = 0;
            for (auto Index = OpenAt; Index < InStripped.Len(); ++Index)
            {
                if (InStripped[Index] == TEXT('('))
                { ++Depth; }
                else if (InStripped[Index] == TEXT(')'))
                {
                    --Depth;
                    if (Depth == 0)
                    {
                        OutParamList = InStripped.Mid(OpenAt + 1, Index - OpenAt - 1);
                        return true;
                    }
                }
            }
            return false;   // unbalanced parens â€” treat as not found
        }
    }

    static auto Parse_ParamList(const FString& InParamList) -> TArray<FHlslParam>
    {
        TArray<FString> Entries;
        InParamList.ParseIntoArray(Entries, TEXT(","), /*CullEmpty=*/ true);

        TArray<FHlslParam> Params;
        for (auto& Entry : Entries)
        {
            TArray<FString> Tokens;
            Entry.TrimStartAndEndInline();
            Entry.ParseIntoArrayWS(Tokens);
            if (Tokens.IsEmpty())
            { continue; }

            FHlslParam Param;
            Param.Name = Tokens.Last();
            Tokens.RemoveAt(Tokens.Num() - 1);
            Param.Type = FString::Join(Tokens, TEXT(" "));
            Params.Add(Param);
        }
        return Params;
    }

    static auto Compare_Signature(const FName InLookName, const FString& InFnName,
        const FString& InFirstParamType, const FString& InReturnType,
        const TArray<FHlslParam>& InExpected, const FString& InFoundParamList,
        ck::usf_editor::FLookValidationResult& InOutResult) -> void
    {
        auto Found = Parse_ParamList(InFoundParamList);

        if (Found.IsEmpty() || NOT Found[0].Type.Contains(InFirstParamType))
        {
            InOutResult.Errors.Add(FString::Printf(
                TEXT("Look [%s]: [%s]'s first parameter must be `%s In` â€” expected signature: %s"),
                *InLookName.ToString(), *InFnName, *InFirstParamType,
                *Render_Signature(InReturnType, InFnName, InFirstParamType + TEXT(" In"), InExpected)));
            return;
        }
        Found.RemoveAt(0);

        if (Found.Num() != InExpected.Num())
        {
            InOutResult.Errors.Add(FString::Printf(
                TEXT("Look [%s]: [%s] takes %d parameter(s) after In but the asset declares %d â€” expected signature: %s"),
                *InLookName.ToString(), *InFnName, Found.Num(), InExpected.Num(),
                *Render_Signature(InReturnType, InFnName, InFirstParamType + TEXT(" In"), InExpected)));
            return;
        }

        for (auto Index = 0; Index < InExpected.Num(); ++Index)
        {
            if (Found[Index].Type != InExpected[Index].Type)
            {
                InOutResult.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: [%s] parameter %d is [%s %s] but the asset declares [%s %s] â€” wiring is positional, this would mis-bind"),
                    *InLookName.ToString(), *InFnName, Index + 1,
                    *Found[Index].Type, *Found[Index].Name,
                    *InExpected[Index].Type, *InExpected[Index].Name));
                continue;
            }
            if (Found[Index].Name != InExpected[Index].Name)
            {
                InOutResult.Warnings.Add(FString::Printf(
                    TEXT("Look [%s]: [%s] parameter %d is named [%s] in HLSL but [%s] in the asset â€” works (positional), but rename one to match"),
                    *InLookName.ToString(), *InFnName, Index + 1,
                    *Found[Index].Name, *InExpected[Index].Name));
            }
        }
    }

    static auto Validate_EntryPoint(const FName InLookName, const FString& InStripped,
        const FString& InFnName, const FString& InReturnType, const FString& InFirstParamType,
        const TArray<FHlslParam>& InExpected, const FString& InIncludePath,
        ck::usf_editor::FLookValidationResult& InOutResult) -> void
    {
        FString ParamList;
        auto NameAppears = false;
        if (NOT Find_FunctionParamList(InStripped, InFnName, InReturnType, ParamList, NameAppears))
        {
            InOutResult.Errors.Add(NameAppears
                ? FString::Printf(
                    TEXT("Look [%s]: [%s] appears in [%s] but not as `%s %s(...)` â€” wrong return type, or generated via macro (the entry point must be a plain definition)"),
                    *InLookName.ToString(), *InFnName, *InIncludePath, *InReturnType, *InFnName)
                : FString::Printf(
                    TEXT("Look [%s]: entry point [%s] not found in [%s] (it must be defined directly in that file) â€” expected signature: %s"),
                    *InLookName.ToString(), *InFnName, *InIncludePath,
                    *Render_Signature(InReturnType, InFnName, InFirstParamType + TEXT(" In"), InExpected)));
            return;
        }

        Compare_Signature(InLookName, InFnName, InFirstParamType, InReturnType, InExpected, ParamList, InOutResult);
    }

    // A Vector parameter node reads a whole float4 of custom primitive data, not the three channels the look
    // receives, so its fourth slot has to exist as well.
    auto Get_NumCustomPrimitiveDataFloatsRead(const FCk_Usf_ParamDesc& InParam) -> int32
    {
        switch (InParam._Type)
        {
            case ECk_Usf_ParamType::Scalar: return 1;
            case ECk_Usf_ParamType::Vector: return 4;
            default:                        return 0;
        }
    }

    auto Validate_GeneratedPackageRoot(const FName InLookName, const UCkUsf_LookDefinition* InDef,
        ck::usf_editor::FLookValidationResult& InOutResult) -> void
    {
        const auto& Root = InDef->_GeneratedPackageRoot;
        if (Root.IsEmpty())
        { return; }

        const auto AddRootError = [&](const FString& InProblem) -> void
        {
            InOutResult.Errors.Add(FString::Printf(
                TEXT("Look [%s]: _GeneratedPackageRoot [%s] %s"), *InLookName.ToString(), *Root, *InProblem));
        };

        if (NOT Root.StartsWith(TEXT("/")))
        {
            AddRootError(TEXT("must start with '/' â€” it is a long package path such as /Game/MyGame/GeneratedLooks"));
            return;
        }

        if (Root.EndsWith(TEXT("/")))
        {
            AddRootError(TEXT("must not end with '/' â€” the generated master's name is appended after a separator"));
            return;
        }

        const auto PackagePath = InDef->Get_GeneratedMasterPackagePath();

        auto Reason = FText{};
        if (NOT FPackageName::IsValidTextForLongPackageName(PackagePath, &Reason))
        {
            AddRootError(FString::Printf(
                TEXT("does not form a valid long package name [%s]: %s"), *PackagePath, *Reason.ToString()));
            return;
        }

        constexpr auto IncludeReadOnlyRoots = false;
        if (NOT FPackageName::IsValidLongPackageName(PackagePath, IncludeReadOnlyRoots, &Reason))
        {
            AddRootError(FString::Printf(
                TEXT("is not under a mounted, non-read-only content root: %s"), *Reason.ToString()));
            return;
        }

        auto PackageFilename = FString{};
        if (NOT FPackageName::TryConvertLongPackageNameToFilename(PackagePath, PackageFilename))
        {
            AddRootError(FString::Printf(
                TEXT("does not resolve to a file on disk for package [%s]"), *PackagePath));
            return;
        }

        // The engine's mount points are not read-only, so the mount check above admits /Engine/... and engine
        // plugins; a master saved there would land in the engine install rather than in the game or plugin.
        if (FPaths::IsUnderDirectory(PackageFilename, FPaths::EngineDir()))
        {
            AddRootError(FString::Printf(
                TEXT("resolves to [%s], under the engine directory â€” a generated master must be saved in project or plugin content"),
                *FPaths::ConvertRelativePathToFull(PackageFilename)));
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::usf_editor
{
    static auto Validate_LookDefinitionImpl(TObjectPtr<const UCkUsf_LookDefinition> InDef,
        bool InValidateRenderer, FShadingModelContract& OutResolvedContract) -> FLookValidationResult
    {
        using namespace ck_usf_look_validator;

        FLookValidationResult Result;
        OutResolvedContract = {};

        if (InDef == nullptr)
        {
            Result.Errors.Add(TEXT("Null LookDefinition"));
            return Result;
        }

        const auto LookName = InDef->Get_EffectiveLookName();

        for (const auto& Define : InDef->_Defines)
        {
            FString Name, Value;
            if (NOT Define.Split(TEXT("="), &Name, &Value))
            { Name = Define; }
            Name.TrimStartAndEndInline();
            if (Name == TEXT("CK_USF_SURFACE_SCENE_COLOR"))
            { Result.Errors.Add(TEXT("CK_USF_SURFACE_SCENE_COLOR is generated internally; use _SurfaceSceneColor")); }
        }

        auto Contract = FShadingModelContract{};
        if (NOT detail::Resolve_ShadingModelAuthoringContract(*InDef, Contract, Result.Errors))
        { return Result; }
        OutResolvedContract = Contract;

        if (Contract.SourceOverride || Contract.SurfaceGraph != ECk_Usf_SurfaceGraph::Legacy)
        {
            const auto Platform = GMaxRHIShaderPlatform;
            if (InValidateRenderer)
            { Validate_SubstrateShadingModelSupport(Contract, Get_SubstrateShadingModelSupport(), Result.Errors); }
            if (static_cast<uint32>(Platform) >= static_cast<uint32>(SP_NumPlatforms))
            { Result.Errors.Add(TEXT("Source shading-model selection requires an initialized desktop deferred shader platform.")); }
            else if (NOT FDataDrivenShaderPlatformInfo::IsValid(Platform))
            { Result.Errors.Add(TEXT("Source shading-model selection requires initialized shader-platform metadata.")); }
            else if (IsMobilePlatform(Platform) || IsForwardShadingEnabled(Platform))
            { Result.Errors.Add(TEXT("Source shading-model selection currently supports desktop deferred rendering.")); }
            else if (Requires_AdaptiveGBuffer(Contract.SurfaceGraph)
                && NOT FDataDrivenShaderPlatformInfo::GetSupportsAdaptiveGBuffer(Platform))
            { Result.Errors.Add(TEXT("Adaptive CkUsf graphs require a shader platform supporting Adaptive GBuffer.")); }
            if (NOT Result.Errors.IsEmpty())
            { return Result; }
        }

        if (Contract.Model == ECk_Usf_ShadingModel::Toon)
        {
#if !defined(UE_CK_USF_SHADING_MODEL_VERSION) || UE_CK_USF_SHADING_MODEL_VERSION < 1
            Result.Errors.Add(FString::Printf(
                TEXT("Look [%s]: Toon requires an engine with UE_CK_USF_SHADING_MODEL_VERSION >= 1"),
                *LookName.ToString()));
#endif
            if (InDef->_Domain != ECk_Usf_Domain::SurfaceLit)
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: Toon requires the SurfaceLit domain"), *LookName.ToString()));
            }
            if (InDef->_BlendMode != ECk_Usf_BlendMode::Inherit
                && InDef->_BlendMode != ECk_Usf_BlendMode::Opaque)
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: Toon currently supports only Opaque blending"), *LookName.ToString()));
            }
            if (InValidateRenderer && NOT Contract.SourceOverride)
            { Validate_SubstrateShadingModelSupport(Contract, Get_SubstrateShadingModelSupport(), Result.Errors); }
            const auto ShaderPlatform = GMaxRHIShaderPlatform;
            if (static_cast<uint32>(ShaderPlatform) >= static_cast<uint32>(SP_NumPlatforms))
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: Toon requires an initialized desktop deferred shader platform"),
                    *LookName.ToString()));
            }
            else if (NOT FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform))
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: Toon requires initialized shader platform metadata"), *LookName.ToString()));
            }
            else if (IsMobilePlatform(ShaderPlatform))
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: Toon currently supports only desktop deferred rendering; mobile is unsupported"),
                    *LookName.ToString()));
            }
            else if (IsForwardShadingEnabled(ShaderPlatform))
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: Toon currently supports only desktop deferred rendering; forward shading is unsupported"),
                    *LookName.ToString()));
            }
            if (NOT Result.Errors.IsEmpty())
            { return Result; }
        }

        if (InDef->_UshFunctionName.IsNone() || InDef->_UshIncludePath.IsEmpty())
        {
            Result.Errors.Add(FString::Printf(
                TEXT("Look [%s]: _UshFunctionName/_UshIncludePath not set"), *LookName.ToString()));
            return Result;
        }

        // ---- 1+2: parameter descriptor sanity (cheap, no file IO) ----
        TSet<FName> SeenNames;
        for (const auto& Param : InDef->_Parameters)
        {
            const auto Name = Param._Name.ToString();

            if (NOT Get_IsValidHlslIdentifier(Name))
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: param [%s] is not a valid HLSL identifier"), *LookName.ToString(), *Name));
                continue;
            }

            if (SeenNames.Contains(Param._Name))
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: duplicate param name [%s] (material parameter names are case-insensitive)"),
                    *LookName.ToString(), *Name));
            }
            SeenNames.Add(Param._Name);

            for (const auto* Reserved : kReservedParamNames)
            {
                if (Param._Name == FName{Reserved})
                {
                    Result.Errors.Add(FString::Printf(
                        TEXT("Look [%s]: param [%s] collides with the generated Custom-node input/output/local of the same name â€” rename it"),
                        *LookName.ToString(), *Name));
                    break;
                }
            }

            if (Contract.Model == ECk_Usf_ShadingModel::Toon
                && (Param._Name == FName{TEXT("ToonThreshold")} || Param._Name == FName{TEXT("ToonSoftness")}))
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: param [%s] collides with the generated Toon output of the same name - rename it"),
                    *LookName.ToString(), *Name));
            }

            for (const auto& Output : Get_ShadingModelOutputs(Contract))
            {
                if (Output.Name.StartsWith(TEXT("CkUsf_")) && Param._Name == FName{*Output.Name})
                {
                    Result.Errors.Add(FString::Printf(TEXT("Look [%s]: parameter [%s] collides with a generated shading-model output."), *LookName.ToString(), *Name));
                    break;
                }
            }

            for (const auto* Keyword : kHlslKeywords)
            {
                if (Name.Equals(Keyword, ESearchCase::CaseSensitive))
                {
                    Result.Errors.Add(FString::Printf(
                        TEXT("Look [%s]: param [%s] is an HLSL keyword"), *LookName.ToString(), *Name));
                    break;
                }
            }

            if (Param._PerInstance
                && Param._Type != ECk_Usf_ParamType::Scalar
                && Param._Type != ECk_Usf_ParamType::Vector)
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: param [%s] is _PerInstance but not Scalar/Vector â€” textures cannot ride per-instance custom data"),
                    *LookName.ToString(), *Name));
            }

            if (Param._CustomPrimitiveData
                && Param._Type != ECk_Usf_ParamType::Scalar
                && Param._Type != ECk_Usf_ParamType::Vector)
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: param [%s] is _CustomPrimitiveData but not Scalar/Vector â€” textures cannot ride custom primitive data"),
                    *LookName.ToString(), *Name));
            }

            if (Param._CustomPrimitiveData && Param._PerInstance)
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: param [%s] declares both _PerInstance and _CustomPrimitiveData â€” they are two different data sources and the generator emits one node per param"),
                    *LookName.ToString(), *Name));
            }

            // A negative index would cast to 255 and silently read a slot nothing writes.
            if (Param._CustomPrimitiveData && Param._CustomPrimitiveDataIndex < 0)
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Look [%s]: param [%s] is _CustomPrimitiveData but its _CustomPrimitiveDataIndex is [%d] â€” the CPD layout is owned by the writer, so the index must be set explicitly"),
                    *LookName.ToString(), *Name, Param._CustomPrimitiveDataIndex));
            }

            if (Param._CustomPrimitiveData && Param._CustomPrimitiveDataIndex >= 0)
            {
                const auto NumFloatsRead = Get_NumCustomPrimitiveDataFloatsRead(Param);
                constexpr auto LastIndexCarried = FCustomPrimitiveData::NumCustomPrimitiveDataFloats - 1;
                const auto LastFirstIndexThatFits = LastIndexCarried - (NumFloatsRead - 1);

                if (Param._CustomPrimitiveDataIndex > LastFirstIndexThatFits)
                {
                    Result.Errors.Add(FString::Printf(
                        TEXT("Look [%s]: param [%s] reads [%d] custom primitive data float(s) from index [%d], past the last custom primitive data index [%d] a primitive carries (a Vector reads four floats)"),
                        *LookName.ToString(), *Name, NumFloatsRead, Param._CustomPrimitiveDataIndex, LastIndexCarried));
                }
            }
        }

        Validate_GeneratedPackageRoot(LookName, InDef, Result);

        const auto IsSurfaceDomain = InDef->_Domain == ECk_Usf_Domain::SurfaceLit
                                  || InDef->_Domain == ECk_Usf_Domain::SurfaceUnlit;

        if (InDef->Get_NumPerInstanceFloats() > 0 && NOT IsSurfaceDomain)
        {
            Result.Warnings.Add(FString::Printf(
                TEXT("Look [%s]: has _PerInstance params but a non-surface domain â€” there are no instances to source them from"),
                *LookName.ToString()));
        }

        // Particle inputs are wired on the SURFACE branch of the generator only; asking for them anywhere
        // else silently leaves In.ParticleColor / In.DynamicParameter at their inert defaults.
        if ((InDef->_ParticleColor || InDef->_ParticleDynamicParameter) && NOT IsSurfaceDomain)
        {
            Result.Errors.Add(FString::Printf(
                TEXT("Look [%s]: _ParticleColor/_ParticleDynamicParameter require a Surface domain â€” the generator wires them nowhere else"),
                *LookName.ToString()));
        }

        if ((InDef->_UsedWithNiagaraSprites || InDef->_UsedWithNiagaraMeshParticles || InDef->_UsedWithNiagaraRibbons)
            && NOT IsSurfaceDomain)
        {
            Result.Warnings.Add(FString::Printf(
                TEXT("Look [%s]: a Niagara usage flag is set but the domain is not Surface â€” a particle renderer cannot use this master"),
                *LookName.ToString()));
        }

        // A particle look whose master declares NO renderer usage renders as the DEFAULT material in a packaged
        // build â€” the exact silent-fallback these flags exist to prevent.
        if ((InDef->_ParticleColor || InDef->_ParticleDynamicParameter)
            && NOT InDef->_UsedWithNiagaraSprites && NOT InDef->_UsedWithNiagaraMeshParticles
            && NOT InDef->_UsedWithNiagaraRibbons)
        {
            Result.Warnings.Add(FString::Printf(
                TEXT("Look [%s]: reads particle inputs but declares none of _UsedWithNiagaraSprites / _UsedWithNiagaraMeshParticles / _UsedWithNiagaraRibbons â€” a Niagara renderer would fall back to the default material"),
                *LookName.ToString()));
        }

        if (InDef->_ParticleDynamicParameterNames.Num() > 4)
        {
            Result.Warnings.Add(FString::Printf(
                TEXT("Look [%s]: _ParticleDynamicParameterNames has %d entries â€” only the first 4 are used"),
                *LookName.ToString(), InDef->_ParticleDynamicParameterNames.Num()));
        }

        if (InDef->_SceneTextures.Num() > 0)
        {
            if (InDef->_Domain != ECk_Usf_Domain::PostProcess)
            {
                Result.Warnings.Add(FString::Printf(
                    TEXT("Look [%s]: _SceneTextures is set but the domain is not PostProcess â€” it is ignored"),
                    *LookName.ToString()));
            }

            TSet<ECk_Usf_SceneTexture> SeenTextures;
            for (const auto SceneTexture : InDef->_SceneTextures)
            {
                if (SeenTextures.Contains(SceneTexture))
                {
                    Result.Errors.Add(FString::Printf(
                        TEXT("Look [%s]: duplicate _SceneTextures entry â€” it would wire two Custom-node inputs of the same name"),
                        *LookName.ToString()));
                    break;
                }
                SeenTextures.Add(SceneTexture);
            }
        }

        // Ignored rather than rejected, exactly like _SceneTextures above: surface looks already receive
        // WorldPosition unconditionally, so asking for it there is redundant, not wrong.
        if (InDef->_PostProcessWorldPosition && InDef->_Domain != ECk_Usf_Domain::PostProcess)
        {
            Result.Warnings.Add(FString::Printf(
                TEXT("Look [%s]: _PostProcessWorldPosition is set but the domain is not PostProcess â€” it is ignored (surface looks always receive In.WorldPosition)"),
                *LookName.ToString()));
        }

        if (NOT Result.Get_IsValid())
        { return Result; }

        // ---- 3: resolve the include through the registered shader source mappings ----
        const auto DiskPath = GetShaderSourceFilePath(InDef->_UshIncludePath);
        FString Source;
        if (DiskPath.IsEmpty() || NOT FFileHelper::LoadFileToString(Source, *DiskPath))
        {
            Result.Errors.Add(FString::Printf(
                TEXT("Look [%s]: _UshIncludePath [%s] does not resolve to a readable file â€” check the virtual path and the module's AddShaderSourceDirectoryMapping"),
                *LookName.ToString(), *InDef->_UshIncludePath));
            return Result;
        }

        // ---- 4+5: entry-point signatures ----
        const auto Stripped = Strip_Comments(Source);
        const auto Expected = Build_ExpectedParams(InDef);

        Validate_EntryPoint(LookName, Stripped, InDef->_UshFunctionName.ToString(),
            Get_SurfaceOutputType(Contract), TEXT("FCkUsf_SurfaceInput"), Expected,
            InDef->_UshIncludePath, Result);

        if (NOT InDef->_WpoFunctionName.IsNone())
        {
            if (IsSurfaceDomain)
            {
                Validate_EntryPoint(LookName, Stripped, InDef->_WpoFunctionName.ToString(),
                    TEXT("float3"), TEXT("FCkUsf_VertexInput"), Expected,
                    InDef->_UshIncludePath, Result);
            }
            else
            {
                Result.Warnings.Add(FString::Printf(
                    TEXT("Look [%s]: _WpoFunctionName is set but the domain is not Surface â€” WPO is ignored"),
                    *LookName.ToString()));
            }
        }

        return Result;
    }

    auto Validate_LookDefinition(const UCkUsf_LookDefinition* InDef) -> FLookValidationResult
    {
        auto Contract = FShadingModelContract{};
        return Validate_LookDefinitionImpl(InDef, true, Contract);
    }

    auto Classify_LookForGeneration(TObjectPtr<const UCkUsf_LookDefinition> InDef,
        const FSubstrateShadingModelSupport& InSupport) -> FLookGenerationAdmissionResult
    {
        auto Result = FLookGenerationAdmissionResult{};
        auto Contract = FShadingModelContract{};
        Result.Validation = Validate_LookDefinitionImpl(InDef, false, Contract);
        Result.ResolvedGraph = Contract.SurfaceGraph;
        if (NOT Result.Validation.Get_IsValid())
        { Result.Reason = TEXT("Look authoring validation failed."); return Result; }

        const auto NeedsRendererAdmission = Contract.SurfaceGraph != ECk_Usf_SurfaceGraph::Legacy
            || Contract.SourceOverride || Contract.Model == ECk_Usf_ShadingModel::Toon;
        if (NOT NeedsRendererAdmission)
        {
            Result.RequiredProfile = TEXT("Any");
            Result.Admission = ELookGenerationAdmission::Compatible;
            return Result;
        }
        const auto NeedsAdaptive = Requires_AdaptiveGBuffer(Contract.SurfaceGraph);
        Result.RequiredProfile = NeedsAdaptive ? TEXT("Adaptive") : TEXT("Blendable");
        const auto OtherProfile = InSupport.Enabled && InSupport.BlendableGBuffer == NeedsAdaptive;
        auto AdmissionSupport = InSupport;
        if (OtherProfile)
        {
            // Exclusion describes the current mode mismatch only. Blendable forces the effective
            // closure count to one; it cannot certify the capacity of a future Adaptive process.
            AdmissionSupport.BlendableGBuffer = NOT NeedsAdaptive;
            if (NeedsAdaptive)
            { AdmissionSupport.MaxClosuresPerPixel = FMath::Max(Get_RequiredSubstrateClosureCount(Contract.SurfaceGraph), AdmissionSupport.MaxClosuresPerPixel); }
            // IsGlintEnabled is forced off by Blendable. Exclusion does not certify the
            // feature policy/capacity of a future Adaptive startup; that gate remains strict.
            if (NeedsAdaptive && Contract.AdaptiveBaseGlints)
            { AdmissionSupport.GlintsEnabled = true; }
        }
        if (NOT Validate_SubstrateShadingModelSupport(Contract, AdmissionSupport, Result.Validation.Errors))
        { Result.Reason = TEXT("The renderer lacks required look capabilities."); return Result; }
        if (OtherProfile)
        {
            Result.Admission = ELookGenerationAdmission::OtherRendererProfile;
            Result.Reason = FString::Printf(TEXT("Requires %s GBuffer; the selected renderer uses %s. Target-profile rendering and capacity remain unverified."),
                *Result.RequiredProfile, InSupport.BlendableGBuffer ? TEXT("Blendable") : TEXT("Adaptive"));
            return Result;
        }
        Result.Admission = ELookGenerationAdmission::Compatible;
        return Result;
    }

    auto Classify_LookForGeneration(TObjectPtr<const UCkUsf_LookDefinition> InDef) -> FLookGenerationAdmissionResult
    {
        return Classify_LookForGeneration(InDef, Get_SubstrateShadingModelSupport());
    }
}

// --------------------------------------------------------------------------------------------------------------------
