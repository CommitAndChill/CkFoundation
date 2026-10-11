#pragma once

#include "CoreMinimal.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkPixelSprite/CkPixelSprite_Data.h"

#include <charconv>
#include <system_error>

namespace ck::pixel_sprite::json_private
{
    enum class EKind : uint8
    {
        Object,
        Array,
        String,
        Number,
        Boolean,
        Null
    };

    struct FNode
    {
        EKind _Kind = EKind::Null;
        FString _Value; // Decoded string or unchanged number lexeme; never compare when _ContainsNull.
        bool _bBool = false;
        bool _ContainsNull = false;
        int32 _Start = 0;
        int32 _End = 0;
        TArray<FNode> _Children;
        TArray<FString> _Keys; // Decoded object keys parallel to _Children; empty for arrays.
        TArray<bool> _KeyContainsNull; // Parallel to _Keys; schema keys with NUL are invalid.
    };

    // InRaw is a strict number lexeme already recognized by ParseNumber.
    // OutValue is unchanged on failure. This helper is shared with schema
    // duration conversion; integer metadata never calls it.
    inline auto
        TryGetFiniteNumber(
            const FString& InRaw,
            double& OutValue)
        -> bool
    {
        if (InRaw.IsEmpty())
        {
            return false;
        }
        auto DigitIndex = int32{0};
        auto DigitsBeforePoint = int32{0};
        auto FirstNonzero = int32{INDEX_NONE};
        auto bFraction = false;
        auto Index = InRaw[0] == TEXT('-') ? int32{1} : int32{0};
        for (; Index < InRaw.Len(); ++Index)
        {
            const auto Ch = InRaw[Index];
            if (Ch == TEXT('e') || Ch == TEXT('E'))
            {
                break;
            }
            if (Ch == TEXT('.'))
            {
                bFraction = true;
                continue;
            }
            if (NOT bFraction)
            {
                ++DigitsBeforePoint;
            }
            if (FirstNonzero == INDEX_NONE && Ch != TEXT('0'))
            {
                FirstNonzero = DigitIndex;
            }
            ++DigitIndex;
        }
        const auto IsZero = FirstNonzero == INDEX_NONE;
        auto Exponent = int64{0};
        if (Index < InRaw.Len())
        {
            ++Index;
            auto Sign = int64{1};
            if (Index < InRaw.Len() && (InRaw[Index] == TEXT('+') || InRaw[Index] == TEXT('-')))
            {
                if (InRaw[Index] == TEXT('-'))
                {
                    Sign = -1;
                }
                ++Index;
            }
            for (; Index < InRaw.Len(); ++Index)
            {
                Exponent = FMath::Min<int64>(
                    Exponent * 10 + (InRaw[Index] - TEXT('0')), 1000000000);
            }
            Exponent *= Sign;
        }
        const auto Order = Exponent + DigitsBeforePoint - FirstNonzero - 1;
        if (IsZero)
        {
            OutValue = 0.0;
            return true;
        }
        if (Order > 308)
        {
            return false;
        }

        auto Ascii = TArray<ANSICHAR>{};
        Ascii.SetNumUninitialized(InRaw.Len());
        for (auto CharIndex = int32{0}; CharIndex < InRaw.Len(); ++CharIndex)
        {
            Ascii[CharIndex] = static_cast<ANSICHAR>(InRaw[CharIndex]);
        }
        auto Candidate = double{0.0};
        const auto Result = std::from_chars(
            Ascii.GetData(), Ascii.GetData() + Ascii.Num(), Candidate, std::chars_format::general);
        if (Result.ec == std::errc{} && Result.ptr == Ascii.GetData() + Ascii.Num() &&
            FMath::IsFinite(Candidate))
        {
            OutValue = Candidate;
            return true;
        }
        // from_chars reports result_out_of_range for both overflow and
        // underflow. Only permit the zero-underflow exception for a fully
        // consumed token below decimal order -323. Unexpected conversion
        // failures for ordinary finite values must not silently become zero.
        if (Result.ec == std::errc::result_out_of_range &&
            Result.ptr == Ascii.GetData() + Ascii.Num() && Order <= -324)
        {
            OutValue = 0.0;
            return true;
        }
        return false;
    }

    class FParser
    {
    public:
        FParser(const FString& InText, FCk_PixelSprite_Diagnostic& OutDiagnostic)
            : _Text(InText), _Diagnostic(OutDiagnostic)
        {}

