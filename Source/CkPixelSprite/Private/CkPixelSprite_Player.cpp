#include "CkPixelSprite/CkPixelSprite_Player.h"

#include "CkPixelSprite/CkPixelSprite_Resources.h"
#include "CkCore/Macros/CkMacros.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"

#include <cmath>

namespace ck_pixel_sprite_player
{
    auto
        IsPlaybackValid(
            ECk_PixelSprite_PlayerPlayback InPlayback)
        -> bool
    {
        return InPlayback == ECk_PixelSprite_PlayerPlayback::Loop ||
            InPlayback == ECk_PixelSprite_PlayerPlayback::Once ||
            InPlayback == ECk_PixelSprite_PlayerPlayback::PingPong;
    }

    auto
        ToPlayback(
            ECk_PixelSprite_PlayerPlayback InPlayback)
        -> ECk_PixelSprite_Playback
    {
        return static_cast<ECk_PixelSprite_Playback>(InPlayback);
    }

    auto
        Fail(
            FCk_PixelSprite_Diagnostic& OutDiagnostic,
            const FString& InCode,
            const FString& InMessage)
        -> bool
    {
        OutDiagnostic = {TEXT("PixelSpritePlayer"), InCode, InMessage};
        return false;
    }

    auto
        TryEvaluatePresentation(
            const FCk_PixelSprite_Animation& InAnimation,
            const FCk_PixelSprite_PreparedAnimation& InPrepared,
            double InMilliseconds,
            double InSamplingIntervalMilliseconds,
            int32 InSourceFrameDecimation,
            FCk_PixelSprite_FrameSelection& OutLogical,
            int32& OutDisplayFrame)
        -> bool
    {
        if (NOT FMath::IsFinite(InMilliseconds) || InAnimation._Frames.IsEmpty() ||
            InSourceFrameDecimation < 1)
        {
            return false;
        }
        auto Logical = FCk_PixelSprite_FrameSelection{};
        if (NOT ck::pixel_sprite::TryEvaluate_PreparedAnimationMilliseconds(
            InPrepared, InMilliseconds, Logical))
        {
            return false;
        }
        auto Display = Logical;
        if (InSamplingIntervalMilliseconds > 0.0)
        {
            const auto Quotient = InMilliseconds / InSamplingIntervalMilliseconds;
            if (NOT FMath::IsFinite(Quotient))
            {
                return false;
            }
            const auto Sampled = std::floor(Quotient) * InSamplingIntervalMilliseconds;
            if (NOT FMath::IsFinite(Sampled) ||
                NOT ck::pixel_sprite::TryEvaluate_PreparedAnimationMilliseconds(
                    InPrepared, Sampled, Display))
            {
                return false;
            }
        }
        const auto DisplayIndex = (Display._Index / InSourceFrameDecimation) * InSourceFrameDecimation;
        if (NOT InAnimation._Frames.IsValidIndex(DisplayIndex))
        {
            return false;
        }
        OutLogical = Logical;
        OutDisplayFrame = DisplayIndex;
        return true;
    }
}

auto
    UCk_PixelSpritePlayer_UE::
    TryFindAnimation(
        const FCk_PixelSprite_Manifest& InManifest,
        const FString& InName,
        FCk_PixelSprite_Animation& OutAnimation) const
    -> bool
{
    for (const auto& Animation : InManifest._Animations)
    {
        if (Animation._Name.Equals(InName, ESearchCase::CaseSensitive))
        {
            OutAnimation = Animation;
            return true;
        }
    }
    return false;
}

