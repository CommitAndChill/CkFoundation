#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Utils.h"
#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Utils.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_RecordedStore.h"
#include "CkJolt/Body/CkJoltBody_Fragment_Data.h"

#include "UObject/StrongObjectPtr.h"

class UGameInstance;
class UWorld;

// A disposable production GamePreview world the editor plays one clip in. It advances only through its own world ticks:
// it never moves the engine's frame counter and never calls a processor directly.
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionSolverWorld
{
public:
    enum class EStatus : uint8
    {
        Idle,
        Preparing,
        Ready,
        Seeking,
        Finished,
        Cancelled,
        Error
    };

    FCk_ProceduralActionSolverWorld();
    ~FCk_ProceduralActionSolverWorld();
    FCk_ProceduralActionSolverWorld(const FCk_ProceduralActionSolverWorld&) = delete;
    auto
        operator=(
            const FCk_ProceduralActionSolverWorld&)
        -> FCk_ProceduralActionSolverWorld& = delete;

    auto
        StartClip(
            UCk_ProceduralActionPreview_Config* InConfig,
            UCk_ProceduralAction_Data* InClip,
            const TArray<FCk_ProceduralAction_SlotBinding>& InSlotBindings,
            uint64 InPreviewRevision)
        -> bool;
    auto
        SeekToStep(
            int64 InStep)
        -> bool;
    auto
        Resume()
        -> bool;
    auto
        TickOncePerEditorFrame()
        -> bool;
    auto
        CancelSeek()
        -> void;
    auto
        Reset()
        -> void;

    auto
        Get_Status() const
        -> EStatus
    {
        return _Status;
    }
    auto
        Get_Error() const
        -> const FString&
    {
        return _Error;
    }
    auto
        Get_Step() const
        -> int64
    {
        return _Step;
    }
    auto
        Get_SeekTargetStep() const
        -> int64
    {
        return _SeekTarget;
    }
    auto
        Get_World() const
        -> UWorld*
    {
        return _World;
    }
    auto
        Get_Subject() const
        -> FCk_Handle_ProceduralArticulation
    {
        return _Subject;
    }
    auto
        Get_Player() const
        -> FCk_Handle_ProceduralActionPlayer
    {
        return _Player;
    }
    auto
        Get_Action() const
        -> FCk_Handle_ProceduralAction
    {
        return _Action;
    }
    auto
        Get_IsFloorReady() const
        -> bool;
    auto
        Get_IsActionTerminal() const
        -> bool
    {
        return _Recording.Get_Terminal();
    }
    auto
        Get_ObservationHorizonStep() const
        -> int64
    {
        return _ObservationHorizonStep;
    }
    auto
        Get_ScenarioNextEventStep() const
        -> int64
    {
        return _ScenarioEvents.IsValidIndex(_NextScenarioEvent)
            ? _ScenarioEvents[_NextScenarioEvent].Get_Step() : INDEX_NONE;
    }
    auto
        Get_AreObstaclesReady() const
        -> bool;
    auto
        Get_Recording() const
        -> const FCk_ProceduralActionRecordedStore&
    {
        return _Recording;
    }

private:
    auto
        BuildWorld()
        -> bool;
    auto
        CreateAction()
        -> bool;
    auto
        Get_IsWalkingPrepared() const
        -> bool;
    auto
        DestroyWorld()
        -> void;
    auto
        OnPrePieEnded(
            bool InSimulating)
        -> void;
    auto
        ApplyScenarioEvent()
        -> bool;
    auto
        CreateFloor()
        -> bool;
    auto
        CheckScenarioProgress()
        -> bool;
    auto
        DrainReceipts()
        -> void;
    auto
        SetError(
            FString InError)
        -> bool;

    TStrongObjectPtr<UCk_ProceduralActionPreview_Config> _Config;
    TStrongObjectPtr<UCk_ProceduralAction_Data> _Clip;
    FCk_ProceduralAction_PlaySpec _PlaySpec;
    FCk_ProceduralActionRecordedStore _Recording;
    UWorld* _World = nullptr;
    UGameInstance* _GameInstance = nullptr;
    FCk_Handle_ProceduralArticulation _Subject;
    FCk_Handle_ProceduralActionPlayer _Player;
    FCk_Handle_ProceduralAction _Action;
    FCk_Handle_ProceduralGait _Gait;
    FCk_Handle _FloorEntity;
    FCk_Handle_JoltBody _FloorBody;
    TArray<FCk_Handle_JoltBody> _ObstacleBodies;
    TArray<FCk_ProceduralActionPreview_ScenarioEvent> _ScenarioEvents;
    TArray<FName> _PendingRecoveryIds;
    TArray<FName> _PendingDetachIds;
    FCk_Handle _Root;
    FCk_Handle _Scope;
    int64 _Step = 0;
    int32 _PreparingTicks = 0;
    int64 _SeekTarget = INDEX_NONE;
    int32 _NextScenarioEvent = 0;
    int64 _ObservationHorizonStep = 0;
    uint64 _PreviewRevision = 0;
    bool _ExpectedFloorPresent = false;
    uint64 _LastTickFrame = MAX_uint64;
    EStatus _Status = EStatus::Idle;
    FString _Error;
    FDelegateHandle _PrePieEndedHandle;
};
