#include "CkPixelSprite/CkPixelSprite_Data.h"

#include "Misc/Parse.h"

namespace ck_pixel_sprite_xpm
{
    struct FToken
    {
        FString _Text;
        bool _StringLiteral = false;
        int32 _Line = 1;
        int32 _Column = 1;
    };

    class FLexer
    {
    public:
        FLexer(const FString& InText, const FString& InSource, FCk_PixelSprite_Diagnostic& OutDiagnostic)
            : _Text(InText), _Source(InSource), _Diagnostic(OutDiagnostic)
        {
        }

        auto
            Fail(
                const FString& InCode,
                const FString& InMessage)
            -> bool
        {
            _Diagnostic = {_Source, InCode, InMessage, _Line, _Column};
            return false;
        }

        auto
            Next(
                FToken& OutToken)
            -> bool
        {
            while (_Position < _Text.Len())
            {
                const auto Char = _Text[_Position];
                if (FChar::IsWhitespace(Char) || (_Position == 0 && Char == 0xfeff))
                {
                    Advance();
                    continue;
                }
                if (Char == '/' && Peek(1) == '/')
                {
                    while (_Position < _Text.Len() && _Text[_Position] != '\n')
                    {
                        Advance();
                    }
                    continue;
                }
                if (Char == '/' && Peek(1) == '*')
                {
                    Advance(); Advance();
                    while (_Position < _Text.Len() && NOT (_Text[_Position] == '*' && Peek(1) == '/'))
                    {
                        Advance();
                    }
                    if (_Position == _Text.Len())
                    {
                        return Fail(TEXT("syntax"), TEXT("Unterminated comment"));
                    }
                    Advance(); Advance();
                    continue;
                }
                break;
            }
            OutToken = {{}, false, _Line, _Column};
            if (_Position == _Text.Len())
            {
                return true;
            }
            const auto Char = _Text[_Position];
            if (Char == '"')
            {
                OutToken._StringLiteral = true;
                Advance();
                while (_Position < _Text.Len() && _Text[_Position] != '"')
                {
                    auto Value = static_cast<uint32>(_Text[_Position]);
                    Advance();
                    if (Value == '\\')
                    {
                        if (_Position == _Text.Len())
                        {
                            return Fail(TEXT("syntax"), TEXT("Truncated escape"));
                        }
                        const auto Escape = _Text[_Position];
                        Advance();
                        if (Escape == '\\' || Escape == '"' || Escape == '\'' || Escape == '?')
                        {
                            Value = Escape;
                        }
                        else if (Escape >= '0' && Escape <= '7')
                        {
                            Value = Escape - '0';
                            for (auto Digit = 1; Digit < 3 && Peek(0) >= '0' && Peek(0) <= '7'; ++Digit)
                            {
                                Value = Value * 8 + Peek(0) - '0';
                                Advance();
                            }
                        }
                        else if (Escape == 'x')
                        {
                            Value = 0;
                            auto Count = 0;
                            while (FChar::IsHexDigit(Peek(0)))
                            {
                                Value = Value * 16 + FParse::HexDigit(Peek(0));
                                Advance(); ++Count;
                                if (Value > 255)
                                {
                                    return Fail(TEXT("escape"), TEXT("Hex escape exceeds one byte"));
                                }
                            }
                            if (Count == 0)
                            {
                                return Fail(TEXT("escape"), TEXT("Hex escape requires digits"));
                            }
                        }
                        else
                        {
                            return Fail(TEXT("escape"), TEXT("Unsupported or non-printable escape"));
                        }
                    }
                    if (Value < 32 || Value > 126)
                    {
                        return Fail(TEXT("character"), TEXT("XPM data must contain printable ASCII pixels"));
                    }
                    OutToken._Text.AppendChar(static_cast<TCHAR>(Value));
                }
                if (_Position == _Text.Len())
                {
                    return Fail(TEXT("syntax"), TEXT("Unterminated string literal"));
                }
                Advance();
                return true;
            }
            if ((Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z') || Char == '_')
            {
                do
                {
                    OutToken._Text.AppendChar(_Text[_Position]);
                    Advance();
                }
                while (FChar::IsAlnum(Peek(0)) || Peek(0) == '_');
                return true;
            }
            if (FString{TEXT("*[]=,{};")}.Contains(FString::Chr(Char)))
            {
                OutToken._Text.AppendChar(Char);
                Advance();
                return true;
            }
            return Fail(TEXT("syntax"), TEXT("Unsupported token outside XPM array"));
        }

    private:
        auto
            Peek(
                int32 InOffset) const
            -> TCHAR
        {
            return _Position + InOffset < _Text.Len() ? _Text[_Position + InOffset] : 0;
        }

        auto
            Advance()
            -> void
        {
            if (_Text[_Position++] == '\n')
            {
                ++_Line;
                _Column = 1;
            }
            else
            {
                ++_Column;
            }
        }

        const FString& _Text;
        const FString& _Source;
        FCk_PixelSprite_Diagnostic& _Diagnostic;
        int32 _Position = 0;
        int32 _Line = 1;
        int32 _Column = 1;
    };

    auto
        ParseInteger(
            const FString& InText,
            int32& OutValue)
        -> bool
    {
        if (InText.IsEmpty() || InText.Len() > 10)
        {
            return false;
        }
        auto Value = int64{0};
        for (const auto Char : InText)
        {
            if (Char < '0' || Char > '9')
            {
                return false;
            }
            Value = Value * 10 + Char - '0';
            if (Value > MAX_int32)
            {
                return false;
            }
        }
        OutValue = static_cast<int32>(Value);
        return true;
    }

    auto
        IsSelector(
            const FString& InText)
        -> bool
    {
        return InText.Equals(TEXT("c"), ESearchCase::CaseSensitive) ||
            InText.Equals(TEXT("m"), ESearchCase::CaseSensitive) ||
            InText.Equals(TEXT("g"), ESearchCase::CaseSensitive) ||
            InText.Equals(TEXT("g4"), ESearchCase::CaseSensitive) ||
            InText.Equals(TEXT("s"), ESearchCase::CaseSensitive);
    }

    auto
        ParseColor(
            const FString& InText,
            FColor& OutColor)
        -> bool
    {
        if (InText.Equals(TEXT("None"), ESearchCase::IgnoreCase))
        {
            OutColor = FColor::Transparent;
            return true;
        }
        if (InText.StartsWith(TEXT("#")))
        {
            const auto Digits = InText.Len() - 1;
            if (Digits != 3 && Digits != 6 && Digits != 9 && Digits != 12)
            {
                return false;
            }
            auto Components = TArray<uint8>{};
            const auto ComponentDigits = Digits / 3;
            const auto Maximum = (1 << (ComponentDigits * 4)) - 1;
            for (auto Component = 0; Component < 3; ++Component)
            {
                auto Value = 0;
                for (auto Digit = 0; Digit < ComponentDigits; ++Digit)
                {
                    const auto Char = InText[1 + Component * ComponentDigits + Digit];
                    if (NOT FChar::IsHexDigit(Char))
                    {
                        return false;
                    }
                    Value = Value * 16 + FParse::HexDigit(Char);
                }
                Components.Add(static_cast<uint8>((Value * 255 + Maximum / 2) / Maximum));
            }
            OutColor = FColor{Components[0], Components[1], Components[2], 255};
            return true;
        }
        const auto Name = InText.ToLower();
        const auto Names = TMap<FString, FColor>{
            {TEXT("black"), FColor{0, 0, 0}}, {TEXT("white"), FColor{255, 255, 255}},
            {TEXT("red"), FColor{255, 0, 0}}, {TEXT("green"), FColor{0, 255, 0}},
            {TEXT("blue"), FColor{0, 0, 255}}, {TEXT("yellow"), FColor{255, 255, 0}},
            {TEXT("cyan"), FColor{0, 255, 255}}, {TEXT("magenta"), FColor{255, 0, 255}},
            {TEXT("gray"), FColor{190, 190, 190}}, {TEXT("grey"), FColor{190, 190, 190}}};
        if (Names.Contains(Name))
        {
            OutColor = Names[Name];
            return true;
        }
        if (Name.StartsWith(TEXT("gray")) || Name.StartsWith(TEXT("grey")))
        {
            auto Percent = 0;
            if (ParseInteger(Name.Mid(4), Percent) && Percent <= 100)
            {
                const auto Value = static_cast<uint8>((Percent * 255 + 50) / 100);
                OutColor = FColor{Value, Value, Value};
                return true;
            }
        }
        return false;
    }
}

auto
    FCk_PixelSprite_Diagnostic::Get_Text() const -> FString
{
    return FString::Printf(TEXT("%s:%d:%d: %s: %s"), *_Source, _Line, _Column, *_Code, *_Message);
}

auto
    ck::pixel_sprite::TryParse_Xpm(
        const FString& InText,
        const FString& InSource,
        FCk_PixelSprite_Grid& OutGrid,
        FCk_PixelSprite_Diagnostic& OutDiagnostic,
        const FCk_PixelSprite_Limits& InLimits) -> bool
{
    using namespace ck_pixel_sprite_xpm;
    OutDiagnostic = {};
    auto Lexer = FLexer{InText, InSource, OutDiagnostic};
    if (InLimits._SourceCharacters <= 0 || InText.Len() > InLimits._SourceCharacters ||
        InLimits._Dimension <= 0 || InLimits._Pixels <= 0 || InLimits._PaletteEntries <= 0 ||
        InLimits._PaletteEntries > 65536 || InLimits._CharactersPerPixel <= 0 || InLimits._CharactersPerPixel > 16)
    {
        return Lexer.Fail(TEXT("limit"), TEXT("Source or configured safety limit exceeded"));
    }
    auto Token = FToken{};
    if (NOT Lexer.Next(Token))
    {
        return false;
    }
    if (NOT Token._StringLiteral && Token._Text.Equals(TEXT("static"), ESearchCase::CaseSensitive))
    {
        if (NOT Lexer.Next(Token))
        {
            return false;
        }
    }
    if (NOT Token._StringLiteral && Token._Text.Equals(TEXT("const"), ESearchCase::CaseSensitive))
    {
        if (NOT Lexer.Next(Token))
        {
            return false;
        }
    }
    if (Token._StringLiteral || NOT Token._Text.Equals(TEXT("char"), ESearchCase::CaseSensitive))
    {
        return Lexer.Fail(TEXT("syntax"), TEXT("Expected char pointer array declaration"));
    }
    const auto Require = [&Lexer, &Token](const FString& InExpected) -> bool
    {
        if (NOT Lexer.Next(Token))
        {
            return false;
        }
        if (Token._StringLiteral || NOT Token._Text.Equals(InExpected, ESearchCase::CaseSensitive))
        {
            return Lexer.Fail(TEXT("syntax"), TEXT("Expected ") + InExpected);
        }
        return true;
    };
    if (NOT Require(TEXT("*")) || NOT Lexer.Next(Token))
    {
        return false;
    }
    if (NOT Token._StringLiteral && Token._Text.Equals(TEXT("const"), ESearchCase::CaseSensitive))
    {
        if (NOT Lexer.Next(Token))
        {
            return false;
        }
    }
    if (Token._StringLiteral || Token._Text.IsEmpty() ||
        Token._Text.Equals(TEXT("const"), ESearchCase::CaseSensitive) ||
        Token._Text.Equals(TEXT("static"), ESearchCase::CaseSensitive) ||
        Token._Text.Equals(TEXT("char"), ESearchCase::CaseSensitive) ||
        NOT (FChar::IsAlpha(Token._Text[0]) || Token._Text[0] == '_'))
    {
        return Lexer.Fail(TEXT("syntax"), TEXT("Expected array identifier"));
    }
    if (NOT Require(TEXT("[")) || NOT Require(TEXT("]")) || NOT Require(TEXT("=")) || NOT Require(TEXT("{")))
    {
        return false;
    }
    auto Lines = TArray<FToken>{};
    while (true)
    {
        if (NOT Lexer.Next(Token))
        {
            return false;
        }
        if (Token._Text == TEXT("}") && NOT Token._StringLiteral)
        {
            break;
        }
        if (NOT Token._StringLiteral)
        {
            return Lexer.Fail(TEXT("syntax"), TEXT("Expected XPM string literal"));
        }
        Lines.Add(MoveTemp(Token));
        if (Lines.Num() > static_cast<int64>(InLimits._Dimension) + InLimits._PaletteEntries + 1)
        {
            return Lexer.Fail(TEXT("limit"), TEXT("Too many XPM strings"));
        }
        if (NOT Lexer.Next(Token))
        {
            return false;
        }
        if (Token._Text == TEXT("}") && NOT Token._StringLiteral)
        {
            break;
        }
        if (Token._StringLiteral || Token._Text != TEXT(","))
        {
            return Lexer.Fail(TEXT("syntax"), TEXT("Expected comma between XPM strings"));
        }
    }
    if (NOT Require(TEXT(";")) || NOT Lexer.Next(Token))
    {
        return false;
    }
    if (NOT Token._Text.IsEmpty() || Token._StringLiteral)
    {
        return Lexer.Fail(TEXT("syntax"), TEXT("Unexpected data after XPM array"));
    }
    const auto FailLine = [&OutDiagnostic, &InSource](const FToken& InToken, const FString& InCode, const FString& InMessage) -> bool
    {
        OutDiagnostic = {InSource, InCode, InMessage, InToken._Line, InToken._Column};
        return false;
    };
    if (Lines.IsEmpty())
    {
        return Lexer.Fail(TEXT("header"), TEXT("Missing XPM header"));
    }
    auto Header = TArray<FString>{};
    Lines[0]._Text.ParseIntoArrayWS(Header);
    if (Header.Num() != 4 && Header.Num() != 6)
    {
        return FailLine(Lines[0], TEXT("header"), TEXT("Expected width height colors cpp and optional hotspot; extensions unsupported"));
    }
    auto Values = TArray<int32>{};
    for (const auto& Field : Header)
    {
        auto Value = 0;
        if (NOT ParseInteger(Field, Value))
        {
            return FailLine(Lines[0], TEXT("header"), TEXT("Header must contain bounded decimal integers"));
        }
        Values.Add(Value);
    }
    const auto Width = Values[0];
    const auto Height = Values[1];
    const auto Colors = Values[2];
    const auto Cpp = Values[3];
    if (Width <= 0 || Height <= 0 || Colors <= 0 || Cpp <= 0 ||
        Width > InLimits._Dimension || Height > InLimits._Dimension || Colors > InLimits._PaletteEntries ||
        Cpp > InLimits._CharactersPerPixel || static_cast<int64>(Width) * Height > InLimits._Pixels)
    {
        return FailLine(Lines[0], TEXT("limit"), TEXT("Dimensions, palette or cpp outside safety limits"));
    }
    if (Header.Num() == 6 && (Values[4] >= Width || Values[5] >= Height))
    {
        return FailLine(Lines[0], TEXT("header"), TEXT("Hotspot outside canvas"));
    }
    if (Lines.Num() != int64{1} + Colors + Height)
    {
        return FailLine(Lines[0], TEXT("rows"), TEXT("Palette or pixel row count differs from header"));
    }
    auto Grid = FCk_PixelSprite_Grid{};
    Grid._Width = Width;
    Grid._Height = Height;
    if (Header.Num() == 6)
    {
        Grid._Hotspot = FIntPoint{Values[4], Values[5]};
    }
    auto SymbolIndices = ck::pixel_sprite::TCaseSensitiveStringMap<uint16>{};
    for (auto Index = 0; Index < Colors; ++Index)
    {
        const auto& Line = Lines[1 + Index];
        if (Line._Text.Len() <= Cpp || NOT FChar::IsWhitespace(Line._Text[Cpp]))
        {
            return FailLine(Line, TEXT("palette"), TEXT("Palette symbol must be followed by whitespace and color keys"));
        }
        const auto Symbol = Line._Text.Left(Cpp);
        if (SymbolIndices.Contains(Symbol))
        {
            return FailLine(Line, TEXT("palette"), TEXT("Duplicate palette symbol"));
        }
        auto Fields = TArray<FString>{};
        Line._Text.Mid(Cpp).ParseIntoArrayWS(Fields);
        auto Declarations = ck::pixel_sprite::TCaseSensitiveStringMap<FString>{};
        for (auto Field = 0; Field < Fields.Num();)
        {
            const auto Key = Fields[Field++];
            if (NOT IsSelector(Key) || Declarations.Contains(Key))
            {
                return FailLine(Line, TEXT("palette"), TEXT("Unknown or duplicate color selector"));
            }
            auto Value = FString{};
            while (Field < Fields.Num() && NOT IsSelector(Fields[Field]))
            {
                if (NOT Value.IsEmpty())
                {
                    Value += TEXT(" ");
                }
                Value += Fields[Field++];
            }
            if (Value.IsEmpty())
            {
                return FailLine(Line, TEXT("palette"), TEXT("Color selector has no value"));
            }
            Declarations.Add(Key, MoveTemp(Value));
        }
        auto Color = FColor{};
        if (NOT Declarations.Contains(TEXT("c")) || NOT ParseColor(Declarations[TEXT("c")], Color))
        {
            return FailLine(Line, TEXT("color"), TEXT("A supported explicit c color is required"));
        }
        SymbolIndices.Add(Symbol, static_cast<uint16>(Index));
        Grid._Palette.Add({Symbol, Color});
    }
    Grid._Indices.Reserve(Width * Height);
    for (auto Row = 0; Row < Height; ++Row)
    {
        const auto& Line = Lines[1 + Colors + Row];
        if (Line._Text.Len() != static_cast<int64>(Width) * Cpp)
        {
            return FailLine(Line, TEXT("row_length"), TEXT("Pixel row must contain exactly width * cpp characters"));
        }
        for (auto Column = 0; Column < Width; ++Column)
        {
            const auto Symbol = Line._Text.Mid(Column * Cpp, Cpp);
            if (NOT SymbolIndices.Contains(Symbol))
            {
                return FailLine(Line, TEXT("symbol"), TEXT("Pixel references an undeclared palette symbol"));
            }
            Grid._Indices.Add(SymbolIndices[Symbol]);
        }
    }
    OutGrid = MoveTemp(Grid);
    return true;
}