auto
    UCk_PixelSpritePlayer_UE::
    TrySelectAsset(
        TObjectPtr<UCk_PixelSpriteAsset_UE> InAsset,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    if (NOT IsValid(InAsset.Get()))
    {
        return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("asset"), TEXT("Pixel sprite asset is invalid"));
    }
    const auto Snapshot = InAsset->Get_Snapshot();
    if (NOT Snapshot.IsValid() || Snapshot->_Animations.IsEmpty())
    {
        return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("asset"), TEXT("Asset has no runnable animation"));
    }
    auto Animation = Snapshot->_Animations[0];
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT ck::pixel_sprite::TryPrepare_Animation(Animation, Prepared))
    {
        return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("animation"), TEXT("First animation is invalid"));
    }
    auto Selection = FCk_PixelSprite_FrameSelection{};
    auto DisplayFrame = int32{INDEX_NONE};
    if (NOT ck_pixel_sprite_player::TryEvaluatePresentation(Animation, *Prepared, 0.0,
        _SamplingIntervalMilliseconds, _SourceFrameDecimation, Selection, DisplayFrame))
    {
        return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("animation"), TEXT("First animation is invalid"));
    }
    auto Resources = TStrongObjectPtr<UCk_PixelSpriteResources_UE>{NewObject<UCk_PixelSpriteResources_UE>(this)};
    if (NOT Resources->TryBuild(InAsset, InAsset->Get_RenderSettings(), OutDiagnostic))
    {
        return false;
    }
    _Asset = InAsset;
    _RequestedAsset = InAsset;
    _Resources = Resources.Get();
    _Snapshot = Snapshot;
    _Animation = MoveTemp(Animation);
    _Prepared = MoveTemp(Prepared);
    _PlaybackOverride.Reset();
    _AnimationName = _Animation._Name;
    _LogicalSelection = Selection;
    _DisplayFrame = DisplayFrame;
    _ManualFrame = INDEX_NONE;
    _ElapsedMilliseconds = 0.0;
    _Revision = InAsset->Get_Revision();
    _Playing = false;
    _ReverseComplete = false;
    OutDiagnostic = {};
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_ApplyRequestedAsset()
    -> bool
{
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    return TrySelectAsset(_RequestedAsset, Diagnostic);
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_LoadAsset(
        const FSoftObjectPath& InAssetPath)
    -> bool
{
    if (NOT InAssetPath.IsValid())
    {
        return false;
    }
    const auto Loaded = TObjectPtr<UObject>{InAssetPath.TryLoad()};
    const auto Asset = TObjectPtr<UCk_PixelSpriteAsset_UE>{Cast<UCk_PixelSpriteAsset_UE>(Loaded.Get())};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    return TrySelectAsset(Asset, Diagnostic);
}

auto
    UCk_PixelSpritePlayer_UE::
    TryRefreshGeneration(
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    return TryApplyTimeOperation(ETimeOperation::Refresh, _ElapsedMilliseconds, OutDiagnostic);
}

auto
    UCk_PixelSpritePlayer_UE::
    TryApplyTimeOperation(
        ETimeOperation InOperation,
        double InMilliseconds,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    if (NOT IsValid(_Asset.Get()) || NOT _Prepared.IsValid())
    {
        return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("asset"), TEXT("No selected asset"));
    }
    const auto Revision = _Asset->Get_Revision();
    const auto Changed = Revision != _Revision;
    if (NOT Changed && InOperation == ETimeOperation::Refresh)
    {
        OutDiagnostic = {};
        return true;
    }

    auto Snapshot = _Snapshot;
    auto CandidateAnimation = FCk_PixelSprite_Animation{};
    auto Prepared = _Prepared;
    if (Changed)
    {
        Snapshot = _Asset->Get_Snapshot();
        if (NOT Snapshot.IsValid())
        {
            return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("asset"), TEXT("Replacement snapshot is invalid"));
        }
        if (NOT TryFindAnimation(*Snapshot, _AnimationName, CandidateAnimation))
        {
            return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("animation"), TEXT("Selected animation was removed"));
        }
        if (_PlaybackOverride.IsSet())
        {
            CandidateAnimation._Playback = _PlaybackOverride.GetValue();
        }
        if (NOT ck::pixel_sprite::TryPrepare_Animation(CandidateAnimation, Prepared))
        {
            return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("animation"), TEXT("Replacement animation is invalid"));
        }
    }
    const auto& Animation = Changed ? CandidateAnimation : _Animation;

    auto Target = InMilliseconds;
    if (InOperation == ETimeOperation::Restart)
    {
        Target = 0.0;
        if (_Direction < 0 && Animation._Playback == ECk_PixelSprite_Playback::Once)
        {
            auto StartSelection = FCk_PixelSprite_FrameSelection{};
            if (NOT ck::pixel_sprite::TryEvaluate_PreparedAnimationMilliseconds(*Prepared, 0.0,
                StartSelection))
            {
                return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("animation"), TEXT("Restart timeline is invalid"));
            }
            Target = StartSelection._CycleDuration.Get_Seconds() * 1000.0;
        }
    }
    const auto ReverseEnd = InOperation == ETimeOperation::Advance &&
        Animation._Playback == ECk_PixelSprite_Playback::Once && _Direction < 0 && Target <= 0.0;
    if (ReverseEnd)
    {
        Target = 0.0;
    }
    auto Logical = FCk_PixelSprite_FrameSelection{};
    auto DisplayFrame = int32{INDEX_NONE};
    if (NOT ck_pixel_sprite_player::TryEvaluatePresentation(Animation, *Prepared, Target,
        _SamplingIntervalMilliseconds, _SourceFrameDecimation, Logical, DisplayFrame))
    {
        return ck_pixel_sprite_player::Fail(OutDiagnostic, TEXT("animation"), TEXT("Requested presentation time is invalid"));
    }

    auto Resources = TStrongObjectPtr<UCk_PixelSpriteResources_UE>{};
    if (Changed)
    {
        Resources.Reset(NewObject<UCk_PixelSpriteResources_UE>(this));
        if (NOT Resources->TryBuild(_Asset, _Asset->Get_RenderSettings(), OutDiagnostic))
        {
            return false;
        }
    }

    if (Changed)
    {
        _Snapshot = MoveTemp(Snapshot);
        _Animation = MoveTemp(CandidateAnimation);
        _Prepared = MoveTemp(Prepared);
        _Resources = Resources.Get();
        _Revision = Revision;
    }
    _ElapsedMilliseconds = Target;
    _LogicalSelection = Logical;
    _DisplayFrame = DisplayFrame;
    if (Changed || InOperation != ETimeOperation::Refresh)
    {
        _ManualFrame = INDEX_NONE;
    }
    if (InOperation == ETimeOperation::Seek)
    {
        _ReverseComplete = false;
    }
    else if (InOperation == ETimeOperation::Advance)
    {
        _ReverseComplete = ReverseEnd;
        if (ReverseEnd || (_Direction > 0 && Logical._Complete))
        {
            _Playing = false;
        }
    }
    else if (InOperation == ETimeOperation::Restart)
    {
        _ReverseComplete = false;
        _Playing = true;
    }
    OutDiagnostic = {};
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    TrySelectAnimation(
        const FString& InName)
    -> bool
{
    if (NOT _Snapshot.IsValid())
    {
        return false;
    }
    auto Animation = FCk_PixelSprite_Animation{};
    if (NOT TryFindAnimation(*_Snapshot, InName, Animation))
    {
        return false;
    }
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT ck::pixel_sprite::TryPrepare_Animation(Animation, Prepared))
    {
        return false;
    }
    auto Selection = FCk_PixelSprite_FrameSelection{};
    auto DisplayFrame = int32{INDEX_NONE};
    if (NOT ck_pixel_sprite_player::TryEvaluatePresentation(Animation, *Prepared, 0.0,
        _SamplingIntervalMilliseconds, _SourceFrameDecimation, Selection, DisplayFrame))
    {
        return false;
    }
    _Animation = MoveTemp(Animation);
    _Prepared = MoveTemp(Prepared);
    _PlaybackOverride.Reset();
    _AnimationName = InName;
    _LogicalSelection = Selection;
    _DisplayFrame = DisplayFrame;
    _ManualFrame = INDEX_NONE;
    _ElapsedMilliseconds = 0.0;
    _Playing = false;
    _ReverseComplete = false;
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_SelectAnimation(
        const FString& InName)
    -> bool
{
    if (NOT IsValid(_Asset.Get()))
    {
        return false;
    }
    if (_Revision == _Asset->Get_Revision())
    {
        return TrySelectAnimation(InName);
    }
    const auto Snapshot = _Asset->Get_Snapshot();
    if (NOT Snapshot.IsValid())
    {
        return false;
    }
    auto Animation = FCk_PixelSprite_Animation{};
    if (NOT TryFindAnimation(*Snapshot, InName, Animation))
    {
        return false;
    }
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT ck::pixel_sprite::TryPrepare_Animation(Animation, Prepared))
    {
        return false;
    }
    auto Selection = FCk_PixelSprite_FrameSelection{};
    auto DisplayFrame = int32{INDEX_NONE};
    if (NOT ck_pixel_sprite_player::TryEvaluatePresentation(Animation, *Prepared, 0.0,
        _SamplingIntervalMilliseconds, _SourceFrameDecimation, Selection, DisplayFrame))
    {
        return false;
    }
    auto Resources = TStrongObjectPtr<UCk_PixelSpriteResources_UE>{NewObject<UCk_PixelSpriteResources_UE>(this)};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT Resources->TryBuild(_Asset, _Asset->Get_RenderSettings(), Diagnostic))
    {
        return false;
    }
    _Snapshot = Snapshot;
    _Resources = Resources.Get();
    _Revision = _Asset->Get_Revision();
    _Animation = MoveTemp(Animation);
    _Prepared = MoveTemp(Prepared);
    _PlaybackOverride.Reset();
    _AnimationName = InName;
    _LogicalSelection = Selection;
    _DisplayFrame = DisplayFrame;
    _ManualFrame = INDEX_NONE;
    _ElapsedMilliseconds = 0.0;
    _Playing = false;
    _ReverseComplete = false;
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    TryCommitTime(
        double InMilliseconds)
    -> bool
{
    if (NOT _Prepared.IsValid())
    {
        return false;
    }
    auto Logical = FCk_PixelSprite_FrameSelection{};
    auto DisplayFrame = int32{INDEX_NONE};
    if (NOT ck_pixel_sprite_player::TryEvaluatePresentation(_Animation, *_Prepared, InMilliseconds,
        _SamplingIntervalMilliseconds, _SourceFrameDecimation, Logical, DisplayFrame))
    {
        return false;
    }
    _ElapsedMilliseconds = InMilliseconds;
    _LogicalSelection = Logical;
    _DisplayFrame = DisplayFrame;
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_AdvanceMilliseconds(
        double InDeltaMilliseconds)
    -> bool
{
    if (NOT FMath::IsFinite(InDeltaMilliseconds) || InDeltaMilliseconds < 0.0)
    {
        return false;
    }
    const auto Motion = InDeltaMilliseconds * _Speed * static_cast<double>(_Direction);
    if (NOT FMath::IsFinite(Motion))
    {
        return false;
    }
    const auto Next = _ElapsedMilliseconds + Motion;
    if (NOT FMath::IsFinite(Next))
    {
        return false;
    }
    if (NOT _Playing || Next == _ElapsedMilliseconds)
    {
        auto Diagnostic = FCk_PixelSprite_Diagnostic{};
        return TryRefreshGeneration(Diagnostic);
    }
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    return TryApplyTimeOperation(ETimeOperation::Advance, Next, Diagnostic);
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_SeekMilliseconds(
        double InMilliseconds)
    -> bool
{
    if (NOT FMath::IsFinite(InMilliseconds))
    {
        return false;
    }
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    return TryApplyTimeOperation(ETimeOperation::Seek, InMilliseconds, Diagnostic);
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_StepFrames(
        int32 InCount)
    -> bool
{
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT TryRefreshGeneration(Diagnostic) || NOT _Prepared.IsValid() ||
        _Animation._Frames.IsEmpty())
    {
        return false;
    }
    const auto Base = _ManualFrame != INDEX_NONE ? _ManualFrame : _LogicalSelection._Index;
    const auto Target = FMath::Clamp(static_cast<int64>(Base) + InCount,
        int64{0}, static_cast<int64>(_Animation._Frames.Num() - 1));
    auto Time = 0.0;
    if (NOT ck::pixel_sprite::TryGet_PreparedFrameStartMilliseconds(
        *_Prepared, static_cast<int32>(Target), Time))
    {
        return false;
    }
    auto Logical = FCk_PixelSprite_FrameSelection{};
    if (NOT ck::pixel_sprite::TryEvaluate_PreparedAnimationMilliseconds(
        *_Prepared, Time, Logical))
    {
        return false;
    }
    // An authored frame can be narrower than one double ULP at its absolute
    // time. Keep the exact inspected ordinal separate from the clock's frame.
    _ElapsedMilliseconds = Time;
    _LogicalSelection = Logical;
    _DisplayFrame = Logical._Index;
    _ManualFrame = static_cast<int32>(Target);
    _ReverseComplete = false;
    _Playing = false;
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_Play()
    -> void
{
    if (_LogicalSelection._Index == INDEX_NONE)
    {
        return;
    }
    if (_Animation._Playback == ECk_PixelSprite_Playback::Once &&
        ((_Direction > 0 && _LogicalSelection._Complete) ||
         (_Direction < 0 && _ElapsedMilliseconds <= 0.0)))
    {
        return;
    }
    _Playing = true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_Pause()
    -> void
{
    _Playing = false;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_Stop()
    -> void
{
    _Playing = false;
    _ReverseComplete = false;
    if (TryCommitTime(0.0))
    {
        _ManualFrame = INDEX_NONE;
    }
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_Restart()
    -> bool
{
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    return TryApplyTimeOperation(ETimeOperation::Restart, 0.0, Diagnostic);
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_SetDirection(
        int32 InDirection)
    -> bool
{
    if (InDirection != -1 && InDirection != 1)
    {
        return false;
    }
    _Direction = InDirection;
    _ReverseComplete = false;
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_SetSpeed(
        double InSpeed)
    -> bool
{
    if (NOT FMath::IsFinite(InSpeed) || InSpeed <= 0.0)
    {
        return false;
    }
    _Speed = InSpeed;
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_SetSamplingIntervalMilliseconds(
        double InIntervalMilliseconds)
    -> bool
{
    if (NOT FMath::IsFinite(InIntervalMilliseconds) || InIntervalMilliseconds < 0.0)
    {
        return false;
    }
    const auto Previous = _SamplingIntervalMilliseconds;
    _SamplingIntervalMilliseconds = InIntervalMilliseconds;
    if (_LogicalSelection._Index != INDEX_NONE && NOT TryCommitTime(_ElapsedMilliseconds))
    {
        _SamplingIntervalMilliseconds = Previous;
        return false;
    }
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_SetSourceFrameDecimation(
        int32 InEveryNthFrame)
    -> bool
{
    if (InEveryNthFrame < 1 || InEveryNthFrame > 4096)
    {
        return false;
    }
    const auto Previous = _SourceFrameDecimation;
    _SourceFrameDecimation = InEveryNthFrame;
    if (_LogicalSelection._Index != INDEX_NONE && NOT TryCommitTime(_ElapsedMilliseconds))
    {
        _SourceFrameDecimation = Previous;
        return false;
    }
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Request_SetPlayback(
        ECk_PixelSprite_PlayerPlayback InPlayback)
    -> bool
{
    if (NOT ck_pixel_sprite_player::IsPlaybackValid(InPlayback))
    {
        return false;
    }
    if (_LogicalSelection._Index == INDEX_NONE)
    {
        return false;
    }
    auto Animation = _Animation;
    Animation._Playback = ck_pixel_sprite_player::ToPlayback(InPlayback);
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT ck::pixel_sprite::TryPrepare_Animation(Animation, Prepared))
    {
        return false;
    }
    auto Logical = FCk_PixelSprite_FrameSelection{};
    auto DisplayFrame = int32{INDEX_NONE};
    if (NOT ck_pixel_sprite_player::TryEvaluatePresentation(Animation, *Prepared, _ElapsedMilliseconds,
        _SamplingIntervalMilliseconds, _SourceFrameDecimation, Logical, DisplayFrame))
    {
        return false;
    }
    _Animation = MoveTemp(Animation);
    _Prepared = MoveTemp(Prepared);
    _LogicalSelection = Logical;
    _DisplayFrame = DisplayFrame;
    _PlaybackOverride = _Animation._Playback;
    _ReverseComplete = false;
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_CurrentFrame() const
    -> int32
{
    return _ManualFrame != INDEX_NONE ? _ManualFrame : _DisplayFrame;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_LogicalFrame() const
    -> int32
{
    return _LogicalSelection._Index;
}

auto
    UCk_PixelSpritePlayer_UE::
    TryGet_CurrentPixels(
        TArray<FColor>& OutPixels,
        FIntPoint& OutCanvas) const
    -> bool
{
    const auto Frame = Get_CurrentFrame();
    if (NOT _Snapshot.IsValid() || NOT _Animation._Frames.IsValidIndex(Frame))
    {
        return false;
    }
    const auto Source = _Animation._Frames[Frame]._SourceIndex;
    if (NOT _Snapshot->_Sources.IsValidIndex(Source))
    {
        return false;
    }
    const auto& Grid = _Snapshot->_Sources[Source]._Grid;
    const auto Count = int64{Grid._Width} * Grid._Height;
    if (Grid._Width <= 0 || Grid._Height <= 0 || Count > 1048576
        || Count != Grid._Indices.Num())
    {
        return false;
    }
    auto Pixels = TArray<FColor>{};
    Pixels.Reserve(Grid._Indices.Num());
    for (const auto Index : Grid._Indices)
    {
        if (NOT Grid._Palette.IsValidIndex(Index))
        {
            return false;
        }
        Pixels.Add(Grid._Palette[Index]._Color);
    }
    OutPixels = MoveTemp(Pixels);
    OutCanvas = FIntPoint{Grid._Width, Grid._Height};
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_SourceAsset() const
    -> TObjectPtr<UCk_PixelSpriteAsset_UE>
{
    return _Asset;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_NormalizedTime() const
    -> double
{
    const auto Cycle = _LogicalSelection._CycleDuration.Get_Seconds() * 1000.0;
    if (NOT FMath::IsFinite(Cycle) || Cycle <= 0.0)
    {
        return 0.0;
    }
    if (_Animation._Playback == ECk_PixelSprite_Playback::Once)
    {
        return FMath::Clamp(_ElapsedMilliseconds / Cycle, 0.0, 1.0);
    }
    auto Position = std::fmod(_ElapsedMilliseconds, Cycle);
    if (Position < 0.0)
    {
        Position += Cycle;
    }
    return Position / Cycle;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_ElapsedMilliseconds() const
    -> double
{
    return _ElapsedMilliseconds;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_IsPlaying() const
    -> bool
{
    return _Playing;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_IsComplete() const
    -> bool
{
    if (_Animation._Playback != ECk_PixelSprite_Playback::Once)
    {
        return false;
    }
    return _Direction < 0 ? _ReverseComplete : _LogicalSelection._Complete;
}

auto
    UCk_PixelSpritePlayer_UE::
    TryResolveAnchor(
        const FString& InName,
        FIntPoint& OutPosition) const
    -> bool
{
    const auto DisplayFrame = Get_CurrentFrame();
    if (NOT _Animation._Frames.IsValidIndex(DisplayFrame))
    {
        return false;
    }
    const auto& Anchors = _Animation._Frames[DisplayFrame]._Anchors;
    if (NOT Anchors.Contains(InName))
    {
        return false;
    }
    OutPosition = Anchors.FindChecked(InName);
    return true;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_CurrentTexture() const
    -> TObjectPtr<UTexture2D>
{
    const auto DisplayFrame = Get_CurrentFrame();
    if (NOT IsValid(_Resources.Get()) || NOT _Animation._Frames.IsValidIndex(DisplayFrame))
    {
        return {};
    }
    return _Resources->Get_Texture(_Animation._Frames[DisplayFrame]._SourceIndex);
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_Resources() const
    -> TObjectPtr<UCk_PixelSpriteResources_UE>
{
    return _Resources;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_Revision() const
    -> uint64
{
    return _Revision;
}

auto
    UCk_PixelSpritePlayer_UE::
    Get_PreparedAnimation() const
    -> TSharedPtr<const FCk_PixelSprite_PreparedAnimation>
{
    return _Prepared;
}
