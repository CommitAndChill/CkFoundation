#include "CkProceduralBodyEnvelopeCertificate.h"

#include "CkJolt/CkJolt_Utils.h"

#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <Jolt/Physics/PhysicsSettings.h>
#if defined(_MSC_VER) && defined(_M_X64) && NOT defined(__clang__)
#include <float.h>
#include <immintrin.h>
#pragma float_control(precise, on, push)
#define CK_CERT_NOINLINE __declspec(noinline)
#else
#define CK_CERT_NOINLINE FORCENOINLINE
#endif

// No expression crosses a primitive call: each non-inlined primitive rounds just one operation.
// The scoped precise region also contains every comparison and the complete certificate decision.
namespace ck_body_envelope_certificate
{
    namespace
    {
        constexpr auto MinimumDouble = std::numeric_limits<double>::min();
        constexpr auto MinimumFloat = static_cast<double>(std::numeric_limits<float>::min());
        constexpr auto MaximumFloat = static_cast<double>(std::numeric_limits<float>::max());
        using FVectorInterval = std::array<FInterval, 3>;
        using FQuaternionInterval = std::array<FInterval, 4>;
        using FMatrixInterval = std::array<FVectorInterval, 3>;

        auto Bits(double InValue) -> uint64
        { return std::bit_cast<uint64>(InValue); }

