#pragma once

#include "CoreMinimal.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

#include "CkPixelSprite_Player.generated.h"

class UCk_PixelSpriteResources_UE;
class UTexture2D;

UENUM(BlueprintType)
enum class ECk_PixelSprite_PlayerPlayback : uint8
{
    Loop,
    Once,
    PingPong
};

/** Explicitly advanced sprite player. Its authored millisecond clock is shared by every presentation widget. */
UCLASS(BlueprintType, Blueprintable)
class CKPIXELSPRITE_API UCk_PixelSpritePlayer_UE : public UObject
{
    GENERATED_BODY()

public:
    auto
        TrySelectAsset(
            TObjectPtr<UCk_PixelSpriteAsset_UE> InAsset,
            FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool;
    auto
        TrySelectAnimation(
            const FString& InName)
        -> bool;
    auto
        TryRefreshGeneration(
            FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool;

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_ApplyRequestedAsset();

    /** Loads a cooked asset by object path and selects it atomically. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_LoadAsset(
        const FSoftObjectPath& InAssetPath);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_SelectAnimation(
        const FString& InName);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_AdvanceMilliseconds(
        double InDeltaMilliseconds);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_SeekMilliseconds(
        double InMilliseconds);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_StepFrames(
        int32 InCount);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    void
    Request_Play();

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    void
    Request_Pause();

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    void
    Request_Stop();

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_Restart();

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_SetDirection(
        int32 InDirection);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_SetSpeed(
        double InSpeed);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_SetSamplingIntervalMilliseconds(
        double InIntervalMilliseconds);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_SetSourceFrameDecimation(
        int32 InEveryNthFrame);

    UFUNCTION(BlueprintCallable, Category = "Ck|Pixel Sprite")
    bool
    Request_SetPlayback(
        ECk_PixelSprite_PlayerPlayback InPlayback);

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    int32
    Get_CurrentFrame() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    int32
    Get_LogicalFrame() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    double
    Get_NormalizedTime() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    double
    Get_ElapsedMilliseconds() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    bool
    Get_IsPlaying() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    bool
    Get_IsComplete() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    bool
    TryResolveAnchor(
        const FString& InName,
        FIntPoint& OutPosition) const;

    auto
        Get_CurrentTexture() const
        -> TObjectPtr<UTexture2D>;
    auto
        Get_Resources() const
        -> TObjectPtr<UCk_PixelSpriteResources_UE>;
    auto
        Get_Revision() const
        -> uint64;
    auto
        Get_PreparedAnimation() const
        -> TSharedPtr<const FCk_PixelSprite_PreparedAnimation>;

private:
    enum class ETimeOperation : uint8
    {
        Refresh,
        Seek,
        Advance,
        Restart
    };

    auto
        TryApplyTimeOperation(
            ETimeOperation InOperation,
            double InMilliseconds,
            FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool;
    auto
        TryCommitTime(
            double InMilliseconds)
        -> bool;
    auto
        TryFindAnimation(
            const FCk_PixelSprite_Manifest& InManifest,
            const FString& InName,
            FCk_PixelSprite_Animation& OutAnimation) const
        -> bool;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|Pixel Sprite", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCk_PixelSpriteAsset_UE> _RequestedAsset;

    UPROPERTY(Transient)
    TObjectPtr<UCk_PixelSpriteAsset_UE> _Asset;

    UPROPERTY(Transient)
    TObjectPtr<UCk_PixelSpriteResources_UE> _Resources;

    TSharedPtr<const FCk_PixelSprite_Manifest> _Snapshot;
    TSharedPtr<const FCk_PixelSprite_PreparedAnimation> _Prepared;
    FCk_PixelSprite_Animation _Animation;
    TOptional<ECk_PixelSprite_Playback> _PlaybackOverride;
    FString _AnimationName;
    FCk_PixelSprite_FrameSelection _LogicalSelection;
    int32 _DisplayFrame = INDEX_NONE;
    int32 _ManualFrame = INDEX_NONE;
    double _ElapsedMilliseconds = 0.0;
    double _Speed = 1.0;
    double _SamplingIntervalMilliseconds = 0.0;
    int32 _SourceFrameDecimation = 1;
    int32 _Direction = 1;
    uint64 _Revision = 0;
    bool _Playing = false;
    bool _ReverseComplete = false;
};
