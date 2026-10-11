#include "CkPixelSprite/CkPixelSprite_Asset.h"

#include "CkPixelSprite_Json.h"
#include "CkPixelSprite_File.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "UObject/AssetRegistryTagsContext.h"
#endif

namespace ck_pixel_sprite_asset
{
    auto
        Fail(FCk_PixelSprite_Diagnostic& OutDiagnostic, const FString& InCode,
        const FString& InMessage)
        -> bool
    {
        OutDiagnostic = {TEXT("PixelSpriteAsset"), InCode, InMessage};
        return false;
    }

    auto
        ValidName(const FString& InName)
        -> bool
    {
        if (InName.IsEmpty() || InName.Len() > 128)
        {
            return false;
        }
        for (const auto Character : InName)
        {
            if (Character == 0)
            {
                return false;
            }
        }
        return true;
    }

    auto
        ContainsNull(const FString& InText)
        -> bool
    {
        for (const auto Character : InText)
        {
            if (Character == 0)
            {
                return true;
            }
        }
        return false;
    }

    auto
        ValidSourcePath(const FString& InPath)
        -> bool
    {
        if (InPath.IsEmpty() || ContainsNull(InPath))
        {
            return false;
        }
        for (const auto Character : InPath)
        {
            if (Character < 0x20)
            {
                return false;
            }
        }
        return true;
    }

    auto
        ValidExtra(const FString& InJson, ck::pixel_sprite::json_private::EKind InKind)
        -> bool
    {
        auto Node = ck::pixel_sprite::json_private::FNode{};
        auto Diagnostic = FCk_PixelSprite_Diagnostic{};
        return ck::pixel_sprite::json_private::TryParse(InJson, Node, Diagnostic) && Node._Kind == InKind;
    }

    auto
        ValidExtras(const FString& InEvents, const FString& InMetadata)
        -> bool
    {
        return ValidExtra(InEvents, ck::pixel_sprite::json_private::EKind::Array) &&
            ValidExtra(InMetadata, ck::pixel_sprite::json_private::EKind::Object);
    }

