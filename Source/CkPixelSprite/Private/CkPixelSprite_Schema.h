#pragma once

#include "CkPixelSprite_File.h"
#include "CkPixelSprite/CkPixelSprite_Data.h"
#include "CkPixelSprite_Json.h"
#include "Misc/Paths.h"

namespace ck::pixel_sprite::manifest_private
{
    using json_private::EKind;
    using json_private::FNode;

    class FSchema
    {
    public:
        FSchema(const FString& InText, const FString& InSource, FCk_PixelSprite_Diagnostic& OutDiagnostic)
            : _Text(InText), _Source(InSource), _Diagnostic(OutDiagnostic)
        {}

        auto
            Parse(
                const FNode& InRoot,
                FCk_PixelSprite_Manifest& OutManifest)
            -> bool
        {
            if (NOT Object(InRoot, {TEXT("version"), TEXT("canvas"), TEXT("animations"),
                TEXT("events"), TEXT("metadata")}, {TEXT("version"), TEXT("canvas"), TEXT("animations")}))
            {
                return false;
            }
            auto Version = int32{0};
            if (NOT Integer(Field(InRoot, TEXT("version")), 1, 1, Version))
            {
                _Diagnostic._Code = TEXT("version");
                return false;
            }
            const auto& Canvas = Field(InRoot, TEXT("canvas"));
            if (NOT Object(Canvas, {TEXT("width"), TEXT("height")}, {TEXT("width"), TEXT("height")}))
            {
                return false;
            }
            auto Candidate = FCk_PixelSprite_Manifest{};
            if (NOT Integer(Field(Canvas, TEXT("width")), 1, 4096, Candidate._Width) ||
                NOT Integer(Field(Canvas, TEXT("height")), 1, 4096, Candidate._Height))
            {
                return false;
            }
            const auto Pixels = int64{Candidate._Width} * Candidate._Height;
            if (Pixels > 1024 * 1024)
            {
                return Fail(Canvas, TEXT("limits"), TEXT("Canvas exceeds pixel budget"));
            }
            if (NOT Extras(InRoot, Candidate._EventsJson, Candidate._MetadataJson))
            {
                return false;
            }
            const auto& Definitions = Field(InRoot, TEXT("animations"));
            if (Definitions._Kind != EKind::Object || Definitions._Children.IsEmpty() ||
                Definitions._Children.Num() > 1024)
            {
                return Fail(Definitions, TEXT("limits"), TEXT("Animations must be an object with 1..1024 entries"));
            }
            auto TotalFrames = int32{0};
            for (auto AnimationIndex = int32{0}; AnimationIndex < Definitions._Children.Num(); ++AnimationIndex)
            {
                const auto& Name = Definitions._Keys[AnimationIndex];
                const auto& Definition = Definitions._Children[AnimationIndex];
                if (Definitions._KeyContainsNull[AnimationIndex] || NOT NameValid(Name))
                {
                    return Fail(Definition, TEXT("schema"), TEXT("Animation name must have 1..128 characters"));
                }
                if (NOT Object(Definition, {TEXT("playback"), TEXT("frames"), TEXT("events"), TEXT("metadata")},
                    {TEXT("playback"), TEXT("frames")}))
                {
                    return false;
                }
                auto Animation = FCk_PixelSprite_Animation{};
                Animation._Name = Name;
                const auto& Playback = Field(Definition, TEXT("playback"));
                if (Playback._Kind != EKind::String || Playback._ContainsNull)
                {
                    return Fail(Playback, TEXT("playback"), TEXT("Playback must be loop, once or ping-pong"));
                }
                if (Equal(Playback._Value, TEXT("loop")))
                {
                    Animation._Playback = ECk_PixelSprite_Playback::Loop;
                }
                else if (Equal(Playback._Value, TEXT("once")))
                {
                    Animation._Playback = ECk_PixelSprite_Playback::Once;
                }
                else if (Equal(Playback._Value, TEXT("ping-pong")))
                {
                    Animation._Playback = ECk_PixelSprite_Playback::PingPong;
                }
                else
                {
                    return Fail(Playback, TEXT("playback"), TEXT("Unsupported playback mode"));
                }
                if (NOT Extras(Definition, Animation._EventsJson, Animation._MetadataJson))
                {
                    return false;
                }
                const auto& Frames = Field(Definition, TEXT("frames"));
                if (Frames._Kind != EKind::Array || Frames._Children.IsEmpty())
                {
                    return Fail(Frames, TEXT("schema"), TEXT("Frames must be a nonempty array"));
                }
                for (const auto& Value : Frames._Children)
                {
                    ++TotalFrames;
                    if (TotalFrames > 4096 || int64{TotalFrames} * Pixels > 16 * 1024 * 1024)
                    {
                        return Fail(Value, TEXT("limits"), TEXT("Aggregate frame budget exceeded"));
                    }
                    auto Frame = FCk_PixelSprite_Frame{};
                    if (NOT ParseFrame(Value, Candidate._Width, Candidate._Height, Frame))
                    {
                        return false;
                    }
                    Animation._Frames.Add(MoveTemp(Frame));
                }
                Candidate._Animations.Add(MoveTemp(Animation));
            }
            OutManifest = MoveTemp(Candidate);
            return true;
        }

