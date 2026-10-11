#pragma once

#include "CoreMinimal.h"
#include "CkCore/Macros/CkMacros.h"

#include <array>
#include <charconv>
#include <system_error>

namespace ck::pixel_sprite::decimal_private
{
    constexpr int32 MaxDigits = 1024;

    // A normalized base-ten coefficient in little-endian order. The value is
    // coefficient * 10^_Exponent, with sign kept separately.
    struct FDecimal
    {
        FDecimal()
        {
            _Digits.Add(uint8{0});
        }

        TArray<uint8> _Digits;
        int32 _Exponent = 0;
        bool _Negative = false;
    };

    inline auto
        IsZero(
            const FDecimal& InValue)
        -> bool
    {
        return InValue._Digits.Num() == 1 && InValue._Digits[0] == 0;
    }

    inline auto
        IsNormalized(
            const FDecimal& InValue)
        -> bool
    {
        if (InValue._Digits.IsEmpty() || InValue._Digits.Num() > MaxDigits)
        {
            return false;
        }
        for (const auto Digit : InValue._Digits)
        {
            if (Digit > 9)
            {
                return false;
            }
        }
        if (IsZero(InValue))
        {
            return InValue._Exponent == 0 && NOT InValue._Negative;
        }
        return InValue._Digits[0] != 0 && InValue._Digits.Last() != 0;
    }

    inline auto
        Normalize(
            FDecimal& InOutValue)
        -> bool
    {
        while (InOutValue._Digits.Num() > 1 && InOutValue._Digits.Last() == 0)
        {
            InOutValue._Digits.Pop();
        }
        if (IsZero(InOutValue))
        {
            InOutValue._Exponent = 0;
            InOutValue._Negative = false;
            return true;
        }
        while (InOutValue._Digits.Num() > 1 && InOutValue._Digits[0] == 0)
        {
            if (InOutValue._Exponent == MAX_int32)
            {
                return false;
            }
            InOutValue._Digits.RemoveAt(0);
            ++InOutValue._Exponent;
        }
        return InOutValue._Digits.Num() <= MaxDigits;
    }

    inline auto
        TryFromDouble(
        double InValue,
        int32 InExponentShift,
        FDecimal& OutValue)
        -> bool
    {
        if (NOT FMath::IsFinite(InValue))
        {
            return false;
        }
        auto Buffer = std::array<char, 64>{};
        const auto Written = std::to_chars(
            Buffer.data(), Buffer.data() + Buffer.size(), InValue, std::chars_format::general);
        if (Written.ec != std::errc{})
        {
            return false;
        }

        auto Candidate = FDecimal{};
        Candidate._Digits.Reset();
        Candidate._Negative = InValue < 0.0;
        auto Index = int32{0};
        const auto Length = static_cast<int32>(Written.ptr - Buffer.data());
        if (Index < Length && Buffer[Index] == '-')
        {
            ++Index;
        }
        auto Digits = TArray<uint8>{};
        auto FractionDigits = int32{0};
        auto Fraction = false;
        for (; Index < Length; ++Index)
        {
            const auto Character = Buffer[Index];
            if (Character == 'e' || Character == 'E')
            {
                break;
            }
            if (Character == '.')
            {
                if (Fraction)
                {
                    return false;
                }
                Fraction = true;
                continue;
            }
            if (Character < '0' || Character > '9' || Digits.Num() >= MaxDigits)
            {
                return false;
            }
            Digits.Add(static_cast<uint8>(Character - '0'));
            if (Fraction)
            {
                ++FractionDigits;
            }
        }
        if (Digits.IsEmpty())
        {
            return false;
        }
        auto ScientificExponent = int32{0};
        if (Index < Length)
        {
            ++Index;
            auto ExponentNegative = false;
            if (Index < Length && (Buffer[Index] == '+' || Buffer[Index] == '-'))
            {
                ExponentNegative = Buffer[Index++] == '-';
            }
            if (Index == Length)
            {
                return false;
            }
            for (; Index < Length; ++Index)
            {
                if (Buffer[Index] < '0' || Buffer[Index] > '9' || ScientificExponent > 100000)
                {
                    return false;
                }
                ScientificExponent = ScientificExponent * 10 + Buffer[Index] - '0';
            }
            if (ExponentNegative)
            {
                ScientificExponent = -ScientificExponent;
            }
        }
        const auto Exponent = int64{InExponentShift} + ScientificExponent - FractionDigits;
        if (Exponent < MIN_int32 || Exponent > MAX_int32)
        {
            return false;
        }
        Candidate._Exponent = static_cast<int32>(Exponent);
        for (auto DigitIndex = Digits.Num() - 1; DigitIndex >= 0; --DigitIndex)
        {
            Candidate._Digits.Add(Digits[DigitIndex]);
        }
        if (NOT Normalize(Candidate))
        {
            return false;
        }
        OutValue = MoveTemp(Candidate);
        return true;
    }