    auto
        Validate(const FCk_PixelSprite_Manifest& InManifest,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
        const auto Width = InManifest._Width;
        const auto Height = InManifest._Height;
        if (Width < 1 || Width > 4096 || Height < 1 || Height > 4096 ||
            int64{Width} * Height > 1024 * 1024)
        {
            return Fail(OutDiagnostic, TEXT("canvas"), TEXT("Canvas is outside supported limits"));
        }
        auto TotalExtraCharacters = int64{InManifest._EventsJson.Len()} + InManifest._MetadataJson.Len();
        if (TotalExtraCharacters > 8 * 1024 * 1024)
        {
            return Fail(OutDiagnostic, TEXT("extras"), TEXT("JSON extras exceed source budget"));
        }
        if (NOT ValidExtras(InManifest._EventsJson, InManifest._MetadataJson))
        {
            return Fail(OutDiagnostic, TEXT("extras"), TEXT("Root events or metadata is invalid JSON"));
        }
        if (InManifest._Sources.IsEmpty() || InManifest._Sources.Num() > 4096)
        {
            return Fail(OutDiagnostic, TEXT("sources"), TEXT("Source count is outside supported limits"));
        }

        auto TotalBytes = int64{0};
        auto TotalSourcePixels = int64{0};
        auto SeenPaths = ck::pixel_sprite::TCaseSensitiveStringMap<bool>{};
        for (auto SourceIndex = int32{0}; SourceIndex < InManifest._Sources.Num(); ++SourceIndex)
        {
            const auto& Source = InManifest._Sources[SourceIndex];
            if (NOT ValidSourcePath(Source._Path) ||
                Source._Bytes <= 0 || Source._Bytes > 8 * 1024 * 1024)
            {
                return Fail(OutDiagnostic, TEXT("source"), TEXT("Invalid source path or byte count"));
            }
            if (SeenPaths.Contains(Source._Path))
            {
                return Fail(OutDiagnostic, TEXT("source"), TEXT("Source paths are duplicated"));
            }
            SeenPaths.Add(Source._Path, true);
            TotalBytes += Source._Bytes;
            if (TotalBytes > 64 * 1024 * 1024)
            {
                return Fail(OutDiagnostic, TEXT("source_limit"), TEXT("Source byte budget exceeded"));
            }
            TotalSourcePixels += int64{Width} * Height;
            if (TotalSourcePixels > 16 * 1024 * 1024)
            {
                return Fail(OutDiagnostic, TEXT("source_limit"), TEXT("Source pixel budget exceeded"));
            }
            const auto& Grid = Source._Grid;
            if (Grid._Width != Width || Grid._Height != Height ||
                Grid._Palette.IsEmpty() || Grid._Palette.Num() > 4096 ||
                Grid._Indices.Num() != Width * Height)
            {
                return Fail(OutDiagnostic, TEXT("grid"), TEXT("Source grid does not match canvas"));
            }
            if (Grid._Hotspot.IsSet())
            {
                const auto Point = Grid._Hotspot.GetValue();
                if (Point.X < 0 || Point.Y < 0 || Point.X >= Width || Point.Y >= Height)
                {
                    return Fail(OutDiagnostic, TEXT("hotspot"), TEXT("Hotspot is outside canvas"));
                }
            }
            const auto SymbolWidth = Grid._Palette[0]._Symbol.Len();
            if (SymbolWidth < 1 || SymbolWidth > 4)
            {
                return Fail(OutDiagnostic, TEXT("palette"), TEXT("Palette symbol width is invalid"));
            }
            auto SeenSymbols = ck::pixel_sprite::TCaseSensitiveStringMap<bool>{};
            for (auto PaletteIndex = int32{0}; PaletteIndex < Grid._Palette.Num(); ++PaletteIndex)
            {
                const auto& Symbol = Grid._Palette[PaletteIndex]._Symbol;
                if (Symbol.Len() != SymbolWidth)
                {
                    return Fail(OutDiagnostic, TEXT("palette"), TEXT("Palette symbols have different widths"));
                }
                for (const auto Character : Symbol)
                {
                    if (Character < 0x20 || Character > 0x7E)
                    {
                        return Fail(OutDiagnostic, TEXT("palette"), TEXT("Palette symbol is not printable ASCII"));
                    }
                }
                if (SeenSymbols.Contains(Symbol))
                {
                    return Fail(OutDiagnostic, TEXT("palette"), TEXT("Palette symbols are duplicated"));
                }
                SeenSymbols.Add(Symbol, true);
            }
            for (const auto Index : Grid._Indices)
            {
                if (Index >= Grid._Palette.Num())
                {
                    return Fail(OutDiagnostic, TEXT("index"), TEXT("Pixel index exceeds palette"));
                }
            }
        }

        if (InManifest._Animations.IsEmpty() || InManifest._Animations.Num() > 1024)
        {
            return Fail(OutDiagnostic, TEXT("animations"), TEXT("Animation count is outside supported limits"));
        }
        const auto IsStandaloneImage = InManifest._Sources.Num() == 1 &&
            InManifest._Animations.Num() == 1 &&
            InManifest._Animations[0]._Name.Equals(TEXT("default"), ESearchCase::CaseSensitive) &&
            InManifest._Animations[0]._Frames.Num() == 1;
        auto TotalFrames = int32{0};
        for (auto AnimationIndex = int32{0}; AnimationIndex < InManifest._Animations.Num(); ++AnimationIndex)
        {
            const auto& Animation = InManifest._Animations[AnimationIndex];
            if (NOT ValidName(Animation._Name) ||
                static_cast<uint8>(Animation._Playback) > static_cast<uint8>(ECk_PixelSprite_Playback::PingPong))
            {
                return Fail(OutDiagnostic, TEXT("animation"), TEXT("Invalid animation name or playback"));
            }
            for (auto Prior = int32{0}; Prior < AnimationIndex; ++Prior)
            {
                if (Animation._Name.Equals(InManifest._Animations[Prior]._Name, ESearchCase::CaseSensitive))
                {
                    return Fail(OutDiagnostic, TEXT("animation"), TEXT("Animation names are duplicated"));
                }
            }
            TotalExtraCharacters += int64{Animation._EventsJson.Len()} + Animation._MetadataJson.Len();
            if (TotalExtraCharacters > 8 * 1024 * 1024 ||
                NOT ValidExtras(Animation._EventsJson, Animation._MetadataJson) || Animation._Frames.IsEmpty())
            {
                return Fail(OutDiagnostic, TEXT("animation"), TEXT("Animation extras or frames are invalid"));
            }
            if (Animation._Frames.Num() > 4096 - TotalFrames)
            {
                return Fail(OutDiagnostic, TEXT("limits"), TEXT("Aggregate frame budget exceeded"));
            }
            TotalFrames += Animation._Frames.Num();
            if (int64{TotalFrames} * Width * Height > 16 * 1024 * 1024)
            {
                return Fail(OutDiagnostic, TEXT("limits"), TEXT("Aggregate frame budget exceeded"));
            }
            for (const auto& Frame : Animation._Frames)
            {
                auto PathDiagnostic = FString{};
                const auto ValidRelativeSource =
                    ck::pixel_sprite::file_private::Validate_SourcePath(Frame._Source, PathDiagnostic);
                const auto ValidStandaloneSource = IsStandaloneImage &&
                    NOT FPaths::IsRelative(Frame._Source) &&
                    ValidSourcePath(Frame._Source) &&
                    Frame._Source.Equals(InManifest._Sources[0]._Path, ESearchCase::CaseSensitive);
                if ((NOT ValidRelativeSource && NOT ValidStandaloneSource) ||
                    NOT InManifest._Sources.IsValidIndex(Frame._SourceIndex))
                {
                    return Fail(OutDiagnostic, TEXT("frame"), TEXT("Frame source is invalid"));
                }
                auto IndexedPath = InManifest._Sources[Frame._SourceIndex]._Path;
                IndexedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
                auto FramePath = Frame._Source;
                FramePath.ReplaceInline(TEXT("\\"), TEXT("/"));
                if (NOT IndexedPath.Equals(FramePath, ESearchCase::CaseSensitive) &&
                    NOT IndexedPath.EndsWith(FString{TEXT("/")} + FramePath, ESearchCase::CaseSensitive))
                {
                    return Fail(OutDiagnostic, TEXT("frame"), TEXT("Frame source index does not match path"));
                }
                if (NOT FMath::IsFinite(Frame._DurationMilliseconds) ||
                    Frame._DurationMilliseconds <= 0.0 || Frame._DurationMilliseconds > 86400000.0 ||
                    Frame.Get_Duration().Get_Seconds() <= 0.0)
                {
                    return Fail(OutDiagnostic, TEXT("duration"), TEXT("Frame duration is invalid"));
                }
                TotalExtraCharacters += int64{Frame._EventsJson.Len()} + Frame._MetadataJson.Len();
                if (TotalExtraCharacters > 8 * 1024 * 1024 ||
                    NOT ValidExtras(Frame._EventsJson, Frame._MetadataJson) || Frame._Anchors.Num() > 256)
                {
                    return Fail(OutDiagnostic, TEXT("frame"), TEXT("Frame extras or anchors are invalid"));
                }
                for (const auto& Anchor : Frame._Anchors)
                {
                    const auto& Point = Anchor.Value;
                    if (NOT ValidName(Anchor.Key) || Point.X < 0 || Point.Y < 0 ||
                        Point.X >= Width || Point.Y >= Height)
                    {
                        return Fail(OutDiagnostic, TEXT("anchor"), TEXT("Frame anchor is invalid"));
                    }
                }
            }
        }
        OutDiagnostic = {};
        return true;
    }

