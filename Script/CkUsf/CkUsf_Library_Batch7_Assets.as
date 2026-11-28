namespace CkUsf
{
    asset LibraryNylon of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryNylon.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryNylon";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryNylon";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryNylon_BaseColor.T_LibraryNylon_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryNylon_Normal.T_LibraryNylon_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryNylon_Roughness.T_LibraryNylon_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.28, 0.42, 0.5, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 0.8, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.22, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.75, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"SheenWeight", 0.7, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibrarySlate of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibrarySlate.ush";
        _UshFunctionName = n"CkUsf_Look_LibrarySlate";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibrarySlate";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySlate_BaseColor.T_LibrarySlate_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySlate_Normal.T_LibrarySlate_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySlate_Roughness.T_LibrarySlate_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.3, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.45, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 1, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.12, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"ReliefContrast", 1, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryCopper of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryCopper.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryCopper";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryCopper";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCopper_BaseColor.T_LibraryCopper_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCopper_Normal.T_LibraryCopper_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCopper_Roughness.T_LibraryCopper_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.95, 0.45, 0.25, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.08, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.75, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.18, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishVariation", 0.15, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryGlossyPlastic of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryGlossyPlastic.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryGlossyPlastic";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryGlossyPlastic";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGlossyPlastic_BaseColor.T_LibraryGlossyPlastic_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGlossyPlastic_Normal.T_LibraryGlossyPlastic_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryGlossyPlastic_Roughness.T_LibraryGlossyPlastic_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.35, 0.72, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.08, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.35, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.05, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlossWeight", 1, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryWax of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryWax.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryWax";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::Subsurface;
        _LookName = n"LibraryWax";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryWax_BaseColor.T_LibraryWax_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryWax_Normal.T_LibraryWax_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryWax_Roughness.T_LibraryWax_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 0.9, 0.72, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"ScatterTint", FLinearColor(1, 0.6, 0.25, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.3, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.22, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.8, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"ScatterWeight", 0.55, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryFiberglass of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryFiberglass.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryFiberglass";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryFiberglass";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryFiberglassV2_BaseColor.T_LibraryFiberglassV2_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryFiberglassV2_Normal.T_LibraryFiberglassV2_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryFiberglassV2_Roughness.T_LibraryFiberglassV2_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.85, 0.91, 0.83, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 6, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.12, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.9, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.03, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"ResinWeight", 0.85, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"ResinRoughness", 0.16, n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 80));
    }
}