        auto IsFinite(double InValue) -> bool
        { return (Bits(InValue) & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL; }

        auto IsNormalOrZero(double InValue) -> bool
        {
            const auto Magnitude = Bits(InValue) & 0x7fffffffffffffffULL;
            return Magnitude == 0 || (Magnitude >= 0x0010000000000000ULL && IsFinite(InValue));
        }

        auto IsNormalFloatOrZero(float InValue) -> bool
        {
            const auto Magnitude = std::bit_cast<uint32>(InValue) & 0x7fffffffU;
            return Magnitude == 0 || (Magnitude >= 0x00800000U && Magnitude < 0x7f800000U);
        }

        auto IsInterval(FInterval InValue) -> bool
        {
            return InValue._Valid && IsNormalOrZero(InValue._Lo) && IsNormalOrZero(InValue._Hi)
                && InValue._Lo <= InValue._Hi;
        }

        auto Negate(FInterval InValue) -> FInterval
        { return InValue._Valid ? FInterval{-InValue._Hi, -InValue._Lo, true} : FInterval{}; }

        auto Absolute(FInterval InValue) -> FInterval
        {
            if (NOT InValue._Valid)
            { return {}; }
            if (InValue._Lo >= 0.0)
            { return InValue; }
            if (InValue._Hi <= 0.0)
            { return Negate(InValue); }
            return {0.0, FMath::Max(-InValue._Lo, InValue._Hi), true};
        }

        auto IsPoint(FInterval InValue, double InPoint) -> bool
        { return InValue._Valid && InValue._Lo == InPoint && InValue._Hi == InPoint; }

        // DAZ/FTZ are not modified. A non-exact zero/subnormal result receives the whole underflow interval.
        auto Expand(double InValue) -> FInterval
        {
            if (NOT IsFinite(InValue))
            { return {}; }
            const auto Magnitude = Bits(InValue) & 0x7fffffffffffffffULL;
            if (Magnitude < 0x0010000000000000ULL)
            { return {-MinimumDouble, MinimumDouble, true}; }
            const auto ValueBits = Bits(InValue);
            auto Lo = std::bit_cast<double>(InValue > 0.0 ? ValueBits - 1 : ValueBits + 1);
            auto Hi = std::bit_cast<double>(InValue > 0.0 ? ValueBits + 1 : ValueBits - 1);
            if (NOT IsFinite(Lo) || NOT IsFinite(Hi))
            { return {}; }
            if (NOT IsNormalOrZero(Lo))
            { Lo = InValue > 0.0 ? 0.0 : -MinimumDouble; }
            if (NOT IsNormalOrZero(Hi))
            { Hi = InValue > 0.0 ? MinimumDouble : 0.0; }
            return {Lo, Hi, true};
        }

        CK_CERT_NOINLINE auto RoundedAdd(double InA, double InB) -> FInterval
        {
            if (InA == 0.0)
            { return {InB, InB, true}; }
            if (InB == 0.0)
            { return {InA, InA, true}; }
            if (InA == -InB)
            { return {0.0, 0.0, true}; }
            const double Result = InA + InB;
            return Expand(Result);
        }
        CK_CERT_NOINLINE auto RoundedSubtract(double InA, double InB) -> FInterval
        {
            if (InA == InB)
            { return {0.0, 0.0, true}; }
            const double Result = InA - InB;
            return Expand(Result);
        }
        CK_CERT_NOINLINE auto RoundedMultiply(double InA, double InB) -> FInterval
        {
            if (InA == 0.0 || InB == 0.0)
            { return {0.0, 0.0, true}; }
            const double Result = InA * InB;
            return Expand(Result);
        }
        CK_CERT_NOINLINE auto RoundedDivide(double InA, double InB) -> FInterval
        {
            if (InA == 0.0)
            { return {0.0, 0.0, true}; }
            const double Result = InA / InB;
            return Expand(Result);
        }
        CK_CERT_NOINLINE auto RoundedSquareRoot(double InValue) -> FInterval
        { const double Result = std::sqrt(InValue); return Expand(Result); }
        CK_CERT_NOINLINE auto ConvertFloat(double InValue) -> float
        { return static_cast<float>(InValue); }

        auto Hull(FInterval InA, FInterval InB) -> FInterval
        {
            if (NOT InA._Valid || NOT InB._Valid)
            { return {}; }
            return {FMath::Min(InA._Lo, InB._Lo), FMath::Max(InA._Hi, InB._Hi), true};
        }

        auto Square(FInterval InValue) -> FInterval
        { const auto Value = Absolute(InValue); return Multiply(Value, Value); }

        auto SumSquares(const FVectorInterval& InVector) -> FInterval
        { return Add(Add(Square(InVector[0]), Square(InVector[1])), Square(InVector[2])); }

        auto Dot(const FQuaternionInterval& InA, const FQuaternionInterval& InB) -> FInterval
        {
            auto Result = Point(0.0);
            for (int32 Index = 0; Index < 4; ++Index)
            { Result = Add(Result, Multiply(InA[Index], InB[Index])); }
            return Result;
        }

        auto Normalize(const FQuaternionInterval& InValue) -> FQuaternionInterval
        {
            const auto Length = SquareRoot(Dot(InValue, InValue));
            auto Result = FQuaternionInterval{};
            for (int32 Index = 0; Index < 4; ++Index)
            { Result[Index] = Divide(InValue[Index], Length); }
            return Result;
        }

        auto Quaternion(const FQuat& InValue) -> FQuaternionInterval
        { return {Point(InValue.X), Point(InValue.Y), Point(InValue.Z), Point(InValue.W)}; }

        auto Conjugate(FQuaternionInterval InValue) -> FQuaternionInterval
        {
            for (int32 Index = 0; Index < 3; ++Index)
            { InValue[Index] = Negate(InValue[Index]); }
            return InValue;
        }

        auto QuaternionProduct(const FQuaternionInterval& InA, const FQuaternionInterval& InB)
            -> FQuaternionInterval
        {
            auto Result = FQuaternionInterval{};
            for (int32 Index = 0; Index < 3; ++Index)
            {
                const auto Next = (Index + 1) % 3;
                const auto Last = (Index + 2) % 3;
                Result[Index] = Subtract(Add(Add(Multiply(InA[3], InB[Index]), Multiply(InA[Index], InB[3])),
                    Multiply(InA[Next], InB[Last])), Multiply(InA[Last], InB[Next]));
            }
            Result[3] = Subtract(Subtract(Subtract(Multiply(InA[3], InB[3]), Multiply(InA[0], InB[0])),
                Multiply(InA[1], InB[1])), Multiply(InA[2], InB[2]));
            return Result;
        }

        // Row-major quaternion polynomial, before native rounding. Also used for the ideal midpoint.
        auto RotationMatrix(const FQuaternionInterval& InQ) -> FMatrixInterval
        {
            auto Result = FMatrixInterval{};
            for (int32 Index = 0; Index < 3; ++Index)
            {
                const auto Next = (Index + 1) % 3;
                const auto Last = (Index + 2) % 3;
                Result[Index][Index] = Subtract(Subtract(Point(1.0), Multiply(Point(2.0), Square(InQ[Next]))),
                    Multiply(Point(2.0), Square(InQ[Last])));
                Result[Index][Next] = Multiply(Point(2.0), Subtract(Multiply(InQ[Index], InQ[Next]),
                    Multiply(InQ[Last], InQ[3])));
                Result[Next][Index] = Multiply(Point(2.0), Add(Multiply(InQ[Index], InQ[Next]),
                    Multiply(InQ[Last], InQ[3])));
            }
            return Result;
        }

        auto Gamma(int32 InOperations) -> FInterval
        {
            const auto Product = Multiply(Point(static_cast<double>(InOperations)), Point(0x1p-24));
            return Divide(Product, Subtract(Point(1.0), Product));
        }

        auto Inflate(FInterval InValue, FInterval InError) -> FInterval
        {
            if (NOT InValue._Valid || NOT InError._Valid || InError._Hi < 0.0)
            { return {}; }
            return Add(InValue, {-InError._Hi, InError._Hi, true});
        }

        auto NativeMatrix(const FQuaternionInterval& InQ) -> FMatrixInterval
        {
            auto Result = RotationMatrix(InQ);
            // All possible nonzero monomials must stay normal in the native float source polynomial.
            for (int32 Row = 0; Row < 4; ++Row)
            {
                for (int32 Col = 0; Col < 4; ++Col)
                {
                    const auto Term = Absolute(Multiply(InQ[Row], InQ[Col]));
                    if (NOT Term._Valid || (Term._Hi != 0.0 && Term._Lo < MinimumFloat))
                    { return {}; }
                }
            }
            const auto ErrorFactor = Gamma(5);
            for (int32 Row = 0; Row < 3; ++Row)
            {
                for (int32 Col = 0; Col < 3; ++Col)
                {
                    auto Magnitudes = Point(0.0);
                    if (Row == Col)
                    {
                        for (const auto Other : {(Row + 1) % 3, (Row + 2) % 3})
                        {
                            const auto Partial = Subtract(Point(1.0), Multiply(Point(2.0), Square(InQ[Other])));
                            const auto PartialMagnitude = Add(Point(1.0), Multiply(Point(2.0), Square(InQ[Other])));
                            const auto PartialBound = Absolute(Inflate(Partial, Multiply(ErrorFactor, PartialMagnitude)));
                            if (NOT PartialBound._Valid || PartialBound._Lo < MinimumFloat)
                            { return {}; }
                        }
                        Magnitudes = Add(Point(1.0), Multiply(Point(2.0),
                            Add(Square(InQ[(Row + 1) % 3]), Square(InQ[(Row + 2) % 3]))));
                    }
                    else
                    {
                        Magnitudes = Multiply(Point(2.0), Add(Absolute(Multiply(InQ[Row], InQ[Col])),
                            Absolute(Multiply(InQ[3 - Row - Col], InQ[3]))));
                    }
                    const auto Error = Multiply(ErrorFactor, Magnitudes);
                    // A cancellation that may enter float underflow is outside this supported domain.
                    // Exact symbolic zero (both off-diagonal monomials zero) remains exact.
                    if (IsPoint(Magnitudes, 0.0))
                    { Result[Row][Col] = Point(0.0); continue; }
                    const auto Expanded = Inflate(Result[Row][Col], Error);
                    const auto AbsExpanded = Absolute(Expanded);
                    if (NOT AbsExpanded._Valid || AbsExpanded._Lo < MinimumFloat)
                    { return {}; }
                    Result[Row][Col] = Expanded;
                }
            }
            return Result;
        }

        auto Inverse(const FMatrixInterval& InMatrix) -> FMatrixInterval
        {
            auto Cofactors = FMatrixInterval{};
            for (int32 Row = 0; Row < 3; ++Row)
            {
                for (int32 Col = 0; Col < 3; ++Col)
                {
                    Cofactors[Row][Col] = Subtract(Multiply(InMatrix[(Row + 1) % 3][(Col + 1) % 3],
                        InMatrix[(Row + 2) % 3][(Col + 2) % 3]), Multiply(InMatrix[(Row + 1) % 3][(Col + 2) % 3],
                        InMatrix[(Row + 2) % 3][(Col + 1) % 3]));
                }
            }
            auto Determinant = Point(0.0);
            for (int32 Index = 0; Index < 3; ++Index)
            { Determinant = Add(Determinant, Multiply(InMatrix[0][Index], Cofactors[0][Index])); }
            auto Result = FMatrixInterval{};
            for (int32 Row = 0; Row < 3; ++Row)
            {
                for (int32 Col = 0; Col < 3; ++Col)
                { Result[Row][Col] = Divide(Cofactors[Col][Row], Determinant); }
            }
            return Result;
        }

        auto MatrixProduct(const FMatrixInterval& InA, const FMatrixInterval& InB) -> FMatrixInterval
        {
            auto Result = FMatrixInterval{};
            for (int32 Row = 0; Row < 3; ++Row)
            {
                for (int32 Col = 0; Col < 3; ++Col)
                {
                    Result[Row][Col] = Point(0.0);
                    for (int32 Inner = 0; Inner < 3; ++Inner)
                    { Result[Row][Col] = Add(Result[Row][Col], Multiply(InA[Row][Inner], InB[Inner][Col])); }
                }
            }
            return Result;
        }

        auto MapAbsolute(const FMatrixInterval& InMatrix, const FVectorInterval& InVector) -> FVectorInterval
        {
            auto Result = FVectorInterval{};
            for (int32 Row = 0; Row < 3; ++Row)
            {
                Result[Row] = Point(0.0);
                for (int32 Col = 0; Col < 3; ++Col)
                { Result[Row] = Add(Result[Row], Multiply(Absolute(InMatrix[Row][Col]), InVector[Col])); }
            }
            return Result;
        }

        auto MinimumBound(FInterval InA, FInterval InB) -> FInterval
        {
            if (NOT InA._Valid || NOT InB._Valid)
            { return {}; }
            return {FMath::Min(InA._Lo, InB._Lo), FMath::Min(InA._Hi, InB._Hi), true};
        }

        auto UnionBound(const FVectorInterval& InExtents, const FQuaternionInterval& InRelative)
            -> FVectorInterval
        {
            const auto Denominator = Add(Point(1.0), InRelative[3]);
            if (NOT Denominator._Valid || Denominator._Lo <= 0.0)
            { return {}; }
            const auto Length = SquareRoot(SumSquares(InExtents));
            auto Result = FVectorInterval{};
            for (int32 Index = 0; Index < 3; ++Index)
            {
                const auto Next = (Index + 1) % 3;
                const auto Last = (Index + 2) % 3;
                auto Triangle = InExtents[Index];
                for (const auto Other : {Next, Last})
                {
                    const auto Remaining = 3 - Index - Other;
                    const auto Coefficient = Add(Divide(Absolute(Multiply(InRelative[Index], InRelative[Other])),
                        Denominator), Absolute(InRelative[Remaining]));
                    Triangle = Add(Triangle, Multiply(InExtents[Other], Coefficient));
                }
                const auto Chord = Multiply(Length, SquareRoot(Divide(Multiply(Point(2.0),
                    Add(Square(InRelative[Next]), Square(InRelative[Last]))), Denominator)));
                Result[Index] = MinimumBound(Triangle, Add(InExtents[Index], Chord));
            }
            return Result;
        }
    }

