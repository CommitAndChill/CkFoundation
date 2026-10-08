#include "../CkUiHudGeometry.h"

#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_ui_hud_geometry_test
{
    auto Area(const ck_ui_hud_geometry::FMesh& InMesh) -> double
    {
        double Result = 0.0;
        for (int32 Index = 0; Index < InMesh.Indices.Num(); Index += 3)
        {
            const FVector2f A = InMesh.Vertices[InMesh.Indices[Index]].Position;
            const FVector2f B = InMesh.Vertices[InMesh.Indices[Index + 1]].Position;
            const FVector2f C = InMesh.Vertices[InMesh.Indices[Index + 2]].Position;
            Result += FMath::Abs(static_cast<double>(B.X - A.X) * (C.Y - A.Y)
                - static_cast<double>(B.Y - A.Y) * (C.X - A.X)) * 0.5;
        }
        return Result;
    }

    auto IsInBounds(const ck_ui_hud_geometry::FMesh& InMesh, const FVector2f& InSize) -> bool
    {
        for (const ck_ui_hud_geometry::FVertex& Vertex : InMesh.Vertices)
        {
            if (!FMath::IsFinite(Vertex.Position.X) || !FMath::IsFinite(Vertex.Position.Y)
                || Vertex.Position.X < -KINDA_SMALL_NUMBER || Vertex.Position.Y < -KINDA_SMALL_NUMBER
                || Vertex.Position.X > InSize.X + KINDA_SMALL_NUMBER || Vertex.Position.Y > InSize.Y + KINDA_SMALL_NUMBER)
            { return false; }
        }
        return true;
    }

    auto HasSentinel(const ck_ui_hud_geometry::FMesh& InMesh) -> bool
    {
        return InMesh.Vertices.Num() == 1 && InMesh.Indices.Num() == 1
            && InMesh.Vertices[0].Position == FVector2f(17.0f, 29.0f)
            && InMesh.Indices[0] == 42;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkUiHudGeometryTest,
    "Ck.UiAuthoring.SlateLayout.HudGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkUiHudGeometryTest::RunTest(const FString&) -> bool
{
    using namespace ck_ui_hud_geometry;
    using namespace ck_ui_hud_geometry_test;

    const FVector2f Size(120.0f, 120.0f);
    const FLinearColor Color(0.2f, 0.4f, 0.8f, 0.7f);
    FMesh Mesh;
    if (!TestTrue(TEXT("Full cooldown builds"), TryBuildCooldown(Size, 1.0f, Color, Mesh))) { return false; }
    TestTrue(TEXT("Full cooldown fills rectangle exactly"), FMath::IsNearlyEqual(Area(Mesh), 14400.0, 0.01));
    TestTrue(TEXT("Full cooldown stays in bounds"), IsInBounds(Mesh, Size));
    if (!TestTrue(TEXT("Half cooldown builds"), TryBuildCooldown(Size, 0.5f, Color, Mesh))) { return false; }
    TestTrue(TEXT("Half cooldown covers half rectangle"), FMath::IsNearlyEqual(Area(Mesh), 7200.0, 0.01));
    TestTrue(TEXT("Half cooldown stays in bounds"), IsInBounds(Mesh, Size));
    if (!TestTrue(TEXT("Quarter cooldown builds"), TryBuildCooldown(Size, 0.25f, Color, Mesh))) { return false; }
    TestTrue(TEXT("Quarter cooldown covers quarter rectangle"), FMath::IsNearlyEqual(Area(Mesh), 3600.0, 0.01));
    TestTrue(TEXT("Quarter cooldown stays in bounds"), IsInBounds(Mesh, Size));
    if (!TestTrue(TEXT("Empty cooldown builds"), TryBuildCooldown(Size, 0.0f, Color, Mesh))) { return false; }
    TestTrue(TEXT("Empty cooldown has no fill"), Mesh.Vertices.IsEmpty() && Mesh.Indices.IsEmpty());

    FMesh Sentinel;
    Sentinel.Vertices.Add(FVertex{FVector2f(17.0f, 29.0f), FVector2f(0.1f, 0.2f), FLinearColor::Red});
    Sentinel.Indices.Add(42);
    TestFalse(TEXT("Cooldown rejects zero width"), TryBuildCooldown(FVector2f(0.0f, 120.0f), 0.5f, Color, Sentinel));
    TestTrue(TEXT("Invalid cooldown size preserves output"), HasSentinel(Sentinel));
    TestFalse(TEXT("Cooldown rejects out-of-range fraction"), TryBuildCooldown(Size, 1.1f, Color, Sentinel));
    TestTrue(TEXT("Invalid cooldown fraction preserves output"), HasSentinel(Sentinel));
    TestFalse(TEXT("Cooldown rejects NaN fraction"), TryBuildCooldown(Size, std::numeric_limits<float>::quiet_NaN(), Color, Sentinel));
    TestTrue(TEXT("Non-finite cooldown fraction preserves output"), HasSentinel(Sentinel));

    FMesh OrbAtStart;
    FMesh OrbAtQuarter;
    FMesh OrbAtCycle;
    const FVector2f OrbSize(120.0f, 160.0f);
    if (!TestTrue(TEXT("Orb builds at start"), TryBuildOrb(OrbSize, 0.0f, 0.8f, OrbAtStart))) { return false; }
    if (!TestTrue(TEXT("Orb builds at quarter turn"), TryBuildOrb(OrbSize, static_cast<float>(UE_PI * 0.5), 0.8f, OrbAtQuarter))) { return false; }
    if (!TestTrue(TEXT("Orb builds at full turn"), TryBuildOrb(OrbSize, static_cast<float>(UE_PI * 2.0), 0.8f, OrbAtCycle))) { return false; }
    TestEqual(TEXT("Orb uses 20 rings and 64 sectors"), OrbAtStart.Vertices.Num(), 1 + 20 * 64);
    TestEqual(TEXT("Orb triangle topology is stable"), OrbAtStart.Indices.Num(), OrbAtQuarter.Indices.Num());
    TestTrue(TEXT("Orb remains inside allotted bounds"), IsInBounds(OrbAtStart, OrbSize));
    bool bUvChanged = false;
    bool bFixedPositionAndColor = OrbAtStart.Vertices.Num() == OrbAtQuarter.Vertices.Num();
    bool bUvInterior = true;
    bool bPeriodic = OrbAtStart.Vertices.Num() == OrbAtCycle.Vertices.Num();
    for (int32 Index = 0; Index < OrbAtStart.Vertices.Num(); ++Index)
    {
        const FVertex& Start = OrbAtStart.Vertices[Index];
        const FVertex& Quarter = OrbAtQuarter.Vertices[Index];
        const FVertex& Cycle = OrbAtCycle.Vertices[Index];
        bUvChanged |= !Start.UV.Equals(Quarter.UV, 0.001f);
        bFixedPositionAndColor &= Start.Position == Quarter.Position && Start.Color == Quarter.Color;
        bUvInterior &= Start.UV.X >= 0.18f && Start.UV.X <= 0.82f
            && Start.UV.Y >= 0.18f && Start.UV.Y <= 0.82f
            && Quarter.UV.X >= 0.18f && Quarter.UV.X <= 0.82f
            && Quarter.UV.Y >= 0.18f && Quarter.UV.Y <= 0.82f;
        bPeriodic &= Start.UV.Equals(Cycle.UV, 0.001f);
    }
    TestTrue(TEXT("Orb phase moves texture coordinates"), bUvChanged);
    TestTrue(TEXT("Orb UV motion swirls without translating the texture center"),
        OrbAtStart.Vertices[0].UV == OrbAtQuarter.Vertices[0].UV);
    TestTrue(TEXT("Orb phase leaves geometry and hemisphere lighting fixed"), bFixedPositionAndColor);
    TestTrue(TEXT("Orb texture samples remain in sphere interior"), bUvInterior);
    TestTrue(TEXT("Orb UV motion repeats after one cycle"), bPeriodic);

    TestFalse(TEXT("Orb rejects negative brightness"), TryBuildOrb(OrbSize, 0.0f, -0.01f, Sentinel));
    TestTrue(TEXT("Invalid orb brightness preserves output"), HasSentinel(Sentinel));
    TestFalse(TEXT("Orb rejects non-finite phase"), TryBuildOrb(OrbSize, std::numeric_limits<float>::infinity(), 0.8f, Sentinel));
    TestTrue(TEXT("Invalid orb phase preserves output"), HasSentinel(Sentinel));
    return true;
}

#endif
