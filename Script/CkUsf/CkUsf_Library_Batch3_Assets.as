namespace CkUsf
{
    asset LibraryWalnut of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryWalnut.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryWalnut";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryWalnut";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryWalnut_BaseColor.T_LibraryWalnut_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryWalnut_Normal.T_LibraryWalnut_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryWalnut_Roughness.T_LibraryWalnut_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.72, 0.43, 0.22, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 0.9, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.25, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.7, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.03, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GrainContrast", 1.1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishWeight", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishRoughness", 0.22, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryGranite of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryGranite.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryGranite";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryGranite";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGranite_BaseColor.T_LibraryGranite_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGranite_Normal.T_LibraryGranite_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGranite_Roughness.T_LibraryGranite_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"MineralContrast", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Polish", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryBrass of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryBrass.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryBrass";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryBrass";
        _DirectionalShading = true;
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryBrass_BaseColor.T_LibraryBrass_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryBrass_Normal.T_LibraryBrass_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryBrass_Roughness.T_LibraryBrass_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.94, 0.64, 0.22, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 3, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.4, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.04, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishVariation", 0.1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Anisotropy", 0.55, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"BrushRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryFrostedGlass of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryFrostedGlass.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryFrostedGlass";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ThinTranslucent;
        _LookName = n"LibraryFrostedGlass";
        _SurfaceSceneColor = true;
        _SceneColorReplacesBackground = true;
        _BlendMode = ECk_Usf_BlendMode::Translucent;
        _TranslucencyLighting = ECk_Usf_TranslucencyLighting::SurfacePerPixel;
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryFrostedGlass_Normal.T_LibraryFrostedGlass_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryFrostedGlass_Roughness.T_LibraryFrostedGlass_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.98, 0.99, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.25, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.3, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"SpecularLevel", 0.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"TransmissionAmount", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlassCoverage", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"BlurRadius", 8, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

}