    CK_CERT_NOINLINE auto Get_IsSupportedArithmetic() -> bool
    {
#if defined(_MSC_VER) && defined(_M_X64) && NOT defined(__clang__)
        unsigned int Control = 0;
        if (_controlfp_s(&Control, 0, 0) != 0)
        { return false; }
        return std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559
            && std::numeric_limits<float>::digits == 24 && std::numeric_limits<double>::digits == 53
            && (Control & _MCW_RC) == _RC_NEAR && (Control & _MCW_EM) == _MCW_EM
            && (_mm_getcsr() & 0x7f80U) == 0x1f80U;
#else
        return false;
#endif
    }

    CK_CERT_NOINLINE auto Point(double InValue) -> FInterval
    {
        if (NOT IsNormalOrZero(InValue))
        { return {}; }
        return {InValue, InValue, true};
    }

    CK_CERT_NOINLINE auto Add(FInterval InA, FInterval InB) -> FInterval
    {
        if (NOT IsInterval(InA) || NOT IsInterval(InB))
        { return {}; }
        if (IsPoint(InA, 0.0))
        { return InB; }
        if (IsPoint(InB, 0.0))
        { return InA; }
        const auto Lo = RoundedAdd(InA._Lo, InB._Lo);
        const auto Hi = RoundedAdd(InA._Hi, InB._Hi);
        if (NOT Lo._Valid || NOT Hi._Valid)
        { return {}; }
        return {Lo._Lo, Hi._Hi, true};
    }

