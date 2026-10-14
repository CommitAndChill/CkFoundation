#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Processor.h"

#include "CkProceduralAnimation/Core/CkProceduralLimbSolve.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"

#include "CkJolt/Body/CkJoltBody_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralArticulation_Update);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralArticulation_Observe);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_articulation_processor
{
    auto
        Get_IsBoundPartLive(
        const FCk_Handle_Transform& InPart,
        const FCk_Handle& InRoot)
        -> bool
    {
        return ck::IsValid(InPart) && NOT InPart.Has<ck::FTag_DestroyEntity_Initiate>()
            && UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InPart) == InRoot
            && UCk_Utils_Transform_UE::Has(InPart)
            && NOT InPart.Has<ck::FFragment_Transform_MeshSocket>()
            && NOT InPart.Has<ck::FFragment_Transform_RootComponent>()
            && NOT InPart.Has<ck::FFragment_SceneNode>()
            && NOT InPart.Has<ck::FTag_JoltBody_MotionType_Static>()
            && NOT InPart.Has<ck::FTag_JoltBody_MotionType_Kinematic>()
            && NOT InPart.Has<ck::FTag_JoltBody_MotionType_Dynamic>();
    }

    auto
        Get_IsSamePose(
        const FTransform& InExpected,
        const FTransform& InObserved)
        -> bool
    {
        return InExpected.GetLocation().Equals(InObserved.GetLocation(), 0.01)
            && InExpected.GetRotation().Equals(InObserved.GetRotation(), 1.0e-3)
            && InObserved.GetScale3D().Equals(InExpected.GetScale3D());
    }

    auto
        Get_ObservedSubset(
            const FCk_ProceduralArticulation_Pose& InPose,
            const FCk_ArticulationChannelSet& InChannels)
        -> FCk_ProceduralArticulation_Pose
    {
        auto Limbs = TArray<FCk_ProceduralArticulation_LimbPose>{};
        for (const auto& Id : InChannels.Get_LimbIds())
        {
            for (const auto& Limb : InPose.Get_Limbs())
            {
                if (Limb.Get_Id() == Id)
                { Limbs.Add(Limb); break; }
            }
        }
        return FCk_ProceduralArticulation_Pose{InPose.Get_RootFrame(), InPose.Get_BodyPresentation(),
            InChannels.Get_Body() && InPose.Get_HasBodyPresentation(), MoveTemp(Limbs)};
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_ProceduralArticulation_Update::
        ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        const FFragment_ProceduralArticulation_Params& InParams,
        const FFragment_ProceduralArticulation_Bindings& InBindings,
        FFragment_ProceduralArticulation_Pose& InPose,
        FFragment_ProceduralArticulation_Health& InHealth)
        -> void
    {
        if (InHealth._State == ECk_ProceduralArticulation_Health::Faulted)
        { return; }

        // A claim remains authoritative between accepted batches. Gait and rest candidates still
        // update unclaimed channels, but must not replace the last settled pose of a claimed one.
        const auto HoldClaimedChannels = [&](TArray<FCk_Handle_Transform>& InOutHandles,
            TArray<FTransform>& InOutPoses) -> bool
        {
            if (NOT InHandle.Has<FFragment_ProceduralArticulation_Claims>())
            { return true; }
            const auto& Claims = InHandle.Get<FFragment_ProceduralArticulation_Claims>();
            const auto& Bindings = InBindings.Get_Bindings();
            if (Claims._LimbOwners.Num() != Bindings.Num() || InOutHandles.Num() != InOutPoses.Num())
            { return false; }
            if (Claims._BodyOwner == FCk_ArticulationControlToken{}
                && NOT Claims._LimbOwners.ContainsByPredicate([](const FCk_ArticulationControlToken& InOwner)
                { return InOwner != FCk_ArticulationControlToken{}; }))
            { return true; }
            auto PredictedLimbs = InPose._PredictedSubjectPose.Get_Limbs();
            for (auto BindingIndex = 0; BindingIndex < Bindings.Num(); ++BindingIndex)
            {
                if (Claims._LimbOwners[BindingIndex] == FCk_ArticulationControlToken{})
                { continue; }
                const auto& Binding = Bindings[BindingIndex];
                const auto SettledIndex = InPose._SettledSubjectPose.Get_Limbs().IndexOfByPredicate(
                    [&](const FCk_ProceduralArticulation_LimbPose& InLimb)
                    { return InLimb.Get_Id() == Binding.Get_LimbId(); });
                const auto PredictedIndex = PredictedLimbs.IndexOfByPredicate(
                    [&](const FCk_ProceduralArticulation_LimbPose& InLimb)
                    { return InLimb.Get_Id() == Binding.Get_LimbId(); });
                if (SettledIndex == INDEX_NONE || PredictedIndex == INDEX_NONE)
                { return false; }
                const auto& Settled = InPose._SettledSubjectPose.Get_Limbs()[SettledIndex];
                if (Settled.Get_Segments().Num() != Binding.Get_Segments().Num())
                { return false; }
                for (auto PartIndex = 0; PartIndex < Binding.Get_Segments().Num(); ++PartIndex)
                {
                    const auto ExpectedIndex = InOutHandles.Find(Binding.Get_Segments()[PartIndex]);
                    if (ExpectedIndex == INDEX_NONE)
                    { return false; }
                    InOutPoses[ExpectedIndex] = Settled.Get_Segments()[PartIndex];
                }
                if (Binding.Get_Terminal() != FCk_Handle_Transform{})
                {
                    const auto ExpectedIndex = InOutHandles.Find(Binding.Get_Terminal());
                    if (ExpectedIndex == INDEX_NONE)
                    { return false; }
                    const auto VisualScale = InOutPoses[ExpectedIndex].GetScale3D();
                    InOutPoses[ExpectedIndex] = Settled.Get_AcceptedEndpoint();
                    InOutPoses[ExpectedIndex].SetScale3D(VisualScale);
                }
                PredictedLimbs[PredictedIndex] = Settled;
            }
            auto BodyPresentation = InPose._PredictedSubjectPose.Get_BodyPresentation();
            if (Claims._BodyOwner != FCk_ArticulationControlToken{})
            {
                if (NOT InPose._SettledSubjectPose.Get_HasBodyPresentation()
                    || NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(
                        InBindings.Get_Presentation(), InHandle.ConvertToHandle()))
                { return false; }
                BodyPresentation = InPose._SettledSubjectPose.Get_BodyPresentation();
                const auto ExpectedIndex = InOutHandles.Find(InBindings.Get_Presentation());
                if (ExpectedIndex == INDEX_NONE)
                {
                    InOutHandles.Add(InBindings.Get_Presentation());
                    InOutPoses.Add(BodyPresentation);
                }
                else
                { InOutPoses[ExpectedIndex] = BodyPresentation; }
            }
            const auto RootFrame = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(
                UCk_Utils_Transform_UE::CastChecked(InHandle.ConvertToHandle()));
            InPose._PredictedSubjectPose = FCk_ProceduralArticulation_Pose{RootFrame,
                BodyPresentation, InPose._PredictedSubjectPose.Get_HasBodyPresentation(),
                MoveTemp(PredictedLimbs)};
            return true;
        };

        if (InPose._HasControlCandidate)
        {
            const auto Root = InHandle.ConvertToHandle();
            if (InPose._ControlHandles.Num() != InPose._ControlPoses.Num())
            {
                InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                return;
            }
            for (const auto& Part : InPose._ControlHandles)
            {
                if (NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(Part, Root))
                {
                    InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                    InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                    return;
                }
            }
            if (InDeltaT <= FCk_Time{} || InPose._PendingObservation)
            { return; }
            InPose._ExpectedHandles = InPose._ControlHandles;
            InPose._ExpectedParts = InPose._ControlPoses;
            for (auto Index = 0; Index < InPose._ExpectedHandles.Num(); ++Index)
            {
                auto Part = InPose._ExpectedHandles[Index];
                if (InPose._LegacyWalker && Part != InBindings.Get_Presentation())
                {
                    InPose._ExpectedParts[Index].SetScale3D(
                        UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Part).GetScale3D());
                    UCk_Utils_Transform_UE::Request_SetLocationAndRotation(Part,
                        FCk_Request_Transform_SetLocationAndRotation{
                            InPose._ExpectedParts[Index].GetLocation(),
                            InPose._ExpectedParts[Index].GetRotation().Rotator()}, {});
                }
                else
                {
                    UCk_Utils_Transform_UE::Request_SetTransform(Part,
                        FCk_Request_Transform_SetTransform{InPose._ExpectedParts[Index]}, {});
                }
            }
            ++InPose._IssuedRevision;
            InPose._PendingObservation = true;
            InPose._HasControlCandidate = false;
            InPose._HasLegacyCandidate = false;
            InPose._HasLegacyPresentation = false;
            return;
        }

        if (InPose._LegacyWalker)
        {
            const auto Root = InHandle.ConvertToHandle();
            if (InBindings.Get_Bindings().Num() != InBindings.Get_Limbs().Num())
            {
                InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                return;
            }
            for (auto Index = 0; Index < InBindings.Get_Limbs().Num(); ++Index)
            {
                const auto Leg = UCk_Utils_ProceduralLeg_UE::Cast(
                    InBindings.Get_Limbs()[Index].ConvertToHandle());
                if (NOT procedural_leg::Get_IsLive(Leg))
                { continue; }
                const auto& Binding = InBindings.Get_Bindings()[Index];
                for (const auto& Part : Binding.Get_Segments())
                {
                    if (ck_procedural_articulation_processor::Get_IsBoundPartLive(Part, Root))
                    { continue; }
                    InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                    InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                    InHealth._OffendingLimb = Binding.Get_LimbId();
                    return;
                }
                if (Binding.Get_Terminal() != FCk_Handle_Transform{}
                    && NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(Binding.Get_Terminal(), Root))
                {
                    InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                    InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                    InHealth._OffendingLimb = Binding.Get_LimbId();
                    return;
                }
            }
            if (InBindings.Get_Presentation() != FCk_Handle_Transform{}
                && NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(InBindings.Get_Presentation(), Root))
            {
                InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                return;
            }
            if ((NOT InPose._HasLegacyCandidate && NOT InPose._HasLegacyPresentation)
                || InPose._PendingObservation)
            { return; }
            for (const auto& Part : InPose._LegacyHandles)
            {
                if (NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(Part, Root))
                {
                    InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                    InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                    return;
                }
            }
            if (InPose._HasLegacyPresentation
                && NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(InBindings.Get_Presentation(), Root))
            {
                InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                return;
            }
            if (InDeltaT <= FCk_Time{})
            { return; }

            InPose._ExpectedHandles = InPose._LegacyHandles;
            InPose._ExpectedParts = InPose._LegacyPoses;
            if (InPose._HasLegacyPresentation)
            {
                InPose._ExpectedHandles.Add(InBindings.Get_Presentation());
                InPose._ExpectedParts.Add(InPose._LegacyPresentation);
            }
            if (NOT HoldClaimedChannels(InPose._ExpectedHandles, InPose._ExpectedParts))
            {
                InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                return;
            }
            for (auto Index = 0; Index < InPose._LegacyHandles.Num(); ++Index)
            {
                auto Part = InPose._LegacyHandles[Index];
                InPose._ExpectedParts[Index].SetScale3D(
                    UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Part).GetScale3D());
                UCk_Utils_Transform_UE::Request_SetLocationAndRotation(Part,
                    FCk_Request_Transform_SetLocationAndRotation{
                        InPose._ExpectedParts[Index].GetLocation(), InPose._ExpectedParts[Index].GetRotation().Rotator()}, {});
            }
            if (InPose._ExpectedHandles.Num() > InPose._LegacyHandles.Num())
            {
                auto Presentation = InBindings.Get_Presentation();
                UCk_Utils_Transform_UE::Request_SetTransform(Presentation,
                    FCk_Request_Transform_SetTransform{InPose._ExpectedParts.Last()}, {});
            }
            ++InPose._IssuedRevision;
            InPose._PendingObservation = true;
            InPose._HasLegacyCandidate = false;
            InPose._HasLegacyPresentation = false;
            return;
        }

        const auto Root = InHandle.ConvertToHandle();
        const auto RootTransform = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(
            UCk_Utils_Transform_UE::CastChecked(Root));
        const auto& Layouts = InParams.Get_Layout().Get_Limbs();
        const auto& Bindings = InBindings.Get_Bindings();
        const auto& Limbs = InBindings.Get_Limbs();
        auto& Handles = InPose._ScratchHandles;
        auto& Poses = InPose._ScratchPoses;
        Handles.Reset();
        Poses.Reset();

        auto Fault = ECk_ProceduralArticulation_Fault::None;
        FName OffendingLimb = NAME_None;
        int32 OffendingPart = INDEX_NONE;
        if (NOT RootTransform.GetScale3D().Equals(FVector::OneVector)
            || Layouts.Num() != Bindings.Num() || Layouts.Num() != Limbs.Num())
        { Fault = ECk_ProceduralArticulation_Fault::InvalidBinding; }

        for (auto Index = 0; Fault == ECk_ProceduralArticulation_Fault::None && Index < Layouts.Num(); ++Index)
        {
            const auto& Layout = Layouts[Index];
            const auto& Binding = Bindings[Index];
            OffendingLimb = Layout.Get_Id();
            if (ck::Is_NOT_Valid(Limbs[Index]) || Limbs[Index].Has<FTag_DestroyEntity_Initiate>()
                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Limbs[Index]) != Root
                || Binding.Get_LimbId() != Layout.Get_Id()
                || Binding.Get_Segments().Num() != Layout.Get_SegmentLengths().Num())
            { Fault = ECk_ProceduralArticulation_Fault::InvalidBinding; break; }
            for (auto PartIndex = 0; PartIndex < Binding.Get_Segments().Num(); ++PartIndex)
            {
                if (ck_procedural_articulation_processor::Get_IsBoundPartLive(Binding.Get_Segments()[PartIndex], Root))
                { continue; }
                Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                OffendingPart = PartIndex;
                break;
            }
            if (Fault != ECk_ProceduralArticulation_Fault::None)
            { break; }
            if (Binding.Get_Terminal() != FCk_Handle_Transform{}
                && NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(Binding.Get_Terminal(), Root))
            {
                Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                OffendingPart = Binding.Get_Segments().Num();
            }
        }
        if (Fault == ECk_ProceduralArticulation_Fault::None
            && InBindings.Get_Presentation() != FCk_Handle_Transform{}
            && NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(InBindings.Get_Presentation(), Root))
        { Fault = ECk_ProceduralArticulation_Fault::InvalidBinding; }
        if (Fault != ECk_ProceduralArticulation_Fault::None)
        {
            InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
            InHealth._Fault = Fault;
            InHealth._OffendingLimb = OffendingLimb;
            InHealth._OffendingPart = OffendingPart;
            return;
        }
        if (InDeltaT <= FCk_Time{} || InPose._PendingObservation)
        { return; }
        Handles.Reserve(Layouts.Num() * 9 + 1);
        Poses.Reserve(Layouts.Num() * 9 + 1);

        auto Selected = TArray<FProceduralRig_SelectedChain>{};
        if (Fault == ECk_ProceduralArticulation_Fault::None
            && NOT FProcessor_ProceduralRig_Update::EvaluateDedicatedCandidate(InHandle,
                RootTransform, TArrayView<const FProceduralRig_WorldTarget>{}, Selected))
        { Fault = ECk_ProceduralArticulation_Fault::SolveFailed; }
        if (Fault == ECk_ProceduralArticulation_Fault::None && Selected.Num() != Layouts.Num())
        { Fault = ECk_ProceduralArticulation_Fault::SolveFailed; }

        for (auto Index = 0; Fault == ECk_ProceduralArticulation_Fault::None && Index < Layouts.Num(); ++Index)
        {
            const auto& Layout = Layouts[Index];
            const auto& Binding = Bindings[Index];
            OffendingLimb = Layout.Get_Id();
            const auto& Limb = Limbs[Index];
            if (ck::Is_NOT_Valid(Limb) || Limb.Has<FTag_DestroyEntity_Initiate>()
                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Limb) != Root
                || Binding.Get_LimbId() != Layout.Get_Id()
                || Binding.Get_Segments().Num() != Layout.Get_SegmentLengths().Num())
            {
                Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                break;
            }

            const auto& Chain = Selected[Index];
            if (Chain.Get_Id() != Layout.Get_Id()
                || Chain.Get_PartPoses().Num() < Binding.Get_Segments().Num())
            { Fault = ECk_ProceduralArticulation_Fault::SolveFailed; break; }

            for (auto PartIndex = 0; PartIndex < Binding.Get_Segments().Num(); ++PartIndex)
            {
                OffendingPart = PartIndex;
                const auto& Part = Binding.Get_Segments()[PartIndex];
                if (NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(Part, Root))
                {
                    Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                    break;
                }
                Handles.Add(Part);
                Poses.Add(Chain.Get_PartPoses()[PartIndex]);
            }
            if (Fault != ECk_ProceduralArticulation_Fault::None)
            { break; }

            if (Binding.Get_Terminal() != FCk_Handle_Transform{})
            {
                OffendingPart = Binding.Get_Segments().Num();
                if (NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(Binding.Get_Terminal(), Root))
                {
                    Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
                    break;
                }
                Handles.Add(Binding.Get_Terminal());
                Poses.Add(Chain.Get_PartPoses().Last());
            }
        }

        if (Fault == ECk_ProceduralArticulation_Fault::None
            && InBindings.Get_Presentation() != FCk_Handle_Transform{})
        {
            if (NOT ck_procedural_articulation_processor::Get_IsBoundPartLive(InBindings.Get_Presentation(), Root))
            { Fault = ECk_ProceduralArticulation_Fault::InvalidBinding; }
            else
            {
                Handles.Add(InBindings.Get_Presentation());
                Poses.Add(RootTransform);
            }
        }

        if (Fault != ECk_ProceduralArticulation_Fault::None)
        {
            InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
            InHealth._Fault = Fault;
            InHealth._OffendingLimb = OffendingLimb;
            InHealth._OffendingPart = OffendingPart;
            return;
        }

        auto PredictedLimbs = TArray<FCk_ProceduralArticulation_LimbPose>{};
        PredictedLimbs.Reserve(Selected.Num());
        for (auto Index = 0; Index < Selected.Num(); ++Index)
        {
            const auto& Layout = Layouts[Index];
            const auto& Chain = Selected[Index];
            const auto Attachment = Layout.Get_AttachmentLocal() * RootTransform;
            const auto Requested = Layout.Get_RestEndpointLocal() * Attachment;
            const auto Accepted = FTransform{Requested.GetRotation(), Chain.Get_AcceptedEndpoint()};
            auto SegmentPoses = TArray<FTransform>{};
            for (auto PartIndex = 0; PartIndex < Bindings[Index].Get_Segments().Num(); ++PartIndex)
            { SegmentPoses.Add(Chain.Get_PartPoses()[PartIndex]); }
            PredictedLimbs.Emplace(Layout.Get_Id(), Requested, Accepted, MoveTemp(SegmentPoses),
                static_cast<float>(FVector::Distance(Requested.GetLocation(), Accepted.GetLocation())),
                Chain.Get_ChainState(), ECk_ProceduralArticulation_ContactEvidence::None);
        }
        InPose._PredictedSubjectPose = FCk_ProceduralArticulation_Pose{RootTransform, RootTransform,
            InBindings.Get_Presentation() != FCk_Handle_Transform{}, MoveTemp(PredictedLimbs)};

        InPose._ExpectedHandles = Handles;
        InPose._ExpectedParts = Poses;
        if (NOT HoldClaimedChannels(InPose._ExpectedHandles, InPose._ExpectedParts))
        {
            InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
            InHealth._Fault = ECk_ProceduralArticulation_Fault::InvalidBinding;
            return;
        }
        for (auto Index = 0; Index < InPose._ExpectedHandles.Num(); ++Index)
        {
            auto Part = InPose._ExpectedHandles[Index];
            UCk_Utils_Transform_UE::Request_SetTransform(Part,
                FCk_Request_Transform_SetTransform{InPose._ExpectedParts[Index]}, {});
        }
        ++InPose._IssuedRevision;
        InPose._PendingObservation = true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_ProceduralArticulation_Observe::
        ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        const FFragment_ProceduralArticulation_Params& InParams,
        FFragment_ProceduralArticulation_Pose& InPose,
        FFragment_ProceduralArticulation_Health& InHealth)
        -> void
    {
        if (InHealth._State == ECk_ProceduralArticulation_Health::Faulted || NOT InPose._PendingObservation)
        { return; }

        for (auto Index = 0; Index < InPose._ExpectedHandles.Num(); ++Index)
        {
            const auto& Part = InPose._ExpectedHandles[Index];
            const auto Live = ck_procedural_articulation_processor::Get_IsBoundPartLive(Part, InHandle.ConvertToHandle());
            const auto Observed = Live ? UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Part) : FTransform::Identity;
            if (NOT Live || NOT ck_procedural_articulation_processor::Get_IsSamePose(InPose._ExpectedParts[Index], Observed))
            {
                InHealth._State = ECk_ProceduralArticulation_Health::Faulted;
                InHealth._Fault = ECk_ProceduralArticulation_Fault::SettlementMismatch;
                InHealth._OffendingPart = Index;
                InHealth._ObservedPart = Observed;
                InPose._PendingObservation = false;
                return;
            }
        }
        auto Predicted = InPose._PredictedSubjectPose;
        const auto HasClaims = InHandle.Has<FFragment_ProceduralArticulation_Claims>()
            && InHandle.Get<FFragment_ProceduralArticulation_Claims>()._BatchPublicationPending;
        const auto& Bindings = InHandle.Get<FFragment_ProceduralArticulation_Bindings>();
        const auto RootFrame = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(
            UCk_Utils_Transform_UE::CastChecked(InHandle.ConvertToHandle()));
        const auto Presentation = Bindings.Get_Presentation();
        const auto ActualBody = Predicted.Get_HasBodyPresentation() && ck::IsValid(Presentation)
            ? UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Presentation)
            : Predicted.Get_BodyPresentation();
        auto ActualLimbs = TArray<FCk_ProceduralArticulation_LimbPose>{};
        for (const auto& Limb : Predicted.Get_Limbs())
        {
            const auto BindingIndex = Bindings.Get_Bindings().IndexOfByPredicate(
                [&](const FCk_ProceduralLimb_Binding& InBinding)
                { return InBinding.Get_LimbId() == Limb.Get_Id(); });
            if (BindingIndex == INDEX_NONE)
            { continue; }
            const auto& Binding = Bindings.Get_Bindings()[BindingIndex];
            auto Segments = TArray<FTransform>{};
            for (const auto& Segment : Binding.Get_Segments())
            { Segments.Add(UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Segment)); }
            auto Accepted = Limb.Get_AcceptedEndpoint();
            if (Binding.Get_Terminal() != FCk_Handle_Transform{})
            {
                Accepted = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Binding.Get_Terminal());
                Accepted.SetScale3D(FVector::OneVector);
            }
            else if (NOT Segments.IsEmpty())
            {
                auto LastLength = 0.0f;
                if (InPose._LegacyWalker && Bindings.Get_Limbs().IsValidIndex(BindingIndex))
                {
                    const auto Leg = UCk_Utils_ProceduralLeg_UE::Cast(
                        Bindings.Get_Limbs()[BindingIndex].ConvertToHandle());
                    if (ck::IsValid(Leg) && Leg.Has<FFragment_ProceduralLeg_Params>())
                    { LastLength = Leg.Get<FFragment_ProceduralLeg_Params>().Get_Chain().Get_SegmentLengths().Last(); }
                }
                else if (InParams.Get_Layout().Get_Limbs().IsValidIndex(BindingIndex))
                { LastLength = InParams.Get_Layout().Get_Limbs()[BindingIndex].Get_SegmentLengths().Last(); }
                Accepted.SetLocation(Segments.Last().GetLocation()
                    + Segments.Last().GetRotation().GetAxisX() * (LastLength * 0.5));
            }
            ActualLimbs.Emplace(Limb.Get_Id(), Limb.Get_RequestedEndpoint(), Accepted, MoveTemp(Segments),
                static_cast<float>(FVector::Distance(Limb.Get_RequestedEndpoint().GetLocation(),
                    Accepted.GetLocation())), Limb.Get_ChainState(), Limb.Get_ContactEvidence());
        }
        InPose._SettledSubjectPose = FCk_ProceduralArticulation_Pose{RootFrame, ActualBody,
            Predicted.Get_HasBodyPresentation(), MoveTemp(ActualLimbs)};
        if (HasClaims)
        {
            auto& Claims = InHandle.Get<FFragment_ProceduralArticulation_Claims>();
            for (auto Index = 0; Index < Claims._Tickets.Num(); ++Index)
            {
                if (Claims._Tickets[Index].Get_BatchNonce() != Claims._BatchNonce)
                { continue; }
                Claims._TokenPoses[Index] = ck_procedural_articulation_processor::Get_ObservedSubset(
                    InPose._SettledSubjectPose, Claims._TicketChannels[Index]);
                Claims._TicketRevisions[Index] = InPose._IssuedRevision;
            }
            Claims._BatchPublicationPending = false;
        }
        InPose._SettledRevision = InPose._IssuedRevision;
        InPose._PendingObservation = false;
        if (InPose._LegacyWalker && NOT InHandle.Has<FFragment_ProceduralActionPlayer>()
            && InHandle.Has<FFragment_ProceduralBodyPose_Action>()
            && InHandle.Has<FFragment_ProceduralArticulation_Bindings>())
        {
            if (ck::IsValid(Presentation))
            {
                auto& Action = InHandle.Get<FFragment_ProceduralBodyPose_Action>();
                Action._Sample = FCk_ProceduralBodyPose_ActionSample{
                    UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Presentation), Action._SampleId,
                    Action._Sample.Get_PoseRevision() + 1};
            }
        }
    }
}