        auto
            Parse(
                FNode& OutRoot)
            -> bool
        {
            SkipWhitespace();
            auto Candidate = FNode{};
            if (NOT ParseValue(Candidate, 1))
            {
                return false;
            }
            SkipWhitespace();
            if (_Cursor != _Text.Len())
            {
                return Fail(TEXT("json"), TEXT("Trailing JSON content"), _Cursor);
            }
            OutRoot = MoveTemp(Candidate);
            return true;
        }

    private:
        static constexpr int32 MaxDepth = 64;
        static constexpr int32 MaxNodes = 100000;

        auto
            Fail(
                const FString& InCode,
                const FString& InMessage,
                int32 InAt)
            -> bool
        {
            auto Line = int32{1};
            auto Column = int32{1};
            for (auto Index = int32{0}; Index < InAt && Index < _Text.Len(); ++Index)
            {
                if (_Text[Index] == TEXT('\r'))
                {
                    ++Line;
                    Column = 1;
                    if (Index + 1 < InAt && _Text[Index + 1] == TEXT('\n'))
                    {
                        ++Index;
                    }
                }
                else if (_Text[Index] == TEXT('\n'))
                {
                    ++Line;
                    Column = 1;
                }
                else
                {
                    ++Column;
                }
            }
            _Diagnostic = {TEXT("<manifest>"), InCode, InMessage, Line, Column};
            return false;
        }

        auto
            ReserveNode()
            -> bool
        {
            if (_NodeCount >= MaxNodes)
            {
                return Fail(TEXT("limits"), TEXT("JSON node count exceeds 100000"), _Cursor);
            }
            ++_NodeCount;
            return true;
        }

        auto
            Peek() const
            -> TCHAR
        {
            return _Cursor < _Text.Len() ? _Text[_Cursor] : TCHAR{0};
        }

        auto
            SkipWhitespace()
            -> void
        {
            while (_Cursor < _Text.Len())
            {
                const auto Ch = _Text[_Cursor];
                if (Ch != TEXT(' ') && Ch != TEXT('\t') && Ch != TEXT('\r') && Ch != TEXT('\n'))
                {
                    break;
                }
                ++_Cursor;
            }
        }

        auto
            MatchLiteral(
                const FString& InLiteral)
            -> bool
        {
            if (_Cursor + InLiteral.Len() > _Text.Len() ||
                NOT _Text.Mid(_Cursor, InLiteral.Len()).Equals(InLiteral, ESearchCase::CaseSensitive))
            {
                return false;
            }
            _Cursor += InLiteral.Len();
            return true;
        }

        static auto
            HexValue(
                TCHAR InCh)
            -> int32
        {
            if (InCh >= TEXT('0') && InCh <= TEXT('9'))
            {
                return InCh - TEXT('0');
            }
            if (InCh >= TEXT('a') && InCh <= TEXT('f'))
            {
                return 10 + InCh - TEXT('a');
            }
            if (InCh >= TEXT('A') && InCh <= TEXT('F'))
            {
                return 10 + InCh - TEXT('A');
            }
            return -1;
        }

        auto
            ReadHex4(
                uint16& OutCodeUnit)
            -> bool
        {
            if (_Cursor + 4 > _Text.Len())
            {
                return Fail(TEXT("json"), TEXT("Truncated Unicode escape"), _Cursor);
            }
            auto Value = uint16{0};
            for (auto Offset = int32{0}; Offset < 4; ++Offset)
            {
                const auto Digit = HexValue(_Text[_Cursor + Offset]);
                if (Digit < 0)
                {
                    return Fail(TEXT("json"), TEXT("Invalid Unicode escape"), _Cursor + Offset);
                }
                Value = static_cast<uint16>((Value << 4) | Digit);
            }
            _Cursor += 4;
            OutCodeUnit = Value;
            return true;
        }