    auto
        ToStored(const FCk_PixelSprite_Manifest& InManifest)
        -> FCk_PixelSprite_AssetManifest
    {
        auto Stored = FCk_PixelSprite_AssetManifest{};
        Stored._Width = InManifest._Width;
        Stored._Height = InManifest._Height;
        Stored._EventsJson = InManifest._EventsJson;
        Stored._MetadataJson = InManifest._MetadataJson;
        for (const auto& Source : InManifest._Sources)
        {
            auto Item = FCk_PixelSprite_AssetSource{};
            Item._Path = Source._Path;
            Item._Bytes = Source._Bytes;
            Item._Grid._Width = Source._Grid._Width;
            Item._Grid._Height = Source._Grid._Height;
            Item._Grid._Indices = Source._Grid._Indices;
            Item._Grid._HasHotspot = Source._Grid._Hotspot.IsSet();
            if (Item._Grid._HasHotspot)
            {
                Item._Grid._Hotspot = Source._Grid._Hotspot.GetValue();
            }
            for (const auto& Entry : Source._Grid._Palette)
            {
                auto StoredEntry = FCk_PixelSprite_AssetPaletteEntry{};
                StoredEntry._Symbol = Entry._Symbol;
                StoredEntry._Color = Entry._Color;
                Item._Grid._Palette.Add(MoveTemp(StoredEntry));
            }
            Stored._Sources.Add(MoveTemp(Item));
        }
        for (const auto& Animation : InManifest._Animations)
        {
            auto Item = FCk_PixelSprite_AssetAnimation{};
            Item._Name = Animation._Name;
            Item._Playback = static_cast<uint8>(Animation._Playback);
            Item._EventsJson = Animation._EventsJson;
            Item._MetadataJson = Animation._MetadataJson;
            for (const auto& Frame : Animation._Frames)
            {
                auto StoredFrame = FCk_PixelSprite_AssetFrame{};
                StoredFrame._Source = Frame._Source;
                StoredFrame._SourceIndex = Frame._SourceIndex;
                StoredFrame._DurationMilliseconds = Frame._DurationMilliseconds;
                StoredFrame._EventsJson = Frame._EventsJson;
                StoredFrame._MetadataJson = Frame._MetadataJson;
                for (const auto& Anchor : Frame._Anchors)
                {
                    auto StoredAnchor = FCk_PixelSprite_AssetAnchor{};
                    StoredAnchor._Name = Anchor.Key;
                    StoredAnchor._Position = Anchor.Value;
                    StoredFrame._Anchors.Add(MoveTemp(StoredAnchor));
                }
                Item._Frames.Add(MoveTemp(StoredFrame));
            }
            Stored._Animations.Add(MoveTemp(Item));
        }
        return Stored;
    }

