#include "CkPixelSprite/CkPixelSprite_Render.h"
#include "CkCore/Macros/CkMacros.h"

namespace ck::pixel_sprite::render_private
{
    auto
        Validate(const FCk_PixelSprite_Grid& InGrid)
        -> bool
    {
        if (InGrid._Width <= 0 || InGrid._Height <= 0 || InGrid._Width > 4096 || InGrid._Height > 4096 ||
            int64{InGrid._Width} * InGrid._Height > 1048576 ||
            InGrid._Indices.Num() != int64{InGrid._Width} * InGrid._Height ||
            InGrid._Palette.IsEmpty() || InGrid._Palette.Num() > 65536)
        {
            return false;
        }
        for (const auto Index : InGrid._Indices)
        {
            if (NOT InGrid._Palette.IsValidIndex(Index))
            {
                return false;
            }
        }
        return true;
    }

    auto
        Premultiply(const FColor& InColor)
        -> FLinearColor
    {
        auto Color = FLinearColor{InColor};
        Color.R *= Color.A;
        Color.G *= Color.A;
        Color.B *= Color.A;
        return Color;
    }

    auto
        Over(const FLinearColor& InFront, const FLinearColor& InBack)
        -> FLinearColor
    {
        return InFront + InBack * (1.0f - InFront.A);
    }

    auto
        Straight(const FLinearColor& InColor)
        -> FColor
    {
        if (InColor.A <= 0.0f)
        {
            return FColor::Transparent;
        }
        return FLinearColor{InColor.R / InColor.A, InColor.G / InColor.A,
            InColor.B / InColor.A, InColor.A}.ToFColorSRGB();
    }
}

auto
    ck::pixel_sprite::
    TryRender_Grid(const FCk_PixelSprite_Grid& InGrid,
        const FCk_PixelSprite_RenderSettings& InSettings, FCk_PixelSprite_RenderImage& OutImage,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    const auto Fail = [&OutDiagnostic](const FString& InMessage) -> bool
    {
        OutDiagnostic = {TEXT("render"), TEXT("render"), InMessage, 1, 1};
        return false;
    };
    if (NOT render_private::Validate(InGrid) || InSettings._Scale < 1 || InSettings._Scale > 8 ||
        (InSettings._Mode != ECk_PixelSprite_RenderMode::Nearest &&
         InSettings._Mode != ECk_PixelSprite_RenderMode::Bilinear &&
         InSettings._Mode != ECk_PixelSprite_RenderMode::SmoothSilhouette))
    {
        return Fail(TEXT("invalid grid or rendering settings"));
    }
    const auto Width = InGrid._Width * InSettings._Scale;
    const auto Height = InGrid._Height * InSettings._Scale;
    if (int64{Width} * Height > 16777216)
    {
        return Fail(TEXT("rendered image exceeds 16 Mi pixels"));
    }
    for (const auto& Override : InSettings._PaletteOverrides)
    {
        if (NOT InGrid._Palette.IsValidIndex(Override.Key))
        {
            return Fail(TEXT("palette override index is outside the source palette"));
        }
    }
    const auto Pixel = [&InGrid, &InSettings](int32 InX, int32 InY) -> FColor
    {
        if (InX < 0 || InY < 0 || InX >= InGrid._Width || InY >= InGrid._Height)
        {
            return FColor::Transparent;
        }
        const auto Index = InGrid._Indices[InY * InGrid._Width + InX];
        return InSettings._PaletteOverrides.Contains(Index)
            ? InSettings._PaletteOverrides[Index] : InGrid._Palette[Index]._Color;
    };
    auto Candidate = FCk_PixelSprite_RenderImage{Width, Height, {}};
    Candidate._Pixels.SetNumUninitialized(Width * Height);
    const auto Background = render_private::Premultiply(InSettings._Background);
    const auto Outline = render_private::Premultiply(InSettings._OutlineColor);
    for (auto Y = int32{0}; Y < Height; ++Y)
    {
        for (auto X = int32{0}; X < Width; ++X)
        {
            const auto SourceX = X / InSettings._Scale;
            const auto SourceY = Y / InSettings._Scale;
            const auto SourceColor = Pixel(SourceX, SourceY);
            auto Color = render_private::Premultiply(SourceColor);
            if (InSettings._Mode != ECk_PixelSprite_RenderMode::Nearest)
            {
                const auto SampleX = (X + 0.5f) / InSettings._Scale - 0.5f;
                const auto SampleY = (Y + 0.5f) / InSettings._Scale - 0.5f;
                const auto Left = FMath::FloorToInt(SampleX);
                const auto Top = FMath::FloorToInt(SampleY);
                const auto FX = SampleX - Left;
                const auto FY = SampleY - Top;
                auto Filtered = FLinearColor{0, 0, 0, 0};
                for (auto DY = int32{0}; DY < 2; ++DY)
                {
                    for (auto DX = int32{0}; DX < 2; ++DX)
                    {
                        const auto Weight = (DX == 0 ? 1.0f - FX : FX) * (DY == 0 ? 1.0f - FY : FY);
                        Filtered += render_private::Premultiply(Pixel(Left + DX, Top + DY)) * Weight;
                    }
                }
                if (InSettings._Mode == ECk_PixelSprite_RenderMode::Bilinear)
                {
                    Color = Filtered;
                }
                else
                {
                    // Reconstruct only alpha coverage; retain the nearest authored RGB.
                    auto NearestColor = SourceColor;
                    if (NearestColor.A == 0 && Filtered.A > 0)
                    {
                        auto BestDistance = MAX_flt;
                        for (auto DY = int32{0}; DY < 2; ++DY)
                        {
                            for (auto DX = int32{0}; DX < 2; ++DX)
                            {
                                const auto Neighbor = Pixel(Left + DX, Top + DY);
                                const auto Distance = FMath::Square(SampleX - Left - DX) + FMath::Square(SampleY - Top - DY);
                                if (Neighbor.A > 0 && Distance < BestDistance)
                                {
                                    NearestColor = Neighbor;
                                    BestDistance = Distance;
                                }
                            }
                        }
                    }
                    Color = FLinearColor{NearestColor};
                    Color.A = Filtered.A;
                    Color.R *= Color.A;
                    Color.G *= Color.A;
                    Color.B *= Color.A;
                }
            }
            if (InSettings._Outline)
            {
                auto Coverage = float{0};
                for (auto DY = int32{-1}; DY <= 1; ++DY)
                {
                    for (auto DX = int32{-1}; DX <= 1; ++DX)
                    {
                        Coverage = FMath::Max(Coverage, Pixel(SourceX + DX, SourceY + DY).A / 255.0f);
                    }
                }
                Color = render_private::Over(Color, Outline * Coverage);
            }
            Color = render_private::Over(Color, Background);
            Candidate._Pixels[Y * Width + X] = render_private::Straight(Color);
        }
    }
    OutImage = MoveTemp(Candidate);
    OutDiagnostic = {};
    return true;
}

