#include "CkPixelSprite_Schema.h"
#include "CkPixelSprite_File.h"

auto
    ck::pixel_sprite::TryLoad_Manifest(
        const FString& InManifestPath,
        FCk_PixelSprite_Manifest& OutManifest,
        FCk_PixelSprite_Diagnostic& OutDiagnostic) -> bool
{
    // Check counted characters before path APIs can interpret a NUL as an end.
    if (InManifestPath.IsEmpty())
    {
        OutDiagnostic = {InManifestPath, TEXT("io"), TEXT("Manifest path is empty")};
        return false;
    }
    for (const auto Character : InManifestPath)
    {
        if (Character == 0)
        {
            OutDiagnostic = {InManifestPath, TEXT("io"), TEXT("Manifest path contains NUL")};
            return false;
        }
    }
    auto ManifestPath = FPaths::ConvertRelativePathToFull(InManifestPath);
    FPaths::NormalizeFilename(ManifestPath);
    auto Text = FString{};
    auto Bytes = int64{0};
    if (NOT file_private::Load_NativeSource(ManifestPath, Text, Bytes, OutDiagnostic))
    {
        return false;
    }
    auto Root = json_private::FNode{};
    if (NOT json_private::TryParse(Text, Root, OutDiagnostic))
    {
        OutDiagnostic._Source = ManifestPath;
        return false;
    }
    auto Candidate = FCk_PixelSprite_Manifest{};
    auto Schema = manifest_private::FSchema{Text, ManifestPath, OutDiagnostic};
    if (NOT Schema.Parse(Root, Candidate))
    {
        return false;
    }

    const auto SourceRoot = FPaths::GetPath(ManifestPath);
    auto Cache = TCaseSensitiveStringMap<int32>{};
    auto TotalBytes = int64{0};
    for (auto& Animation : Candidate._Animations)
    {
        for (auto& Frame : Animation._Frames)
        {
            auto SourcePath = SourceRoot / Frame._Source;
            FPaths::NormalizeFilename(SourcePath);
            // Cache exact normalized spelling. Case-sensitive Windows directories may
            // contain distinct files whose names differ only by case.
            const auto CacheKey = SourcePath;
            if (Cache.Contains(CacheKey))
            {
                Frame._SourceIndex = Cache.FindChecked(CacheKey);
                continue;
            }
            auto Source = FCk_PixelSprite_Source{};
            Source._Path = SourcePath;
            auto Xpm = FString{};
            if (NOT file_private::Load_NativeSource(SourcePath, Xpm, Source._Bytes, OutDiagnostic))
            {
                return false;
            }
            TotalBytes += Source._Bytes;
            if (TotalBytes > 64 * 1024 * 1024)
            {
                OutDiagnostic = {SourcePath, TEXT("source_limit"), TEXT("Aggregate unique XPM bytes exceed 64 MiB")};
                return false;
            }
            if (NOT TryParse_Xpm(Xpm, SourcePath, Source._Grid, OutDiagnostic))
            {
                return false;
            }
            if (Source._Grid._Width != Candidate._Width || Source._Grid._Height != Candidate._Height)
            {
                OutDiagnostic = {SourcePath, TEXT("canvas"), TEXT("XPM dimensions differ from manifest canvas")};
                return false;
            }
            Frame._SourceIndex = Candidate._Sources.Num();
            Cache.Add(CacheKey, Frame._SourceIndex);
            Candidate._Sources.Add(MoveTemp(Source));
        }
    }
    OutManifest = MoveTemp(Candidate);
    OutDiagnostic = {};
    return true;
}
