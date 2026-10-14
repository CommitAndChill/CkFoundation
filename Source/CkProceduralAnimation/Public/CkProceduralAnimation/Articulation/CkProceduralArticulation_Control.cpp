#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control.h"

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Utils.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment.h"
#include "CkProceduralAnimation/Core/CkProceduralLimbSolve.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Processor.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"

#include "CkJolt/Body/CkJoltBody_Fragment.h"

#include "CoreGlobals.h"

namespace ck_procedural_articulation_control
{
    auto
        Get_IsTokenValid(
            FCk_ArticulationControlToken InToken)
        -> bool
    { return InToken.Get_InstanceId() != 0 && InToken.Get_Generation() != 0; }

    auto
        Get_IsRigid(
            const FTransform& InPose)
        -> bool
    {
        return NOT InPose.ContainsNaN() && InPose.GetRotation().IsNormalized()
            && InPose.GetScale3D().Equals(FVector::OneVector);
    }

    auto
        Get_IsSubjectLive(
            const FCk_Handle_ProceduralArticulation& InSubject)
        -> bool
    {
        return ck::IsValid(InSubject) && NOT InSubject.Has<ck::FTag_DestroyEntity_Initiate>()
            && InSubject.Has<ck::FFragment_ProceduralArticulation_Params>()
            && InSubject.Has<ck::FFragment_ProceduralArticulation_Bindings>()
            && InSubject.Has<ck::FFragment_ProceduralArticulation_Claims>()
            && InSubject.Has<ck::FFragment_ProceduralArticulation_Pose>()
            && InSubject.Has<ck::FFragment_ProceduralArticulation_Health>();
    }

    auto
        Get_IsFaulted(
            const FCk_Handle_ProceduralArticulation& InSubject)
        -> bool
    {
        return InSubject.Get<ck::FFragment_ProceduralArticulation_Health>().Get_State()
            == ECk_ProceduralArticulation_Health::Faulted;
    }

    auto
        Get_IsLimbLive(
            const FCk_Handle_ProceduralLimb& InLimb)
        -> bool
    {
        const auto Leg = UCk_Utils_ProceduralLeg_UE::Cast(InLimb.ConvertToHandle());
        if (ck::IsValid(Leg))
        { return ck::procedural_leg::Get_IsLive(Leg); }
        return ck::IsValid(InLimb) && NOT InLimb.Has<ck::FTag_DestroyEntity_Initiate>();
    }

    auto
        Get_LimbIndex(
            const FCk_Handle_ProceduralArticulation& InSubject,
            FName InId)
        -> int32
    {
        const auto& Bindings = InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Bindings();
        for (auto Index = 0; Index < Bindings.Num(); ++Index)
        {
            if (Bindings[Index].Get_LimbId() == InId)
            { return Index; }
        }
        return INDEX_NONE;
    }

