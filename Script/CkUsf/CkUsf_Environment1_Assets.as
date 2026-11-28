namespace CkUsf
{
    asset EnvironmentGravel of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/EnvironmentGravel.ush";
        _UshFunctionName = n"CkUsf_Look_EnvironmentGravel";
        _WpoFunctionName = n"CkUsf_Look_EnvironmentGravel_WPO";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"EnvironmentGravel";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentGravelV2_BaseColor.T_EnvironmentGravelV2_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentGravelV2_Normal.T_EnvironmentGravelV2_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentGravelV2_Roughness.T_EnvironmentGravelV2_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"HeightMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentGravelV2_Height.T_EnvironmentGravelV2_Height", n"Textures", 30));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"AOMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentGravelV2_AO.T_EnvironmentGravelV2_AO", n"Textures", 40));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 0.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.35, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 1, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"ReliefCm", 12, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"TileSizeCm", 230, n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"PomAmount", 1, n"Surface", 80));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DisplacementAmount", 0, n"Surface", 90));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"StepCount", 32, n"Surface", 100));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"HeightMip", 2, n"Surface", 110));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 120));
    }
}
