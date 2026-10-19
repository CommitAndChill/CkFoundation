namespace CkUsf
{
    asset StudioCeramic of UCkUsf_LookDefinition
    {
        _UshIncludePath  = "/CkUsf/Looks/StudioCeramic.ush";
        _UshFunctionName = n"CkUsf_Look_StudioCeramic";
        _Domain          = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel    = ECk_Usf_ShadingModel::DefaultLit;
        _LookName        = n"StudioCeramic";

        FCk_Usf_ParamDesc TintColor;
        TintColor._Name = n"TintColor";
        TintColor._Type = ECk_Usf_ParamType::Vector;
        TintColor._DefaultVector = FLinearColor(0.78, 0.74, 0.64, 1.0);
        TintColor._Group = n"Body";
        TintColor._SortPriority = 0;
        _Parameters.Add(TintColor);

        FCk_Usf_ParamDesc GlazeBlur;
        GlazeBlur._Name = n"GlazeBlur";
        GlazeBlur._Type = ECk_Usf_ParamType::Scalar;
        GlazeBlur._DefaultScalar = 0.18;
        GlazeBlur._Group = n"Finish";
        GlazeBlur._SortPriority = 10;
        _Parameters.Add(GlazeBlur);

        FCk_Usf_ParamDesc DetailScale;
        DetailScale._Name = n"DetailScale";
        DetailScale._Type = ECk_Usf_ParamType::Scalar;
        DetailScale._DefaultScalar = 0.75;
        DetailScale._Group = n"Detail";
        DetailScale._SortPriority = 20;
        _Parameters.Add(DetailScale);

        FCk_Usf_ParamDesc DetailAmount;
        DetailAmount._Name = n"DetailAmount";
        DetailAmount._Type = ECk_Usf_ParamType::Scalar;
        DetailAmount._DefaultScalar = 0.06;
        DetailAmount._Group = n"Detail";
        DetailAmount._SortPriority = 30;
        _Parameters.Add(DetailAmount);
    }

    asset StudioCopper of UCkUsf_LookDefinition
    {
        _UshIncludePath  = "/CkUsf/Looks/StudioCopper.ush";
        _UshFunctionName = n"CkUsf_Look_StudioCopper";
        _Domain          = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel    = ECk_Usf_ShadingModel::DefaultLit;
        _LookName        = n"StudioCopper";

        FCk_Usf_ParamDesc TintColor;
        TintColor._Name = n"TintColor";
        TintColor._Type = ECk_Usf_ParamType::Vector;
        TintColor._DefaultVector = FLinearColor(0.95, 0.48, 0.22, 1.0);
        TintColor._Group = n"Body";
        TintColor._SortPriority = 0;
        _Parameters.Add(TintColor);

        FCk_Usf_ParamDesc FinishBlur;
        FinishBlur._Name = n"FinishBlur";
        FinishBlur._Type = ECk_Usf_ParamType::Scalar;
        FinishBlur._DefaultScalar = 0.28;
        FinishBlur._Group = n"Finish";
        FinishBlur._SortPriority = 10;
        _Parameters.Add(FinishBlur);

        FCk_Usf_ParamDesc DetailScale;
        DetailScale._Name = n"DetailScale";
        DetailScale._Type = ECk_Usf_ParamType::Scalar;
        DetailScale._DefaultScalar = 1.8;
        DetailScale._Group = n"Detail";
        DetailScale._SortPriority = 20;
        _Parameters.Add(DetailScale);

        FCk_Usf_ParamDesc ScratchAmount;
        ScratchAmount._Name = n"ScratchAmount";
        ScratchAmount._Type = ECk_Usf_ParamType::Scalar;
        ScratchAmount._DefaultScalar = 0.06;
        ScratchAmount._Group = n"Detail";
        ScratchAmount._SortPriority = 30;
        _Parameters.Add(ScratchAmount);
    }

    asset StudioRubber of UCkUsf_LookDefinition
    {
        _UshIncludePath  = "/CkUsf/Looks/StudioRubber.ush";
        _UshFunctionName = n"CkUsf_Look_StudioRubber";
        _Domain          = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel    = ECk_Usf_ShadingModel::DefaultLit;
        _LookName        = n"StudioRubber";

        FCk_Usf_ParamDesc TintColor;
        TintColor._Name = n"TintColor";
        TintColor._Type = ECk_Usf_ParamType::Vector;
        TintColor._DefaultVector = FLinearColor(0.025, 0.031, 0.034, 1.0);
        TintColor._Group = n"Body";
        TintColor._SortPriority = 0;
        _Parameters.Add(TintColor);

        FCk_Usf_ParamDesc FinishBlur;
        FinishBlur._Name = n"FinishBlur";
        FinishBlur._Type = ECk_Usf_ParamType::Scalar;
        FinishBlur._DefaultScalar = 0.79;
        FinishBlur._Group = n"Finish";
        FinishBlur._SortPriority = 10;
        _Parameters.Add(FinishBlur);

        FCk_Usf_ParamDesc DetailScale;
        DetailScale._Name = n"DetailScale";
        DetailScale._Type = ECk_Usf_ParamType::Scalar;
        DetailScale._DefaultScalar = 4.0;
        DetailScale._Group = n"Detail";
        DetailScale._SortPriority = 20;
        _Parameters.Add(DetailScale);

        FCk_Usf_ParamDesc GrainAmount;
        GrainAmount._Name = n"GrainAmount";
        GrainAmount._Type = ECk_Usf_ParamType::Scalar;
        GrainAmount._DefaultScalar = 0.10;
        GrainAmount._Group = n"Detail";
        GrainAmount._SortPriority = 30;
        _Parameters.Add(GrainAmount);
    }

    asset StudioLacquer of UCkUsf_LookDefinition
    {
        _UshIncludePath  = "/CkUsf/Looks/StudioLacquer.ush";
        _UshFunctionName = n"CkUsf_Look_StudioLacquer";
        _Domain          = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel    = ECk_Usf_ShadingModel::ClearCoat;
        _LookName        = n"StudioLacquer";

        FCk_Usf_ParamDesc TintColor;
        TintColor._Name = n"TintColor";
        TintColor._Type = ECk_Usf_ParamType::Vector;
        TintColor._DefaultVector = FLinearColor(0.16, 0.009, 0.027, 1.0);
        TintColor._Group = n"Body";
        TintColor._SortPriority = 0;
        _Parameters.Add(TintColor);

        FCk_Usf_ParamDesc CoatBlur;
        CoatBlur._Name = n"CoatBlur";
        CoatBlur._Type = ECk_Usf_ParamType::Scalar;
        CoatBlur._DefaultScalar = 0.09;
        CoatBlur._Group = n"Finish";
        CoatBlur._SortPriority = 10;
        _Parameters.Add(CoatBlur);

        FCk_Usf_ParamDesc DetailScale;
        DetailScale._Name = n"DetailScale";
        DetailScale._Type = ECk_Usf_ParamType::Scalar;
        DetailScale._DefaultScalar = 3.0;
        DetailScale._Group = n"Detail";
        DetailScale._SortPriority = 20;
        _Parameters.Add(DetailScale);

        FCk_Usf_ParamDesc FlakeAmount;
        FlakeAmount._Name = n"FlakeAmount";
        FlakeAmount._Type = ECk_Usf_ParamType::Scalar;
        FlakeAmount._DefaultScalar = 0.025;
        FlakeAmount._Group = n"Detail";
        FlakeAmount._SortPriority = 30;
        _Parameters.Add(FlakeAmount);
    }

    asset StudioWax of UCkUsf_LookDefinition
    {
        _UshIncludePath  = "/CkUsf/Looks/StudioWax.ush";
        _UshFunctionName = n"CkUsf_Look_StudioWax";
        _Domain          = ECk_Usf_Domain::SurfaceLit;
        _ShadingModel    = ECk_Usf_ShadingModel::Subsurface;
        _LookName        = n"StudioWax";

        FCk_Usf_ParamDesc TintColor;
        TintColor._Name = n"TintColor";
        TintColor._Type = ECk_Usf_ParamType::Vector;
        TintColor._DefaultVector = FLinearColor(0.55, 0.70, 0.41, 1.0);
        TintColor._Group = n"Body";
        TintColor._SortPriority = 0;
        _Parameters.Add(TintColor);

        FCk_Usf_ParamDesc ScatterTint;
        ScatterTint._Name = n"ScatterTint";
        ScatterTint._Type = ECk_Usf_ParamType::Vector;
        ScatterTint._DefaultVector = FLinearColor(0.35, 0.62, 0.18, 1.0);
        ScatterTint._Group = n"Scatter";
        ScatterTint._SortPriority = 10;
        _Parameters.Add(ScatterTint);

        FCk_Usf_ParamDesc ScatterWeight;
        ScatterWeight._Name = n"ScatterWeight";
        ScatterWeight._Type = ECk_Usf_ParamType::Scalar;
        ScatterWeight._DefaultScalar = 0.60;
        ScatterWeight._Group = n"Scatter";
        ScatterWeight._SortPriority = 20;
        _Parameters.Add(ScatterWeight);

        FCk_Usf_ParamDesc FinishBlur;
        FinishBlur._Name = n"FinishBlur";
        FinishBlur._Type = ECk_Usf_ParamType::Scalar;
        FinishBlur._DefaultScalar = 0.42;
        FinishBlur._Group = n"Finish";
        FinishBlur._SortPriority = 30;
        _Parameters.Add(FinishBlur);

        FCk_Usf_ParamDesc DetailScale;
        DetailScale._Name = n"DetailScale";
        DetailScale._Type = ECk_Usf_ParamType::Scalar;
        DetailScale._DefaultScalar = 0.20;
        DetailScale._Group = n"Detail";
        DetailScale._SortPriority = 40;
        _Parameters.Add(DetailScale);
    }

}
