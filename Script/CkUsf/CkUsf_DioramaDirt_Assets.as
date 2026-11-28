namespace CkUsf
{
    asset EnvironmentDioramaDirt of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/EnvironmentDioramaDirt.ush";
        _UshFunctionName = n"CkUsf_Look_EnvironmentDioramaDirt";
        _WpoFunctionName = n"CkUsf_Look_EnvironmentDioramaDirt_WPO";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"EnvironmentDioramaDirt";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentDioramaDirt_BaseColor.T_EnvironmentDioramaDirt_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentDioramaDirt_Normal.T_EnvironmentDioramaDirt_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentDioramaDirt_Roughness.T_EnvironmentDioramaDirt_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"HeightMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentDioramaDirt_Height.T_EnvironmentDioramaDirt_Height", n"Textures", 30));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"AOMap",
            "/CkFoundation/CkUsf/Textures/Environment/T_EnvironmentDioramaDirt_AO.T_EnvironmentDioramaDirt_AO", n"Textures", 40));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 1, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 1, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"ReliefCm", 2.87664306164, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"TileSizeCm", 80, n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"PomAmount", 1, n"Surface", 80));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DisplacementAmount", 0, n"Surface", 90));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"StepCount", 64, n"Surface", 100));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"HeightMip", 0, n"Surface", 110));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 120));
    }
}
