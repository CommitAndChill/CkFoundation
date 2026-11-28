namespace CkUsf
{
    asset IndustrialPaint of UCkUsf_LookDefinition
    {
        _UshIncludePath = "/CkUsf/Looks/IndustrialPaint.ush";
        _UshFunctionName = n"CkUsf_Look_IndustrialPaint";
        _Domain = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel = ECk_Usf_ShadingModel::ClearCoat;
        _LookName = n"IndustrialPaint";

        _Parameters.Add(CkUsf::Usf_TextureIn(n"DetailData",
            "/CkFoundation/CkUsf/Textures/Industrial/T_IndustrialDetail.T_IndustrialDetail", n"Surfaces", 0));
        _Parameters.Add(CkUsf::Usf_TextureIn(n"PlacementData",
            "/CkFoundation/CkUsf/Textures/Industrial/T_IndustrialPlacement.T_IndustrialPlacement", n"Placement", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"PaintColor", FLinearColor(0.028, 0.19, 0.16, 1.0), n"Paint", 0));
        _Parameters.Add(CkUsf::Usf_VectorIn(n"PrimerColor", FLinearColor(0.19, 0.045, 0.022, 1.0), n"Paint", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Wear", 0.45, n"Aging", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"RustAmount", 0.3, n"Aging", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DustAmount", 0.1, n"Aging", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"ScratchAmount", 0.4, n"Aging", 30));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"Wetness", 0.0, n"Finish", 0));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"SurfaceTiling", 3.0, n"Surfaces", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"MaskTiling", 1.2, n"Placement", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"MaskOffset", 0.0, n"Placement", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"PaintBlur", 0.42, n"Finish", 10));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"NormalStrength", 0.5, n"Surfaces", 20));
        _Parameters.Add(CkUsf::Usf_ScalarIn(n"DebugView", 0.0, n"Inspection", 0));
    }
}
