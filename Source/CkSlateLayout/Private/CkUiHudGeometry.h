#pragma once

#include "CoreMinimal.h"

namespace ck_ui_hud_geometry
{
    struct FVertex
    {
        FVector2f Position;
        FVector2f UV;
        FLinearColor Color;
    };

    struct FMesh
    {
        TArray<FVertex> Vertices;
        TArray<uint16> Indices;
    };

    auto TryBuildCooldown(const FVector2f& InSize, float InRemaining, const FLinearColor& InColor, FMesh& OutMesh) -> bool;
    auto TryBuildOrb(const FVector2f& InSize, float InPhase, float InBrightness, FMesh& OutMesh) -> bool;
}
