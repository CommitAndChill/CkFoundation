namespace CkUsf
{
    asset EnvironmentWoodland of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/EnvironmentWoodland.ush";
        _UshFunctionName = n"CkUsf_Look_EnvironmentWoodland";
        _WpoFunctionName = n"CkUsf_Look_EnvironmentWoodland_WPO";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"EnvironmentWoodland";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentWoodland_BaseColor.T_EnvironmentWoodland_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentWoodland_Normal.T_EnvironmentWoodland_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentWoodland_Roughness.T_EnvironmentWoodland_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"HeightMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentWoodland_Height.T_EnvironmentWoodland_Height", n"Textures", 30));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"AOMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentWoodland_AO.T_EnvironmentWoodland_AO", n"Textures", 40));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 1, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 1, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"ReliefCm", 2.4801279306411743, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"TileSizeCm", 80, n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"PomAmount", 1, n"Surface", 80));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DisplacementAmount", 0, n"Surface", 90));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"StepCount", 48, n"Surface", 100));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"HeightMip", 0, n"Surface", 110));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 120));
    }
}
