#include "CkPixelSpriteEditor/CkPixelSpriteEditor_Export.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkPixelSprite/CkPixelSprite_Render.h"
#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <sys/stat.h>
#endif

namespace ck_pixel_sprite_editor_export
{
    constexpr auto MaxSheetBytes = int64{64} * 1024 * 1024;
    constexpr auto MaxJsonCharacters = int32{16} * 1024 * 1024;

    auto
        Fail(FCk_PixelSprite_Diagnostic& OutDiagnostic, const FString& InCode,
        const FString& InMessage)
        -> bool
    {
        OutDiagnostic = {TEXT("PixelSpriteExport"), InCode, InMessage};
        return false;
    }

    auto
        OrdinaryDirectory(IPlatformFile& InFile, const FString& InPath)
        -> bool
    {
        const auto Data = InFile.GetStatData(*InPath);
        if (NOT Data.bIsValid || NOT Data.bIsDirectory ||
            NOT InFile.DirectoryExists(*InPath) ||
            InFile.IsSymlink(*InPath) != ESymlinkResult::NonSymlink)
        {
            return false;
        }
#if PLATFORM_WINDOWS
        const auto Attributes = ::GetFileAttributesW(*InPath);
        return Attributes != INVALID_FILE_ATTRIBUTES &&
            (Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (Attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
#elif PLATFORM_LINUX || PLATFORM_MAC
        struct stat NativeStat;
        return ::lstat(TCHAR_TO_UTF8(*InPath), &NativeStat) == 0 && S_ISDIR(NativeStat.st_mode);
#else
        return false;
#endif
    }

    auto
        ValidComponent(const FString& InComponent)
        -> bool
    {
        if (InComponent.IsEmpty() || InComponent == TEXT(".") || InComponent == TEXT("..") ||
            InComponent.EndsWith(TEXT(" ")) || InComponent.EndsWith(TEXT(".")))
        {
            return false;
        }
#if PLATFORM_WINDOWS
        auto Stem = InComponent.Left(InComponent.Find(TEXT(".")) == INDEX_NONE
            ? InComponent.Len() : InComponent.Find(TEXT("."))).ToUpper();
        while (Stem.EndsWith(TEXT(" ")))
        {
            Stem = Stem.Left(Stem.Len() - 1);
        }
        if (Stem == TEXT("CON") || Stem == TEXT("PRN") || Stem == TEXT("AUX") ||
            Stem == TEXT("NUL"))
        {
            return false;
        }
        if (Stem.Len() == 4 && (Stem.StartsWith(TEXT("COM")) || Stem.StartsWith(TEXT("LPT"))) &&
            Stem[3] >= TEXT('1') && Stem[3] <= TEXT('9'))
        {
            return false;
        }
#endif
        for (const auto Character : InComponent)
        {
            if (Character < 0x20 || (Character >= 0x7F && Character <= 0x9F) ||
                Character == TEXT('<') || Character == TEXT('>') || Character == TEXT('"') ||
                Character == TEXT('|') || Character == TEXT('?') || Character == TEXT('*') ||
                Character == TEXT(':'))
            {
                return false;
            }
        }
        return true;
    }

    auto
        ValidateNewDestination(const FString& InFilename,
        const FString& InExtension, FString& OutNormalized,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
#if !(PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC)
        return Fail(OutDiagnostic, TEXT("platform"), TEXT("Native destination inspection is unavailable"));
#else
        auto Path = InFilename;
        Path.ReplaceCharInline(TEXT('\\'), TEXT('/'));
        if (Path.Len() > 32767 ||
            NOT Path.EndsWith(InExtension, ESearchCase::IgnoreCase))
        {
            return Fail(OutDiagnostic, TEXT("destination"), TEXT("Destination extension or length is invalid"));
        }
#if PLATFORM_WINDOWS
        const auto DriveLetter = Path.Len() >= 4 &&
            ((Path[0] >= TEXT('A') && Path[0] <= TEXT('Z')) ||
             (Path[0] >= TEXT('a') && Path[0] <= TEXT('z')));
        if (NOT DriveLetter || Path[1] != TEXT(':') || Path[2] != TEXT('/') || Path[3] == TEXT('/'))
        {
            return Fail(OutDiagnostic, TEXT("destination"), TEXT("Destination requires an absolute local drive path"));
        }
        auto Prefix = Path.Left(3);
        constexpr auto RootLength = int32{3};
#else
        if (Path.Len() < 2 || Path[0] != TEXT('/') || Path[1] == TEXT('/'))
        {
            return Fail(OutDiagnostic, TEXT("destination"), TEXT("Destination requires an absolute local path"));
        }
        auto Prefix = FString{TEXT("/")};
        constexpr auto RootLength = int32{1};
#endif
        auto& File = FPlatformFileManager::Get().GetPlatformPhysical();
        if (NOT OrdinaryDirectory(File, Prefix))
        {
            return Fail(OutDiagnostic, TEXT("destination"), TEXT("Destination root is not an ordinary directory"));
        }
        auto SegmentStart = RootLength;
        for (auto Index = RootLength; Index <= Path.Len(); ++Index)
        {
            if (Index != Path.Len() && Path[Index] != TEXT('/'))
            {
                continue;
            }
            const auto Segment = Path.Mid(SegmentStart, Index - SegmentStart);
            if (NOT ValidComponent(Segment))
            {
                return Fail(OutDiagnostic, TEXT("destination"), TEXT("Destination contains an unsafe path component"));
            }
            Prefix += Segment;
            if (Index == Path.Len())
            {
                if (File.FileExists(*Prefix) || File.DirectoryExists(*Prefix) ||
                    File.IsSymlink(*Prefix) == ESymlinkResult::Symlink)
                {
                    return Fail(OutDiagnostic, TEXT("destination_exists"), TEXT("Destination already exists"));
                }
#if PLATFORM_WINDOWS
                if (::GetFileAttributesW(*Prefix) != INVALID_FILE_ATTRIBUTES)
                {
                    return Fail(OutDiagnostic, TEXT("destination_exists"), TEXT("Destination already exists"));
                }
#elif PLATFORM_LINUX || PLATFORM_MAC
                struct stat NativeStat;
                if (::lstat(TCHAR_TO_UTF8(*Prefix), &NativeStat) == 0)
                {
                    return Fail(OutDiagnostic, TEXT("destination_exists"), TEXT("Destination already exists"));
                }
#endif
            }
            else if (NOT OrdinaryDirectory(File, Prefix))
            {
                return Fail(OutDiagnostic, TEXT("destination"), TEXT("Destination ancestor is missing or linked"));
            }
            Prefix += TEXT("/");
            SegmentStart = Index + 1;
        }
        OutNormalized = MoveTemp(Path);
        return true;
#endif
    }

    auto
        EncodePng(const FCk_PixelSprite_RenderImage& InImage,
        TArray64<uint8>& OutBytes, FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
        if (InImage._Width < 1 || InImage._Height < 1 ||
            InImage._Width > 16384 || InImage._Height > 16384 ||
            int64{InImage._Width} * InImage._Height * sizeof(FColor) > MaxSheetBytes ||
            InImage._Pixels.Num() != int64{InImage._Width} * InImage._Height)
        {
            return Fail(OutDiagnostic, TEXT("dimensions"), TEXT("Export image exceeds 16384 pixels or 64 MiB"));
        }
        if (NOT FImageUtils::CompressImage(OutBytes, TEXT(".png"),
            FImageView{InImage._Pixels.GetData(), InImage._Width, InImage._Height}))
        {
            return Fail(OutDiagnostic, TEXT("png"), TEXT("PNG encoding failed"));
        }
        if (OutBytes.Num() > MaxSheetBytes)
        {
            return Fail(OutDiagnostic, TEXT("png_limit"), TEXT("Encoded PNG exceeds 64 MiB"));
        }
        return true;
    }

    auto
        Quote(const FString& InText)
        -> FString
    {
        auto Result = FString{TEXT("\"")};
        for (const auto Character : InText)
        {
            switch (Character)
            {
            case TEXT('"'): Result += TEXT("\\\""); break;
            case TEXT('\\'): Result += TEXT("\\\\"); break;
            case TEXT('\b'): Result += TEXT("\\b"); break;
            case TEXT('\f'): Result += TEXT("\\f"); break;
            case TEXT('\n'): Result += TEXT("\\n"); break;
            case TEXT('\r'): Result += TEXT("\\r"); break;
            case TEXT('\t'): Result += TEXT("\\t"); break;
            default:
                if (Character < 0x20)
                {
                    Result += FString::Printf(TEXT("\\u%04X"), static_cast<uint32>(Character));
                }
                else
                {
                    Result.AppendChar(Character);
                }
                break;
            }
        }
        Result += TEXT("\"");
        return Result;
    }

    auto
        FindAnimation(const FCk_PixelSprite_Manifest& InManifest,
        const FString& InName)
        -> int32
    {
        for (auto Index = int32{0}; Index < InManifest._Animations.Num(); ++Index)
        {
            if (InManifest._Animations[Index]._Name.Equals(InName, ESearchCase::CaseSensitive))
            {
                return Index;
            }
        }
        return INDEX_NONE;
    }

    auto
        SourceFrame(const FCk_PixelSprite_Manifest& InManifest,
        const FCk_PixelSprite_Frame& InFrame)
        -> bool
    {
        return InManifest._Sources.IsValidIndex(InFrame._SourceIndex);
    }

    auto
        StageAndPublish(const TArray64<uint8>& InPng, const FString& InPngPath,
        const TOptional<TArray64<uint8>>& InJson, const FString& InJsonPath,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
        const auto Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        const auto TempPng = InPngPath + TEXT(".") + Id + TEXT(".tmp");
        const auto TempJson = InJsonPath + TEXT(".") + Id + TEXT(".tmp");
        auto TempPngOwned = false;
        auto TempJsonOwned = false;
        auto PngPublished = false;
        auto Success = false;
        ON_SCOPE_EXIT
        {
            if (TempPngOwned)
            {
                IFileManager::Get().Delete(*TempPng, false, false, true);
            }
            if (TempJsonOwned)
            {
                IFileManager::Get().Delete(*TempJson, false, false, true);
            }
            if (PngPublished && NOT Success)
            {
                IFileManager::Get().Delete(*InPngPath, false, false, true);
            }
        };
        if (IFileManager::Get().FileExists(*TempPng) ||
            (InJson.IsSet() && IFileManager::Get().FileExists(*TempJson)))
        {
            return Fail(OutDiagnostic, TEXT("write"), TEXT("Temporary export path already exists"));
        }
        TempPngOwned = true;
        if (NOT FFileHelper::SaveArrayToFile(InPng, *TempPng,
            &IFileManager::Get(), FILEWRITE_NoReplaceExisting))
        {
            return Fail(OutDiagnostic, TEXT("write"), TEXT("PNG staging failed"));
        }
        if (InJson.IsSet())
        {
            TempJsonOwned = true;
            if (NOT FFileHelper::SaveArrayToFile(InJson.GetValue(), *TempJson,
                &IFileManager::Get(), FILEWRITE_NoReplaceExisting))
            {
                return Fail(OutDiagnostic, TEXT("write"), TEXT("JSON staging failed"));
            }
        }
        if (NOT IFileManager::Get().Move(*InPngPath, *TempPng, false, false, false, true))
        {
            return Fail(OutDiagnostic, TEXT("publish"), TEXT("PNG publication failed"));
        }
        TempPngOwned = false;
        PngPublished = true;
        if (InJson.IsSet())
        {
            if (NOT IFileManager::Get().Move(*InJsonPath, *TempJson, false, false, false, true))
            {
                return Fail(OutDiagnostic, TEXT("publish"), TEXT("JSON publication failed"));
            }
            TempJsonOwned = false;
        }
        Success = true;
        OutDiagnostic = {};
        return true;
    }

    auto
        ToUtf8(const FString& InText)
        -> TArray64<uint8>
    {
        const auto Converted = FTCHARToUTF8{*InText};
        auto Bytes = TArray64<uint8>{};
        Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
        return Bytes;
    }
}

auto
    ck::pixel_sprite_editor::
    TryExport_Frame(
        const UCk_PixelSpriteAsset_UE& InAsset,
        const FString& InAnimationName,
        int32 InFrameIndex,
        const FCk_PixelSprite_RenderSettings& InSettings,
        const FString& InOutputPng,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    auto PngPath = FString{};
    if (NOT ck_pixel_sprite_editor_export::ValidateNewDestination(
        InOutputPng, TEXT(".png"), PngPath, OutDiagnostic))
    {
        return false;
    }
    const auto Snapshot = InAsset.Get_Snapshot();
    if (NOT Snapshot.IsValid())
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("asset"), TEXT("Asset has no valid sprite snapshot"));
    }
    const auto AnimationIndex = ck_pixel_sprite_editor_export::FindAnimation(*Snapshot, InAnimationName);
    if (NOT Snapshot->_Animations.IsValidIndex(AnimationIndex) ||
        NOT Snapshot->_Animations[AnimationIndex]._Frames.IsValidIndex(InFrameIndex))
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("frame"), TEXT("Animation or frame does not exist"));
    }
    const auto& Frame = Snapshot->_Animations[AnimationIndex]._Frames[InFrameIndex];
    if (NOT ck_pixel_sprite_editor_export::SourceFrame(*Snapshot, Frame))
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("source"), TEXT("Frame source index is invalid"));
    }
    auto Image = FCk_PixelSprite_RenderImage{};
    if (NOT ck::pixel_sprite::TryRender_Grid(
        Snapshot->_Sources[Frame._SourceIndex]._Grid, InSettings, Image, OutDiagnostic))
    {
        return false;
    }
    auto Png = TArray64<uint8>{};
    if (NOT ck_pixel_sprite_editor_export::EncodePng(Image, Png, OutDiagnostic))
    {
        return false;
    }
    return ck_pixel_sprite_editor_export::StageAndPublish(Png, PngPath, {}, {}, OutDiagnostic);
}