        static auto
            AppendUnit(
            TArray<TCHAR>& OutUnits,
            bool& OutContainsNull,
            FString& OutIdentity,
            bool InBuildIdentity,
            TCHAR InUnit)
            -> void
        {
            OutUnits.Add(InUnit);
            OutContainsNull = OutContainsNull || InUnit == 0;
            if (NOT InBuildIdentity)
            {
                return;
            }
            // Every decoded native code unit has the same eight-character representation.
            // This identity never contains NUL and is safe for TSet hashing/equality.
            static constexpr TCHAR Hex[] = TEXT("0123456789ABCDEF");
            const auto Unit = static_cast<uint32>(InUnit);
            OutIdentity.AppendChar(Hex[(Unit >> 28) & 0xF]);
            OutIdentity.AppendChar(Hex[(Unit >> 24) & 0xF]);
            OutIdentity.AppendChar(Hex[(Unit >> 20) & 0xF]);
            OutIdentity.AppendChar(Hex[(Unit >> 16) & 0xF]);
            OutIdentity.AppendChar(Hex[(Unit >> 12) & 0xF]);
            OutIdentity.AppendChar(Hex[(Unit >> 8) & 0xF]);
            OutIdentity.AppendChar(Hex[(Unit >> 4) & 0xF]);
            OutIdentity.AppendChar(Hex[Unit & 0xF]);
        }