    auto
        Get_Subset(
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
                {
                    Limbs.Add(Limb);
                    break;
                }
            }
        }
        return FCk_ProceduralArticulation_Pose{InPose.Get_RootFrame(), InPose.Get_BodyPresentation(),
            InChannels.Get_Body() && InPose.Get_HasBodyPresentation(), MoveTemp(Limbs)};
    }

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
        Get_DefaultBodyFrame(
            const FCk_Handle_ProceduralArticulation& InSubject,
            const FTransform& InRoot)
        -> FTransform
    {
        return InSubject.Has<ck::FFragment_ProceduralBodyPose>()
            ? InSubject.Get<ck::FFragment_ProceduralBodyPose>().Get_BaseOffset() * InRoot
            : InRoot;
    }

    auto
        Get_Target(
            TArrayView<const FCk_ArticulationLimbTarget> InTargets,
            FName InId)
        -> const FCk_ArticulationLimbTarget*
    {
        for (const auto& Target : InTargets)
        {
            if (Target.Get_Id() == InId)
            { return &Target; }
        }
        return nullptr;
    }

    struct FEvaluatedSubject
    {
        FCk_ProceduralArticulation_Pose Pose;
        TArray<FCk_Handle_Transform> Parts;
        TArray<FTransform> PartPoses;
    };

    auto
        DoEvaluateSubject(
            const FCk_Handle_ProceduralArticulation& InSubject,
            TArrayView<const FCk_ArticulationLimbTarget> InTargets,
            const TOptional<FTransform>& InBodyOverride,
            FEvaluatedSubject& OutEvaluation)
        -> bool
    {
        if (NOT Get_IsSubjectLive(InSubject) || Get_IsFaulted(InSubject))
        { return false; }
        const auto Root = InSubject.ConvertToHandle();
        const auto RootFrame = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(
            UCk_Utils_Transform_UE::CastChecked(Root));
        if (NOT Get_IsRigid(RootFrame))
        { return false; }
        const auto BodyFrame = InBodyOverride.IsSet()
            ? *InBodyOverride : Get_DefaultBodyFrame(InSubject, RootFrame);
        if (NOT Get_IsRigid(BodyFrame))
        { return false; }
        const auto& Params = InSubject.Get<ck::FFragment_ProceduralArticulation_Params>();
        const auto& Bindings = InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>();
        auto LimbPoses = TArray<FCk_ProceduralArticulation_LimbPose>{};
        OutEvaluation.Parts.Reset();
        OutEvaluation.PartPoses.Reset();

        if (InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>().Get_LegacyWalker())
        {
            const auto Gait = UCk_Utils_ProceduralGait_UE::Cast(Root);
            if (ck::Is_NOT_Valid(Gait))
            { return false; }
            auto RigTargets = TArray<ck::FProceduralRig_WorldTarget>{};
            RigTargets.Reserve(InTargets.Num());
            for (const auto& Target : InTargets)
            {
                RigTargets.Emplace(Target.Get_Id(), Target.Get_RequestedEndpointWorld(), Target.Get_PoleWorld(),
                    Target.Get_AuthoredOrientationWorld());
            }
            auto Chains = TArray<ck::FProceduralRig_SelectedChain>{};
            if (NOT ck::FProcessor_ProceduralRig_Update::EvaluateSubjectCandidate(Gait,
                MakeArrayView(RigTargets), TOptional<FTransform>{BodyFrame}, Chains))
            {
                for (const auto& Limb : Bindings.Get_Limbs())
                {
                    const auto Leg = UCk_Utils_ProceduralLeg_UE::Cast(Limb.ConvertToHandle());
                    if (ck::procedural_leg::Get_IsLive(Leg))
                    { return false; }
                }
            }
            for (const auto& Chain : Chains)
            {
                for (const auto& Part : Chain.Get_Parts())
                {
                    if (NOT Get_IsBoundPartLive(Part, Root))
                    { return false; }
                }
                const auto& LegFoot = Chain.Get_Leg().Get<ck::FFragment_ProceduralLeg>().Get_Foot();
                const auto* Target = Get_Target(InTargets, Chain.Get_Id());
                const auto Rotation = ck::Get_ProceduralTerminalRotation(LegFoot.Get_Rotation(),
                    Target != nullptr ? Target->Get_AuthoredOrientationWorld() : TOptional<FQuat>{});
                const auto Requested = FTransform{Rotation, Chain.Get_RequestedEndpoint()};
                const auto Accepted = FTransform{Rotation, Chain.Get_AcceptedEndpoint()};
                if (Target != nullptr && Target->Get_ReachPolicy() == ECk_ArticulationReachPolicy::RejectUnreachable
                    && FVector::Distance(Requested.GetLocation(), Accepted.GetLocation()) > 0.1)
                { return false; }
                auto Segments = Chain.Get_PartPoses();
                const auto& BindingList = Bindings.Get_Bindings();
                const auto BindingIndex = Get_LimbIndex(InSubject, Chain.Get_Id());
                if (NOT BindingList.IsValidIndex(BindingIndex))
                { return false; }
                Segments.SetNum(BindingList[BindingIndex].Get_Segments().Num());
                auto Evidence = ECk_ProceduralArticulation_ContactEvidence::Untrusted;
                if (LegFoot.Get_Contact() == ECk_ProceduralLeg_FootContact::Trusted)
                { Evidence = ECk_ProceduralArticulation_ContactEvidence::Trusted; }
                LimbPoses.Emplace(Chain.Get_Id(), Requested, Accepted, MoveTemp(Segments),
                    static_cast<float>(FVector::Distance(Requested.GetLocation(), Accepted.GetLocation())),
                    Chain.Get_ChainState(), Evidence);
                OutEvaluation.Parts.Append(Chain.Get_Parts());
                OutEvaluation.PartPoses.Append(Chain.Get_PartPoses());
            }
        }
        else
        {
            const auto& Layouts = Params.Get_Layout().Get_Limbs();
            if (Layouts.Num() != Bindings.Get_Bindings().Num())
            { return false; }
            auto RigTargets = TArray<ck::FProceduralRig_WorldTarget>{};
            RigTargets.Reserve(InTargets.Num());
            for (const auto& Target : InTargets)
            {
                RigTargets.Emplace(Target.Get_Id(), Target.Get_RequestedEndpointWorld(), Target.Get_PoleWorld(),
                    Target.Get_AuthoredOrientationWorld());
            }
            auto Chains = TArray<ck::FProceduralRig_SelectedChain>{};
            if (NOT ck::FProcessor_ProceduralRig_Update::EvaluateDedicatedCandidate(InSubject,
                BodyFrame, MakeArrayView(RigTargets), Chains) || Chains.Num() != Layouts.Num())
            { return false; }
            for (auto Index = 0; Index < Chains.Num(); ++Index)
            {
                const auto& Layout = Layouts[Index];
                const auto& Binding = Bindings.Get_Bindings()[Index];
                const auto& Chain = Chains[Index];
                if (Binding.Get_LimbId() != Chain.Get_Id()
                    || Binding.Get_Segments().Num() > Chain.Get_PartPoses().Num())
                { return false; }
                const auto Attachment = Layout.Get_AttachmentLocal() * BodyFrame;
                const auto Default = Layout.Get_RestEndpointLocal() * Attachment;
                const auto* Target = Get_Target(InTargets, Layout.Get_Id());
                const auto Requested = Target != nullptr
                    ? FTransform{ck::Get_ProceduralTerminalRotation(Default.GetRotation(),
                        Target->Get_AuthoredOrientationWorld()), Target->Get_RequestedEndpointWorld().GetLocation()}
                    : Default;
                const auto Accepted = FTransform{Requested.GetRotation(), Chain.Get_AcceptedEndpoint()};
                if (Target != nullptr && Target->Get_ReachPolicy() == ECk_ArticulationReachPolicy::RejectUnreachable
                    && FVector::Distance(Requested.GetLocation(), Accepted.GetLocation()) > 0.1)
                { return false; }
                auto SegmentPoses = TArray<FTransform>{};
                for (auto PartIndex = 0; PartIndex < Binding.Get_Segments().Num(); ++PartIndex)
                {
                    const auto& Part = Binding.Get_Segments()[PartIndex];
                    if (NOT Get_IsBoundPartLive(Part, Root))
                    { return false; }
                    SegmentPoses.Add(Chain.Get_PartPoses()[PartIndex]);
                }
                if (Binding.Get_Terminal() != FCk_Handle_Transform{})
                {
                    if (NOT Get_IsBoundPartLive(Binding.Get_Terminal(), Root))
                    { return false; }
                }
                OutEvaluation.Parts.Append(Chain.Get_Parts());
                OutEvaluation.PartPoses.Append(Chain.Get_PartPoses());
                LimbPoses.Emplace(Layout.Get_Id(), Requested, Accepted, MoveTemp(SegmentPoses),
                    static_cast<float>(FVector::Distance(Requested.GetLocation(), Accepted.GetLocation())),
                    Chain.Get_ChainState(), ECk_ProceduralArticulation_ContactEvidence::None);
            }
        }

        const auto Presentation = Bindings.Get_Presentation();
        if (Presentation != FCk_Handle_Transform{})
        {
            if (NOT Get_IsBoundPartLive(Presentation, Root))
            { return false; }
            OutEvaluation.Parts.Add(Presentation);
            OutEvaluation.PartPoses.Add(BodyFrame);
        }
        OutEvaluation.Pose = FCk_ProceduralArticulation_Pose{RootFrame, BodyFrame,
            Presentation != FCk_Handle_Transform{}, MoveTemp(LimbPoses)};
        return true;
    }
}

