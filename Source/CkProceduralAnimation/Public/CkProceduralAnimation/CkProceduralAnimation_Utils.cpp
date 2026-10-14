#include "CkProceduralAnimation/CkProceduralAnimation_Utils.h"

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Utils.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Fragment.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Utils.h"

#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Utils.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_animation_utils
{
    auto
        Get_IsBodyAdmissible(
            const FCk_Handle_Transform& InBody)
        -> bool
    {
        return ck::IsValid(InBody)
            && NOT InBody.Has<ck::FTag_DestroyEntity_Initiate>()
            && UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InBody)
            && NOT InBody.Has<ck::FFragment_ProceduralArticulation_Params>()
            && NOT UCk_Utils_ProceduralGait_UE::Has(InBody)
            && UCk_Utils_ProceduralLeg_UE::Get_Legs(InBody).IsEmpty();
    }

    auto
        Get_AreAssetsAdmissible(
            const UCk_ProceduralRig_Data* InRig,
            const UCk_ProceduralGait_Data* InGait)
        -> bool
    {
        if (ck::Is_NOT_Valid(InRig) || ck::Is_NOT_Valid(InGait))
        { return false; }

        const auto& Legs = InRig->Get_Legs();
        auto Ids = TSet<FName>{};
        for (const auto& Leg : Legs)
        {
            if (ck::Is_NOT_Valid(Leg) || Ids.Contains(Leg.Get_Id()))
            { return false; }

            Ids.Add(Leg.Get_Id());
        }

        return Legs.Num() >= 2 && Legs.Num() <= 64
            && ck::IsValid(InGait->Get_Timing())
            && ck::IsValid(InGait->Get_Step())
            && ck::IsValid(InGait->Get_Probe())
            && ck::IsValid(InGait->Get_Foothold())
            && InGait->Get_Timing().Get_MaxSimultaneousSwings() <= Legs.Num()
            && ck::algo::AllOf(Legs, [&](const FCk_ProceduralLeg_Spec& InLeg) -> bool
            {
                return ck_procedural_gait_utils::Get_IsRestWithinReach(InLeg.Get_Placement(), InLeg.Get_Chain(), InGait->Get_Step());
            });
    }

    auto
        Get_AreChainsAdmissible(
            const FCk_Handle_Transform& InBody,
            const UCk_ProceduralRig_Data& InRig,
            const TArray<FCk_ProceduralWalker_LegChain>& InChains)
        -> bool
    {
        auto BoundLegIds = TSet<FName>{};
        auto BoundParts = TSet<FCk_Handle>{};
        const auto IsPartAdmissible = [&](const FCk_Handle_Transform& InPart) -> bool
        {
            const auto Admissible = ck::IsValid(InPart)
                && NOT InPart.Has<ck::FTag_DestroyEntity_Initiate>()
                && InPart != InBody
                && UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InPart) == InBody.ConvertToHandle()
                && NOT BoundParts.Contains(InPart);

            BoundParts.Add(InPart);
            return Admissible;
        };

        for (const auto& Chain : InChains)
        {
            const auto LegIndex = InRig.Get_Legs().IndexOfByPredicate([&](const FCk_ProceduralLeg_Spec& InLeg) -> bool
            {
                return InLeg.Get_Id() == Chain.Get_LegId();
            });

            if (LegIndex == INDEX_NONE || BoundLegIds.Contains(Chain.Get_LegId()) || ck::Is_NOT_Valid(Chain.Get_Rig()))
            { return false; }

            BoundLegIds.Add(Chain.Get_LegId());

            const auto& Rig = Chain.Get_Rig();
            if (Rig.Get_Segments().Num() != InRig.Get_Legs()[LegIndex].Get_Chain().Get_SegmentLengths().Num())
            { return false; }

            const auto HasFoot = Rig.Get_Foot() != FCk_Handle_Transform{};
            const auto PartsAdmissible = ck::algo::AllOf(Rig.Get_Segments(), IsPartAdmissible)
                && (NOT HasFoot || IsPartAdmissible(Rig.Get_Foot()));

            if (NOT PartsAdmissible)
            { return false; }
        }

        return true;
    }

    auto
        Get_IsBodyPoseAdmissible(
            const FCk_Handle_Transform& InBody,
            const FCk_ProceduralBodyPose_Spec& InSpec,
            const TArray<FCk_ProceduralWalker_LegChain>& InChains)
        -> bool
    {
        const auto& Presentation = InSpec.Get_Presentation();
        const auto IsRigPart = ck::algo::AnyOf(InChains, [&](const FCk_ProceduralWalker_LegChain& InChain) -> bool
        {
            return InChain.Get_Rig().Get_Segments().Contains(Presentation) || InChain.Get_Rig().Get_Foot() == Presentation;
        });

        return ck::IsValid(InSpec)
            && ck::IsValid(Presentation)
            && NOT Presentation.Has<ck::FTag_DestroyEntity_Initiate>()
            && Presentation != InBody
            && UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Presentation) == InBody.ConvertToHandle()
            && NOT IsRigPart;
    }

    auto
        Get_IsActionPlayerAdmissible(
            const FCk_ProceduralActionPlayer_Spec& InSpec)
        -> bool
    {
        auto InvalidField = FName{};
        return ck::procedural_action::ValidateTuning(InSpec.Get_Tuning(), InvalidField)
            && ck::procedural_action::ValidateCapacity(InSpec.Get_Capacity(), InvalidField)
            && FCk_ProceduralLimbControl_Control::Get_IsLocomotionValid(InSpec.Get_Locomotion());
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralAnimation_UE::
    Add_Walker(
        FCk_Handle_Transform& InBody,
        const FCk_ProceduralWalker_Spec& InSpec)
    -> FCk_ProceduralWalker
{
    const auto* RigData = InSpec.Get_Rig().Get();
    const auto* GaitData = InSpec.Get_Gait().Get();
    const auto& Chains = InSpec.Get_Chains();
    const auto WithBodyPose = InSpec.Get_BodyPose() == ECk_EnableDisable::Enable;
    const auto WithActionPlayer = InSpec.Get_ActionPlayer() == ECk_EnableDisable::Enable;
    const auto Valid = ck_procedural_animation_utils::Get_IsBodyAdmissible(InBody)
        && ck_procedural_animation_utils::Get_AreAssetsAdmissible(RigData, GaitData)
        && ck_procedural_animation_utils::Get_AreChainsAdmissible(InBody, *RigData, Chains)
        && (NOT WithBodyPose
            || ck_procedural_animation_utils::Get_IsBodyPoseAdmissible(InBody, InSpec.Get_BodyPoseSpec(), Chains))
        && (NOT WithActionPlayer
            || ck_procedural_animation_utils::Get_IsActionPlayerAdmissible(InSpec.Get_ActionPlayerSpec()));
    CK_ENSURE_IF_NOT(Valid,
        TEXT("Procedural animation Add_Walker rejected body [{}]. It needs a live transform body that can own children with "
             "no gait and no legs; a rig layout of 2..64 valid legs with unique Ids; a gait preset with valid timing, step, "
             "probe and foothold settings whose MaxSimultaneousSwings fits the leg count and whose TargetReachFraction of "
             "each leg's chain length reaches its rest foot; chains that each name a distinct rig leg, match its segment "
             "count and bind unique live transform parts that are direct lifetime children of the body; when enabled, a "
             "valid body pose spec whose presentation is a live direct child of the body and not a rig part, and an action "
             "player spec with valid tuning, capacity and locomotion."),
        InBody)
    { return {}; }

    const auto Legs = ck::algo::Transform<TArray<FCk_Handle_ProceduralLeg>>(RigData->Get_Legs(),
    [&](const FCk_ProceduralLeg_Spec& InLegParams)
    {
        return UCk_Utils_ProceduralLeg_UE::Create(InBody, InLegParams);
    });

    auto Gait = UCk_Utils_ProceduralGait_UE::Add(Legs, GaitData);

    auto Rigs = TArray<FCk_Handle_ProceduralRig>{};
    Rigs.Reserve(Chains.Num());
    for (const auto& Chain : Chains)
    {
        auto Leg = UCk_Utils_ProceduralLeg_UE::TryGet_Leg(InBody, Chain.Get_LegId());
        Rigs.Add(UCk_Utils_ProceduralRig_UE::Add(Leg, Chain.Get_Rig()));
    }

    // A legacy walker may have fewer visual chains than gait legs, including none. Only rigged legs have controllable
    // articulation parts. Legacy geometry remains owned by Leg/Rig Params; the binding map never mirrors chain lengths.
    auto Bindings = TArray<FCk_ProceduralLimb_Binding>{};
    auto Limbs = TArray<FCk_Handle_ProceduralLimb>{};
    Bindings.Reserve(Chains.Num());
    Limbs.Reserve(Chains.Num());
    for (auto Index = 0; Index < RigData->Get_Legs().Num(); ++Index)
    {
        const auto Id = RigData->Get_Legs()[Index].Get_Id();
        auto Leg = UCk_Utils_ProceduralLeg_UE::TryGet_Leg(InBody, Id);
        if (NOT Leg.Has<ck::FFragment_ProceduralRig_Params>())
        { continue; }
        const auto& Rig = Leg.Get<ck::FFragment_ProceduralRig_Params>();
        Leg.Add<ck::FFragment_ProceduralLimb>(Id, Bindings.Num());
        Limbs.Add(UCk_Utils_ProceduralLimb_UE::CastChecked(Leg));
        auto Binding = FCk_ProceduralLimb_Binding{Id, Rig.Get_Segments()};
        Binding.Set_Terminal(Rig.Get_Foot());
        Bindings.Add(MoveTemp(Binding));
    }
    auto EmptyClaims = TArray<FCk_ArticulationControlToken>{};
    EmptyClaims.SetNum(Limbs.Num());
    InBody.Add<ck::FFragment_ProceduralArticulation_Params>(
        FCk_ProceduralArticulation_Layout{TArray<FCk_ProceduralLimb_Layout>{}},
        FCk_ProceduralArticulation_ClearanceSettings{});
    InBody.Add<ck::FFragment_ProceduralArticulation_Bindings>(MoveTemp(Bindings), MoveTemp(Limbs),
        FCk_Handle_Transform{});
    InBody.Add<ck::FFragment_ProceduralArticulation_Claims>(MoveTemp(EmptyClaims));
    InBody.Add<ck::FFragment_ProceduralArticulation_Pose>();
    InBody.Add<ck::FFragment_ProceduralArticulation_LegacyCandidate>()._LegacyWalker = true;
    InBody.Add<ck::FFragment_ProceduralArticulation_ControlCandidate>();
    InBody.Add<ck::FFragment_ProceduralArticulation_Health>();
    auto Articulation = UCk_Utils_ProceduralArticulation_UE::CastChecked(InBody);

    auto Walker = FCk_ProceduralWalker{Gait, Legs, Articulation};
    Walker.Set_Rigs(MoveTemp(Rigs));
    if (WithBodyPose)
    { Walker.Set_BodyPose(UCk_Utils_ProceduralBodyPose_UE::Add(Gait, InSpec.Get_BodyPoseSpec())); }
    if (WithActionPlayer)
    { Walker.Set_ActionPlayer(UCk_Utils_ProceduralActionPlayer_UE::Add(Articulation, InSpec.Get_ActionPlayerSpec())); }

    return Walker;
}

// --------------------------------------------------------------------------------------------------------------------
