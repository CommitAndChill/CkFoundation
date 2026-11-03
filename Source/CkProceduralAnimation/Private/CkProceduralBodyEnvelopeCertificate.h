#pragma once

#include <CoreMinimal.h>
#include <array>

// Private arithmetic surface shared with the certificate's focused controls; not an authoring API.
namespace ck_body_envelope_certificate
{
    struct FInterval
    {
        double _Lo = 0.0;
        double _Hi = 0.0;
        bool _Valid = false;
    };
    using FVectorInterval = std::array<FInterval, 3>;
    using FMatrixInterval = std::array<FVectorInterval, 3>;

    CKPROCEDURALANIMATION_API auto Get_IsSupportedArithmetic() -> bool;
    CKPROCEDURALANIMATION_API auto Point(double InValue) -> FInterval;
    CKPROCEDURALANIMATION_API auto Add(FInterval InA, FInterval InB) -> FInterval;
    CKPROCEDURALANIMATION_API auto Subtract(FInterval InA, FInterval InB) -> FInterval;
    CKPROCEDURALANIMATION_API auto Multiply(FInterval InA, FInterval InB) -> FInterval;
    CKPROCEDURALANIMATION_API auto Divide(FInterval InA, FInterval InB) -> FInterval;
    CKPROCEDURALANIMATION_API auto SquareRoot(FInterval InValue) -> FInterval;
    CKPROCEDURALANIMATION_API auto NativeFloat(FInterval InValue) -> FInterval;
    CKPROCEDURALANIMATION_API auto Get_NativeMatrixBounds(const FQuat& InRepresentedQuaternion, FMatrixInterval& OutMatrix) -> void;
    CKPROCEDURALANIMATION_API auto Get_InverseBounds(const FMatrixInterval& InMatrix, FMatrixInterval& OutInverse) -> void;

    CKPROCEDURALANIMATION_API auto TryGet_CertifiedHalfExtents(
        const FVector& InAuthoredHalfExtents,
        float InMargin,
        const FTransform& InStart,
        const FTransform& InEnd,
        const FRotator& InOldRotation,
        const FVector& InOldHalfExtents) -> TOptional<FVector>;
}