auto
    ck::pixel_sprite_editor::
    TryExport_Sheet(
        const UCk_PixelSpriteAsset_UE& InAsset,
        const FString& InAnimationName,
        const FCk_PixelSprite_RenderSettings& InSettings,
        const FString& InOutputPng,
        const FString& InOutputJson,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    auto PngPath = FString{};
    auto JsonPath = FString{};
    if (NOT ck_pixel_sprite_editor_export::ValidateNewDestination(
            InOutputPng, TEXT(".png"), PngPath, OutDiagnostic) ||
        NOT ck_pixel_sprite_editor_export::ValidateNewDestination(
            InOutputJson, TEXT(".json"), JsonPath, OutDiagnostic))
    {
        return false;
    }
    if (NOT FPaths::GetPath(PngPath).Equals(FPaths::GetPath(JsonPath), ESearchCase::CaseSensitive))
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("destination"), TEXT("Sheet outputs must share one directory"));
    }
    const auto Snapshot = InAsset.Get_Snapshot();
    if (NOT Snapshot.IsValid())
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("asset"), TEXT("Asset has no valid sprite snapshot"));
    }
    const auto AnimationIndex = ck_pixel_sprite_editor_export::FindAnimation(*Snapshot, InAnimationName);
    if (NOT Snapshot->_Animations.IsValidIndex(AnimationIndex))
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("animation"), TEXT("Animation does not exist"));
    }
    const auto& Animation = Snapshot->_Animations[AnimationIndex];
    if (InSettings._Scale < 1 || InSettings._Scale > 8)
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("settings"), TEXT("Export scale is invalid"));
    }
    const auto FrameWidth = int64{Snapshot->_Width} * InSettings._Scale;
    const auto FrameHeight = int64{Snapshot->_Height} * InSettings._Scale;
    const auto SheetWidth = FrameWidth * Animation._Frames.Num();
    if (FrameWidth < 1 || FrameHeight < 1 || SheetWidth < 1 ||
        SheetWidth > 16384 || FrameHeight > 16384 ||
        SheetWidth * FrameHeight * sizeof(FColor) > ck_pixel_sprite_editor_export::MaxSheetBytes)
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("dimensions"), TEXT("Sheet exceeds 16384 pixels or 64 MiB"));
    }
    auto Sheet = FCk_PixelSprite_RenderImage{};
    Sheet._Width = static_cast<int32>(SheetWidth);
    Sheet._Height = static_cast<int32>(FrameHeight);
    Sheet._Pixels.Init(FColor::Transparent, static_cast<int32>(SheetWidth * FrameHeight));
    auto Json = FString{TEXT("{\"format\":\"CkPixelSpriteSheet\",\"version\":1,\"animation\":")};
    Json += ck_pixel_sprite_editor_export::Quote(Animation._Name);
    Json += TEXT(",\"playback\":");
    Json += ck_pixel_sprite_editor_export::Quote(
        Animation._Playback == ECk_PixelSprite_Playback::Once ? TEXT("once") :
        Animation._Playback == ECk_PixelSprite_Playback::PingPong ? TEXT("ping-pong") : TEXT("loop"));
    Json += FString::Printf(TEXT(",\"sheet_width\":%d,\"sheet_height\":%d,\"frame_width\":%d,\"frame_height\":%d"),
        Sheet._Width, Sheet._Height, static_cast<int32>(FrameWidth), static_cast<int32>(FrameHeight));
    Json += TEXT(",\"root_events\":") + Snapshot->_EventsJson +
        TEXT(",\"root_metadata\":") + Snapshot->_MetadataJson +
        TEXT(",\"animation_events\":") + Animation._EventsJson +
        TEXT(",\"animation_metadata\":") + Animation._MetadataJson + TEXT(",\"sources\":[");
    for (auto Index = int32{0}; Index < Snapshot->_Sources.Num(); ++Index)
    {
        if (Index > 0)
        {
            Json += TEXT(",");
        }
        const auto& Source = Snapshot->_Sources[Index];
        Json += TEXT("{\"path\":") + ck_pixel_sprite_editor_export::Quote(Source._Path) +
            TEXT(",\"bytes\":") + LexToString(Source._Bytes) + TEXT("}");
    }
    Json += TEXT("],\"frames\":[");
    if (Json.Len() > ck_pixel_sprite_editor_export::MaxJsonCharacters)
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("json_limit"), TEXT("Sheet metadata exceeds 16 Mi characters"));
    }
    for (auto Index = int32{0}; Index < Animation._Frames.Num(); ++Index)
    {
        const auto& Frame = Animation._Frames[Index];
        if (NOT ck_pixel_sprite_editor_export::SourceFrame(*Snapshot, Frame))
        {
            return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("source"), TEXT("Frame source index is invalid"));
        }
        auto Image = FCk_PixelSprite_RenderImage{};
        if (NOT ck::pixel_sprite::TryRender_Grid(
            Snapshot->_Sources[Frame._SourceIndex]._Grid, InSettings, Image, OutDiagnostic))
        {
            return false;
        }
        if (Image._Width != FrameWidth || Image._Height != FrameHeight)
        {
            return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("dimensions"), TEXT("Rendered frame size differs from sheet cell"));
        }
        for (auto Y = int32{0}; Y < Image._Height; ++Y)
        {
            FMemory::Memcpy(Sheet._Pixels.GetData() + Y * Sheet._Width + Index * Image._Width,
                Image._Pixels.GetData() + Y * Image._Width, Image._Width * sizeof(FColor));
        }
        if (Index > 0)
        {
            Json += TEXT(",");
        }
        Json += TEXT("{\"source\":") + ck_pixel_sprite_editor_export::Quote(Frame._Source) +
            FString::Printf(TEXT(",\"source_index\":%d,\"x\":%d,\"y\":0,\"width\":%d,\"height\":%d,\"duration_ms\":%.17g,\"anchors\":{"),
                Frame._SourceIndex, Index * Image._Width, Image._Width, Image._Height,
                Frame._DurationMilliseconds);
        auto AnchorNames = TArray<FString>{};
        Frame._Anchors.GetKeys(AnchorNames);
        AnchorNames.Sort([](const FString& InLeft, const FString& InRight)
        { return InLeft.Compare(InRight, ESearchCase::CaseSensitive) < 0; });
        for (auto AnchorIndex = int32{0}; AnchorIndex < AnchorNames.Num(); ++AnchorIndex)
        {
            if (AnchorIndex > 0)
            {
                Json += TEXT(",");
            }
            const auto& Position = Frame._Anchors.FindChecked(AnchorNames[AnchorIndex]);
            Json += ck_pixel_sprite_editor_export::Quote(AnchorNames[AnchorIndex]) +
                FString::Printf(TEXT(":[%d,%d]"), Position.X, Position.Y);
        }
        Json += TEXT("},\"events\":") + Frame._EventsJson +
            TEXT(",\"metadata\":") + Frame._MetadataJson + TEXT("}");
        if (Json.Len() > ck_pixel_sprite_editor_export::MaxJsonCharacters)
        {
            return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("json_limit"), TEXT("Sheet metadata exceeds 16 Mi characters"));
        }
    }
    Json += TEXT("]}");
    if (Json.Len() > ck_pixel_sprite_editor_export::MaxJsonCharacters)
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("json_limit"), TEXT("Sheet metadata exceeds 16 Mi characters"));
    }
    auto Png = TArray64<uint8>{};
    if (NOT ck_pixel_sprite_editor_export::EncodePng(Sheet, Png, OutDiagnostic))
    {
        return false;
    }
    auto JsonBytes = ck_pixel_sprite_editor_export::ToUtf8(Json);
    if (JsonBytes.Num() > ck_pixel_sprite_editor_export::MaxJsonCharacters)
    {
        return ck_pixel_sprite_editor_export::Fail(OutDiagnostic, TEXT("json_limit"), TEXT("UTF-8 sheet metadata exceeds 16 MiB"));
    }
    return ck_pixel_sprite_editor_export::StageAndPublish(Png, PngPath,
        TOptional<TArray64<uint8>>{MoveTemp(JsonBytes)}, JsonPath, OutDiagnostic);
}
