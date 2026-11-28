namespace CkUsf
{
    asset LibraryCanvas of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryCanvas.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryCanvas";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryCanvas";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCanvas_BaseColor.T_LibraryCanvas_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCanvas_Normal.T_LibraryCanvas_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCanvas_Roughness.T_LibraryCanvas_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1.0, 1.0, 1.0, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 0.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0.0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.55, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.8, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.15, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"WeaveContrast", 1.0, n"Weave", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0.0, n"Inspection", 0));
    }

    asset LibrarySuede of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibrarySuede.ush";
        _UshFunctionName = n"CkUsf_Look_LibrarySuede";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::Cloth;
        _LookName = n"LibrarySuede";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySuede_BaseColor.T_LibrarySuede_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySuede_Normal.T_LibrarySuede_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySuede_Roughness.T_LibrarySuede_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1.0, 1.0, 1.0, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2.0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0.0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.3, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.1, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NapWeight", 0.65, n"Nap", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"NapColor", FLinearColor(0.3, 0.2, 0.12, 1.0), n"Nap", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NapContrast", 0.75, n"Nap", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0.0, n"Inspection", 0));
    }
}
