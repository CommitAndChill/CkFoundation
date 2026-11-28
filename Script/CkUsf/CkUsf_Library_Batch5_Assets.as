namespace CkUsf
{
    asset LibraryDenim of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryDenim.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryDenim";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::Cloth;
        _LookName = n"LibraryDenim";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryDenim_BaseColor.T_LibraryDenim_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryDenim_Normal.T_LibraryDenim_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryDenim_Roughness.T_LibraryDenim_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2.4, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.45, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FuzzWeight", 0.16, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"FuzzColor", FLinearColor(0.07, 0.09, 0.13, 1), n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 80));
    }

    asset LibraryConcrete of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryConcrete.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryConcrete";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryConcrete";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryConcrete_BaseColor.T_LibraryConcrete_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryConcrete_Normal.T_LibraryConcrete_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryConcrete_Roughness.T_LibraryConcrete_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.8, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.45, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.12, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"PoreContrast", 1, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryBrick of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryBrick.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryBrick";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryBrick";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryBrick_BaseColor.T_LibraryBrick_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryBrick_Normal.T_LibraryBrick_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryBrick_Roughness.T_LibraryBrick_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.55, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.12, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVAspect", 2, n"Surface", 55));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FiringVariation", 1, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

    asset LibraryStainlessSteel of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryStainlessSteel.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryStainlessSteel";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryStainlessSteel";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryStainlessSteel_BaseColor.T_LibraryStainlessSteel_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryStainlessSteel_Normal.T_LibraryStainlessSteel_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryStainlessSteel_Roughness.T_LibraryStainlessSteel_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(0.56, 0.57, 0.58, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.12, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.75, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.28, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"MetalColorVariation", 0.18, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Polish", 0.35, n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 80));
    }

    asset LibraryPlywood of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryPlywood.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryPlywood";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"LibraryPlywood";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPlywood_BaseColor.T_LibraryPlywood_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPlywood_Normal.T_LibraryPlywood_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryPlywood_Roughness.T_LibraryPlywood_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 1.6, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.3, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.8, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.08, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishWeight", 0.25, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FinishRoughness", 0.3, n"Surface", 70));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 80));
    }

    asset LibraryCardboard of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/LibraryCardboard.ush";
        _UshFunctionName = n"CkUsf_Look_LibraryCardboard";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::DefaultLit;
        _LookName = n"LibraryCardboard";
        _Parameters.Add(CkUsf::Usf_TextureIn(n"BaseColorMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCardboard_BaseColor.T_LibraryCardboard_BaseColor", n"Textures", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"NormalMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCardboard_Normal.T_LibraryCardboard_Normal", n"Textures", 10));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"RoughnessMap",
            "/CkFoundation/CkUsf/Textures/Library/T_LibraryCardboard_Roughness.T_LibraryCardboard_Roughness", n"Textures", 20));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"TintColor", FLinearColor(1, 1, 1, 1), n"Surface", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVScale", 2, n"Surface", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"UVRotation", 0, n"Surface", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.3, n"Surface", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessScale", 0.85, n"Surface", 40));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RoughnessBias", 0.16, n"Surface", 50));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"FiberContrast", 1, n"Surface", 60));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0, n"Surface", 70));
    }

}
