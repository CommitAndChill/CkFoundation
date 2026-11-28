namespace CkUsf
{
    asset LibraryVelvet of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryVelvet.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryVelvet";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::Cloth;
        _LookName = n"LibraryVelvet";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryVelvet_BaseColor.T_LibraryVelvet_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryVelvet_Normal.T_LibraryVelvet_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryVelvet_Roughness.T_LibraryVelvet_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.45, 0.06, 0.1, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2.0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0.0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.18, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.05, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FuzzWeight", 0.9, n"Fuzz", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"FuzzColor", FLinearColor(0.16, 0.02, 0.035, 1.0), n"Fuzz", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0.0, n"Inspection", 0));
    }

    asset LibraryCarbonFiber of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryCarbonFiber.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryCarbonFiber";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryCarbonFiber";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCarbonFiber_BaseColor.T_LibraryCarbonFiber_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCarbonFiber_Normal.T_LibraryCarbonFiber_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCarbonFiber_Roughness.T_LibraryCarbonFiber_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1.0, 1.0, 1.0, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 12.0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 45.0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.04, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.65, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.15, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"CoatWeight", 0.9, n"Resin", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"CoatRoughness", 0.2, n"Resin", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0.0, n"Inspection", 0));
    }
}
