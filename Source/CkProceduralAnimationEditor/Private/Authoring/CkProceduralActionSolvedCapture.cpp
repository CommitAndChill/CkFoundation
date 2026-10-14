#include "CkProceduralAnimationEditor/CkProceduralActionSolvedCapture.h"

#include "CkProceduralAnimationEditor/CkProceduralActionSolverWorld.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_ProceduralActionSolvedSnapshot::
    Make(
        const FCk_ProceduralActionSolverWorld& InSolver)
    -> FCk_ProceduralActionSolvedSnapshot
{
    auto Snapshot = FCk_ProceduralActionSolvedSnapshot{};
    const auto& Recording = InSolver.Get_Recording();
    Snapshot._PreviewRevision = Recording.Get_Provenance().Get_PreviewRevision();
    // A seek whose target step is reached is satisfied, not pending: scrubbing or frame-stepping to a pose and capturing
    // it is the authoring loop. A Finished preview never reads Ready, because its subject has begun returning.
    const auto SeekTarget = InSolver.Get_SeekTargetStep();
    Snapshot._Ready = InSolver.Get_Status() == FCk_ProceduralActionSolverWorld::EStatus::Ready
        && (SeekTarget == INDEX_NONE || SeekTarget == InSolver.Get_Step());

    const auto& Records = Recording.Get_Records();
    for (auto Index = Records.Num() - 1; Index >= 0; --Index)
    {
        const auto& Record = Records[Index];
        if (Record.Get_Kind() != ECk_ProceduralAction_RecordKind::AcceptedPose
            || NOT Record.Get_Receipt().Get_HasAcceptedPose())
        { continue; }

        Snapshot._Pose = Record.Get_Receipt().Get_CurrentPose();
        const auto Subject = InSolver.Get_Subject();
        Snapshot._Settled = ck::IsValid(Subject)
            && Record.Get_Receipt().Get_Stamp().Get_SubjectPoseRevision()
                == UCk_Utils_ProceduralArticulation_UE::Get_SettledRevision(Subject);
        break;
    }
    return Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