    auto
        StoredIsBounded(const FCk_PixelSprite_AssetManifest& InStored)
        -> bool
    {
        if (InStored._Width < 1 || InStored._Width > 4096 ||
            InStored._Height < 1 || InStored._Height > 4096 ||
            int64{InStored._Width} * InStored._Height > 1024 * 1024 ||
            InStored._Sources.IsEmpty() || InStored._Sources.Num() > 4096 ||
            InStored._Animations.IsEmpty() || InStored._Animations.Num() > 1024)
        {
            return false;
        }
        auto TotalSourcePixels = int64{0};
        auto TotalExtraCharacters = int64{InStored._EventsJson.Len()} + InStored._MetadataJson.Len();
        if (TotalExtraCharacters > 8 * 1024 * 1024)
        {
            return false;
        }
        for (const auto& Source : InStored._Sources)
        {
            TotalSourcePixels += int64{InStored._Width} * InStored._Height;
            if (TotalSourcePixels > 16 * 1024 * 1024 || Source._Path.Len() > 32767)
            {
                return false;
            }
            if (Source._Grid._Palette.IsEmpty() || Source._Grid._Palette.Num() > 4096 ||
                Source._Grid._Indices.Num() != InStored._Width * InStored._Height)
            {
                return false;
            }
            for (const auto& Entry : Source._Grid._Palette)
            {
                if (Entry._Symbol.Len() < 1 || Entry._Symbol.Len() > 4)
                {
                    return false;
                }
            }
        }
        auto TotalFrames = int32{0};
        for (const auto& Animation : InStored._Animations)
        {
            TotalExtraCharacters += int64{Animation._EventsJson.Len()} + Animation._MetadataJson.Len();
            if (TotalExtraCharacters > 8 * 1024 * 1024)
            {
                return false;
            }
            if (Animation._Frames.IsEmpty() || Animation._Frames.Num() > 4096 - TotalFrames)
            {
                return false;
            }
            TotalFrames += Animation._Frames.Num();
            if (int64{TotalFrames} * InStored._Width * InStored._Height > 16 * 1024 * 1024)
            {
                return false;
            }
            for (const auto& Frame : Animation._Frames)
            {
                TotalExtraCharacters += int64{Frame._EventsJson.Len()} + Frame._MetadataJson.Len();
                if (TotalExtraCharacters > 8 * 1024 * 1024 ||
                    Frame._Source.Len() > 32767 || Frame._Anchors.Num() > 256)
                {
                    return false;
                }
                auto SeenAnchors = ck::pixel_sprite::TCaseSensitiveStringMap<bool>{};
                for (const auto& Anchor : Frame._Anchors)
                {
                    if (SeenAnchors.Contains(Anchor._Name))
                    {
                        return false;
                    }
                    SeenAnchors.Add(Anchor._Name, true);
                }
            }
        }
        return true;
    }