    CK_CERT_NOINLINE auto Subtract(FInterval InA, FInterval InB) -> FInterval
    {
        if (NOT IsInterval(InA) || NOT IsInterval(InB))
        { return {}; }
        if (InA._Lo == InA._Hi && InB._Lo == InB._Hi && InA._Lo == InB._Lo)
        { return Point(0.0); }
        if (IsPoint(InB, 0.0))
        { return InA; }
        const auto Lo = RoundedSubtract(InA._Lo, InB._Hi);
        const auto Hi = RoundedSubtract(InA._Hi, InB._Lo);
        if (NOT Lo._Valid || NOT Hi._Valid)
        { return {}; }
        return {Lo._Lo, Hi._Hi, true};
    }

    CK_CERT_NOINLINE auto Multiply(FInterval InA, FInterval InB) -> FInterval
    {
        if (NOT IsInterval(InA) || NOT IsInterval(InB))
        { return {}; }
        if (IsPoint(InA, 0.0) || IsPoint(InB, 0.0))
        { return Point(0.0); }
        if (IsPoint(InA, 1.0))
        { return InB; }
        if (IsPoint(InB, 1.0))
        { return InA; }
        return Hull(Hull(RoundedMultiply(InA._Lo, InB._Lo), RoundedMultiply(InA._Lo, InB._Hi)),
            Hull(RoundedMultiply(InA._Hi, InB._Lo), RoundedMultiply(InA._Hi, InB._Hi)));
    }

