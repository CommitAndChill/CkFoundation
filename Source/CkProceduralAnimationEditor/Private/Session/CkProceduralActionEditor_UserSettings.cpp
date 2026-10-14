#include "CkProceduralAnimationEditor/CkProceduralActionEditor_UserSettings.h"

auto
    UCk_ProceduralActionEditor_UserSettings::
    GetPreviewPath(
        const FString& InClipPath) const
    -> FString
{
    const auto* Path = _PreviewPathsByClip.Find(InClipPath);
    return Path != nullptr ? *Path : FString{};
}

auto
    UCk_ProceduralActionEditor_UserSettings::
    SetPreviewPath(
        const FString& InClipPath,
        const FString& InPreviewPath)
    -> bool
{
    if (InClipPath.IsEmpty() || GetPreviewPath(InClipPath) == InPreviewPath)
    { return false; }
    if (InPreviewPath.IsEmpty())
    { _PreviewPathsByClip.Remove(InClipPath); }
    else
    { _PreviewPathsByClip.Add(InClipPath, InPreviewPath); }
    return true;
}
