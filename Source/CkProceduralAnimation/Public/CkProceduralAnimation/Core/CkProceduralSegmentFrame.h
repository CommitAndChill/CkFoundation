#pragma once

#include "CoreMinimal.h"

namespace ck
{
    struct FProceduralSegmentFrameQuery
    {
        FVector From;
        FVector To;
        FVector Pole;
        TOptional<FQuat> PreviousRotation;
    };

    // The first solve follows the authored pole. A prior settled rotation supplies the continuous roll branch on
    // subsequent solves; a finite pole point can cross a lower segment's axis without the physical leg twisting.
    inline auto
        ComputeProceduralSegmentFrame(
            const FProceduralSegmentFrameQuery& InQuery)
        -> FQuat
    {
        const auto Along = (InQuery.To - InQuery.From).GetSafeNormal();
        if (Along.IsNearlyZero())
        { return FQuat::Identity; }

        auto TowardPole = FVector::VectorPlaneProject(InQuery.Pole - InQuery.From, Along).GetSafeNormal();
        if (TowardPole.IsNearlyZero() && InQuery.PreviousRotation.IsSet())
        { TowardPole = FVector::VectorPlaneProject(InQuery.PreviousRotation->GetRightVector(), Along).GetSafeNormal(); }
        if (TowardPole.IsNearlyZero())
        { TowardPole = FVector::VectorPlaneProject(FVector::UpVector, Along).GetSafeNormal(); }
        if (TowardPole.IsNearlyZero())
        { TowardPole = FVector::VectorPlaneProject(FVector::RightVector, Along).GetSafeNormal(); }

        const auto Authored = FRotationMatrix::MakeFromXY(Along, TowardPole).ToQuat();
        if (InQuery.PreviousRotation.IsSet())
        {
            const auto OtherBranch = (Authored * FQuat{FVector::ForwardVector, PI}).GetNormalized();
            if (InQuery.PreviousRotation->AngularDistance(OtherBranch) + 1.0e-8
                < InQuery.PreviousRotation->AngularDistance(Authored))
            { return OtherBranch; }
        }
        return Authored;
    }
}
