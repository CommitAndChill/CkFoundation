namespace CkUsf
{
    asset LibraryKnit of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryKnit.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryKnit";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::Cloth;
        _LookName = n"LibraryKnit";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryKnit_BaseColor.T_LibraryKnit_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryKnit_Normal.T_LibraryKnit_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryKnit_Roughness.T_LibraryKnit_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.7, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.04, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FuzzWeight", 0.25, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"FuzzColor", FLinearColor(0.12, 0.12, 0.12, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryPatentLeather of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryPatentLeather.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryPatentLeather";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryPatentLeather";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPatentLeather_BaseColor.T_LibraryPatentLeather_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPatentLeather_Normal.T_LibraryPatentLeather_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPatentLeather_Roughness.T_LibraryPatentLeather_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2.2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.055, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"CoatWeight", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"CoatRoughness", 0.075, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryPorcelain of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryPorcelain.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryPorcelain";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryPorcelain";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPorcelain_BaseColor.T_LibraryPorcelain_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPorcelain_Normal.T_LibraryPorcelain_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPorcelain_Roughness.T_LibraryPorcelain_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"TileMask",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPorcelain_TileMask.T_LibraryPorcelain_TileMask", n"Textures", 30));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"GroutColor", FLinearColor(0.13, 0.14, 0.15, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.12, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.6, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.04, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GroutNormalStrength", 0.7, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GroutRoughness", 0.88, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlazeWeight", 0.9, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlazeRoughness", 0.11, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryTerrazzo of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryTerrazzo.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryTerrazzo";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryTerrazzo";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTerrazzo_BaseColor.T_LibraryTerrazzo_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTerrazzo_Normal.T_LibraryTerrazzo_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTerrazzo_Roughness.T_LibraryTerrazzo_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.4, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.15, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"AggregateContrast", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Polish", 0.7, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryTranslucentPlastic of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryTranslucentPlastic.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryTranslucentPlastic";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ThinTranslucent;
        _LookName = n"LibraryTranslucentPlastic";
        _BlendMode = ECk_Usf_BlendMode::Translucent;
        _TranslucencyLighting = ECk_Usf_TranslucencyLighting::SurfacePerPixel;
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTranslucentPlastic_BaseColor.T_LibraryTranslucentPlastic_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTranslucentPlastic_Normal.T_LibraryTranslucentPlastic_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTranslucentPlastic_Roughness.T_LibraryTranslucentPlastic_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.38, 0.72, 0.9, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.08, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.12, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"BodyCoverage", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"TransmissionAmount", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TransmissionTint", FLinearColor(0.65, 0.85, 0.95, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlassCoverage", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Ior", 1.0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryCork of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryCork.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryCork";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryCork";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCork_BaseColor.T_LibraryCork_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCork_Normal.T_LibraryCork_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCork_Roughness.T_LibraryCork_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.45, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.8, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.22, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"PoreContrast", 1.15, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

}
