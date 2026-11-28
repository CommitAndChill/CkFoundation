namespace CkUsf
{
    asset LibraryTile of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryTile.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryTile";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryTile";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTile_BaseColor.T_LibraryTile_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTile_Normal.T_LibraryTile_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTile_Roughness.T_LibraryTile_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"TileMask",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryTile_Mask.T_LibraryTile_Mask", n"Textures", 30));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1.0, 1.0, 1.0, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"GroutColor", FLinearColor(0.58, 0.55, 0.48, 1.0), n"Grout", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 3.0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0.0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.55, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.55, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.02, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GroutNormalStrength", 1.0, n"Grout", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GroutRoughness", 0.85, n"Grout", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlazeWeight", 0.85, n"Glaze", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"GlazeRoughness", 0.13, n"Glaze", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0.0, n"Inspection", 0));
    }

    asset LibrarySilk of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibrarySilk.ush";
        _UshFunctionName = n"CkUsf_Look_LibrarySilk";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibrarySilk";
        _DirectionalShading = true;

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySilk_BaseColor.T_LibrarySilk_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySilk_Normal.T_LibrarySilk_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySilk_Roughness.T_LibrarySilk_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"DirectionMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySilk_Direction.T_LibrarySilk_Direction", n"Textures", 30));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"AnisotropyMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibrarySilk_Anisotropy.T_LibrarySilk_Anisotropy", n"Textures", 40));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1.0, 1.0, 1.0, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.14, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.38, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.02, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Anisotropy", 0.72, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FiberRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryLeather of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryLeather.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryLeather";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryLeather";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLeather_BaseColor.T_LibraryLeather_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLeather_Normal.T_LibraryLeather_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryLeather_Roughness.T_LibraryLeather_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1.0, 1.0, 1.0, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.8, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.02, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"PoreContrast", 1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryOak of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryOak.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryOak";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryOak";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryOak_BaseColor.T_LibraryOak_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryOak_Normal.T_LibraryOak_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryOak_Roughness.T_LibraryOak_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.35, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishWeight", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishRoughness", 0.24, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryAluminum of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryAluminum.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryAluminum";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryAluminum";
        _DirectionalShading = true;

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryAluminum_BaseColor.T_LibraryAluminum_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryAluminumLinear_Normal.T_LibraryAluminumLinear_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryAluminumLinear_Roughness.T_LibraryAluminumLinear_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"DirectionMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryAluminumLinear_Direction.T_LibraryAluminumLinear_Direction", n"Textures", 30));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.8, 0.82, 0.85, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 3, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.65, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.7, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Anisotropy", 0.55, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"BrushRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

    asset LibraryPlastic of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryPlastic.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryPlastic";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryPlastic";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPlastic_BaseColor.T_LibraryPlastic_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPlastic_Normal.T_LibraryPlastic_Normal", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPlastic_Roughness.T_LibraryPlastic_Roughness", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.055, 0.19, 0.16, 1.0), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 4, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.32, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.8, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.1, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"SpecularLevel", 0.5, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 10));
    }

}