    inline auto
        DigitAt(
            const FDecimal& InValue,
            int64 InPower)
        -> uint8
    {
        const auto Index = InPower - InValue._Exponent;
        return Index >= 0 && Index < InValue._Digits.Num()
            ? InValue._Digits[static_cast<int32>(Index)] : uint8{0};
    }

    inline auto
        CompareMagnitude(
            const FDecimal& InA,
            const FDecimal& InB)
        -> int32
    {
        if (IsZero(InA))
        {
            return IsZero(InB) ? 0 : -1;
        }
        if (IsZero(InB))
        {
            return 1;
        }
        const auto ATop = int64{InA._Exponent} + InA._Digits.Num() - 1;
        const auto BTop = int64{InB._Exponent} + InB._Digits.Num() - 1;
        if (ATop != BTop)
        {
            return ATop < BTop ? -1 : 1;
        }
        const auto Bottom = FMath::Min<int64>(InA._Exponent, InB._Exponent);
        for (auto Power = ATop; Power >= Bottom; --Power)
        {
            const auto A = DigitAt(InA, Power);
            const auto B = DigitAt(InB, Power);
            if (A != B)
            {
                return A < B ? -1 : 1;
            }
        }
        return 0;
    }

    inline auto
        TryAlignedLength(
            const FDecimal& InValue,
            int64 InExponent,
            int32& OutLength)
        -> bool
    {
        const auto Length = int64{InValue._Digits.Num()} + int64{InValue._Exponent} - InExponent;
        if (Length < 1 || Length > MaxDigits)
        {
            return false;
        }
        OutLength = static_cast<int32>(Length);
        return true;
    }

    inline auto
        TryAddPositive(
        const FDecimal& InA,
        const FDecimal& InB,
        FDecimal& OutValue)
        -> bool
    {
        if (NOT IsNormalized(InA) || NOT IsNormalized(InB) || InA._Negative || InB._Negative)
        {
            return false;
        }
        const auto Exponent = FMath::Min<int64>(InA._Exponent, InB._Exponent);
        auto ALength = int32{0};
        auto BLength = int32{0};
        if (NOT TryAlignedLength(InA, Exponent, ALength) ||
            NOT TryAlignedLength(InB, Exponent, BLength))
        {
            return false;
        }
        auto Candidate = FDecimal{};
        Candidate._Digits.Reset();
        Candidate._Exponent = static_cast<int32>(Exponent);
        auto Carry = int32{0};
        const auto Length = FMath::Max(ALength, BLength);
        for (auto Index = int32{0}; Index < Length; ++Index)
        {
            const auto Sum = int32{DigitAt(InA, Exponent + Index)} +
                DigitAt(InB, Exponent + Index) + Carry;
            Candidate._Digits.Add(static_cast<uint8>(Sum % 10));
            Carry = Sum / 10;
        }
        if (Carry != 0)
        {
            if (Candidate._Digits.Num() >= MaxDigits)
            {
                return false;
            }
            Candidate._Digits.Add(static_cast<uint8>(Carry));
        }
        if (NOT Normalize(Candidate))
        {
            return false;
        }
        OutValue = MoveTemp(Candidate);
        return true;
    }

    inline auto
        TrySubtractPositive(
        const FDecimal& InA,
        const FDecimal& InB,
        FDecimal& OutValue)
        -> bool
    {
        if (NOT IsNormalized(InA) || NOT IsNormalized(InB) ||
            InA._Negative || InB._Negative || CompareMagnitude(InA, InB) < 0)
        {
            return false;
        }
        const auto Exponent = FMath::Min<int64>(InA._Exponent, InB._Exponent);
        auto ALength = int32{0};
        auto BLength = int32{0};
        if (NOT TryAlignedLength(InA, Exponent, ALength) ||
            NOT TryAlignedLength(InB, Exponent, BLength))
        {
            return false;
        }
        auto Candidate = FDecimal{};
        Candidate._Digits.Reset();
        Candidate._Exponent = static_cast<int32>(Exponent);
        auto Borrow = int32{0};
        for (auto Index = int32{0}; Index < ALength; ++Index)
        {
            auto Difference = int32{DigitAt(InA, Exponent + Index)} -
                DigitAt(InB, Exponent + Index) - Borrow;
            Borrow = Difference < 0 ? 1 : 0;
            if (Borrow != 0)
            {
                Difference += 10;
            }
            Candidate._Digits.Add(static_cast<uint8>(Difference));
        }
        if (Borrow != 0 || NOT Normalize(Candidate))
        {
            return false;
        }
        OutValue = MoveTemp(Candidate);
        return true;
    }

