#include "CkProceduralAnimation/Core/CkProceduralLimbSolve.h"

#include "CkProceduralAnimation/Core/CkProceduralLegCurve.h"

#include <FABRIK.h>
#include <TwoBoneIK.h>

namespace ck
{
    auto
    SolveProceduralLimb(
        ECk_ProceduralRig_ChainSolver InSolver,
        TArrayView<const float> InLengths,
        const FVector& InHip,
        const FVector& InTarget,
        const FVector& InPole,
        const FQuat& InBodyRotation,
        TArrayView<FVector> OutJoints)
        -> bool
    {
        if (InLengths.Num() < 1 || InLengths.Num() > 8 || OutJoints.Num() != InLengths.Num() + 1)
        { return false; }

        OutJoints[0] = InHip;
        auto PoleDirection = (InPole - InHip).GetSafeNormal();
        if (PoleDirection.IsNearlyZero())
        { PoleDirection = InBodyRotation.GetAxisZ(); }

        if (InLengths.Num() == 1)
        {
            if (InSolver != ECk_ProceduralRig_ChainSolver::Auto)
            { return false; }
            auto Direction = (InTarget - InHip).GetSafeNormal();
            if (Direction.IsNearlyZero())
            { Direction = -InBodyRotation.GetAxisZ(); }
            OutJoints[1] = InHip + Direction * InLengths[0];
            return true;
        }

        if (InSolver == ECk_ProceduralRig_ChainSolver::Auto && InLengths.Num() == 2)
        {
            constexpr auto AllowStretching = false;
            const auto SeedJoint = InHip + PoleDirection * InLengths[0];
            auto Joint = SeedJoint;
            auto End = InTarget;
            AnimationCore::SolveTwoBoneIK(InHip, SeedJoint, InTarget, InPole, InTarget, Joint, End,
                InLengths[0], InLengths[1], AllowStretching, 1.0, 1.0);
            OutJoints[1] = Joint;
            OutJoints[2] = End;
            return true;
        }

        if (InSolver == ECk_ProceduralRig_ChainSolver::Fabrik)
        {
            constexpr auto Precision = 0.1;
            constexpr auto MaxIterations = int32{10};
            auto Chain = TArray<FFABRIKChainLink>{};
            Chain.Reserve(InLengths.Num() + 1);
            Chain.Emplace(InHip, 0.0, 0, 0);
            auto MaximumReach = 0.0;
            for (auto LinkIndex = 1; LinkIndex <= InLengths.Num(); ++LinkIndex)
            {
                const auto Parent = Chain[LinkIndex - 1].Position;
                const auto Length = InLengths[LinkIndex - 1];
                auto Direction = LinkIndex <= InLengths.Num() / 2 ? PoleDirection : (InTarget - Parent).GetSafeNormal();
                if (Direction.IsNearlyZero())
                { Direction = PoleDirection; }
                Chain.Emplace(Parent + Direction * Length, Length, LinkIndex, LinkIndex);
                MaximumReach += Length;
            }
            AnimationCore::SolveFabrik(Chain, InTarget, MaximumReach, Precision, MaxIterations);
            for (auto LinkIndex = 0; LinkIndex < Chain.Num(); ++LinkIndex)
            { OutJoints[LinkIndex] = Chain[LinkIndex].Position; }
            return true;
        }

        if (InSolver == ECk_ProceduralRig_ChainSolver::Auto || InSolver == ECk_ProceduralRig_ChainSolver::Curve)
        {
            for (const auto& Bend : {InPole - InHip, InBodyRotation.GetAxisZ(), InBodyRotation.GetAxisY()})
            {
                if (SolveProceduralLegCurve(InHip, InTarget, Bend, InLengths, OutJoints))
                { return true; }
            }
        }
        return false;
    }

    auto
    Get_ProceduralLimbSegmentRotation(
        const FVector& InFrom,
        const FVector& InTo,
        const FVector& InPole)
        -> FQuat
    {
        const auto Along = (InTo - InFrom).GetSafeNormal();
        if (Along.IsNearlyZero())
        { return FQuat::Identity; }
        auto TowardPole = FVector::VectorPlaneProject(InPole - InFrom, Along).GetSafeNormal();
        if (TowardPole.IsNearlyZero())
        { TowardPole = FVector::VectorPlaneProject(FVector::UpVector, Along).GetSafeNormal(); }
        if (TowardPole.IsNearlyZero())
        { TowardPole = FVector::VectorPlaneProject(FVector::RightVector, Along).GetSafeNormal(); }
        return FRotationMatrix::MakeFromXY(Along, TowardPole).ToQuat();
    }
}