    CK_CERT_NOINLINE auto Divide(FInterval InA, FInterval InB) -> FInterval
    {
        if (NOT IsInterval(InA) || NOT IsInterval(InB) || (InB._Lo <= 0.0 && InB._Hi >= 0.0))
        { return {}; }
        if (IsPoint(InA, 0.0))
        { return Point(0.0); }
        if (IsPoint(InB, 1.0))
        { return InA; }
        return Hull(Hull(RoundedDivide(InA._Lo, InB._Lo), RoundedDivide(InA._Lo, InB._Hi)),
            Hull(RoundedDivide(InA._Hi, InB._Lo), RoundedDivide(InA._Hi, InB._Hi)));
    }

    CK_CERT_NOINLINE auto SquareRoot(FInterval InValue) -> FInterval
    {
        if (NOT IsInterval(InValue) || InValue._Lo < 0.0)
        { return {}; }
        if (IsPoint(InValue, 0.0) || IsPoint(InValue, 1.0))
        { return InValue; }
        const auto Lo = InValue._Lo == 0.0 ? Point(0.0) : RoundedSquareRoot(InValue._Lo);
        const auto Hi = RoundedSquareRoot(InValue._Hi);
        if (NOT Lo._Valid || NOT Hi._Valid)
        { return {}; }
        return {Lo._Lo, Hi._Hi, true};
    }