auto
    FCk_ProceduralArticulation_Control::
    TryReserveClaims(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        const FCk_ArticulationChannelSet& InChannels)
    -> ECk_ArticulationControlResult
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return ECk_ArticulationControlResult::InvalidSubject; }
    if (Get_IsFaulted(InSubject))
    { return ECk_ArticulationControlResult::Faulted; }
    if (NOT Get_IsTokenValid(InToken) || (InChannels.Get_LimbIds().IsEmpty() && NOT InChannels.Get_Body()))
    { return ECk_ArticulationControlResult::InvalidChannels; }

    auto Subject = InSubject;
    auto& Claims = Subject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    const auto NoOwner = FCk_ArticulationControlToken{};
    if (Claims._LimbOwners.Num()
        != InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Bindings().Num())
    { return ECk_ArticulationControlResult::Faulted; }
    if (InChannels.Get_Body())
    {
        if (Claims._BodyOwner != NoOwner
            || NOT ck::IsValid(InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Presentation()))
        { return ECk_ArticulationControlResult::ClaimConflict; }
        for (const auto& Owner : Claims._LimbOwners)
        {
            if (Owner != NoOwner)
            { return ECk_ArticulationControlResult::ClaimConflict; }
        }
    }
    else if (Claims._BodyOwner != NoOwner)
    { return ECk_ArticulationControlResult::ClaimConflict; }

    auto Indices = TArray<int32>{};
    auto Seen = TSet<FName>{};
    for (const auto& Id : InChannels.Get_LimbIds())
    {
        const auto Index = Get_LimbIndex(InSubject, Id);
        if (Id.IsNone() || Seen.Contains(Id) || Index == INDEX_NONE)
        { return ECk_ArticulationControlResult::InvalidChannels; }
        Seen.Add(Id);
        if (Claims._LimbOwners[Index] != NoOwner)
        { return ECk_ArticulationControlResult::ClaimConflict; }
        Indices.Add(Index);
    }

    const auto& Pose = InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    if (Pose.Get_PendingObservation() || Pose._HasControlCandidate
        || Pose.Get_IssuedRevision() != Pose.Get_SettledRevision())
    { return ECk_ArticulationControlResult::Busy; }
    for (const auto Index : Indices)
    { Claims._LimbOwners[Index] = InToken; }
    if (InChannels.Get_Body())
    { Claims._BodyOwner = InToken; }
    ++Claims._Revision;
    return ECk_ArticulationControlResult::Accepted;
}

