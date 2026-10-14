#include "CkProceduralAnimationEditor/CkProceduralActionSolverWorld.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkJolt/Body/CkJoltBody_Utils.h"
#include "CkJolt/Subsystem/CkJolt_Subsystem.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkProceduralAnimation/CkProceduralAnimation_Utils.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Utils.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"

#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

namespace ck_procedural_action_solver_world
{
    constexpr auto FixedStep = ck::time::Hz(60);
    constexpr auto MaximumPreparationTicks = 600;

    auto
        IsFiniteRigid(
            const FTransform& InFrame)
        -> bool
    {
        return NOT InFrame.ContainsNaN() && InFrame.GetRotation().IsNormalized()
            && InFrame.GetScale3D().Equals(FVector::OneVector, 1.0e-6);
    }

}

FCk_ProceduralActionSolverWorld::FCk_ProceduralActionSolverWorld()
{
    _PrePieEndedHandle = FEditorDelegates::PrePIEEnded.AddRaw(
        this, &FCk_ProceduralActionSolverWorld::OnPrePieEnded);
}

FCk_ProceduralActionSolverWorld::~FCk_ProceduralActionSolverWorld()
{
    FEditorDelegates::PrePIEEnded.Remove(_PrePieEndedHandle);
    Reset();
}

auto
    FCk_ProceduralActionSolverWorld::
    OnPrePieEnded(
        bool)
    -> void
{
    if (_World == nullptr)
    { return; }
    DestroyWorld();
    _Status = EStatus::Cancelled;
    _SeekTarget = INDEX_NONE;
    _Error = TEXT("PIE ended; the production preview world was released. Restart Solver to replay from the authored clip.");
}

auto
    FCk_ProceduralActionSolverWorld::
    SetError(
        FString InError)
    -> bool
{
    _Error = MoveTemp(InError);
    _Status = EStatus::Error;
    return false;
}