    inline auto
        CompareIntegers(
            const TArray<uint8>& InA,
            const TArray<uint8>& InB)
        -> int32
    {
        if (InA.Num() != InB.Num())
        {
            return InA.Num() < InB.Num() ? -1 : 1;
        }
        for (auto Index = InA.Num() - 1; Index >= 0; --Index)
        {
            if (InA[Index] != InB[Index])
            {
                return InA[Index] < InB[Index] ? -1 : 1;
            }
        }
        return 0;
    }

    inline auto
        TryPositiveModulo(
        const FDecimal& InA,
        const FDecimal& InB,
        FDecimal& OutValue)
        -> bool
    {
        if (NOT IsNormalized(InA) || NOT IsNormalized(InB) ||
            InA._Negative || InB._Negative || IsZero(InB))
        {
            return false;
        }
        if (CompareMagnitude(InA, InB) < 0)
        {
            OutValue = InA;
            return true;
        }
        const auto Exponent = FMath::Min<int64>(InA._Exponent, InB._Exponent);
        auto ALength = int32{0};
        auto BLength = int32{0};
        if (NOT TryAlignedLength(InA, Exponent, ALength) ||
            NOT TryAlignedLength(InB, Exponent, BLength))
        {
            return false;
        }
        auto Divisor = TArray<uint8>{};
        Divisor.SetNumZeroed(BLength);
        for (auto Index = int32{0}; Index < BLength; ++Index)
        {
            Divisor[Index] = DigitAt(InB, Exponent + Index);
        }
        auto Remainder = TArray<uint8>{uint8{0}};
        for (auto Index = ALength - 1; Index >= 0; --Index)
        {
            const auto Digit = DigitAt(InA, Exponent + Index);
            if (Remainder.Num() == 1 && Remainder[0] == 0)
            {
                Remainder[0] = Digit;
            }
            else
            {
                Remainder.Insert(uint8{0}, 0);
                Remainder[0] = Digit;
            }
            while (CompareIntegers(Remainder, Divisor) >= 0)
            {
                auto Borrow = int32{0};
                for (auto Place = int32{0}; Place < Remainder.Num(); ++Place)
                {
                    auto Difference = int32{Remainder[Place]} -
                        (Place < Divisor.Num() ? Divisor[Place] : 0) - Borrow;
                    Borrow = Difference < 0 ? 1 : 0;
                    if (Borrow != 0)
                    {
                        Difference += 10;
                    }
                    Remainder[Place] = static_cast<uint8>(Difference);
                }
                if (Borrow != 0)
                {
                    return false;
                }
                while (Remainder.Num() > 1 && Remainder.Last() == 0)
                {
                    Remainder.Pop();
                }
            }
        }
        auto Candidate = FDecimal{};
        Candidate._Digits = MoveTemp(Remainder);
        Candidate._Exponent = static_cast<int32>(Exponent);
        if (NOT Normalize(Candidate))
        {
            return false;
        }
        OutValue = MoveTemp(Candidate);
        return true;
    }

    inline auto
        TryToSeconds(
            const FDecimal& InMilliseconds,
            double& OutSeconds)
        -> bool
    {
        if (NOT IsNormalized(InMilliseconds))
        {
            return false;
        }
        if (IsZero(InMilliseconds))
        {
            OutSeconds = 0.0;
            return true;
        }
        auto Text = TArray<char>{};
        Text.Reserve(InMilliseconds._Digits.Num() + 16);
        if (InMilliseconds._Negative)
        {
            Text.Add('-');
        }
        for (auto Index = InMilliseconds._Digits.Num() - 1; Index >= 0; --Index)
        {
            Text.Add(static_cast<char>('0' + InMilliseconds._Digits[Index]));
        }
        Text.Add('e');
        auto ExponentBuffer = std::array<char, 32>{};
        const auto Written = std::to_chars(
            ExponentBuffer.data(), ExponentBuffer.data() + ExponentBuffer.size(),
            int64{InMilliseconds._Exponent} - 3);
        if (Written.ec != std::errc{})
        {
            return false;
        }
        for (auto Index = int32{0}; Index < Written.ptr - ExponentBuffer.data(); ++Index)
        {
            Text.Add(ExponentBuffer[Index]);
        }
        auto Candidate = double{0.0};
        const auto Read = std::from_chars(
            Text.GetData(), Text.GetData() + Text.Num(), Candidate, std::chars_format::general);
        if (Read.ec == std::errc{} && Read.ptr == Text.GetData() + Text.Num() &&
            FMath::IsFinite(Candidate))
        {
            OutSeconds = Candidate;
            return true;
        }
        if (Read.ec == std::errc::result_out_of_range &&
            Read.ptr == Text.GetData() + Text.Num() &&
            int64{InMilliseconds._Exponent} - 3 +
                InMilliseconds._Digits.Num() - 1 <= -324)
        {
            OutSeconds = InMilliseconds._Negative ? -0.0 : 0.0;
            return true;
        }
        return false;
    }
}