auto
    FCk_ProceduralArticulation_Control::
    GetClaimant(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FName InLimbId,
        bool InBody)
    -> FCk_ArticulationControlToken
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return {}; }
    const auto& Claims = InSubject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    if (InBody)
    { return Claims._BodyOwner; }
    const auto Index = Get_LimbIndex(InSubject, InLimbId);
    return Claims._LimbOwners.IsValidIndex(Index) ? Claims._LimbOwners[Index] : FCk_ArticulationControlToken{};
}

auto
    FCk_ProceduralArticulation_Control::
    GetControlState(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken)
    -> FCk_ArticulationControlState
{
    auto Result = FCk_ArticulationControlState{};
    if (NOT ck_procedural_articulation_control::Get_IsSubjectLive(InSubject)
        || NOT ck_procedural_articulation_control::Get_IsTokenValid(InToken))
    { return Result; }
    const auto& Claims = InSubject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    const auto& Pose = InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    Result._HasClaims = Claims._BodyOwner == InToken || Claims._LimbOwners.Contains(InToken);
    Result._HasPendingSample = Claims._BatchPublicationPending && Claims._Tickets.ContainsByPredicate(
        [&](const FCk_ArticulationStageKey& InKey) { return InKey.Get_Token() == InToken; });
    Result._LastIssuedRevision = Pose.Get_IssuedRevision();
    Result._LastSettledRevision = Pose.Get_SettledRevision();
    return Result;
}

auto
    FCk_ProceduralArticulation_Control::
    CaptureAcceptedEntry(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        uint64 InExpectedSettledRevision,
        FCk_ProceduralArticulation_Pose& OutPose)
    -> ECk_ArticulationControlResult
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return ECk_ArticulationControlResult::InvalidSubject; }
    if (Get_IsFaulted(InSubject))
    { return ECk_ArticulationControlResult::Faulted; }
    const auto& Pose = InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    if (Pose.Get_PendingObservation() || Pose.Get_IssuedRevision() != Pose.Get_SettledRevision()
        || Pose.Get_SettledRevision() == 0 || Pose.Get_SettledRevision() != InExpectedSettledRevision)
    { return ECk_ArticulationControlResult::Busy; }
    const auto& Claims = InSubject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    const auto& Bindings = InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Bindings();
    auto Ids = TArray<FName>{};
    for (auto Index = 0; Index < Claims._LimbOwners.Num(); ++Index)
    {
        if (Claims._LimbOwners[Index] == InToken && Bindings.IsValidIndex(Index))
        { Ids.Add(Bindings[Index].Get_LimbId()); }
    }
    const auto Body = Claims._BodyOwner == InToken;
    if (Ids.IsEmpty() && NOT Body)
    { return ECk_ArticulationControlResult::ClaimLost; }
    OutPose = Get_Subset(Pose._SettledSubjectPose, FCk_ArticulationChannelSet{MoveTemp(Ids), Body});
    return ECk_ArticulationControlResult::Accepted;
}

auto
    FCk_ProceduralArticulation_Control::
    GetDefaultPose(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ProceduralArticulation_Pose& OutPose)
    -> ECk_ArticulationControlResult
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return ECk_ArticulationControlResult::InvalidSubject; }
    if (Get_IsFaulted(InSubject))
    { return ECk_ArticulationControlResult::Faulted; }
    auto Evaluation = FEvaluatedSubject{};
    if (NOT DoEvaluateSubject(InSubject, TArrayView<const FCk_ArticulationLimbTarget>{},
        TOptional<FTransform>{}, Evaluation))
    { return ECk_ArticulationControlResult::InvalidCandidate; }
    OutPose = MoveTemp(Evaluation.Pose);
    return ECk_ArticulationControlResult::Accepted;
}

