namespace CkUsf
{
    asset LibraryTerracotta of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryTerracotta.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryTerracotta";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryTerracotta";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTerracotta_BaseColor.T_LibraryTerracotta_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTerracotta_Normal.T_LibraryTerracotta_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTerracotta_Roughness.T_LibraryTerracotta_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1.4, 0.72, 0.45, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2.0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.6, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.75, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.15, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FiringVariation", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryMarble of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryMarble.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryMarble";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryMarble";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryMarble_BaseColor.T_LibraryMarble_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryMarble_Normal.T_LibraryMarble_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryMarble_Roughness.T_LibraryMarble_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 0.8, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.08, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.7, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"VeinContrast", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Polish", 0.7, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryRubber of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryRubber.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryRubber";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryRubber";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryRubber_BaseColor.T_LibraryRubber_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryRubber_Normal.T_LibraryRubber_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryRubber_Roughness.T_LibraryRubber_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(2, 2, 2, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 4, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.3, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.15, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"SpecularLevel", 0.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryGlass of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryGlass.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryGlass";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ThinTranslucent;
        _LookName = n"LibraryGlass";
        _BlendMode = ECk_Usf_BlendMode::Translucent;
        _TranslucencyLighting = ECk_Usf_TranslucencyLighting::SurfacePerPixel;
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGlass_Normal.T_LibraryGlass_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGlass_Roughness.T_LibraryGlass_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.98, 0.99, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.04, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.035, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.045, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"SpecularLevel", 0.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"TransmissionAmount", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlassCoverage", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Ior", 1.02, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

}
