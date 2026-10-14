#include "CkProceduralAnimation/Limb/CkProceduralLimb_Utils.h"

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Fragment.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_ProceduralLimb_UE, FCk_Handle_ProceduralLimb, ck::FFragment_ProceduralLimb);

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralLimb_UE::
    Get_Id(
    const FCk_Handle_ProceduralLimb& InLimb)
    -> FName
{
    return ck::IsValid(InLimb) && InLimb.Has<ck::FFragment_ProceduralLimb>()
        ? InLimb.Get<ck::FFragment_ProceduralLimb>().Get_Id() : NAME_None;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralLimb_UE::
    Get_Layout(
    const FCk_Handle_ProceduralLimb& InLimb)
    -> FCk_ProceduralLimb_Layout
{
    auto Layout = FCk_ProceduralLimb_Layout{};
    Try_GetLayout(InLimb, Layout);
    return Layout;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralLimb_UE::
    Try_GetLayout(
        const FCk_Handle_ProceduralLimb& InLimb,
        FCk_ProceduralLimb_Layout& OutLayout)
    -> bool
{
    OutLayout = {};
    if (ck::Is_NOT_Valid(InLimb) || NOT InLimb.Has<ck::FFragment_ProceduralLimb>())
    { return false; }

    const auto Owner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InLimb);
    if (ck::Is_NOT_Valid(Owner) || NOT Owner.Has<ck::FFragment_ProceduralArticulation_Params>())
    { return false; }

    const auto Index = InLimb.Get<ck::FFragment_ProceduralLimb>().Get_Index();
    if (Owner.Has<ck::FFragment_ProceduralArticulation_LegacyCandidate>()
        && Owner.Get<ck::FFragment_ProceduralArticulation_LegacyCandidate>().Get_LegacyWalker())
    {
        if (NOT Owner.Has<ck::FFragment_ProceduralArticulation_Bindings>())
        { return false; }
        const auto& BindingMap = Owner.Get<ck::FFragment_ProceduralArticulation_Bindings>();
        if (NOT BindingMap.Get_Bindings().IsValidIndex(Index)
            || NOT BindingMap.Get_Limbs().IsValidIndex(Index)
            || BindingMap.Get_Limbs()[Index].ConvertToHandle() != InLimb.ConvertToHandle())
        { return false; }
        if (NOT InLimb.Has<ck::FFragment_ProceduralLeg_Params>()
            || NOT InLimb.Has<ck::FFragment_ProceduralRig_Params>())
        { return false; }
        const auto& Leg = InLimb.Get<ck::FFragment_ProceduralLeg_Params>();
        const auto& Rig = InLimb.Get<ck::FFragment_ProceduralRig_Params>();
        if (Leg.Get_Id() != InLimb.Get<ck::FFragment_ProceduralLimb>().Get_Id()
            || BindingMap.Get_Bindings()[Index].Get_LimbId() != Leg.Get_Id()
            || NOT ck::IsValid(Leg)
            || Rig.Get_Segments().Num() != Leg.Get_Chain().Get_SegmentLengths().Num()
            || (Rig.Get_Solver() != ECk_ProceduralRig_ChainSolver::Auto
                && Rig.Get_Solver() != ECk_ProceduralRig_ChainSolver::Fabrik
                && Rig.Get_Solver() != ECk_ProceduralRig_ChainSolver::Curve)
            || (Rig.Get_Clearance() != ECk_ProceduralRig_Clearance::None
                && Rig.Get_Clearance() != ECk_ProceduralRig_Clearance::Swivel)
            || (Rig.Get_Segments().Num() == 1 && Rig.Get_Solver() != ECk_ProceduralRig_ChainSolver::Auto)
            || (NOT Rig.Get_SegmentClearanceRadii().IsEmpty()
                && Rig.Get_SegmentClearanceRadii().Num() != Rig.Get_Segments().Num()))
        { return false; }
        for (const auto Radius : Rig.Get_SegmentClearanceRadii())
        {
            if (NOT FMath::IsFinite(Radius) || Radius < 0.0f)
            { return false; }
        }
        const auto Hip = Leg.Get_Placement().Get_HipLocal();
        auto Layout = FCk_ProceduralLimb_Layout{Leg.Get_Id(), FTransform{Hip},
            Leg.Get_Chain().Get_SegmentLengths(),
            FTransform{Leg.Get_Placement().Get_RestFootLocal() - Hip},
            Leg.Get_Chain().Get_PoleLocal() - Hip};
        Layout.Set_Solver(Rig.Get_Solver());
        Layout.Set_Clearance(Rig.Get_Clearance());
        Layout.Set_SegmentClearanceRadii(Rig.Get_SegmentClearanceRadii());
        Layout.Set_Role(ECk_ProceduralLimb_Role::Locomotor);
        OutLayout = MoveTemp(Layout);
        return true;
    }
    const auto& Layouts = Owner.Get<ck::FFragment_ProceduralArticulation_Params>().Get_Layout().Get_Limbs();
    if (NOT Layouts.IsValidIndex(Index)
        || Layouts[Index].Get_Id() != InLimb.Get<ck::FFragment_ProceduralLimb>().Get_Id())
    { return false; }
    OutLayout = Layouts[Index];
    return true;
}