auto
    FCk_ProceduralArticulation_Control::
    TryEvaluateCandidateBatch(
        const FCk_Handle_ProceduralArticulation& InSubject,
        TArrayView<const FCk_ArticulationTokenCandidate> InCandidates,
        FCk_ArticulationBatchEvaluation& OutEvaluation)
    -> ECk_ArticulationControlResult
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return ECk_ArticulationControlResult::InvalidSubject; }
    if (Get_IsFaulted(InSubject))
    { return ECk_ArticulationControlResult::Faulted; }
    const auto& Pose = InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    const auto& Claims = InSubject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    if (Pose.Get_PendingObservation() || Pose.Get_IssuedRevision() != Pose.Get_SettledRevision()
        || Claims._BatchStaged)
    { return ECk_ArticulationControlResult::Busy; }
    if (Claims._LimbOwners.Num()
        != InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Bindings().Num())
    { return ECk_ArticulationControlResult::Faulted; }

    auto Targets = TArray<FCk_ArticulationLimbTarget>{};
    auto BodyOverride = TOptional<FTransform>{};
    auto SeenIds = TSet<FName>{};
    auto SeenTokens = TArray<FCk_ArticulationControlToken>{};
    for (const auto& Candidate : InCandidates)
    {
        const auto Token = Candidate.Get_Token();
        if (NOT Get_IsTokenValid(Token) || SeenTokens.Contains(Token))
        { return ECk_ArticulationControlResult::InvalidCandidate; }
        SeenTokens.Add(Token);
        if (Candidate.Get_BodyPresentationWorld().IsSet())
        {
            if (Claims._BodyOwner != Token || BodyOverride.IsSet()
                || NOT Get_IsRigid(*Candidate.Get_BodyPresentationWorld()))
            { return ECk_ArticulationControlResult::InvalidCandidate; }
            BodyOverride = Candidate.Get_BodyPresentationWorld();
        }
        else if (Claims._BodyOwner == Token)
        { return ECk_ArticulationControlResult::InvalidCandidate; }
        for (const auto& Target : Candidate.Get_Limbs())
        {
            const auto Index = Get_LimbIndex(InSubject, Target.Get_Id());
            const auto Policy = Target.Get_ReachPolicy();
            if (Target.Get_Id().IsNone() || SeenIds.Contains(Target.Get_Id())
                || NOT Claims._LimbOwners.IsValidIndex(Index) || Claims._LimbOwners[Index] != Token
                || NOT Get_IsRigid(Target.Get_RequestedEndpointWorld())
                || (Target.Get_PoleWorld().IsSet() && Target.Get_PoleWorld()->ContainsNaN())
                || (Policy != ECk_ArticulationReachPolicy::RejectUnreachable
                    && Policy != ECk_ArticulationReachPolicy::ProjectToReach))
            { return ECk_ArticulationControlResult::InvalidCandidate; }
            SeenIds.Add(Target.Get_Id());
            Targets.Add(Target);
        }
    }
    const auto& Bindings = InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Bindings();
    for (auto Index = 0; Index < Claims._LimbOwners.Num(); ++Index)
    {
        const auto& Owner = Claims._LimbOwners[Index];
        if (Owner != FCk_ArticulationControlToken{}
            && (NOT SeenTokens.Contains(Owner) || NOT SeenIds.Contains(Bindings[Index].Get_LimbId())))
        { return ECk_ArticulationControlResult::InvalidCandidate; }
    }
    if (Claims._BodyOwner != FCk_ArticulationControlToken{} && NOT BodyOverride.IsSet())
    { return ECk_ArticulationControlResult::InvalidCandidate; }
    if (SeenTokens.IsEmpty())
    { return ECk_ArticulationControlResult::InvalidCandidate; }

    auto Evaluated = FEvaluatedSubject{};
    if (NOT DoEvaluateSubject(InSubject, MakeArrayView(Targets), BodyOverride, Evaluated))
    { return ECk_ArticulationControlResult::InvalidCandidate; }
    for (const auto& Target : Targets)
    {
        const auto Found = Evaluated.Pose.Get_Limbs().ContainsByPredicate(
            [&](const FCk_ProceduralArticulation_LimbPose& InLimb) { return InLimb.Get_Id() == Target.Get_Id(); });
        if (NOT Found)
        { return ECk_ArticulationControlResult::ClaimLost; }
    }

    auto Result = FCk_ArticulationBatchEvaluation{};
    Result._PredictedSubjectPose = MoveTemp(Evaluated.Pose);
    Result._BoundParts = MoveTemp(Evaluated.Parts);
    Result._PartPoses = MoveTemp(Evaluated.PartPoses);
    Result._Subject = InSubject;
    Result._Candidates.Append(InCandidates.GetData(), InCandidates.Num());
    Result._InputRevision = Pose.Get_SettledRevision();
    Result._ClaimsRevisionAtEvaluation = Claims._Revision;
    Result._FrameAtEvaluation = GFrameCounter;
    Result._RootAtEvaluation = Result._PredictedSubjectPose.Get_RootFrame();
    Result._BodyOffsetAtEvaluation = InSubject.Has<ck::FFragment_ProceduralBodyPose>()
        ? InSubject.Get<ck::FFragment_ProceduralBodyPose>().Get_BaseOffset() : FTransform::Identity;
    Result._GaitSequenceAtEvaluation = InSubject.Has<ck::FFragment_ProceduralGait>()
        ? InSubject.Get<ck::FFragment_ProceduralGait>().Get_SolveSequence() : 0;
    for (const auto& Candidate : InCandidates)
    {
        auto Ids = TArray<FName>{};
        for (const auto& Target : Candidate.Get_Limbs())
        { Ids.Add(Target.Get_Id()); }
        Result._PredictedTokenPoses.Add(Get_Subset(Result._PredictedSubjectPose,
            FCk_ArticulationChannelSet{MoveTemp(Ids), Candidate.Get_BodyPresentationWorld().IsSet()}));
    }
    OutEvaluation = MoveTemp(Result);
    return ECk_ArticulationControlResult::Accepted;
}