    private:
        static auto
            Equal(
                const FString& InLeft,
                const FString& InRight)
            -> bool
        {
            return InLeft.Equals(InRight, ESearchCase::CaseSensitive);
        }

        static auto
            FieldIndex(
                const FNode& InObject,
                const FString& InName)
            -> int32
        {
            for (auto Index = int32{0}; Index < InObject._Keys.Num(); ++Index)
            {
                if (NOT InObject._KeyContainsNull[Index] && Equal(InObject._Keys[Index], InName))
                {
                    return Index;
                }
            }
            return INDEX_NONE;
        }

        static auto
            Contains(
                const TArray<FString>& InNames,
                const FString& InName)
            -> bool
        {
            for (const auto& Name : InNames)
            {
                if (Equal(Name, InName))
                {
                    return true;
                }
            }
            return false;
        }

        auto
            Fail(
                const FNode& InNode,
                const FString& InCode,
                const FString& InMessage)
            -> bool
        {
            auto Line = int32{1};
            auto Column = int32{1};
            for (auto Index = int32{0}; Index < InNode._Start; ++Index)
            {
                if (_Text[Index] == TEXT('\r'))
                {
                    ++Line; Column = 1;
                    if (Index + 1 < InNode._Start && _Text[Index + 1] == TEXT('\n'))
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
            _Diagnostic = {_Source, InCode, InMessage, Line, Column};
            return false;
        }

        static auto
            NameValid(
                const FString& InName)
            -> bool
        {
            auto Count = int32{0};
            for (auto Index = int32{0}; Index < InName.Len(); ++Index)
            {
                ++Count;
                if (InName[Index] >= 0xD800 && InName[Index] <= 0xDBFF)
                {
                    ++Index;
                }
            }
            return Count >= 1 && Count <= 128;
        }

        auto
            Object(
                const FNode& InNode, const TArray<FString>& InAllowed,
                const TArray<FString>& InRequired)
            -> bool
        {
            if (InNode._Kind != EKind::Object)
            {
                return Fail(InNode, TEXT("schema"), TEXT("Expected an object"));
            }
            for (auto KeyIndex = int32{0}; KeyIndex < InNode._Keys.Num(); ++KeyIndex)
            {
                if (NOT InNode._KeyContainsNull[KeyIndex] && Equal(InNode._Keys[KeyIndex], TEXT("layers")))
                {
                    return Fail(InNode, TEXT("layers_unsupported"), TEXT("Layer composition is unsupported in version 1"));
                }
            }
            for (auto KeyIndex = int32{0}; KeyIndex < InNode._Keys.Num(); ++KeyIndex)
            {
                const auto& Key = InNode._Keys[KeyIndex];
                if (InNode._KeyContainsNull[KeyIndex] || NOT Contains(InAllowed, Key))
                { return Fail(InNode, TEXT("schema"),
                    TEXT("Unsupported field: ") + Key); }
            }
            for (const auto& Key : InRequired)
            {
                if (FieldIndex(InNode, Key) == INDEX_NONE)
                {
                    return Fail(InNode, TEXT("schema"), TEXT("Missing field: ") + Key);
                }
            }
            return true;
        }

        // Called only after Object confirms a required field; no failure sentinel.
        static auto
            Field(
                const FNode& InObject,
                const FString& InName)
            -> const FNode&
        {
            return InObject._Children[FieldIndex(InObject, InName)];
        }

        auto
            Integer(
                const FNode& InNode,
                int32 InLow,
                int32 InHigh,
                int32& OutValue)
            -> bool
        {
            if (InNode._Kind != EKind::Number || InNode._Value.IsEmpty())
            {
                return Fail(InNode, TEXT("schema"), TEXT("Expected an integer token"));
            }
            auto Number = int64{0};
            const auto Token = InNode._Value == TEXT("-0") ? FString{TEXT("0")} : InNode._Value;
            for (const auto Character : Token)
            {
                if (Character < TEXT('0') || Character > TEXT('9'))
                {
                    return Fail(InNode, TEXT("schema"), TEXT("Expected a nonnegative integer token"));
                }
                Number = Number * 10 + Character - TEXT('0');
                if (Number > InHigh)
                {
                    return Fail(InNode, TEXT("schema"), TEXT("Integer is outside its allowed range"));
                }
            }
            if (Number < InLow)
            {
                return Fail(InNode, TEXT("schema"), TEXT("Integer is outside its allowed range"));
            }
            OutValue = static_cast<int32>(Number);
            return true;
        }

        auto
            Extras(
                const FNode& InNode,
                FString& OutEvents,
                FString& OutMetadata)
            -> bool
        {
            for (const auto& Name : {FString{TEXT("events")}, FString{TEXT("metadata")}})
            {
                const auto Index = FieldIndex(InNode, Name);
                if (Index == INDEX_NONE)
                {
                    continue;
                }
                const auto& Value = InNode._Children[Index];
                const auto Expected = Equal(Name, TEXT("events")) ? EKind::Array : EKind::Object;
                if (Value._Kind != Expected)
                {
                    return Fail(Value, TEXT("schema"), Name + TEXT(" has the wrong JSON type"));
                }
                auto& Destination = Equal(Name, TEXT("events")) ? OutEvents : OutMetadata;
                Destination = _Text.Mid(Value._Start, Value._End - Value._Start);
            }
            return true;
        }

        auto
            ParseFrame(
                const FNode& InNode, int32 InWidth, int32 InHeight,
                FCk_PixelSprite_Frame& OutFrame)
            -> bool
        {
            if (NOT Object(InNode, {TEXT("source"), TEXT("duration_ms"), TEXT("anchors"),
                TEXT("events"), TEXT("metadata")}, {TEXT("source"), TEXT("duration_ms")}))
            {
                return false;
            }
            const auto& Source = Field(InNode, TEXT("source"));
            if (Source._Kind != EKind::String || Source._ContainsNull)
            {
                return Fail(Source, TEXT("source_path"), TEXT("Source must be a string"));
            }
            auto PathDiagnostic = FString{};
            if (NOT file_private::Validate_SourcePath(Source._Value, PathDiagnostic))
            {
                return Fail(Source, TEXT("source_path"), PathDiagnostic);
            }
            OutFrame._Source = Source._Value;
            const auto& Duration = Field(InNode, TEXT("duration_ms"));
            if (Duration._Kind != EKind::Number)
            {
                return Fail(Duration, TEXT("duration"), TEXT("Duration must be numeric"));
            }
            auto Milliseconds = 0.0;
            if (NOT json_private::TryGetFiniteNumber(Duration._Value, Milliseconds))
            {
                return Fail(Duration, TEXT("duration"), TEXT("Duration cannot be represented"));
            }
            if (NOT FMath::IsFinite(Milliseconds) || Milliseconds <= 0.0 || Milliseconds > 86400000.0)
            {
                return Fail(Duration, TEXT("duration"), TEXT("Duration must be finite, positive and at most one day"));
            }
            OutFrame._DurationMilliseconds = Milliseconds;
            if (OutFrame.Get_Duration().Get_Seconds() <= 0.0)
            {
                return Fail(Duration, TEXT("duration"), TEXT("Duration is too small to represent"));
            }
            if (NOT Extras(InNode, OutFrame._EventsJson, OutFrame._MetadataJson))
            {
                return false;
            }
            const auto AnchorsIndex = FieldIndex(InNode, TEXT("anchors"));
            if (AnchorsIndex == INDEX_NONE)
            {
                return true;
            }
            const auto& Anchors = InNode._Children[AnchorsIndex];
            if (Anchors._Kind != EKind::Object || Anchors._Children.Num() > 256)
            {
                return Fail(Anchors, TEXT("anchors"), TEXT("Anchors must be an object with at most 256 entries"));
            }
            for (auto Index = int32{0}; Index < Anchors._Children.Num(); ++Index)
            {
                const auto& Point = Anchors._Children[Index];
                if (Anchors._KeyContainsNull[Index] || NOT NameValid(Anchors._Keys[Index]) || Point._Kind != EKind::Array || Point._Children.Num() != 2)
                {
                    return Fail(Point, TEXT("anchors"), TEXT("Anchor must name a two-element point"));
                }
                auto Location = FIntPoint{};
                if (NOT Integer(Point._Children[0], 0, InWidth - 1, Location.X) ||
                    NOT Integer(Point._Children[1], 0, InHeight - 1, Location.Y))
                {
                    return false;
                }
                OutFrame._Anchors.Add(Anchors._Keys[Index], Location);
            }
            return true;
        }

        const FString& _Text;
        const FString& _Source;
        FCk_PixelSprite_Diagnostic& _Diagnostic;
    };
}