    auto
        ToManifest(const FCk_PixelSprite_AssetManifest& InStored)
        -> FCk_PixelSprite_Manifest
    {
        auto Manifest = FCk_PixelSprite_Manifest{};
        Manifest._Width = InStored._Width;
        Manifest._Height = InStored._Height;
        Manifest._EventsJson = InStored._EventsJson;
        Manifest._MetadataJson = InStored._MetadataJson;
        for (const auto& Source : InStored._Sources)
        {
            auto Item = FCk_PixelSprite_Source{};
            Item._Path = Source._Path;
            Item._Bytes = Source._Bytes;
            Item._Grid._Width = Source._Grid._Width;
            Item._Grid._Height = Source._Grid._Height;
            Item._Grid._Indices = Source._Grid._Indices;
            if (Source._Grid._HasHotspot)
            {
                Item._Grid._Hotspot = Source._Grid._Hotspot;
            }
            for (const auto& Entry : Source._Grid._Palette)
            {
                Item._Grid._Palette.Add({Entry._Symbol, Entry._Color});
            }
            Manifest._Sources.Add(MoveTemp(Item));
        }
        for (const auto& Animation : InStored._Animations)
        {
            auto Item = FCk_PixelSprite_Animation{};
            Item._Name = Animation._Name;
            Item._Playback = static_cast<ECk_PixelSprite_Playback>(Animation._Playback);
            Item._EventsJson = Animation._EventsJson;
            Item._MetadataJson = Animation._MetadataJson;
            for (const auto& Frame : Animation._Frames)
            {
                auto NativeFrame = FCk_PixelSprite_Frame{};
                NativeFrame._Source = Frame._Source;
                NativeFrame._SourceIndex = Frame._SourceIndex;
                NativeFrame._DurationMilliseconds = Frame._DurationMilliseconds;
                NativeFrame._EventsJson = Frame._EventsJson;
                NativeFrame._MetadataJson = Frame._MetadataJson;
                for (const auto& Anchor : Frame._Anchors)
                {
                    NativeFrame._Anchors.Add(Anchor._Name, Anchor._Position);
                }
                Item._Frames.Add(MoveTemp(NativeFrame));
            }
            Manifest._Animations.Add(MoveTemp(Item));
        }
        return Manifest;
    }
}

UCk_PixelSpriteAsset_UE::
    UCk_PixelSpriteAsset_UE()
{
#if WITH_EDITORONLY_DATA
    _ImportData = CreateDefaultSubobject<UAssetImportData>(TEXT("ImportData"));
    _ImportData->SetFlags(RF_Transactional);
#endif
}

auto
    UCk_PixelSpriteAsset_UE::
    Get_Snapshot() const
    -> TSharedPtr<const FCk_PixelSprite_Manifest>
{
    if (NOT _Snapshot.IsValid())
    {
        if (_DataFormatVersion != 1 || NOT ck_pixel_sprite_asset::StoredIsBounded(_ManifestData))
        {
            return {};
        }
        auto Candidate = ck_pixel_sprite_asset::ToManifest(_ManifestData);
        auto Diagnostic = FCk_PixelSprite_Diagnostic{};
        if (NOT ck_pixel_sprite_asset::Validate(Candidate, Diagnostic))
        {
            return {};
        }
        _Snapshot = MakeShared<const FCk_PixelSprite_Manifest>(MoveTemp(Candidate));
    }
    return _Snapshot;
}