auto
    ck::pixel_sprite::
    TryCompare_Grids(const FCk_PixelSprite_Grid& InLeft,
        const FCk_PixelSprite_Grid& InRight, FCk_PixelSprite_Difference& OutDifference,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
    -> bool
{
    if (NOT render_private::Validate(InLeft) || NOT render_private::Validate(InRight) ||
        InLeft._Width != InRight._Width || InLeft._Height != InRight._Height)
    {
        OutDiagnostic = {TEXT("comparison"), TEXT("dimensions"), TEXT("comparison requires valid equal-size grids"), 1, 1};
        return false;
    }
    auto Candidate = FCk_PixelSprite_Difference{};
    Candidate._Image = {InLeft._Width, InLeft._Height, {}};
    Candidate._Image._Pixels.Init(FColor::Transparent, InLeft._Indices.Num());
    auto Bounds = FIntRect{InLeft._Width, InLeft._Height, 0, 0};
    for (auto Index = int32{0}; Index < InLeft._Indices.Num(); ++Index)
    {
        if (InLeft._Palette[InLeft._Indices[Index]]._Color == InRight._Palette[InRight._Indices[Index]]._Color)
        {
            continue;
        }
        ++Candidate._ChangedPixels;
        Candidate._Image._Pixels[Index] = FColor{255, 64, 128, 255};
        const auto X = Index % InLeft._Width;
        const auto Y = Index / InLeft._Width;
        Bounds.Min.X = FMath::Min(Bounds.Min.X, X);
        Bounds.Min.Y = FMath::Min(Bounds.Min.Y, Y);
        Bounds.Max.X = FMath::Max(Bounds.Max.X, X + 1);
        Bounds.Max.Y = FMath::Max(Bounds.Max.Y, Y + 1);
    }
    if (Candidate._ChangedPixels > 0)
    {
        Candidate._Bounds = Bounds;
    }
    OutDifference = MoveTemp(Candidate);
    OutDiagnostic = {};
    return true;
}
