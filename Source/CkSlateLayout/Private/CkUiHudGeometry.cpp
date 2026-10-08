#include "CkUiHudGeometry.h"

namespace ck_ui_hud_geometry
{
    constexpr double Pi = UE_PI;
    constexpr double HalfPi = Pi * 0.5;
    constexpr double TwoPi = Pi * 2.0;

    auto IsValidSize(const FVector2f& InSize) -> bool
    {
        return FMath::IsFinite(InSize.X) && FMath::IsFinite(InSize.Y)
            && InSize.X > 0.0f && InSize.Y > 0.0f;
    }

    auto IsValidColor(const FLinearColor& InColor) -> bool
    {
        return FMath::IsFinite(InColor.R) && FMath::IsFinite(InColor.G)
            && FMath::IsFinite(InColor.B) && FMath::IsFinite(InColor.A);
    }

    auto CooldownVertex(const FVector2f& InPosition, const FVector2f& InSize, const FLinearColor& InColor) -> FVertex
    {
        return FVertex{InPosition, FVector2f(InPosition.X / InSize.X, InPosition.Y / InSize.Y), InColor};
    }

    auto RayToRect(const FVector2f& InSize, const double InAngle) -> FVector2f
    {
        const double HalfWidth = static_cast<double>(InSize.X) * 0.5;
        const double HalfHeight = static_cast<double>(InSize.Y) * 0.5;
        const double X = FMath::Cos(InAngle);
        const double Y = FMath::Sin(InAngle);
        const double HorizontalDistance = FMath::Abs(X) > 1e-12 ? HalfWidth / FMath::Abs(X) : TNumericLimits<double>::Max();
        const double VerticalDistance = FMath::Abs(Y) > 1e-12 ? HalfHeight / FMath::Abs(Y) : TNumericLimits<double>::Max();
        const double Distance = FMath::Min(HorizontalDistance, VerticalDistance);
        return FVector2f(static_cast<float>(HalfWidth + X * Distance), static_cast<float>(HalfHeight + Y * Distance));
    }

    auto AddCooldownTriangle(FMesh& InOutMesh, const uint16 InSecond, const uint16 InThird) -> void
    {
        InOutMesh.Indices.Add(0);
        InOutMesh.Indices.Add(InSecond);
        InOutMesh.Indices.Add(InThird);
    }

    auto TryBuildCooldown(const FVector2f& InSize, const float InRemaining, const FLinearColor& InColor, FMesh& OutMesh) -> bool
    {
        if (!IsValidSize(InSize) || !FMath::IsFinite(InRemaining) || InRemaining < 0.0f || InRemaining > 1.0f
            || !IsValidColor(InColor))
        { return false; }

        FMesh Mesh;
        if (InRemaining == 0.0f)
        {
            OutMesh = MoveTemp(Mesh);
            return true;
        }
        if (InRemaining == 1.0f)
        {
            Mesh.Vertices.Add(CooldownVertex(FVector2f(0.0f, 0.0f), InSize, InColor));
            Mesh.Vertices.Add(CooldownVertex(FVector2f(InSize.X, 0.0f), InSize, InColor));
            Mesh.Vertices.Add(CooldownVertex(InSize, InSize, InColor));
            Mesh.Vertices.Add(CooldownVertex(FVector2f(0.0f, InSize.Y), InSize, InColor));
            Mesh.Indices.Add(0);
            Mesh.Indices.Add(1);
            Mesh.Indices.Add(2);
            Mesh.Indices.Add(0);
            Mesh.Indices.Add(2);
            Mesh.Indices.Add(3);
            OutMesh = MoveTemp(Mesh);
            return true;
        }

        const double Start = -HalfPi + TwoPi * (1.0 - static_cast<double>(InRemaining));
        const double End = -HalfPi + TwoPi;
        const FVector2f Center(InSize.X * 0.5f, InSize.Y * 0.5f);
        Mesh.Vertices.Add(CooldownVertex(Center, InSize, InColor));
        Mesh.Vertices.Add(CooldownVertex(RayToRect(InSize, Start), InSize, InColor));

        struct FCorner
        {
            double Angle;
            FVector2f Position;
        };
        const double CornerAngle = FMath::Atan2(static_cast<double>(InSize.Y), static_cast<double>(InSize.X));
        const FCorner Corners[] = {
            {-CornerAngle, FVector2f(InSize.X, 0.0f)},
            {CornerAngle, InSize},
            {Pi - CornerAngle, FVector2f(0.0f, InSize.Y)},
            {Pi + CornerAngle, FVector2f(0.0f, 0.0f)},
        };
        for (const FCorner& Corner : Corners)
        {
            if (Corner.Angle <= Start || Corner.Angle >= End) { continue; }
            const uint16 Previous = static_cast<uint16>(Mesh.Vertices.Num() - 1);
            Mesh.Vertices.Add(CooldownVertex(Corner.Position, InSize, InColor));
            AddCooldownTriangle(Mesh, Previous, static_cast<uint16>(Mesh.Vertices.Num() - 1));
        }
        const uint16 Previous = static_cast<uint16>(Mesh.Vertices.Num() - 1);
        Mesh.Vertices.Add(CooldownVertex(RayToRect(InSize, End), InSize, InColor));
        AddCooldownTriangle(Mesh, Previous, static_cast<uint16>(Mesh.Vertices.Num() - 1));
        OutMesh = MoveTemp(Mesh);
        return true;
    }