auto
    FCk_ProceduralActionSolverWorld::
    StartClip(
        UCk_ProceduralActionPreview_Config* InConfig,
        UCk_ProceduralAction_Data* InClip,
        const TArray<FCk_ProceduralAction_SlotBinding>& InSlotBindings,
        uint64 InPreviewRevision)
    -> bool
{
    Reset();
    if (InConfig == nullptr || InClip == nullptr)
    { return SetError(TEXT("Select an authored clip and a separate preview subject config before starting Solver.")); }
    auto Compiled = ck::procedural_action::FCompiledClip{};
    auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{};
    if (NOT ck::procedural_action::Compile(*InClip, Compiled, Diagnostics))
    { return SetError(TEXT("The authored draft is invalid; correct its compile diagnostics before starting Solver.")); }
    _Config.Reset(InConfig);
    if (NOT InConfig->ValidateScenario(_Error))
    { return SetError(_Error); }
    _ScenarioEvents = InConfig->Get_ScenarioEvents();
    _ObservationHorizonStep = InConfig->Get_ObservationHorizonStep();
    _Clip.Reset(InClip);
    _PlaySpec = FCk_ProceduralAction_PlaySpec{TSoftObjectPtr<UCk_ProceduralAction_Data>{InClip}};
    _PlaySpec.Set_SlotBindings(InSlotBindings);
    _PlaySpec.Set_Delivery(ECk_ProceduralAction_Delivery::Explicit);
    _PreviewRevision = InPreviewRevision;
    if (NOT BuildWorld())
    {
        DestroyWorld();
        return false;
    }
    const auto ActionState = ck::IsValid(_Action)
        ? UCk_Utils_ProceduralAction_UE::Get_State(_Action)
        : ECk_ProceduralAction_State::PendingAdmission;
    _Status = ActionState == ECk_ProceduralAction_State::Playing ? EStatus::Ready : EStatus::Preparing;
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    BuildWorld()
    -> bool
{
    if (NOT _Config.IsValid() || GEngine == nullptr)
    { return SetError(TEXT("Preview config or editor engine is unavailable.")); }
    _Recording.Reset({}, {});
    const auto& Config = *_Config.Get();
    if (NOT ck_procedural_action_solver_world::IsFiniteRigid(Config.Get_RootFrame())
        || NOT ck_procedural_action_solver_world::IsFiniteRigid(Config.Get_PresentationFrame()))
    { return SetError(TEXT("Preview root and presentation frames must be rigid and finite.")); }
    auto SurfaceError = FString{};
    if (NOT Config.ValidateSurface(SurfaceError))
    { return SetError(SurfaceError); }
    const auto& Layouts = Config.Get_Layout().Get_Limbs();
    const auto& PreviewBindings = Config.Get_LimbBindings();
    auto AppearanceError = FString{};
    if (NOT Config.ValidateAppearance(AppearanceError))
    { return SetError(AppearanceError); }
    auto PlayerSpec = FCk_ProceduralActionPlayer_Spec{};
    PlayerSpec.Set_Tuning(Config.Get_ActionTuning());
    auto InvalidTuningField = FName{};
    if (NOT ck::procedural_action::ValidateTuning(PlayerSpec.Get_Tuning(), InvalidTuningField))
    { return SetError(FString::Printf(TEXT("Preview Action Tuning has invalid %s."),
        *InvalidTuningField.ToString())); }
    const auto Walking = Config.Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking;
    const auto* Rig = Config.Get_WalkingRig().Get();
    const auto* Gait = Config.Get_WalkingGait().Get();
    if (Walking && (Rig == nullptr || Gait == nullptr || NOT Config.Get_HasStaticFloor()))
    { return SetError(TEXT("Walking preview requires a canonical rig, gait preset, and static Jolt floor.")); }
    if (NOT Walking && Layouts.Num() != PreviewBindings.Num())
    { return SetError(TEXT("Preview binding count must match the articulation topology.")); }
    auto SeenIds = TSet<FName>{};
    for (const auto& Binding : PreviewBindings)
    {
        if (Binding.Get_LimbId().IsNone() || SeenIds.Contains(Binding.Get_LimbId()))
        { return SetError(TEXT("Preview limb bindings need distinct, named limb IDs.")); }
        SeenIds.Add(Binding.Get_LimbId());
        const auto* Layout = Walking
            ? Rig->Get_Legs().FindByPredicate([&Binding](const auto& InLeg)
                { return InLeg.Get_Id() == Binding.Get_LimbId(); })
            : nullptr;
        const auto* StandaloneLayout = Walking ? nullptr
            : Layouts.FindByPredicate([&Binding](const auto& InLimb)
                { return InLimb.Get_Id() == Binding.Get_LimbId(); });
        const auto ExpectedCount = Walking
            ? Layout != nullptr ? Layout->Get_Chain().Get_SegmentLengths().Num() : INDEX_NONE
            : StandaloneLayout != nullptr ? StandaloneLayout->Get_SegmentLengths().Num() : INDEX_NONE;
        if (ExpectedCount == INDEX_NONE || Binding.Get_SegmentFrames().Num() != ExpectedCount)
        { return SetError(FString::Printf(TEXT("Preview limb %s has missing or mismatched segment frames."),
            *Binding.Get_LimbId().ToString())); }
        if (NOT ck_procedural_action_solver_world::IsFiniteRigid(Binding.Get_TerminalFrame()))
        { return SetError(TEXT("Preview terminal frames must be rigid and finite.")); }
        for (const auto& Frame : Binding.Get_SegmentFrames())
        {
            if (NOT ck_procedural_action_solver_world::IsFiniteRigid(Frame))
            { return SetError(TEXT("Preview segment frames must be rigid and finite.")); }
        }
    }
    if (NOT Walking)
    {
        for (const auto& Layout : Layouts)
        {
            if (NOT SeenIds.Contains(Layout.Get_Id()))
            { return SetError(TEXT("Each standalone articulation limb needs a preview binding.")); }
        }
    }

    _GameInstance = NewObject<UGameInstance>(GEngine);
    auto Initialization = UWorld::InitializationValues{};
    Initialization.CreatePhysicsScene(true).ShouldSimulatePhysics(false)
        .EnableTraceCollision(true).CreateNavigation(false).CreateAISystem(false)
        .SetDefaultGameMode(AGameModeBase::StaticClass());
    _World = UWorld::CreateWorld(EWorldType::GamePreview, false,
        TEXT("CkProceduralActionSolverWorld"), nullptr, false, ERHIFeatureLevel::Num,
        &Initialization, true);
    if (_GameInstance == nullptr || _World == nullptr)
    { return SetError(TEXT("Could not create a private production preview world.")); }
    _World->SetShouldTick(false);
    _World->AddToRoot();
    if (NOT UCk_EcsWorld_Subsystem_UE::Mark_ProductionPreviewWorld(*_World))
    { return SetError(TEXT("Could not mark the private GamePreview world before subsystem initialization.")); }
    _World->InitWorld(Initialization);
    auto& Context = GEngine->CreateNewWorldContext(EWorldType::GamePreview);
    Context.OwningGameInstance = _GameInstance;
    _World->SetGameInstance(_GameInstance);
    Context.SetCurrentWorld(_World);
    _GameInstance->Init();

    const auto Url = FURL{};
    _World->SetGameMode(Url);
    if (_World->GetAuthGameMode() == nullptr
        || _World->GetAuthGameMode()->GetClass() != AGameModeBase::StaticClass())
    { return SetError(TEXT("Private Game world did not create the isolated base GameMode.")); }
    _World->InitializeActorsForPlay(Url);
    _World->BeginPlay();
    if (NOT _World->HasBegunPlay()
        || _World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>() == nullptr
        || _World->GetSubsystem<UCk_Jolt_Subsystem>() == nullptr)
    { return SetError(TEXT("Production ECS/Jolt world did not begin play.")); }

    _Root = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(_World);
    auto RootTransform = UCk_Utils_Transform_UE::Add(_Root, Config.Get_RootFrame(),
        ECk_Replication::DoesNotReplicate);
    if (ck::Is_NOT_Valid(RootTransform))
    { return SetError(TEXT("Could not compose the preview root transform.")); }

    auto Bindings = TArray<FCk_ProceduralLimb_Binding>{};
    auto Chains = TArray<FCk_ProceduralWalker_LegChain>{};
    Bindings.Reserve(PreviewBindings.Num());
    Chains.Reserve(PreviewBindings.Num());
    for (const auto& Preview : PreviewBindings)
    {
        auto Segments = TArray<FCk_Handle_Transform>{};
        Segments.Reserve(Preview.Get_SegmentFrames().Num());
        for (const auto& Frame : Preview.Get_SegmentFrames())
        {
            auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(_Root);
            auto Segment = UCk_Utils_Transform_UE::Add(Entity, Frame,
                ECk_Replication::DoesNotReplicate);
            if (ck::Is_NOT_Valid(Segment))
            { return SetError(TEXT("Could not compose a preview limb segment.")); }
            Segments.Add(Segment);
        }
        auto TerminalEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(_Root);
        auto Terminal = UCk_Utils_Transform_UE::Add(TerminalEntity,
            Preview.Get_TerminalFrame(), ECk_Replication::DoesNotReplicate);
        if (ck::Is_NOT_Valid(Terminal))
        { return SetError(TEXT("Could not compose a preview limb terminal.")); }
        if (Walking)
        {
            auto ChainSpec = FCk_ProceduralRig_Spec{MoveTemp(Segments)};
            ChainSpec.Set_Foot(Terminal);
            Chains.Add(FCk_ProceduralWalker_LegChain{Preview.Get_LimbId(), MoveTemp(ChainSpec)});
        }
        else
        {
            auto Binding = FCk_ProceduralLimb_Binding{Preview.Get_LimbId(), MoveTemp(Segments)};
            Binding.Set_Terminal(Terminal);
            Bindings.Add(MoveTemp(Binding));
        }
    }
    if (Walking)
    {
        const auto Walker = UCk_Utils_ProceduralAnimation_UE::Add_Walker(RootTransform, Rig, Gait, Chains);
        _Gait = Walker.Get_Gait();
        _Subject = UCk_Utils_ProceduralArticulation_UE::Cast(_Root);
        // A walker's body presentation rides a body pose on its gait, as a game walker composes it; a clip's body
        // channel offsets that presentation.
        auto PresentationEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(_Root);
        auto Presentation = UCk_Utils_Transform_UE::Add(PresentationEntity,
            Config.Get_PresentationFrame(), ECk_Replication::DoesNotReplicate);
        if (ck::Is_NOT_Valid(Presentation) || ck::Is_NOT_Valid(_Gait))
        { return SetError(TEXT("Could not compose the walking preview's body presentation.")); }
        if (ck::Is_NOT_Valid(UCk_Utils_ProceduralBodyPose_UE::Add(_Gait, FCk_ProceduralBodyPose_Spec{Presentation})))
        { return SetError(TEXT("Production BodyPose rejected the walking preview's body presentation.")); }
    }
    else
    {
        auto PresentationEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(_Root);
        auto Presentation = UCk_Utils_Transform_UE::Add(PresentationEntity,
            Config.Get_PresentationFrame(), ECk_Replication::DoesNotReplicate);
        if (ck::Is_NOT_Valid(Presentation))
        { return SetError(TEXT("Could not compose the preview body presentation.")); }
        auto Spec = FCk_ProceduralArticulation_Spec{Config.Get_Layout(), MoveTemp(Bindings)};
        Spec.Set_Presentation(Presentation);
        Spec.Set_ClearanceSettings(Config.Get_StandaloneClearanceSettings());
        _Subject = UCk_Utils_ProceduralArticulation_UE::Add(RootTransform, Spec);
    }
    if (ck::Is_NOT_Valid(_Subject))
    { return SetError(TEXT("Production Articulation rejected the preview subject.")); }
    if (Walking)
    { PlayerSpec.Set_Locomotion(Config.Get_WalkingLocomotion()); }
    _Player = UCk_Utils_ProceduralActionPlayer_UE::Add(_Subject, PlayerSpec);
    if (ck::Is_NOT_Valid(_Player))
    { return SetError(TEXT("Production ActionPlayer rejected the preview subject.")); }
    _Scope = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(_Root);

    if (Config.Get_HasStaticFloor() && NOT CreateFloor())
    { return false; }
    _ObstacleBodies.Reserve(Config.Get_StaticObstacles().Num());
    for (auto Index = 0; Index < Config.Get_StaticObstacles().Num(); ++Index)
    {
        const auto& Obstacle = Config.Get_StaticObstacles()[Index];
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(_World);
        if (ck::Is_NOT_Valid(UCk_Utils_Transform_UE::Add(Entity, Obstacle.Get_Frame(),
            ECk_Replication::DoesNotReplicate)))
        { return SetError(FString::Printf(TEXT("Could not compose preview obstacle %d."), Index)); }
        auto Shape = FCk_Jolt_ShapeDimensions{ECk_Jolt_ShapeType::Box};
        Shape.Set_HalfExtents(Obstacle.Get_HalfExtents());
        auto Spec = FCk_JoltBody_Spec{ECk_JoltBody_ShapeSource::ExplicitShape};
        Spec.Set_ShapeDimensions(Shape);
        Spec.Set_MotionType(ECk_MotionType::Static);
        Spec.Set_CollisionProfileName(TEXT("BlockAll"));
        const auto Body = UCk_Utils_JoltBody_UE::Add(Entity, Spec);
        if (ck::Is_NOT_Valid(Body))
        { return SetError(FString::Printf(TEXT("Production Jolt rejected preview obstacle %d."), Index)); }
        _ObstacleBodies.Add(Body);
    }

    _Step = 0;
    _PreparingTicks = 0;
    _NextScenarioEvent = 0;
    _ExpectedFloorPresent = Config.Get_HasStaticFloor();
    _PendingRecoveryIds.Reset();
    _PendingDetachIds.Reset();
    return Walking || Config.Get_HasStaticFloor() || NOT Config.Get_StaticObstacles().IsEmpty()
        || CreateAction();
}

auto
    FCk_ProceduralActionSolverWorld::
    CreateFloor()
    -> bool
{
    if (_World == nullptr || NOT _Config.IsValid() || ck::IsValid(_FloorEntity))
    { return SetError(TEXT("Scenario cannot create a duplicate or unowned preview floor.")); }
    _FloorEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(_World);
    if (ck::Is_NOT_Valid(UCk_Utils_Transform_UE::Add(_FloorEntity,
        _Config->Get_StaticFloorFrame(), ECk_Replication::DoesNotReplicate)))
    { return SetError(TEXT("Could not compose the preview floor transform.")); }
    auto Shape = FCk_Jolt_ShapeDimensions{ECk_Jolt_ShapeType::Box};
    Shape.Set_HalfExtents(_Config->Get_StaticFloorHalfExtents());
    auto FloorSpec = FCk_JoltBody_Spec{ECk_JoltBody_ShapeSource::ExplicitShape};
    FloorSpec.Set_ShapeDimensions(Shape);
    FloorSpec.Set_MotionType(ECk_MotionType::Static);
    FloorSpec.Set_CollisionProfileName(TEXT("BlockAll"));
    _FloorBody = UCk_Utils_JoltBody_UE::Add(_FloorEntity, FloorSpec);
    if (ck::Is_NOT_Valid(_FloorBody))
    { return SetError(TEXT("Production Jolt rejected the preview floor.")); }
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    CreateAction()
    -> bool
{
    _Action = UCk_Utils_ProceduralAction_UE::Create_FromClip(_Scope, _Player, _PlaySpec);
    if (ck::Is_NOT_Valid(_Action))
    { return SetError(TEXT("Production action rejected the preview clip.")); }
    const auto& Config = *_Config.Get();
    _Recording.Reset(UCk_Utils_ProceduralAction_UE::Get_Identity(_Action),
        FCk_ProceduralActionRecordedStore::FProvenance{
            Config.GetPathName(), _Clip.IsValid() ? _Clip->GetPathName() : FString{},
            TEXT("Clip playback"),
            ck_procedural_action_solver_world::FixedStep, _PreviewRevision});
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    IsWalkingPrepared() const
    -> bool
{
    if (NOT _Config.IsValid() || ck::Is_NOT_Valid(_Gait) || NOT GetIsFloorReady()
        || NOT GetAreObstaclesReady())
    { return false; }
    return UCk_Utils_ProceduralArticulation_UE::Get_SettledRevision(_Subject) > 0
        && UCk_Utils_ProceduralGait_UE::Get_PlantedCount(_Gait) == _Config->Get_WalkingRig()->Get_Legs().Num()
        && UCk_Utils_ProceduralGait_UE::Get_TrustedContactCount(_Gait)
            >= FMath::Max(1, _Config->Get_WalkingLocomotion().Get_Support().Get_MinimumTrustedContacts());
}

auto
    FCk_ProceduralActionSolverWorld::
    SeekToStep(
        int64 InStep)
    -> bool
{
    if (NOT _Config.IsValid() || InStep < 0)
    { _Error = TEXT("Seek needs an active preview and a nonnegative target step."); return false; }
    if (_ObservationHorizonStep > 0 && InStep > _ObservationHorizonStep)
    { _Error = TEXT("Seek target exceeds the preview observation horizon."); return false; }
    _Error.Reset();
    if (InStep < _Step)
    {
        DestroyWorld();
        if (NOT BuildWorld())
        {
            DestroyWorld();
            return false;
        }
    }
    _SeekTarget = InStep;
    const auto ActionReady = ck::IsValid(_Action)
        && UCk_Utils_ProceduralAction_UE::Get_State(_Action) == ECk_ProceduralAction_State::Playing;
    if (_Recording.Get_Terminal())
    {
        _Status = _SeekTarget != _Step ? EStatus::Seeking
            : _ObservationHorizonStep == 0 || _Step >= _ObservationHorizonStep
                ? EStatus::Finished : EStatus::Ready;
    }
    else
    {
        _Status = ActionReady
            ? _SeekTarget == _Step ? EStatus::Ready : EStatus::Seeking
            : EStatus::Preparing;
    }
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    Resume()
    -> bool
{
    if (_World == nullptr || _Status == EStatus::Error || _Status == EStatus::Finished)
    { return false; }
    _SeekTarget = INDEX_NONE;
    const auto ActionReady = ck::IsValid(_Action)
        && UCk_Utils_ProceduralAction_UE::Get_State(_Action) == ECk_ProceduralAction_State::Playing;
    _Status = ActionReady || (_Recording.Get_Terminal() && _ObservationHorizonStep > _Step)
        ? EStatus::Ready : EStatus::Preparing;
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    GetIsFloorReady() const
    -> bool
{
    return _Config.IsValid() && _Config->Get_HasStaticFloor()
        && ck::IsValid(_FloorBody) && UCk_Utils_JoltBody_UE::Get_IsBodyAdded(_FloorBody);
}

auto
    FCk_ProceduralActionSolverWorld::
    GetAreObstaclesReady() const
    -> bool
{
    if (NOT _Config.IsValid() || _ObstacleBodies.Num() != _Config->Get_StaticObstacles().Num())
    { return false; }
    for (const auto& Body : _ObstacleBodies)
    {
        if (ck::Is_NOT_Valid(Body) || NOT UCk_Utils_JoltBody_UE::Get_IsBodyAdded(Body))
        { return false; }
    }
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    ApplyScenarioEvent()
    -> bool
{
    if (NOT _ScenarioEvents.IsValidIndex(_NextScenarioEvent)
        || _ScenarioEvents[_NextScenarioEvent].Get_Step() != _Step + 1)
    { return true; }
    const auto& Event = _ScenarioEvents[_NextScenarioEvent];
    switch (Event.Get_Kind())
    {
    case ECk_ProceduralActionPreview_ScenarioEventKind::RemoveFloor:
        if (ck::Is_NOT_Valid(_FloorEntity) || NOT GetIsFloorReady())
        { return SetError(TEXT("Scenario RemoveFloor reached its step before the owned Jolt floor was ready.")); }
        UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(_FloorEntity);
        _FloorEntity = {};
        _FloorBody = {};
        _ExpectedFloorPresent = false;
        break;
    case ECk_ProceduralActionPreview_ScenarioEventKind::RestoreFloor:
        if (NOT CreateFloor())
        { return false; }
        _ExpectedFloorPresent = true;
        break;
    case ECk_ProceduralActionPreview_ScenarioEventKind::RecoverLimbs:
        if (ck::Is_NOT_Valid(_Subject) || NOT GetIsFloorReady())
        { return SetError(TEXT("Scenario RecoverLimbs needs the restored, settled Jolt floor and a live walking subject.")); }
        for (const auto Id : Event.Get_LimbIds())
        {
            if (UCk_Utils_ProceduralArticulation_UE::Get_LimbControl(_Subject, Id).Get_State()
                != ECk_ProceduralLimbControl_State::Unplanted)
            {
                return SetError(FString::Printf(TEXT("Scenario RecoverLimbs needs named leg %s to be Unplanted before its public request."),
                    *Id.ToString()));
            }
        }
        UCk_Utils_ProceduralArticulation_UE::Request_RecoverUnplantedLimbs(_Subject,
            FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs{}, {});
        _PendingRecoveryIds.Append(Event.Get_LimbIds());
        break;
    case ECk_ProceduralActionPreview_ScenarioEventKind::DetachLimb:
    {
        const auto LimbId = Event.Get_LimbIds()[0];
        auto Leg = UCk_Utils_ProceduralLeg_UE::TryGet_Leg(_Root, LimbId);
        if (ck::Is_NOT_Valid(Leg))
        { return SetError(FString::Printf(TEXT("Scenario DetachLimb cannot find live leg %s."), *LimbId.ToString())); }
        UCk_Utils_ProceduralLeg_UE::Request_Detach(Leg, FCk_Request_ProceduralLeg_Detach{}, {});
        _PendingDetachIds.Add(LimbId);
        break;
    }
    default:
        return SetError(TEXT("Scenario has an invalid event kind."));
    }
    ++_NextScenarioEvent;
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    CheckScenarioProgress()
    -> bool
{
    _PendingRecoveryIds.RemoveAll([this](const FName InId)
    {
        const auto Observation = UCk_Utils_ProceduralArticulation_UE::Get_LimbControl(_Subject, InId);
        return Observation.Get_State() == ECk_ProceduralLimbControl_State::Default
            && Observation.Get_TrustedSupport();
    });
    _PendingDetachIds.RemoveAll([this](const FName InId)
    {
        const auto Leg = UCk_Utils_ProceduralLeg_UE::TryGet_Leg(_Root, InId);
        return ck::IsValid(Leg)
            && UCk_Utils_ProceduralLeg_UE::Get_Status(Leg) == ECk_ProceduralLeg_Status::Detached;
    });
    if (_ObservationHorizonStep > 0 && _Step >= _ObservationHorizonStep)
    {
        if (_ExpectedFloorPresent && NOT GetIsFloorReady())
        { return SetError(TEXT("Scenario restored floor did not become a production Jolt body before ObservationHorizonStep.")); }
        if (NOT _PendingRecoveryIds.IsEmpty())
        {
            const auto Observation = UCk_Utils_ProceduralArticulation_UE::Get_LimbControl(
                _Subject, _PendingRecoveryIds[0]);
            return SetError(FString::Printf(TEXT("Scenario RecoverLimbs for %s did not reach trusted Default support before ObservationHorizonStep (state %d, trusted %d, reason %s)."),
                *_PendingRecoveryIds[0].ToString(), static_cast<int32>(Observation.Get_State()),
                Observation.Get_TrustedSupport() ? 1 : 0, *Observation.Get_Reason().ToString()));
        }
        if (NOT _PendingDetachIds.IsEmpty())
        {
            const auto Leg = UCk_Utils_ProceduralLeg_UE::TryGet_Leg(_Root, _PendingDetachIds[0]);
            return SetError(FString::Printf(TEXT("Scenario DetachLimb for %s did not reach Detached before ObservationHorizonStep (leg valid %d, status %d)."),
                *_PendingDetachIds[0].ToString(), ck::IsValid(Leg) ? 1 : 0,
                ck::IsValid(Leg) ? static_cast<int32>(UCk_Utils_ProceduralLeg_UE::Get_Status(Leg)) : -1));
        }
    }
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    TickOncePerEditorFrame()
    -> bool
{
    if (_World == nullptr || _Status == EStatus::Idle || _Status == EStatus::Error
        || _Status == EStatus::Cancelled
        || _Status == EStatus::Finished
        || _LastTickFrame == GFrameCounter)
    { return false; }
    if (_Status == EStatus::Preparing)
    {
        _LastTickFrame = GFrameCounter;
        _World->Tick(LEVELTICK_All,
            static_cast<float>(ck_procedural_action_solver_world::FixedStep.Get_Seconds()));
        ++_PreparingTicks;
        if (_Recording.Get_Identity().Get_Value() != 0)
        {
            DrainReceipts();
            if (_Status == EStatus::Error)
            { return false; }
            const auto ActionState = UCk_Utils_ProceduralAction_UE::Get_State(_Action);
            if (ActionState == ECk_ProceduralAction_State::Playing)
            {
                _Status = _SeekTarget != INDEX_NONE && _SeekTarget > _Step
                    ? EStatus::Seeking : EStatus::Ready;
            }
            else if (ActionState == ECk_ProceduralAction_State::Finished)
            { _Status = _ObservationHorizonStep > _Step ? EStatus::Ready : EStatus::Finished; }
        }
        else if ((_Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking
                && IsWalkingPrepared())
            || (_Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Standalone
                && (NOT _Config->Get_HasStaticFloor() || GetIsFloorReady())
                && GetAreObstaclesReady()))
        {
            if (NOT CreateAction())
            { return false; }
        }
        if (_Status == EStatus::Preparing
            && _PreparingTicks >= ck_procedural_action_solver_world::MaximumPreparationTicks)
        { return SetError(TEXT("Preview Jolt surfaces or subject did not settle and admit its action within 10 seconds; check rig, gait, floor, obstacles, and bindings.")); }
        return true;
    }
    if (_SeekTarget != INDEX_NONE && _Step >= _SeekTarget)
    { return false; }
    if (NOT ApplyScenarioEvent())
    { return false; }
    _LastTickFrame = GFrameCounter;
    _World->Tick(LEVELTICK_All,
        static_cast<float>(ck_procedural_action_solver_world::FixedStep.Get_Seconds()));
    ++_Step;
    DrainReceipts();
    if (_Status == EStatus::Error)
    { return false; }
    if (NOT CheckScenarioProgress())
    { return false; }
    if (_ObservationHorizonStep > 0 && _Step >= _ObservationHorizonStep)
    { _Status = EStatus::Finished; }
    else if (_SeekTarget != INDEX_NONE && _Step >= _SeekTarget)
    { _Status = _Recording.Get_Terminal() && _ObservationHorizonStep == 0
        ? EStatus::Finished : EStatus::Ready; }
    else if (_SeekTarget != INDEX_NONE)
    { _Status = EStatus::Seeking; }
    else if (_Recording.Get_Terminal() && _ObservationHorizonStep == 0)
    { _Status = EStatus::Finished; }
    return true;
}

auto
    FCk_ProceduralActionSolverWorld::
    DrainReceipts()
    -> void
{
    if (_Recording.Get_Terminal())
    { return; }
    if (ck::Is_NOT_Valid(_Player))
    { SetError(TEXT("Production ActionPlayer became invalid while draining receipts.")); return; }
    constexpr auto MaximumPagesPerFrame = 4;
    const auto CopiedBeforeDrain = _Recording.Get_LastCopiedSequence();
    for (auto PageIndex = 0; PageIndex < MaximumPagesPerFrame; ++PageIndex)
    {
        const auto Before = _Recording.Get_LastCopiedSequence();
        const auto Page = UCk_Utils_ProceduralActionPlayer_UE::Read_Receipts(
            _Player, _Recording.Get_Identity(), Before, 64);
        if (NOT _Recording.AppendPage(Page))
        {
            SetError(FString::Printf(TEXT("Receipt page read failed with result %d: %s"),
                static_cast<int32>(Page.Get_Result()), *_Recording.Get_LastError()));
            return;
        }
        if (NOT Page.Get_HasMore())
        { break; }
        if (_Recording.Get_LastCopiedSequence() <= Before)
        { SetError(TEXT("Receipt pagination reported more records without advancing its cursor.")); return; }
    }
    if (_Recording.Get_LastCopiedSequence() > CopiedBeforeDrain)
    {
        UCk_Utils_ProceduralActionPlayer_UE::Request_ConsumeReceipts(_Player,
            FCk_Request_ProceduralActionPlayer_ConsumeReceipts{
                _Recording.Get_Identity(), _Recording.Get_LastCopiedSequence()}, {});
    }
}

auto
    FCk_ProceduralActionSolverWorld::
    CancelSeek()
    -> void
{
    _SeekTarget = INDEX_NONE;
    if (_Status == EStatus::Seeking)
    { _Status = EStatus::Cancelled; }
}

auto
    FCk_ProceduralActionSolverWorld::
    DestroyWorld()
    -> void
{
    _Action = {};
    _Gait = {};
    _FloorEntity = {};
    _FloorBody = {};
    _ObstacleBodies.Reset();
    _Player = {};
    _Subject = {};
    _Scope = {};
    _Root = {};
    if (_World == nullptr)
    { _GameInstance = nullptr; return; }
    auto* World = _World;
    _World = nullptr;
    if (World->HasBegunPlay())
    {
        World->BeginTearingDown();
        World->EndPlay(EEndPlayReason::Quit);
    }
    World->RemoveFromRoot();
    if (_GameInstance != nullptr)
    { _GameInstance->Shutdown(); }
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    _GameInstance = nullptr;
}

auto
    FCk_ProceduralActionSolverWorld::
    Reset()
    -> void
{
    DestroyWorld();
    _Config.Reset();
    _Clip.Reset();
    _PlaySpec = {};
    _Recording.Reset({}, {});
    _ScenarioEvents.Reset();
    _PendingRecoveryIds.Reset();
    _PendingDetachIds.Reset();
    _Step = 0;
    _PreparingTicks = 0;
    _SeekTarget = INDEX_NONE;
    _NextScenarioEvent = 0;
    _ObservationHorizonStep = 0;
    _PreviewRevision = 0;
    _ExpectedFloorPresent = false;
    _Status = EStatus::Idle;
    _Error.Reset();
}