auto
    FCk_ProceduralArticulation_Control::
    TryStageCandidateBatch(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationBatchEvaluation&& InEvaluation,
        TArray<FCk_ArticulationStageKey>& OutKeys)
    -> ECk_ArticulationControlResult
{
    using namespace ck_procedural_articulation_control;
    OutKeys.Reset();
    if (NOT Get_IsSubjectLive(InSubject) || InEvaluation._Subject != InSubject)
    { return ECk_ArticulationControlResult::InvalidSubject; }
    if (Get_IsFaulted(InSubject))
    { return ECk_ArticulationControlResult::Faulted; }
    auto Subject = InSubject;
    auto& Claims = Subject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    auto& Pose = Subject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    if (Claims._BatchStaged || Pose.Get_PendingObservation())
    { return ECk_ArticulationControlResult::Busy; }
    if (Claims._Revision != InEvaluation._ClaimsRevisionAtEvaluation
        || Pose.Get_SettledRevision() != InEvaluation._InputRevision
        || Pose.Get_IssuedRevision() != Pose.Get_SettledRevision()
        || InEvaluation._Candidates.Num() != InEvaluation._PredictedTokenPoses.Num())
    { return ECk_ArticulationControlResult::StaleInput; }
    const auto RootFrame = UCk_Utils_Transform_UE::Get_EntityCurrentTransform(
        UCk_Utils_Transform_UE::CastChecked(InSubject.ConvertToHandle()));
    const auto BodyOffset = InSubject.Has<ck::FFragment_ProceduralBodyPose>()
        ? InSubject.Get<ck::FFragment_ProceduralBodyPose>().Get_BaseOffset() : FTransform::Identity;
    const auto GaitSequence = InSubject.Has<ck::FFragment_ProceduralGait>()
        ? InSubject.Get<ck::FFragment_ProceduralGait>().Get_SolveSequence() : 0;
    if (InEvaluation._FrameAtEvaluation != GFrameCounter
        || NOT RootFrame.Equals(InEvaluation._RootAtEvaluation, 0.001)
        || NOT BodyOffset.Equals(InEvaluation._BodyOffsetAtEvaluation, 0.001)
        || GaitSequence != InEvaluation._GaitSequenceAtEvaluation
        || InEvaluation._BoundParts.Num() != InEvaluation._PartPoses.Num())
    { return ECk_ArticulationControlResult::StaleInput; }
    for (auto Index = 0; Index < InEvaluation._BoundParts.Num(); ++Index)
    {
        if (NOT Get_IsBoundPartLive(InEvaluation._BoundParts[Index], InSubject.ConvertToHandle())
            || InEvaluation._PartPoses[Index].ContainsNaN()
            || NOT InEvaluation._PartPoses[Index].GetRotation().IsNormalized())
        { return ECk_ArticulationControlResult::StaleInput; }
    }

    const auto Nonce = Claims._NextBatchNonce++;
    Claims._BatchNonce = Nonce;
    Claims._BatchStaged = true;
    Claims._BatchPublicationPending = true;
    Claims._Tickets.Reserve(Claims._Tickets.Num() + InEvaluation._Candidates.Num());
    Claims._TicketChannels.Reserve(Claims._TicketChannels.Num() + InEvaluation._Candidates.Num());
    Claims._TokenPoses.Reserve(Claims._TokenPoses.Num() + InEvaluation._Candidates.Num());
    Claims._TicketRevisions.Reserve(Claims._TicketRevisions.Num() + InEvaluation._Candidates.Num());
    for (const auto& Candidate : InEvaluation._Candidates)
    {
        Claims._Tickets.Emplace(Candidate.Get_Token(), Candidate.Get_SampleSequence(), Nonce);
        auto Ids = TArray<FName>{};
        for (const auto& Target : Candidate.Get_Limbs())
        { Ids.Add(Target.Get_Id()); }
        Claims._TicketChannels.Emplace(MoveTemp(Ids), Candidate.Get_BodyPresentationWorld().IsSet());
        Claims._TokenPoses.AddDefaulted();
        Claims._TicketRevisions.Add(0);
    }
    Pose._ControlHandles = MoveTemp(InEvaluation._BoundParts);
    Pose._ControlPoses = MoveTemp(InEvaluation._PartPoses);
    Pose._PredictedSubjectPose = MoveTemp(InEvaluation._PredictedSubjectPose);
    Pose._HasControlCandidate = true;
    for (const auto& Candidate : InEvaluation._Candidates)
    { OutKeys.Emplace(Candidate.Get_Token(), Candidate.Get_SampleSequence(), Nonce); }
    return ECk_ArticulationControlResult::Accepted;
}