    CK_CERT_NOINLINE auto NativeFloat(FInterval InValue) -> FInterval
    {
        if (NOT IsInterval(InValue) || InValue._Lo < -MaximumFloat || InValue._Hi > MaximumFloat)
        { return {}; }
        if (IsPoint(InValue, 0.0))
        { return InValue; }
        const auto ExpandFloat = [](float InFloat) -> FInterval
        {
            const auto ValueBits = std::bit_cast<uint32>(InFloat);
            const auto Magnitude = ValueBits & 0x7fffffffU;
            if (Magnitude >= 0x7f800000U)
            { return {}; }
            if (Magnitude < 0x00800000U)
            { return {-MinimumFloat, MinimumFloat, true}; }
            const auto LoBits = InFloat > 0.0f ? ValueBits - 1 : ValueBits + 1;
            const auto HiBits = InFloat > 0.0f ? ValueBits + 1 : ValueBits - 1;
            auto Lo = static_cast<double>(std::bit_cast<float>(LoBits));
            auto Hi = static_cast<double>(std::bit_cast<float>(HiBits));
            if (NOT IsFinite(Lo) || NOT IsFinite(Hi))
            { return {}; }
            if ((LoBits & 0x7fffffffU) < 0x00800000U)
            { Lo = InFloat > 0.0f ? 0.0 : -MinimumFloat; }
            if ((HiBits & 0x7fffffffU) < 0x00800000U)
            { Hi = InFloat > 0.0f ? MinimumFloat : 0.0; }
            return {Lo, Hi, true};
        };
        const auto Lo = ExpandFloat(ConvertFloat(InValue._Lo));
        const auto Hi = ExpandFloat(ConvertFloat(InValue._Hi));
        return Hull(Lo, Hi);
    }

    CK_CERT_NOINLINE auto Get_NativeMatrixBounds(const FQuat& InRepresentedQuaternion, FMatrixInterval& OutMatrix) -> void
    { OutMatrix = NativeMatrix(Quaternion(InRepresentedQuaternion)); }

    CK_CERT_NOINLINE auto Get_InverseBounds(const FMatrixInterval& InMatrix, FMatrixInterval& OutInverse) -> void
    { OutInverse = Inverse(InMatrix); }

