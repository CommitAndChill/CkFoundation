#pragma once

#include "CoreMinimal.h"
#include "CkPixelSprite/CkPixelSprite_Data.h"
#include "CkPixelSprite/CkPixelSprite_Render.h"
#include "Engine/DataAsset.h"

#include "CkPixelSprite_Asset.generated.h"

class UAssetImportData;
class FAssetRegistryTagsContext;

USTRUCT()
struct CKPIXELSPRITE_API FCk_PixelSprite_AssetPaletteEntry
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _Symbol;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FColor _Color = FColor::Transparent;
};

USTRUCT()
struct CKPIXELSPRITE_API FCk_PixelSprite_AssetGrid
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    int32 _Width = 0;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    int32 _Height = 0;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    TArray<FCk_PixelSprite_AssetPaletteEntry> _Palette;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    TArray<uint16> _Indices;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    bool _HasHotspot = false;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FIntPoint _Hotspot = FIntPoint::ZeroValue;
};

USTRUCT()
struct CKPIXELSPRITE_API FCk_PixelSprite_AssetSource
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _Path;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    int64 _Bytes = 0;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FCk_PixelSprite_AssetGrid _Grid;
};

USTRUCT()
struct CKPIXELSPRITE_API FCk_PixelSprite_AssetAnchor
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _Name;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FIntPoint _Position = FIntPoint::ZeroValue;
};

USTRUCT()
struct CKPIXELSPRITE_API FCk_PixelSprite_AssetFrame
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _Source;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    int32 _SourceIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    double _DurationMilliseconds = 0.0;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    TArray<FCk_PixelSprite_AssetAnchor> _Anchors;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _EventsJson = TEXT("[]");

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _MetadataJson = TEXT("{}");
};

USTRUCT()
struct CKPIXELSPRITE_API FCk_PixelSprite_AssetAnimation
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _Name;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    uint8 _Playback = 0;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    TArray<FCk_PixelSprite_AssetFrame> _Frames;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _EventsJson = TEXT("[]");

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _MetadataJson = TEXT("{}");
};

USTRUCT()
struct CKPIXELSPRITE_API FCk_PixelSprite_AssetManifest
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    int32 _Width = 0;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    int32 _Height = 0;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    TArray<FCk_PixelSprite_AssetSource> _Sources;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    TArray<FCk_PixelSprite_AssetAnimation> _Animations;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _EventsJson = TEXT("[]");

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _MetadataJson = TEXT("{}");
};

/** Imported indexed sprite data. The reflected manifest is the persistent source of truth. */
UCLASS(BlueprintType)
class CKPIXELSPRITE_API UCk_PixelSpriteAsset_UE : public UDataAsset
{
    GENERATED_BODY()

public:
    UCk_PixelSpriteAsset_UE();

    /** Hold the returned snapshot while reading; replacement does not alter an older snapshot. */
    auto
    Get_Snapshot() const -> TSharedPtr<const FCk_PixelSprite_Manifest>;
    auto
    Request_ReplaceManifest(const FCk_PixelSprite_Manifest& InManifest,
        FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool;
    auto
    Get_Revision() const -> uint64;
    auto
    Get_SourceFilename() const -> const FString&;
    auto
    Set_SourceFilename(const FString& InFilename) -> void;
    auto
    Get_RenderSettings() const -> const FCk_PixelSprite_RenderSettings&;
    auto
    Request_SetRenderSettings(const FCk_PixelSprite_RenderSettings& InSettings) -> void;

#if WITH_EDITORONLY_DATA
    auto
    Get_ImportData() const -> TObjectPtr<UAssetImportData>;
    auto
    Set_ImportData(TObjectPtr<UAssetImportData> InImportData) -> void;
#endif

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    int32
    Get_Width() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    int32
    Get_Height() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    TArray<FString>
    Get_AnimationNames() const;

    UFUNCTION(BlueprintPure, Category = "Ck|Pixel Sprite")
    int32
    Get_FrameCount(const FString& InAnimationName) const;

    virtual auto
    PostLoad() -> void override;
    virtual auto
    PostDuplicate(EDuplicateMode::Type InDuplicateMode) -> void override;
#if WITH_EDITOR
    virtual auto
    GetAssetRegistryTags(FAssetRegistryTagsContext InContext) const -> void override;
    virtual auto
    PostEditUndo() -> void override;
    virtual auto
    PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) -> void override;
#endif

private:
    friend class FCkTest_PixelSprite_Asset;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    int32 _DataFormatVersion = 1;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FCk_PixelSprite_AssetManifest _ManifestData;

    UPROPERTY(VisibleAnywhere, Category = "Pixel Sprite")
    FString _SourceFilename;

    UPROPERTY(EditAnywhere, Category = "Pixel Sprite|Render")
    FCk_PixelSprite_RenderSettings _RenderSettings;

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Instanced, Category = "Pixel Sprite|Import")
    TObjectPtr<UAssetImportData> _ImportData;
#endif

    uint64 _Revision = 0;
    mutable TSharedPtr<const FCk_PixelSprite_Manifest> _Snapshot;
};
