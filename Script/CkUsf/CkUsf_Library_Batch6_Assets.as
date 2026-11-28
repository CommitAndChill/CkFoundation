namespace CkUsf
{
    asset LibraryLinen of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryLinen.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryLinen";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::Cloth;
        _LookName = n"LibraryLinen";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLinen_BaseColor.T_LibraryLinen_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLinen_Normal.T_LibraryLinen_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLinen_Roughness.T_LibraryLinen_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2.3, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.45, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FuzzWeight", 0.12, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"FuzzColor", FLinearColor(0.08, 0.07, 0.05, 1), n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 80));
    }

    asset LibrarySmoothLeather of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibrarySmoothLeather.ush";
        _UshFunctionName = n"CkUsf_Look_LibrarySmoothLeather";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibrarySmoothLeather";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySmoothLeather_BaseColor.T_LibrarySmoothLeather_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySmoothLeather_Normal.T_LibrarySmoothLeather_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySmoothLeather_Roughness.T_LibrarySmoothLeather_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.25, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GrainContrast", 1, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryPine of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryPine.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryPine";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryPine";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPine_BaseColor.T_LibraryPine_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPine_Normal.T_LibraryPine_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPine_Roughness.T_LibraryPine_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.6, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.3, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.75, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishWeight", 0.35, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishRoughness", 0.25, n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 80));
    }

    asset LibraryLimestone of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryLimestone.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryLimestone";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryLimestone";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLimestone_BaseColor.T_LibraryLimestone_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLimestone_Normal.T_LibraryLimestone_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLimestone_Roughness.T_LibraryLimestone_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.6, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.45, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.1, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"PoreContrast", 1, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryGalvanizedSteel of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryGalvanizedSteel.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryGalvanizedSteel";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryGalvanizedSteel";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGalvanizedSteel_BaseColor.T_LibraryGalvanizedSteel_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGalvanizedSteel_Normal.T_LibraryGalvanizedSteel_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGalvanizedSteel_Roughness.T_LibraryGalvanizedSteel_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.62, 0.65, 0.7, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.025, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 1.2, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.15, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"SpangleContrast", 0.65, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryMosaic of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryMosaic.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryMosaic";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryMosaic";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryMosaic_BaseColor.T_LibraryMosaic_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryMosaic_Normal.T_LibraryMosaic_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryMosaic_Roughness.T_LibraryMosaic_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"TileMask",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryMosaic_TileMask.T_LibraryMosaic_TileMask", n"Textures", 30));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"GroutColor", FLinearColor(0.35, 0.36, 0.35, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.8, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.12, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.6, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.04, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GroutNormalStrength", 0.7, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GroutRoughness", 0.88, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlazeWeight", 0.85, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlazeRoughness", 0.12, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }
}