    CK_CERT_NOINLINE auto TryGet_CertifiedHalfExtents(
        const FVector& InAuthoredHalfExtents,
        float InMargin,
        const FTransform& InStart,
        const FTransform& InEnd,
        const FRotator& InOldRotation,
        const FVector& InOldHalfExtents) -> TOptional<FVector>
    {
        if (NOT Get_IsSupportedArithmetic() || InStart.ContainsNaN() || InEnd.ContainsNaN()
            || InStart.GetScale3D() != FVector::OneVector || InEnd.GetScale3D() != FVector::OneVector
            || InStart.GetRotation() == InEnd.GetRotation() || InOldRotation.ContainsNaN()
            || NOT IsFinite(InOldRotation.Pitch) || NOT IsFinite(InOldRotation.Yaw) || NOT IsFinite(InOldRotation.Roll)
            || InAuthoredHalfExtents.ContainsNaN() || InAuthoredHalfExtents.GetMin() <= 0.0
            || NOT IsFinite(InMargin) || InMargin <= 0.0f || InOldHalfExtents.ContainsNaN())
        { return {}; }
        const auto RawStart = Quaternion(InStart.GetRotation());
        const auto RawEnd = Quaternion(InEnd.GetRotation());
        const auto Start = Normalize(RawStart);
        auto End = Normalize(RawEnd);
        const auto Sign = Dot(Start, End);
        auto RawMagnitudes = Point(0.0);
        for (int32 Index = 0; Index < 4; ++Index)
        { RawMagnitudes = Add(RawMagnitudes, Absolute(Multiply(RawStart[Index], RawEnd[Index]))); }
        const auto DotRoundoff = Multiply(Point(7.0), Point(0x1p-53));
        const auto RawSign = Inflate(Dot(RawStart, RawEnd),
            Multiply(Divide(DotRoundoff, Subtract(Point(1.0), DotRoundoff)), RawMagnitudes));
        if (NOT Sign._Valid || NOT RawSign._Valid
            || (Sign._Lo <= 0.0 && Sign._Hi >= 0.0)
            || (Sign._Lo > 0.0 && RawSign._Lo <= 0.0) || (Sign._Hi < 0.0 && RawSign._Hi >= 0.0))
        { return {}; }
        if (Sign._Hi < 0.0)
        {
            for (auto& Component : End)
            { Component = Negate(Component); }
        }
        auto Sum = FQuaternionInterval{};
        for (int32 Index = 0; Index < 4; ++Index)
        { Sum[Index] = Add(Start[Index], End[Index]); }
        const auto Midpoint = Normalize(Sum);
        const auto Relative = QuaternionProduct(Conjugate(Start), End);
        const auto RotationSize = SumSquares({Relative[0], Relative[1], Relative[2]});
        if (NOT RotationSize._Valid || RotationSize._Lo <= 0.0)
        { return {}; }
        // Independent target uses original authored dimensions plus the original margin, without radius.
        auto Target = FVectorInterval{};
        auto Proposed = FVectorInterval{};
        const auto Radius = Point(static_cast<double>(JPH::cDefaultConvexRadius));
        for (int32 Index = 0; Index < 3; ++Index)
        {
            Target[Index] = Add(Point(InAuthoredHalfExtents[Index]), Point(InMargin));
            Proposed[Index] = Add(Target[Index], Radius);
        }
        const auto Bound = UnionBound(Target, Relative);
        Proposed = UnionBound(Proposed, Relative);
        auto Candidate = FVector::ZeroVector;
        const auto OldNative = ck::jolt::Conv(InOldHalfExtents);
        const double OldComponents[] = {OldNative.GetX(), OldNative.GetY(), OldNative.GetZ()};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (NOT Proposed[Index]._Valid || NOT IsFinite(OldComponents[Index])
                || Proposed[Index]._Hi <= 0.0 || Proposed[Index]._Hi > MaximumFloat)
            { return {}; }
            Candidate[Index] = FMath::Min(Proposed[Index]._Hi, InOldHalfExtents[Index]);
        }
        const auto Native = ck::jolt::Conv(Candidate);
        const double Half[] = {Native.GetX(), Native.GetY(), Native.GetZ()};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (NOT IsFinite(Half[Index]) || Half[Index] <= Radius._Hi || Half[Index] > OldComponents[Index])
            { return {}; }
        }
        const auto NativeQuaternion = ck::jolt::Conv(InOldRotation.Quaternion());
        if (NOT IsNormalFloatOrZero(NativeQuaternion.GetX()) || NOT IsNormalFloatOrZero(NativeQuaternion.GetY())
            || NOT IsNormalFloatOrZero(NativeQuaternion.GetZ()) || NOT IsNormalFloatOrZero(NativeQuaternion.GetW()))
        { return {}; }
        const double Q[] = {NativeQuaternion.GetX(), NativeQuaternion.GetY(), NativeQuaternion.GetZ(),
            NativeQuaternion.GetW()};
        auto NativeQ = FQuaternionInterval{};
        for (int32 Index = 0; Index < 4; ++Index)
        {
            if (NOT IsFinite(Q[Index]) || std::abs(Q[Index]) > 2.0
                || (Q[Index] != 0.0 && std::abs(Q[Index]) < MinimumFloat))
            { return {}; }
            NativeQ[Index] = Point(Q[Index]);
        }
        if (NOT NativeQuaternion.IsNormalized())
        { return {}; }
        const auto NativeInverse = Inverse(NativeMatrix(NativeQ));
        auto A = MapAbsolute(MatrixProduct(NativeInverse, RotationMatrix(Midpoint)), Bound);
        const auto RawError = Multiply(Point(2.0), Multiply(SquareRoot(SumSquares(Target)),
            Hull(Absolute(Subtract(Dot(RawStart, RawStart), Point(1.0))),
                Absolute(Subtract(Dot(RawEnd, RawEnd), Point(1.0))))));
        const auto StartPosition = InStart.GetLocation();
        const auto EndPosition = InEnd.GetLocation();
        const auto NativeStart = ck::jolt::Conv(StartPosition);
        const auto NativeEnd = ck::jolt::Conv(EndPosition);
        const auto NativeDirection = ck::jolt::Conv(EndPosition - StartPosition);
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (NOT IsNormalFloatOrZero(NativeStart[Index]) || NOT IsNormalFloatOrZero(NativeEnd[Index])
                || NOT IsNormalFloatOrZero(NativeDirection[Index]))
            { return {}; }
        }
        const double S[] = {NativeStart.GetX(), NativeStart.GetY(), NativeStart.GetZ()};
        const double E[] = {NativeEnd.GetX(), NativeEnd.GetY(), NativeEnd.GetZ()};
        const double D[] = {NativeDirection.GetX(), NativeDirection.GetY(), NativeDirection.GetZ()};
        auto WorldError = FVectorInterval{};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (NOT IsNormalOrZero(S[Index]) || NOT IsNormalOrZero(E[Index]) || NOT IsNormalOrZero(D[Index])
                || (S[Index] != 0.0 && std::abs(S[Index]) < MinimumFloat)
                || (E[Index] != 0.0 && std::abs(E[Index]) < MinimumFloat)
                || (D[Index] != 0.0 && std::abs(D[Index]) < MinimumFloat))
            { return {}; }
            const auto StartError = Absolute(Subtract(Point(S[Index]), Point(StartPosition[Index])));
            const auto CastEndError = Absolute(Subtract(Add(Point(S[Index]), Point(D[Index])), Point(EndPosition[Index])));
            const auto EndError = Absolute(Subtract(Point(E[Index]), Point(EndPosition[Index])));
            // ShapeCast.h::GetPointOnRay: S + fraction*D. gamma3 covers multiply/add, fraction conversion,
            // and either fused or non-fused evaluation. The absolute underflow allowance covers FTZ at t near 0.
            const auto Evaluation = D[Index] == 0.0 ? Point(0.0)
                : Add(Multiply(Gamma(3), Add(Absolute(Point(S[Index])), Absolute(Point(D[Index])))), Point(0x1p-120));
            WorldError[Index] = Add(Add(Hull(Hull(StartError, CastEndError), EndError), Evaluation), RawError);
        }
        const auto LocalError = MapAbsolute(NativeInverse, WorldError);
        auto ExcessSquared = Point(0.0);
        for (int32 Index = 0; Index < 3; ++Index)
        {
            A[Index] = Add(A[Index], LocalError[Index]);
            const auto Core = NativeFloat(Subtract(Point(Half[Index]), Radius));
            if (NOT A[Index]._Valid || NOT Core._Valid || Core._Lo <= 0.0)
            { return {}; }
            const auto Excess = Subtract(A[Index], Point(Core._Lo));
            if (NOT Excess._Valid)
            { return {}; }
            ExcessSquared = Add(ExcessSquared, Square(Point(FMath::Max(0.0, Excess._Hi))));
        }
        const auto RadiusSquared = Square(Radius);
        if (NOT ExcessSquared._Valid || NOT RadiusSquared._Valid || ExcessSquared._Hi > RadiusSquared._Lo)
        { return {}; }
        return Candidate;
    }
}

#if defined(_MSC_VER) && defined(_M_X64) && NOT defined(__clang__)
#pragma float_control(pop)
#endif
#undef CK_CERT_NOINLINE