auto
    FCk_ProceduralArticulation_Control::
    TryTransferClaims(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InFrom,
        FCk_ArticulationControlToken InTo,
        const FCk_ArticulationChannelSet& InChannels)
    -> ECk_ArticulationControlResult
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return ECk_ArticulationControlResult::InvalidSubject; }
    if (Get_IsFaulted(InSubject))
    { return ECk_ArticulationControlResult::Faulted; }
    if (NOT Get_IsTokenValid(InFrom) || NOT Get_IsTokenValid(InTo) || InFrom == InTo
        || (InChannels.Get_LimbIds().IsEmpty() && NOT InChannels.Get_Body()))
    { return ECk_ArticulationControlResult::InvalidChannels; }
    auto Subject = InSubject;
    auto& Claims = Subject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    const auto& Pose = InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    if (Pose.Get_PendingObservation() || Pose.Get_IssuedRevision() != Pose.Get_SettledRevision()
        || Pose._HasControlCandidate)
    { return ECk_ArticulationControlResult::Busy; }
    auto Indices = TArray<int32>{};
    auto Seen = TSet<FName>{};
    for (const auto& Id : InChannels.Get_LimbIds())
    {
        const auto Index = Get_LimbIndex(InSubject, Id);
        if (Id.IsNone() || Seen.Contains(Id) || NOT Claims._LimbOwners.IsValidIndex(Index)
            || Claims._LimbOwners[Index] != InFrom)
        { return ECk_ArticulationControlResult::ClaimLost; }
        Seen.Add(Id);
        Indices.Add(Index);
    }
    if (InChannels.Get_Body() && Claims._BodyOwner != InFrom)
    { return ECk_ArticulationControlResult::ClaimLost; }
    if ((Claims._BodyOwner == InFrom) != InChannels.Get_Body())
    { return ECk_ArticulationControlResult::InvalidChannels; }
    for (auto Index = 0; Index < Claims._LimbOwners.Num(); ++Index)
    {
        if (Claims._LimbOwners[Index] == InFrom && NOT Indices.Contains(Index))
        { return ECk_ArticulationControlResult::InvalidChannels; }
    }

    for (const auto Index : Indices)
    { Claims._LimbOwners[Index] = InTo; }
    if (InChannels.Get_Body())
    { Claims._BodyOwner = InTo; }
    ++Claims._Revision;
    return ECk_ArticulationControlResult::Accepted;
}

auto
    FCk_ProceduralArticulation_Control::
    ReleaseClaims(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken,
        const FCk_ArticulationChannelSet& InChannels)
    -> ECk_ArticulationControlResult
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return ECk_ArticulationControlResult::InvalidSubject; }
    if (NOT Get_IsTokenValid(InToken) || (InChannels.Get_LimbIds().IsEmpty() && NOT InChannels.Get_Body()))
    { return ECk_ArticulationControlResult::InvalidChannels; }
    auto Subject = InSubject;
    auto& Claims = Subject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    const auto& Pose = InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    if (NOT Get_IsFaulted(InSubject)
        && (Pose.Get_PendingObservation() || Pose.Get_IssuedRevision() != Pose.Get_SettledRevision()
            || Pose._HasControlCandidate))
    { return ECk_ArticulationControlResult::Busy; }
    // A lost token's dead limbs were released when they died; the rest of its channels still release normally.
    const auto Lost = Claims._LostLimbOwners.Contains(InToken);
    auto Indices = TArray<int32>{};
    auto Seen = TSet<FName>{};
    for (const auto& Id : InChannels.Get_LimbIds())
    {
        const auto Index = Get_LimbIndex(InSubject, Id);
        if (Id.IsNone() || Seen.Contains(Id) || NOT Claims._LimbOwners.IsValidIndex(Index))
        { return ECk_ArticulationControlResult::ClaimLost; }
        Seen.Add(Id);
        if (Lost && Claims._LimbOwners[Index] == FCk_ArticulationControlToken{})
        { continue; }
        if (Claims._LimbOwners[Index] != InToken)
        { return ECk_ArticulationControlResult::ClaimLost; }
        Indices.Add(Index);
    }
    if (InChannels.Get_Body() && Claims._BodyOwner != InToken)
    { return ECk_ArticulationControlResult::ClaimLost; }
    if ((Claims._BodyOwner == InToken) != InChannels.Get_Body())
    { return ECk_ArticulationControlResult::InvalidChannels; }
    for (auto Index = 0; Index < Claims._LimbOwners.Num(); ++Index)
    {
        if (Claims._LimbOwners[Index] == InToken && NOT Indices.Contains(Index))
        { return ECk_ArticulationControlResult::InvalidChannels; }
    }

    for (const auto Index : Indices)
    { Claims._LimbOwners[Index] = FCk_ArticulationControlToken{}; }
    if (InChannels.Get_Body())
    { Claims._BodyOwner = FCk_ArticulationControlToken{}; }
    Claims._LostLimbOwners.Remove(InToken);
    ++Claims._Revision;
    return ECk_ArticulationControlResult::Accepted;
}

