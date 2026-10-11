#pragma once

#include "CoreMinimal.h"
#include "CkCore/Time/CkTime.h"

struct CKPIXELSPRITE_API FCk_PixelSprite_Limits
{
    int32 _SourceCharacters = 8 * 1024 * 1024;
    int32 _Dimension = 4096;
    int32 _Pixels = 1024 * 1024;
    int32 _PaletteEntries = 4096;
    int32 _CharactersPerPixel = 4;
};

struct CKPIXELSPRITE_API FCk_PixelSprite_Diagnostic
{
    FString _Source;
    FString _Code;
    FString _Message;
    int32 _Line = 1;
    int32 _Column = 1;

    auto
        Get_Text() const
        -> FString;
};

struct CKPIXELSPRITE_API FCk_PixelSprite_PaletteEntry
{
    FString _Symbol;
    FColor _Color = FColor::Transparent;
};

/** Owned value data; indices address Palette and never depend on a render texture. */
struct CKPIXELSPRITE_API FCk_PixelSprite_Grid
{
    int32 _Width = 0;
    int32 _Height = 0;
    TArray<FCk_PixelSprite_PaletteEntry> _Palette;
    TArray<uint16> _Indices;
    TOptional<FIntPoint> _Hotspot;
};

enum class ECk_PixelSprite_Playback : uint8
{
    Loop,
    Once,
    PingPong
};

struct CKPIXELSPRITE_API FCk_PixelSprite_Timing
{
    TArray<FCk_Time> _Durations;
    ECk_PixelSprite_Playback _Playback = ECk_PixelSprite_Playback::Loop;
};

struct CKPIXELSPRITE_API FCk_PixelSprite_FrameSelection
{
    int32 _Index = INDEX_NONE;
    FCk_Time _FrameTime;
    FCk_Time _CycleDuration;
    bool _Complete = false;
};

namespace ck::pixel_sprite
{
    // Source paths and authored names retain their exact case. FString's default
    // map equality ignores case, so these maps need explicit key semantics.
    template <typename TValue>
    struct TCaseSensitiveStringKeyFuncs : TDefaultMapHashableKeyFuncs<FString, TValue, false>
    {
        static auto
            Matches(
                const FString& InLeft,
                const FString& InRight)
            -> bool
        {
            return InLeft.Equals(InRight, ESearchCase::CaseSensitive);
        }
    };

    template <typename TValue>
    using TCaseSensitiveStringMap = TMap<FString, TValue, FDefaultSetAllocator,
        TCaseSensitiveStringKeyFuncs<TValue>>;
}

struct CKPIXELSPRITE_API FCk_PixelSprite_Frame
{
    FString _Source;
    int32 _SourceIndex = INDEX_NONE;
    double _DurationMilliseconds = 0.0;
    ck::pixel_sprite::TCaseSensitiveStringMap<FIntPoint> _Anchors;
    FString _EventsJson = TEXT("[]");
    FString _MetadataJson = TEXT("{}");

    auto
        Get_Duration() const
        -> FCk_Time
    {
        return FCk_Time{_DurationMilliseconds / 1000.0};
    }
};

struct CKPIXELSPRITE_API FCk_PixelSprite_Animation
{
    FString _Name;
    ECk_PixelSprite_Playback _Playback = ECk_PixelSprite_Playback::Loop;
    TArray<FCk_PixelSprite_Frame> _Frames;
    FString _EventsJson = TEXT("[]");
    FString _MetadataJson = TEXT("{}");
};

/** Immutable, validated animation timeline. Retain a shared pointer across authored-data changes. */
struct FCk_PixelSprite_PreparedAnimation;

struct CKPIXELSPRITE_API FCk_PixelSprite_Source
{
    FString _Path;
    int64 _Bytes = 0;
    FCk_PixelSprite_Grid _Grid;
};

struct CKPIXELSPRITE_API FCk_PixelSprite_Manifest
{
    int32 _Width = 0;
    int32 _Height = 0;
    TArray<FCk_PixelSprite_Animation> _Animations;
    TArray<FCk_PixelSprite_Source> _Sources;
    FString _EventsJson = TEXT("[]");
    FString _MetadataJson = TEXT("{}");
};


namespace ck::pixel_sprite
{
    /** Total data recognition: failure reports a diagnostic and leaves OutGrid unchanged. */
    CKPIXELSPRITE_API auto TryParse_Xpm(
        const FString& InText,
        const FString& InSource,
        FCk_PixelSprite_Grid& OutGrid,
        FCk_PixelSprite_Diagnostic& OutDiagnostic,
        const FCk_PixelSprite_Limits& InLimits = {}) -> bool;

    /** Absolute signed time; half-open frame intervals, ping-pong endpoints occur once per cycle. */
    CKPIXELSPRITE_API auto TryEvaluate_Frame(
        const FCk_PixelSprite_Timing& InTiming,
        FCk_Time InTime,
        FCk_PixelSprite_FrameSelection& OutSelection) -> bool;

    /** Quantizes presentation time; never changes source durations or playback speed. */
    CKPIXELSPRITE_API auto TrySample_Time(
        FCk_Time InTime,
        FCk_Time InInterval,
        FCk_Time& OutTime) -> bool;

    /** Ordinary-files loader; failure leaves OutManifest unchanged. */
    CKPIXELSPRITE_API auto TryLoad_Manifest(
        const FString& InManifestPath,
        FCk_PixelSprite_Manifest& OutManifest,
        FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool;

    CKPIXELSPRITE_API auto TryLoad_Xpm(const FString& InSourcePath,
        FCk_PixelSprite_Grid& OutGrid, FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool;

    /** Failure leaves OutPrepared unchanged. The result owns its timing independently of InAnimation. */
    CKPIXELSPRITE_API auto TryPrepare_Animation(
        const FCk_PixelSprite_Animation& InAnimation,
        TSharedPtr<const FCk_PixelSprite_PreparedAnimation>& OutPrepared) -> bool;

    /** Absolute signed milliseconds; failure leaves OutSelection unchanged. */
    CKPIXELSPRITE_API auto TryEvaluate_PreparedAnimationMilliseconds(
        const FCk_PixelSprite_PreparedAnimation& InPrepared,
        double InMilliseconds,
        FCk_PixelSprite_FrameSelection& OutSelection) -> bool;

    /** Approximate first forward visit start for an animation-frame ordinal, in milliseconds. */
    CKPIXELSPRITE_API auto TryGet_PreparedFrameStartMilliseconds(
        const FCk_PixelSprite_PreparedAnimation& InPrepared,
        int32 InAnimationFrameIndex,
        double& OutMilliseconds) -> bool;

    /** Authored milliseconds use shortest-roundtrip decimal sums and signed modulo. */
    CKPIXELSPRITE_API auto TryEvaluate_AnimationMilliseconds(
        const FCk_PixelSprite_Animation& InAnimation,
        double InMilliseconds,
        FCk_PixelSprite_FrameSelection& OutSelection) -> bool;

    /** Converts shortest-decimal seconds to milliseconds without multiplying a double. */
    CKPIXELSPRITE_API auto TryEvaluate_Animation(
        const FCk_PixelSprite_Animation& InAnimation,
        FCk_Time InTime,
        FCk_PixelSprite_FrameSelection& OutSelection) -> bool;
}