auto
    UCk_PixelSpriteAsset_UE::
    Request_ReplaceManifest(
        const FCk_PixelSprite_Manifest& InManifest,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    if (NOT ck_pixel_sprite_asset::Validate(InManifest, OutDiagnostic))
    {
        return false;
    }
    auto Stored = ck_pixel_sprite_asset::ToStored(InManifest);
    _ManifestData = MoveTemp(Stored);
    _DataFormatVersion = 1;
    _Snapshot.Reset();
    ++_Revision;
    OutDiagnostic = {};
    return true;
}

auto
    UCk_PixelSpriteAsset_UE::
    Get_Revision() const
    -> uint64
{
    return _Revision;
}

auto
    UCk_PixelSpriteAsset_UE::
    Get_SourceFilename() const
    -> const FString&
{
    return _SourceFilename;
}

auto
    UCk_PixelSpriteAsset_UE::
    Set_SourceFilename(const FString& InFilename)
    -> void
{
    _SourceFilename = InFilename;
}

auto
    UCk_PixelSpriteAsset_UE::
    Get_RenderSettings() const
    -> const FCk_PixelSprite_RenderSettings&
{
    return _RenderSettings;
}

auto
    UCk_PixelSpriteAsset_UE::
    Request_SetRenderSettings(const FCk_PixelSprite_RenderSettings& InSettings)
    -> void
{
    _RenderSettings = InSettings; ++_Revision;
}

#if WITH_EDITORONLY_DATA
auto
    UCk_PixelSpriteAsset_UE::
    Get_ImportData() const
    -> TObjectPtr<UAssetImportData>
{
    return _ImportData;
}

auto
    UCk_PixelSpriteAsset_UE::
    Set_ImportData(TObjectPtr<UAssetImportData> InImportData)
    -> void
{
    _ImportData = InImportData;
}
#endif

int32
    UCk_PixelSpriteAsset_UE::
    Get_Width() const
{
    return _ManifestData._Width;
}

int32
    UCk_PixelSpriteAsset_UE::
    Get_Height() const
{
    return _ManifestData._Height;
}

TArray<FString>
    UCk_PixelSpriteAsset_UE::
    Get_AnimationNames() const
{
    auto Names = TArray<FString>{};
    Names.Reserve(_ManifestData._Animations.Num());
    for (const auto& Animation : _ManifestData._Animations)
    {
        Names.Add(Animation._Name);
    }
    return Names;
}

int32
    UCk_PixelSpriteAsset_UE::
    Get_FrameCount(const FString& InAnimationName) const
{
    for (const auto& Animation : _ManifestData._Animations)
    {
        if (Animation._Name.Equals(InAnimationName, ESearchCase::CaseSensitive))
        {
            return Animation._Frames.Num();
        }
    }
    return 0;
}

auto
    UCk_PixelSpriteAsset_UE::
    PostLoad()
    -> void
{
    Super::PostLoad();
    _Snapshot.Reset();
    ++_Revision;
}

auto
    UCk_PixelSpriteAsset_UE::
    PostDuplicate(EDuplicateMode::Type InDuplicateMode)
    -> void
{
    Super::PostDuplicate(InDuplicateMode);
    _Snapshot.Reset();
    ++_Revision;
}

#if WITH_EDITOR
auto
    UCk_PixelSpriteAsset_UE::
    GetAssetRegistryTags(FAssetRegistryTagsContext InContext) const
    -> void
{
    Super::GetAssetRegistryTags(InContext);
#if WITH_EDITORONLY_DATA
    if (_ImportData)
    {
        InContext.AddTag(FAssetRegistryTag(UObject::SourceFileTagName(),
            _ImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
    }
#endif
}

auto
    UCk_PixelSpriteAsset_UE::
    PostEditUndo()
    -> void
{
    Super::PostEditUndo();
    _Snapshot.Reset();
    ++_Revision;
}

auto
    UCk_PixelSpriteAsset_UE::
    PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
    -> void
{
    Super::PostEditChangeProperty(InPropertyChangedEvent);
    _Snapshot.Reset();
    ++_Revision;
}
#endif
