#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment.h"

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Utils.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Fragment.h"
#include "CkProceduralAnimation/Limb/CkProceduralLimb_Utils.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkJolt/Body/CkJoltBody_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_ProceduralArticulation_UE, FCk_Handle_ProceduralArticulation,
    ck::FFragment_ProceduralArticulation_Params);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_articulation_utils
{
    constexpr auto MaxPendingRecoveryRequests = 32;

    auto
        Get_LimbIdsInState(
            const FCk_Handle_ProceduralArticulation& InArticulation,
            ECk_ProceduralLimbControl_State InState)
        -> TArray<FName>
    {
        if (ck::Is_NOT_Valid(InArticulation) || NOT InArticulation.Has<ck::FFragment_ProceduralArticulation_Bindings>())
        { return {}; }
        auto LimbIds = TArray<FName>{};
        for (const auto& Binding : InArticulation.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Bindings())
        {
            if (FCk_ProceduralLimbControl_Control::Get_Observation(InArticulation, Binding.Get_LimbId()).Get_State()
                == InState)
            { LimbIds.Add(Binding.Get_LimbId()); }
        }
        return LimbIds;
    }

    auto
        Get_IsRigid(
        const FTransform& InTransform)
        -> bool
    {
        return NOT InTransform.ContainsNaN()
            && InTransform.GetScale3D().Equals(FVector::OneVector, 1.0e-6)
            && FMath::Abs(InTransform.GetRotation().SizeSquared() - 1.0) <= 1.0e-4;
    }

    auto
        Get_IsSupportedSolver(
        ECk_ProceduralRig_ChainSolver InSolver)
        -> bool
    {
        switch (InSolver)
        {
            case ECk_ProceduralRig_ChainSolver::Auto:
            case ECk_ProceduralRig_ChainSolver::Fabrik:
            case ECk_ProceduralRig_ChainSolver::Curve:
            { return true; }
        }
        return false;
    }

    auto
        Get_IsPartAdmissible(
        const FCk_Handle_Transform& InPart,
        const FCk_Handle_Transform& InRoot,
        TSet<FCk_Handle>& InOutSeen)
        -> bool
    {
        if (ck::Is_NOT_Valid(InPart) || InPart.Has<ck::FTag_DestroyEntity_Initiate>()
            || InPart == InRoot || InOutSeen.Contains(InPart)
            || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InPart) != InRoot.ConvertToHandle()
            || NOT UCk_Utils_Transform_UE::Has(InPart)
            || InPart.Has<ck::FFragment_Transform_MeshSocket>()
            || InPart.Has<ck::FFragment_Transform_RootComponent>()
            || InPart.Has<ck::FFragment_SceneNode>()
            || InPart.Has<ck::FTag_JoltBody_MotionType_Static>()
            || InPart.Has<ck::FTag_JoltBody_MotionType_Kinematic>()
            || InPart.Has<ck::FTag_JoltBody_MotionType_Dynamic>())
        { return false; }

        // Bound-part scale is visual and may be nonuniform or mirrored. Reject unusable transforms
        // before creating any limb; publication preserves this scale rather than repairing it.
        const auto Transform = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(InPart);
        if (Transform.ContainsNaN()
            || FMath::Abs(Transform.GetRotation().SizeSquared() - 1.0) > 1.0e-4
            || Transform.GetScale3D().GetAbsMin() <= UE_SMALL_NUMBER)
        { return false; }

        InOutSeen.Add(InPart);
        return true;
    }

    auto
        Get_IsLayoutAdmissible(
        const FCk_ProceduralArticulation_Spec& InSpec,
        const FCk_Handle_Transform& InRoot)
        -> bool
    {
        const auto& Layout = InSpec.Get_Layout();
        const auto& Limbs = Layout.Get_Limbs();
        const auto& Bindings = InSpec.Get_Bindings();
        const auto& ClearanceSettings = InSpec.Get_ClearanceSettings();
        if (Layout.Get_CompatibilityVersion() != 1 || Limbs.Num() > 64 || Bindings.Num() != Limbs.Num()
            || Layout.Get_BodyEnvelopeMin().ContainsNaN() || Layout.Get_BodyEnvelopeMax().ContainsNaN()
            || Layout.Get_BodyEnvelopeMin().X > Layout.Get_BodyEnvelopeMax().X
            || Layout.Get_BodyEnvelopeMin().Y > Layout.Get_BodyEnvelopeMax().Y
            || Layout.Get_BodyEnvelopeMin().Z > Layout.Get_BodyEnvelopeMax().Z)
        { return false; }

        auto Ids = TSet<FName>{};
        auto Parts = TSet<FCk_Handle>{};
        auto NeedsClearanceWorld = false;
        const auto HasPresentation = InSpec.Get_Presentation() != FCk_Handle_Transform{};
        if (HasPresentation && (NOT Get_IsPartAdmissible(InSpec.Get_Presentation(), InRoot, Parts)
            || NOT Get_IsRigid(UCk_Utils_Transform_UE::Get_EntityCurrentTransform(InSpec.Get_Presentation()))))
        { return false; }

        for (auto Index = 0; Index < Limbs.Num(); ++Index)
        {
            const auto& Limb = Limbs[Index];
            const auto& Binding = Bindings[Index];
            const auto& Lengths = Limb.Get_SegmentLengths();
            const auto& Radii = Limb.Get_SegmentClearanceRadii();
            if (Limb.Get_Id().IsNone() || Ids.Contains(Limb.Get_Id()) || Binding.Get_LimbId() != Limb.Get_Id()
                || Limb.Get_Role() != ECk_ProceduralLimb_Role::Dedicated
                || NOT Get_IsRigid(Limb.Get_AttachmentLocal()) || NOT Get_IsRigid(Limb.Get_RestEndpointLocal())
                || Limb.Get_BendLocal().ContainsNaN() || Limb.Get_BendLocal().IsNearlyZero()
                || Limb.Get_BendLocal().GetSafeNormal().IsNearlyZero()
                || Lengths.Num() < 1 || Lengths.Num() > 8 || Binding.Get_Segments().Num() != Lengths.Num()
                || NOT Get_IsSupportedSolver(Limb.Get_Solver())
                || (Limb.Get_Clearance() != ECk_ProceduralRig_Clearance::None
                    && Limb.Get_Clearance() != ECk_ProceduralRig_Clearance::Swivel)
                || (Lengths.Num() == 1 && Limb.Get_Solver() != ECk_ProceduralRig_ChainSolver::Auto)
                || (NOT Radii.IsEmpty() && Radii.Num() != Lengths.Num()))
            { return false; }

            NeedsClearanceWorld |= Limb.Get_Clearance() == ECk_ProceduralRig_Clearance::Swivel;

            Ids.Add(Limb.Get_Id());
            for (const auto Length : Lengths)
            {
                if (NOT FMath::IsFinite(Length) || Length <= 0.0f)
                { return false; }
            }
            for (const auto Radius : Radii)
            {
                if (NOT FMath::IsFinite(Radius) || Radius < 0.0f)
                { return false; }
            }
            for (const auto& Part : Binding.Get_Segments())
            {
                if (NOT Get_IsPartAdmissible(Part, InRoot, Parts))
                { return false; }
            }
            if (Binding.Get_Terminal() != FCk_Handle_Transform{}
                && NOT Get_IsPartAdmissible(Binding.Get_Terminal(), InRoot, Parts))
            { return false; }
        }
        if (NeedsClearanceWorld)
        {
            const auto& Filter = ClearanceSettings.Get_Filter();
            if (ClearanceSettings.Get_Query() != ECk_ProceduralArticulation_ClearanceQuery::JoltFilter
                || ck::Is_NOT_Valid(UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InRoot))
                || Filter.Get_Channel().GetValue() >= ECC_MAX
                || (Filter.Get_MinResponse() != ECk_Jolt_PairInteraction::Overlap
                    && Filter.Get_MinResponse() != ECk_Jolt_PairInteraction::Block))
            { return false; }
        }
        else if (ClearanceSettings.Get_Query() != ECk_ProceduralArticulation_ClearanceQuery::None
            && ClearanceSettings.Get_Query() != ECk_ProceduralArticulation_ClearanceQuery::JoltFilter)
        { return false; }
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralArticulation_UE::
    Add(
    FCk_Handle_Transform& InRoot,
    const FCk_ProceduralArticulation_Spec& InSpec)
    -> FCk_Handle_ProceduralArticulation
{
    const auto RootValid = ck::IsValid(InRoot) && NOT InRoot.Has<ck::FTag_DestroyEntity_Initiate>()
        && UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InRoot) && NOT Has(InRoot)
        && NOT UCk_Utils_ProceduralGait_UE::Has(InRoot)
        && NOT UCk_Utils_ProceduralBodyPose_UE::Has(InRoot)
        && UCk_Utils_ProceduralLeg_UE::Get_Legs(InRoot).IsEmpty()
        && ck_procedural_articulation_utils::Get_IsRigid(
            UCk_Utils_Transform_UE::Get_EntityCurrentTransform(InRoot));
    const auto Valid = RootValid && ck_procedural_articulation_utils::Get_IsLayoutAdmissible(InSpec, InRoot);
    if (NOT Valid)
    {
        CK_ENSURE_IF_NOT(false,
            TEXT("Procedural articulation Add rejected root [{}]: require a live rigid root/presentation, sealed 0..64 dedicated limb layout, "
                 "1..8 positive links, finite normalized direct-child part transforms with nonzero visual scale, "
                 "unique bindings and no competing gait, leg or part writer."), InRoot)
        { }
        return {};
    }

    auto Created = TArray<FCk_Handle_ProceduralLimb>{};
    const auto& Layouts = InSpec.Get_Layout().Get_Limbs();
    Created.Reserve(Layouts.Num());
    for (auto Index = 0; Index < Layouts.Num(); ++Index)
    {
        const auto& Id = Layouts[Index].Get_Id();
        auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InRoot, [&](FCk_Handle InNewEntity)
        {
#if NOT CK_DISABLE_ECS_HANDLE_DEBUGGING
            UCk_Utils_Handle_UE::Set_DebugName(InNewEntity, Id);
#endif
            InNewEntity.Add<ck::FFragment_ProceduralLimb>(Id, Index);
        });
        if (ck::Is_NOT_Valid(NewEntity))
        {
            for (auto& Child : Created)
            {
                auto Handle = Child.ConvertToHandle();
                UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
            }
            return {};
        }
        Created.Add(UCk_Utils_ProceduralLimb_UE::CastChecked(NewEntity));
    }

    InRoot.Add<ck::FFragment_ProceduralArticulation_Params>(InSpec.Get_Layout(), InSpec.Get_ClearanceSettings());
    InRoot.Add<ck::FFragment_ProceduralArticulation_Bindings>(InSpec.Get_Bindings(), Created, InSpec.Get_Presentation());
    InRoot.Add<ck::FFragment_ProceduralArticulation_Pose>();
    auto EmptyClaims = TArray<FCk_ArticulationControlToken>{};
    EmptyClaims.SetNum(Layouts.Num());
    InRoot.Add<ck::FFragment_ProceduralArticulation_Claims>(MoveTemp(EmptyClaims));
    InRoot.Add<ck::FFragment_ProceduralArticulation_Health>();
    return CastChecked(InRoot);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_Health(
    const FCk_Handle_ProceduralArticulation& InArticulation)
    -> ECk_ProceduralArticulation_Health
{
    return ck::IsValid(InArticulation) && InArticulation.Has<ck::FFragment_ProceduralArticulation_Health>()
        ? InArticulation.Get<ck::FFragment_ProceduralArticulation_Health>().Get_State()
        : ECk_ProceduralArticulation_Health::Faulted;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_Status(
    const FCk_Handle_ProceduralArticulation& InArticulation)
    -> ECk_ProceduralAnimation_Status
{
    if (ck::Is_NOT_Valid(InArticulation)
        || Get_Health(InArticulation) == ECk_ProceduralArticulation_Health::Faulted)
    { return ECk_ProceduralAnimation_Status::Failed; }
    return Get_SettledRevision(InArticulation) > 0
        ? ECk_ProceduralAnimation_Status::Ready
        : ECk_ProceduralAnimation_Status::PendingSetup;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_Fault(
    const FCk_Handle_ProceduralArticulation& InArticulation)
    -> ECk_ProceduralArticulation_Fault
{
    return ck::IsValid(InArticulation) && InArticulation.Has<ck::FFragment_ProceduralArticulation_Health>()
        ? InArticulation.Get<ck::FFragment_ProceduralArticulation_Health>().Get_Fault()
        : ECk_ProceduralArticulation_Fault::InvalidBinding;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_SettledRevision(
    const FCk_Handle_ProceduralArticulation& InArticulation)
    -> int64
{
    return ck::IsValid(InArticulation) && InArticulation.Has<ck::FFragment_ProceduralArticulation_Pose>()
        ? static_cast<int64>(InArticulation.Get<ck::FFragment_ProceduralArticulation_Pose>().Get_SettledRevision()) : 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_Limb(
    const FCk_Handle_ProceduralArticulation& InArticulation,
    FName InId)
    -> FCk_Handle_ProceduralLimb
{
    if (ck::Is_NOT_Valid(InArticulation) || NOT InArticulation.Has<ck::FFragment_ProceduralArticulation_Bindings>())
    { return {}; }
    const auto& Bindings = InArticulation.Get<ck::FFragment_ProceduralArticulation_Bindings>();
    for (auto Index = 0; Index < Bindings.Get_Bindings().Num(); ++Index)
    {
        if (Bindings.Get_Bindings()[Index].Get_LimbId() == InId)
        {
            return Bindings.Get_Limbs().IsValidIndex(Index)
                ? Bindings.Get_Limbs()[Index] : FCk_Handle_ProceduralLimb{};
        }
    }
    return {};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_Limbs(
    const FCk_Handle_ProceduralArticulation& InArticulation)
    -> TArray<FCk_Handle_ProceduralLimb>
{
    return ck::IsValid(InArticulation) && InArticulation.Has<ck::FFragment_ProceduralArticulation_Bindings>()
        ? InArticulation.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Limbs()
        : TArray<FCk_Handle_ProceduralLimb>{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_LimbControl(
        const FCk_Handle_ProceduralArticulation& InArticulation,
        FName InLimbId)
    -> FCk_ProceduralLimbControl_Observation
{
    return FCk_ProceduralLimbControl_Control::Get_Observation(InArticulation, InLimbId);
}

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_IsAnyLimbControlled(
        const FCk_Handle_ProceduralArticulation& InArticulation)
    -> bool
{
    return NOT ck_procedural_articulation_utils::Get_LimbIdsInState(InArticulation,
        ECk_ProceduralLimbControl_State::Controlled).IsEmpty();
}

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_IsAnyLimbReturning(
        const FCk_Handle_ProceduralArticulation& InArticulation)
    -> bool
{
    return NOT ck_procedural_articulation_utils::Get_LimbIdsInState(InArticulation,
        ECk_ProceduralLimbControl_State::Returning).IsEmpty();
}

auto
    UCk_Utils_ProceduralArticulation_UE::
    Get_UnplantedLimbIds(
        const FCk_Handle_ProceduralArticulation& InArticulation)
    -> TArray<FName>
{
    return ck_procedural_articulation_utils::Get_LimbIdsInState(InArticulation,
        ECk_ProceduralLimbControl_State::Unplanted);
}

auto
    UCk_Utils_ProceduralArticulation_UE::
    Request_RecoverUnplantedLimbs(
        FCk_Handle_ProceduralArticulation& InArticulation,
        const FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs& InRequest,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_ProceduralArticulation
{
    auto Request = InRequest;
    if (InDelegate.IsBound())
    { Request.Set_CompletionDelegate(InDelegate); }
    const auto Valid = ck::IsValid(InArticulation) && Has(InArticulation)
        && NOT InArticulation.Has<ck::FTag_DestroyEntity_Initiate>()
        && InArticulation.Has<ck::FFragment_ProceduralLimbControl>()
        && (NOT InArticulation.Has<ck::FFragment_ProceduralLimbControl_Requests>()
            || InArticulation.Get<ck::FFragment_ProceduralLimbControl_Requests>()._Requests.Num()
                < ck_procedural_articulation_utils::MaxPendingRecoveryRequests);
    CK_ENSURE_IF_NOT(Valid,
        TEXT("ProceduralArticulation RecoverUnplantedLimbs rejected articulation [{}]: it needs live limb control "
            "from an action player and fewer than [{}] pending recovery requests."),
        InArticulation, ck_procedural_articulation_utils::MaxPendingRecoveryRequests)
    {
        Request.TryFireCompletion(InArticulation, ECk_Request_OperationResult::Failed_NotEnqueued);
        return InArticulation;
    }

    InArticulation.AddOrGet<ck::FFragment_ProceduralLimbControl_Requests>()._Requests.Emplace(MoveTemp(Request));
    return InArticulation;
}