    auto TryBuildOrb(const FVector2f& InSize, const float InPhase, const float InBrightness, FMesh& OutMesh) -> bool
    {
        if (!IsValidSize(InSize) || !FMath::IsFinite(InPhase) || !FMath::IsFinite(InBrightness)
            || InBrightness < 0.0f || InBrightness > 1.0f)
        { return false; }

        constexpr int32 RingCount = 20;
        constexpr int32 SectorCount = 64;
        constexpr float TextureRadius = 0.28f;
        constexpr float TextureTravel = 0.04f;
        const float Radius = 0.435f * FMath::Min(InSize.X, InSize.Y);
        const FVector2f Center(InSize.X * 0.5f, InSize.Y * 0.5f);
        const double Phase = FMath::Fmod(static_cast<double>(InPhase), TwoPi);
        const FVector3f Light = FVector3f(0.45f, -0.55f, 0.7f).GetSafeNormal();
        const auto MakeVertex = [Center, Radius, Phase, InBrightness, Light, TextureRadius, TextureTravel](const float InRadius, const float InAngle) -> FVertex
        {
            const float X = InRadius * FMath::Cos(InAngle);
            const float Y = InRadius * FMath::Sin(InAngle);
            const float Z = FMath::Sqrt(FMath::Max(0.0f, 1.0f - InRadius * InRadius));
            const FVector3f Normal(X, Y, Z);
            const float Diffuse = FMath::Max(0.0f, FVector3f::DotProduct(Normal, Light));
            const float RimShade = 0.16f * InRadius * InRadius * InRadius;
            const float LowerShade = 0.10f * FMath::Max(0.0f, Y);
            const float Shade = FMath::Clamp(0.18f + 0.68f * Diffuse - RimShade - LowerShade, 0.0f, 1.0f) * InBrightness;
            const float Swirl = TextureTravel * static_cast<float>(FMath::Sin(Phase + TwoPi * (0.5 * X + 0.35 * Y)));
            return FVertex{
                Center + FVector2f(X, Y) * Radius,
                FVector2f(0.5f, 0.5f) + FVector2f(X, Y) * TextureRadius + FVector2f(-Y, X) * Swirl,
                FLinearColor(Shade, Shade, Shade, 1.0f),
            };
        };

        FMesh Mesh;
        Mesh.Vertices.Reserve(1 + RingCount * SectorCount);
        Mesh.Indices.Reserve(SectorCount * 3 + (RingCount - 1) * SectorCount * 6);
        Mesh.Vertices.Add(MakeVertex(0.0f, 0.0f));
        for (int32 Ring = 1; Ring <= RingCount; ++Ring)
        {
            const float PolarRadius = static_cast<float>(Ring) / RingCount;
            for (int32 Sector = 0; Sector < SectorCount; ++Sector)
            {
                const float Angle = static_cast<float>(TwoPi * Sector / SectorCount);
                Mesh.Vertices.Add(MakeVertex(PolarRadius, Angle));
            }
        }
        for (int32 Sector = 0; Sector < SectorCount; ++Sector)
        {
            Mesh.Indices.Add(0);
            Mesh.Indices.Add(static_cast<uint16>(1 + Sector));
            Mesh.Indices.Add(static_cast<uint16>(1 + (Sector + 1) % SectorCount));
        }
        for (int32 Ring = 1; Ring < RingCount; ++Ring)
        {
            const int32 InnerStart = 1 + (Ring - 1) * SectorCount;
            const int32 OuterStart = InnerStart + SectorCount;
            for (int32 Sector = 0; Sector < SectorCount; ++Sector)
            {
                const int32 Next = (Sector + 1) % SectorCount;
                Mesh.Indices.Add(static_cast<uint16>(InnerStart + Sector));
                Mesh.Indices.Add(static_cast<uint16>(OuterStart + Sector));
                Mesh.Indices.Add(static_cast<uint16>(OuterStart + Next));
                Mesh.Indices.Add(static_cast<uint16>(InnerStart + Sector));
                Mesh.Indices.Add(static_cast<uint16>(OuterStart + Next));
                Mesh.Indices.Add(static_cast<uint16>(InnerStart + Next));
            }
        }
        OutMesh = MoveTemp(Mesh);
        return true;
    }
}