        auto
            ParseString(
                FString& OutDecoded,
                bool& OutContainsNull,
                FString& OutIdentity,
                bool InBuildIdentity)
            -> bool
        {
            if (Peek() != TEXT('"'))
            {
                return Fail(TEXT("json"), TEXT("Expected string"), _Cursor);
            }
            ++_Cursor;
            auto Units = TArray<TCHAR>{};
            auto ContainsNull = false;
            auto Identity = FString{};

            while (_Cursor < _Text.Len())
            {
                const auto Ch = _Text[_Cursor++];
                if (Ch == TEXT('"'))
                {
                    // ConstructFromPtrSize keeps U+0000 even in the leading slot;
                    // FString's count/pointer constructor checks *Str and drops
                    // a string whose first decoded code unit is NUL.
                    // Consumers must honor OutContainsNull: some FString operations
                    // truncate at NUL even when the stored length includes it.
                    OutDecoded = Units.IsEmpty()
                        ? FString{}
                        : FString::ConstructFromPtrSize(Units.GetData(), Units.Num());
                    OutContainsNull = ContainsNull;
                    OutIdentity = MoveTemp(Identity);
                    return true;
                }
                if (Ch < 0x20)
                {
                    return Fail(TEXT("json"), TEXT("Unescaped control character"), _Cursor - 1);
                }

                if (Ch == TEXT('\\'))
                {
                    if (_Cursor >= _Text.Len())
                    {
                        return Fail(TEXT("json"), TEXT("Truncated escape"), _Cursor);
                    }
                    const auto Escape = _Text[_Cursor++];
                    switch (Escape)
                    {
                        case TEXT('"'): AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, TEXT('"')); break;
                        case TEXT('\\'): AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, TEXT('\\')); break;
                        case TEXT('/'): AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, TEXT('/')); break;
                        case TEXT('b'): AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, TEXT('\b')); break;
                        case TEXT('f'): AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, TEXT('\f')); break;
                        case TEXT('n'): AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, TEXT('\n')); break;
                        case TEXT('r'): AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, TEXT('\r')); break;
                        case TEXT('t'): AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, TEXT('\t')); break;
                        case TEXT('u'):
                        {
                            auto High = uint16{0};
                            if (NOT ReadHex4(High))
                            {
                                return false;
                            }
                            if (High >= 0xDC00 && High <= 0xDFFF)
                            {
                                return Fail(TEXT("json"), TEXT("Unpaired low surrogate"), _Cursor - 4);
                            }
                            if (High >= 0xD800 && High <= 0xDBFF)
                            {
                                if (_Cursor + 6 > _Text.Len() ||
                                    _Text[_Cursor] != TEXT('\\') || _Text[_Cursor + 1] != TEXT('u'))
                                {
                                    return Fail(TEXT("json"), TEXT("Unpaired high surrogate"), _Cursor);
                                }
                                _Cursor += 2;
                                auto Low = uint16{0};
                                if (NOT ReadHex4(Low))
                                {
                                    return false;
                                }
                                if (Low < 0xDC00 || Low > 0xDFFF)
                                {
                                    return Fail(TEXT("json"), TEXT("Invalid low surrogate"), _Cursor - 4);
                                }
                                if constexpr (sizeof(TCHAR) == 2)
                                {
                                    AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, static_cast<TCHAR>(High));
                                    AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, static_cast<TCHAR>(Low));
                                }
                                else
                                {
                                    const auto Scalar = uint32{0x10000} +
                                        ((uint32{High} - 0xD800) << 10) + (uint32{Low} - 0xDC00);
                                    AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, static_cast<TCHAR>(Scalar));
                                }
                            }
                            else
                            {
                                AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, static_cast<TCHAR>(High));
                            }
                            break;
                        }
                        default:
                            return Fail(TEXT("json"), TEXT("Unsupported JSON escape"), _Cursor - 1);
                    }
                    continue;
                }

                // On Windows, TCHAR is UTF-16. Strict UTF-8 decoding by the caller
                // should already have ruled out malformed byte sequences.
                if constexpr (sizeof(TCHAR) == 4)
                {
                    const auto Scalar = static_cast<uint32>(Ch);
                    if (Scalar > 0x10FFFF || (Scalar >= 0xD800 && Scalar <= 0xDFFF))
                    {
                        return Fail(TEXT("json"), TEXT("Invalid Unicode scalar"), _Cursor - 1);
                    }
                    AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, Ch);
                }
                else if (Ch >= 0xD800 && Ch <= 0xDBFF)
                {
                    if (_Cursor >= _Text.Len() ||
                        _Text[_Cursor] < 0xDC00 || _Text[_Cursor] > 0xDFFF)
                    {
                        return Fail(TEXT("json"), TEXT("Unpaired high surrogate"), _Cursor - 1);
                    }
                    AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, Ch);
                    AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, _Text[_Cursor++]);
                }
                else if (Ch >= 0xDC00 && Ch <= 0xDFFF)
                {
                    return Fail(TEXT("json"), TEXT("Unpaired low surrogate"), _Cursor - 1);
                }
                else
                {
                    AppendUnit(Units, ContainsNull, Identity, InBuildIdentity, Ch);
                }
            }
            return Fail(TEXT("json"), TEXT("Unterminated string"), _Cursor);
        }

        static auto
            IsDigit(
                TCHAR InCh)
            -> bool
        {
            return InCh >= TEXT('0') && InCh <= TEXT('9');
        }

        auto
            ParseNumber(
                FNode& OutNode)
            -> bool
        {
            const auto Start = _Cursor;
            if (Peek() == TEXT('-'))
            {
                ++_Cursor;
            }
            if (Peek() == TEXT('0'))
            {
                ++_Cursor;
            }
            else
            {
                if (Peek() < TEXT('1') || Peek() > TEXT('9'))
                {
                    return Fail(TEXT("json"), TEXT("Invalid JSON number"), _Cursor);
                }
                do
                {
                    ++_Cursor;
                } while (IsDigit(Peek()));
            }

            auto bFloating = false;
            if (Peek() == TEXT('.'))
            {
                bFloating = true;
                ++_Cursor;
                if (NOT IsDigit(Peek()))
                {
                    return Fail(TEXT("json"), TEXT("Missing fraction digit"), _Cursor);
                }
                do
                {
                    ++_Cursor;
                } while (IsDigit(Peek()));
            }
            if (Peek() == TEXT('e') || Peek() == TEXT('E'))
            {
                bFloating = true;
                ++_Cursor;
                if (Peek() == TEXT('+') || Peek() == TEXT('-'))
                {
                    ++_Cursor;
                }
                if (NOT IsDigit(Peek()))
                {
                    return Fail(TEXT("json"), TEXT("Missing exponent digit"), _Cursor);
                }
                do
                {
                    ++_Cursor;
                } while (IsDigit(Peek()));
            }
            const auto Raw = _Text.Mid(Start, _Cursor - Start);
            if (bFloating)
            {
                auto UnusedValue = double{0.0};
                if (NOT TryGetFiniteNumber(Raw, UnusedValue))
                {
                    return Fail(TEXT("json"), TEXT("Nonfinite JSON number"), Start);
                }
            }

            OutNode._Kind = EKind::Number;
            OutNode._Value = Raw;
            return true;
        }

        auto
            ParseArray(
                FNode& OutNode,
                int32 InDepth)
            -> bool
        {
            ++_Cursor; // [
            OutNode._Kind = EKind::Array;
            SkipWhitespace();
            if (Peek() == TEXT(']'))
            {
                ++_Cursor;
                return true;
            }
            while (true)
            {
                auto Child = FNode{};
                if (NOT ParseValue(Child, InDepth + 1))
                {
                    return false;
                }
                OutNode._Children.Add(MoveTemp(Child));
                SkipWhitespace();
                if (Peek() == TEXT(']'))
                {
                    ++_Cursor;
                    return true;
                }
                if (Peek() != TEXT(','))
                {
                    return Fail(TEXT("json"), TEXT("Expected array comma or closing bracket"), _Cursor);
                }
                ++_Cursor;
                SkipWhitespace();
            }
        }

        auto
            ParseObject(
                FNode& OutNode,
                int32 InDepth)
            -> bool
        {
            ++_Cursor; // {
            OutNode._Kind = EKind::Object;
            auto SeenKeys = TSet<FString>{};
            SkipWhitespace();
            if (Peek() == TEXT('}'))
            {
                ++_Cursor;
                return true;
            }
            while (true)
            {
                if (NOT ReserveNode()) // Count each object key as well as each value.
                {
                    return false;
                }
                auto Key = FString{};
                auto KeyContainsNull = false;
                auto KeyIdentity = FString{};
                if (NOT ParseString(Key, KeyContainsNull, KeyIdentity, true))
                {
                    return false;
                }
                if (SeenKeys.Contains(KeyIdentity))
                {
                    return Fail(TEXT("json"), TEXT("Duplicate decoded object key"), _Cursor);
                }
                SeenKeys.Add(MoveTemp(KeyIdentity));
                SkipWhitespace();
                if (Peek() != TEXT(':'))
                {
                    return Fail(TEXT("json"), TEXT("Expected object colon"), _Cursor);
                }
                ++_Cursor;
                SkipWhitespace();
                auto Child = FNode{};
                if (NOT ParseValue(Child, InDepth + 1))
                {
                    return false;
                }
                OutNode._Keys.Add(MoveTemp(Key));
                OutNode._KeyContainsNull.Add(KeyContainsNull);
                OutNode._Children.Add(MoveTemp(Child));
                SkipWhitespace();
                if (Peek() == TEXT('}'))
                {
                    ++_Cursor;
                    return true;
                }
                if (Peek() != TEXT(','))
                {
                    return Fail(TEXT("json"), TEXT("Expected object comma or closing brace"), _Cursor);
                }
                ++_Cursor;
                SkipWhitespace();
            }
        }

        auto
            ParseValue(
                FNode& OutNode,
                int32 InDepth)
            -> bool
        {
            if (InDepth > MaxDepth)
            {
                return Fail(TEXT("limits"), TEXT("JSON depth exceeds 64"), _Cursor);
            }
            if (NOT ReserveNode())
            {
                return false;
            }
            const auto Start = _Cursor;
            const auto Ch = Peek();
            if (Ch == TEXT('{'))
            {
                if (NOT ParseObject(OutNode, InDepth))
                {
                    return false;
                }
            }
            else if (Ch == TEXT('['))
            {
                if (NOT ParseArray(OutNode, InDepth))
                {
                    return false;
                }
            }
            else if (Ch == TEXT('"'))
            {
                OutNode._Kind = EKind::String;
                auto UnusedIdentity = FString{};
                if (NOT ParseString(OutNode._Value, OutNode._ContainsNull, UnusedIdentity, false))
                {
                    return false;
                }
            }
            else if (Ch == TEXT('-') || IsDigit(Ch))
            {
                if (NOT ParseNumber(OutNode))
                {
                    return false;
                }
            }
            else if (MatchLiteral(TEXT("true")))
            {
                OutNode._Kind = EKind::Boolean;
                OutNode._bBool = true;
            }
            else if (MatchLiteral(TEXT("false")))
            {
                OutNode._Kind = EKind::Boolean;
                OutNode._bBool = false;
            }
            else if (MatchLiteral(TEXT("null")))
            {
                OutNode._Kind = EKind::Null;
            }
            else
            {
                return Fail(TEXT("json"), TEXT("Expected JSON value"), _Cursor);
            }
            OutNode._Start = Start;
            OutNode._End = _Cursor;
            return true;
        }

        const FString& _Text;
        FCk_PixelSprite_Diagnostic& _Diagnostic;
        int32 _Cursor = 0;
        int32 _NodeCount = 0;
    };

    // OutRoot is unchanged on every failure. A successful call clears stale diagnostics.
    inline auto
        TryParse(
        const FString& InText,
        FNode& OutRoot,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
        auto Candidate = FNode{};
        auto Diagnostic = FCk_PixelSprite_Diagnostic{};
        auto Parser = FParser{InText, Diagnostic};
        if (NOT Parser.Parse(Candidate))
        {
            OutDiagnostic = MoveTemp(Diagnostic);
            return false;
        }
        OutRoot = MoveTemp(Candidate);
        OutDiagnostic = {};
        return true;
    }
}
