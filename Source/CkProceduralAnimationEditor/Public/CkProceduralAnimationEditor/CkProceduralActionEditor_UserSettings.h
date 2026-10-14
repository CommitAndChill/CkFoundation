#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "CkProceduralActionEditor_UserSettings.generated.h"

// A preview subject is an editor choice for each clip, never part of the runtime clip asset.
UCLASS(Config = EditorPerProjectUserSettings)
class CKPROCEDURALANIMATIONEDITOR_API UCk_ProceduralActionEditor_UserSettings : public UObject
{
    GENERATED_BODY()
public:
    auto
        GetPreviewPath(
            const FString& InClipPath) const
        -> FString;
    auto
        SetPreviewPath(
            const FString& InClipPath,
            const FString& InPreviewPath)
        -> bool;

private:
    UPROPERTY(Config)
    TMap<FString, FString> _PreviewPathsByClip;
};