auto
    FCk_ProceduralArticulation_Control::
    ReleaseDeadClaims(
        FCk_Handle_ProceduralArticulation& InSubject)
    -> void
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return; }
    auto& Claims = InSubject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    const auto& Limbs = InSubject.Get<ck::FFragment_ProceduralArticulation_Bindings>().Get_Limbs();
    auto Released = false;
    for (auto Index = 0; Index < Claims._LimbOwners.Num(); ++Index)
    {
        const auto Owner = Claims._LimbOwners[Index];
        if (Owner == FCk_ArticulationControlToken{})
        { continue; }
        if (Limbs.IsValidIndex(Index) && Get_IsLimbLive(Limbs[Index]))
        { continue; }
        Claims._LimbOwners[Index] = FCk_ArticulationControlToken{};
        Claims._LostLimbOwners.AddUnique(Owner);
        Released = true;
    }
    if (Released)
    { ++Claims._Revision; }
}

auto
    FCk_ProceduralArticulation_Control::
    FailPendingPublication(
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCk_ArticulationControlToken InToken)
    -> bool
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject) || NOT Get_IsTokenValid(InToken)
        || Get_IsFaulted(InSubject))
    { return false; }
    auto Subject = InSubject;
    auto& Claims = Subject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    auto& Pose = Subject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    const auto OwnsChannel = Claims._BodyOwner == InToken || Claims._LimbOwners.Contains(InToken);
    // A default gait/rest publication can issue while a token owns channels. Its observer can stall just as a staged
    // action publication can, so the return deadline must be able to fault either pending path before releasing claims.
    const auto PublicationPending = Pose._HasControlCandidate || Pose._PendingObservation
        || Pose._IssuedRevision != Pose._SettledRevision;
    if (NOT OwnsChannel || NOT PublicationPending)
    { return false; }

    auto& Health = Subject.Get<ck::FFragment_ProceduralArticulation_Health>();
    Health._FaultObservedHandles.Reset();
    Health._FaultObservedPoses.Reset();
    const auto& Handles = Pose._HasControlCandidate ? Pose._ControlHandles : Pose._ExpectedHandles;
    for (const auto& Part : Handles)
    {
        if (NOT Get_IsBoundPartLive(Part, InSubject.ConvertToHandle()))
        { continue; }
        Health._FaultObservedHandles.Add(Part);
        Health._FaultObservedPoses.Add(UCk_Utils_Transform_UE::Get_EntityCurrentTransform(Part));
    }
    if (NOT Health._FaultObservedPoses.IsEmpty())
    { Health._ObservedPart = Health._FaultObservedPoses[0]; }
    Health._State = ECk_ProceduralArticulation_Health::Faulted;
    Health._Fault = ECk_ProceduralArticulation_Fault::SettlementTimeout;
    Pose._HasControlCandidate = false;
    Claims._BatchPublicationPending = false;
    return true;
}

auto
    FCk_ProceduralArticulation_Control::
    ObserveSettlement(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const FCk_ArticulationStageKey& InKey,
        uint64& OutSubjectRevision,
        FCk_ProceduralArticulation_Pose& OutActualPose)
    -> ECk_ArticulationSettlement
{
    using namespace ck_procedural_articulation_control;
    if (NOT Get_IsSubjectLive(InSubject))
    { return ECk_ArticulationSettlement::ClaimLost; }
    auto Subject = InSubject;
    auto& Claims = Subject.Get<ck::FFragment_ProceduralArticulation_Claims>();
    const auto Index = Claims._Tickets.IndexOfByPredicate([&](const FCk_ArticulationStageKey& InTicket)
    {
        return InTicket.Get_BatchNonce() == InKey.Get_BatchNonce()
            && InTicket.Get_Token() == InKey.Get_Token()
            && InTicket.Get_SampleSequence() == InKey.Get_SampleSequence();
    });
    if (Index == INDEX_NONE)
    { return ECk_ArticulationSettlement::ClaimLost; }
    if (Get_IsFaulted(InSubject))
    {
        Claims._Tickets.RemoveAt(Index);
        Claims._TicketChannels.RemoveAt(Index);
        Claims._TokenPoses.RemoveAt(Index);
        Claims._TicketRevisions.RemoveAt(Index);
        Claims._BatchStaged = NOT Claims._Tickets.IsEmpty();
        return ECk_ArticulationSettlement::Faulted;
    }
    const auto& Pose = InSubject.Get<ck::FFragment_ProceduralArticulation_Pose>();
    if (Pose.Get_PendingObservation() || Pose.Get_IssuedRevision() != Pose.Get_SettledRevision())
    { return ECk_ArticulationSettlement::Pending; }
    if (NOT Claims._TokenPoses.IsValidIndex(Index) || NOT Claims._TicketRevisions.IsValidIndex(Index)
        || Claims._TicketRevisions[Index] == 0)
    { return ECk_ArticulationSettlement::Faulted; }
    OutSubjectRevision = Claims._TicketRevisions[Index];
    OutActualPose = Claims._TokenPoses[Index];
    Claims._Tickets.RemoveAt(Index);
    Claims._TicketChannels.RemoveAt(Index);
    Claims._TokenPoses.RemoveAt(Index);
    Claims._TicketRevisions.RemoveAt(Index);
    Claims._BatchStaged = NOT Claims._Tickets.IsEmpty();
    return ECk_ArticulationSettlement::Settled;
}
