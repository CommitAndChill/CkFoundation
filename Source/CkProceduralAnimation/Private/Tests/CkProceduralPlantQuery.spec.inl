#if WITH_DEV_AUTOMATION_TESTS

#include <Misc/AutomationTest.h>

namespace ck_procedural_gait_query_tests
{
    auto
        Double(uint64 InBits)
        -> double
    {
        double Value = 0.0;
        FMemory::Memcpy(&Value, &InBits, sizeof(Value));
        return Value;
    }

    auto
        Float(uint32 InBits)
        -> float
    {
        float Value = 0.0f;
        FMemory::Memcpy(&Value, &InBits, sizeof(Value));
        return Value;
    }

    struct FInput
    {
        FVector Plant = FVector::ZeroVector;
        FVector Point = FVector::ZeroVector;
        FVector Hip = FVector::ZeroVector;
        FVector Lead{10.0, 0.0, 0.0};
        FVector Outboard{0.0, 1.0, 0.0};
        FVector Normal = FVector::UpVector;
        FVector Up = FVector::UpVector;
        float Reach = 100.0f;
        float Force = 92.0f;
        float Search = 100.0f;

        auto
            Query(
                TArrayView<const ck::FProceduralFootReservation> InReservations = {},
                float InRadius = 0.0f,
                int32 InLegIndex = 1,
                const TOptional<FVector>& InIdenticalPaidPoint = {}) const
            -> TOptional<FVector>
        {
            return ck_procedural_gait::Get_FinalPlantQueryPoint(Plant, Lead, Hip, Outboard, {Point, Normal}, Up,
                Force, Search, ck_procedural_gait::InboardToleranceShareOfReach * Reach,
                InRadius, InLegIndex, InReservations, InIdenticalPaidPoint);
        }
    };

    auto
        Get_CapturedInputs()
        -> TArray<FInput>
    {
        // Exact captured IEEE operands. Up and the empty reservation view are explicit hypothetical inputs.
        return {
            {
                FVector{Double(0x40fd27a980000000ULL), Double(0x4117e72600000000ULL), Double(0x4085e0000000001bULL)},
                FVector{Double(0x40fd27a980000000ULL), Double(0x4117e72600000000ULL), Double(0x4085e00000000000ULL)},
                FVector{Double(0x40fd2cad7a496bf5ULL), Double(0x4117e66d5c49bf01ULL), Double(0x4089280000000000ULL)},
                FVector{Double(0x4043cccc85678000ULL), Double(0x3f8e90b6c4000000ULL), Double(0x0000000000000000ULL)},
                FVector{Double(0xbf38b1d78457c761ULL), Double(0x3fefffffd9e2c0a1ULL), Double(0x0000000000000000ULL)},
                FVector{Double(0x0000000000000000ULL), Double(0x0000000000000000ULL), Double(0x3ff0000000000000ULL)},
                FVector::UpVector,
                Float(0x430c0000U),
                Float(0x4300cccdU),
                Float(0x42280000U)
            },
            {
                FVector{Double(0x40fd27a97ffffff7ULL), Double(0x4117e72600000001ULL), Double(0x4085e0000000001bULL)},
                FVector{Double(0x40fd27a980000000ULL), Double(0x4117e72600000000ULL), Double(0x4085e00000000000ULL)},
                FVector{Double(0x40fd2cad7a496bf5ULL), Double(0x4117e66d5c49bf01ULL), Double(0x4089280000000000ULL)},
                FVector{Double(0x4043ccccb5372000ULL), Double(0x3f8e8f882c000000ULL), Double(0x0000000000000000ULL)},
                FVector{Double(0xbf38b1d78457c761ULL), Double(0x3fefffffd9e2c0a1ULL), Double(0x0000000000000000ULL)},
                FVector{Double(0x0000000000000000ULL), Double(0x0000000000000000ULL), Double(0x3ff0000000000000ULL)},
                FVector::UpVector,
                Float(0x430c0000U),
                Float(0x4300cccdU),
                Float(0x42280000U)
            },
            {
                FVector{Double(0x40fd372d5ffffffcULL), Double(0x4117ee335fffffffULL), Double(0x40885fffffffffacULL)},
                FVector{Double(0x40fd372d60000000ULL), Double(0x4117ee3360000000ULL), Double(0x4088600000000000ULL)},
                FVector{Double(0x40fd3ed5136afd73ULL), Double(0x4117ee09b4edec46ULL), Double(0x408ab80000000000ULL)},
                FVector{Double(0x40417e8850476000ULL), Double(0xbfecaa3a3c400000ULL), Double(0x0000000000000000ULL)},
                FVector{Double(0x3f9985adf849a1e2ULL), Double(0x3feffd74844f8e2dULL), Double(0x0000000000000000ULL)},
                FVector{Double(0x0000000000000000ULL), Double(0x0000000000000000ULL), Double(0x3ff0000000000000ULL)},
                FVector::UpVector,
                Float(0x43100000U),
                Float(0x43047ae1U),
                Float(0x422ccccdU)
            },
            {
                FVector{Double(0x40fd372d5ffffffcULL), Double(0x4117ee335fffffffULL), Double(0x40885fffffffffacULL)},
                FVector{Double(0x40fd372d60000000ULL), Double(0x4117ee3360000000ULL), Double(0x4088600000000000ULL)},
                FVector{Double(0x40fd3ed5136afd73ULL), Double(0x4117ee09b4edec46ULL), Double(0x408ab80000000000ULL)},
                FVector{Double(0x40417e9bb8725000ULL), Double(0xbfebea3563000000ULL), Double(0x0000000000000000ULL)},
                FVector{Double(0x3f9985adf849a1e2ULL), Double(0x3feffd74844f8e2dULL), Double(0x0000000000000000ULL)},
                FVector{Double(0x0000000000000000ULL), Double(0x0000000000000000ULL), Double(0x3ff0000000000000ULL)},
                FVector::UpVector,
                Float(0x43100000U),
                Float(0x43047ae1U),
                Float(0x422ccccdU)
            },
            {
                FVector{Double(0x40fd67469ffffff6ULL), Double(0x4117f49600000000ULL), Double(0x4087200000000000ULL)},
                FVector{Double(0x40fd6746a0000000ULL), Double(0x4117f49600000000ULL), Double(0x4087200000000000ULL)},
                FVector{Double(0x40fd6b4ad266922dULL), Double(0x4117f4fc8318a858ULL), Double(0x4089615d37c50b85ULL)},
                FVector{Double(0x4038fe62df329000ULL), Double(0xbfe1f6db9d680000ULL), Double(0x0000000000000000ULL)},
                FVector{Double(0xbfcccfdb581dababULL), Double(0x3fef2dc53e43f114ULL), Double(0x0000000000000000ULL)},
                FVector{Double(0x0000000000000000ULL), Double(0x0000000000000000ULL), Double(0x3ff0000000000000ULL)},
                FVector::UpVector,
                Float(0x42c80000U),
                Float(0x42b80000U),
                Float(0x41f00001U)
            }
        };
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ProceduralPlantQuery_CapturedGeometry,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.CapturedGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralPlantQuery_CapturedGeometry::RunTest(const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    const auto Inputs = Get_CapturedInputs();
    TestEqual(TEXT("Five exact input tuples"), Inputs.Num(), 5);
    for (const auto& Input : Inputs)
    {
        const auto Legacy = ck_procedural_gait::Get_PlantSeedBoundaryPoint(Input.Plant, Input.Lead, Input.Hip,
            Input.Outboard, {Input.Point, Input.Normal}, Input.Reach, Input.Force, Input.Search);
        TestFalse(TEXT("Unchanged legacy helper declines captured input"), Legacy.IsSet());
        const auto Point = Input.Query();
        TestTrue(TEXT("Supported horizontal empty-view proposal"), Point.IsSet());
        if (NOT Point.IsSet())
        { continue; }
        const auto Q = Point.GetValue();
        TestEqual(TEXT("Native plane height retained"), Q.Z, Input.Point.Z);
        TestEqual(TEXT("X is exactly binary32"), Q.X, static_cast<double>(static_cast<float>(Q.X)));
        TestEqual(TEXT("Y is exactly binary32"), Q.Y, static_cast<double>(static_cast<float>(Q.Y)));
        TestTrue(TEXT("Original squared force bound"), FVector::DistSquared(Q, Input.Hip)
            <= FMath::Square(static_cast<double>(Input.Force)));
        TestTrue(TEXT("Original distance force bound"), FVector::Dist(Q, Input.Hip) <= Input.Force);
        TestTrue(TEXT("Actual plant remains search origin"), FVector::DistSquared(Q, Input.Plant)
            <= FMath::Square(static_cast<double>(Input.Search)));
        TestTrue(TEXT("Original own-side predicate"), ck::Get_IsFootholdOnOwnSide(Q - Input.Hip, Input.Outboard,
            ck_procedural_gait::InboardToleranceShareOfReach * Input.Reach));
        TestTrue(TEXT("Original empty-view contact predicate"),
            ck::Get_IsProceduralFootContactAvailable(Q, 0.0f, 1, {}));
        const auto Repeated = Input.Query();
        TestTrue(TEXT("Same-build repeated result"), Repeated.IsSet() && Repeated.GetValue() == Q);
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ProceduralPlantQuery_Reservations,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.Reservations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralPlantQuery_Reservations::RunTest(const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    const FInput Input;
    const auto Base = Input.Query();
    TestTrue(TEXT("Half-inside raw proposal retained"), Base.IsSet() && Base.GetValue() == FVector(5.0, 0.0, 0.0));
    const TArray<ck::FProceduralFootReservation> Blocking{{FVector{5.0, 0.0, 0.0}, 1.0f, 2}};
    const auto Moved = Input.Query(Blocking, 1.0f);
    TestTrue(TEXT("Open exclusion advances to available tangent"),
        Moved.IsSet() && Moved.GetValue() == FVector(7.0, 0.0, 0.0));
    if (Moved.IsSet())
    {
        TestTrue(TEXT("Tangent validated by actual predicate"),
            ck::Get_IsProceduralFootContactAvailable(Moved.GetValue(), 1.0f, 1, Blocking));
    }
    const TArray<ck::FProceduralFootReservation> Covered{{FVector::ZeroVector, 1000.0f, 2}};
    TestFalse(TEXT("Whole domain covered"), Input.Query(Covered, 1.0f).IsSet());
    const auto NaN = std::numeric_limits<double>::quiet_NaN();
    auto MalformedPosition = FVector::ZeroVector;
    MalformedPosition.X = NaN;
    TestTrue(TEXT("Malformed reservation fixture retains NaN without component construction"),
        MalformedPosition.ContainsNaN());
    const TArray<ck::FProceduralFootReservation> BadSelf{{MalformedPosition, -1.0f, 1}};
    TestFalse(TEXT("Bad self radius rejects before self skip"), Input.Query(BadSelf, 1.0f).IsSet());
    TestTrue(TEXT("Zero query radius bypasses reservation validation"), Input.Query(BadSelf).IsSet());
    const TArray<ck::FProceduralFootReservation> Self{{MalformedPosition, 1.0f, 1}};
    TestTrue(TEXT("Self invalid position skipped after valid radius"), Input.Query(Self, 1.0f).IsSet());
    const TArray<ck::FProceduralFootReservation> Zero{{MalformedPosition, 0.0f, -1}};
    TestTrue(TEXT("Zero reservation radius skips malformed unused fields"), Input.Query(Zero, 1.0f).IsSet());
    const TArray<ck::FProceduralFootReservation> BadActive{{MalformedPosition, 1.0f, 2}};
    TestFalse(TEXT("Active malformed position rejected"), Input.Query(BadActive, 1.0f).IsSet());
    const TArray<ck::FProceduralFootReservation> BadLeg{{FVector::ZeroVector, 1.0f, -1}};
    TestFalse(TEXT("Active negative leg rejected"), Input.Query(BadLeg, 1.0f).IsSet());
    TArray<ck::FProceduralFootReservation> Full;
    for (int32 Index = 0; Index < 64; ++Index)
    { Full.Emplace(FVector{-1000.0 - Index * 4.0, 0.0, 0.0}, 1.0f, 2); }
    TestTrue(TEXT("64 sorted intervals supported"), Input.Query(Full, 1.0f).IsSet());
    Full.Emplace(FVector::ZeroVector, 0.0f, 1);
    TestFalse(TEXT("65 entries decline without truncation"), Input.Query(Full, 1.0f).IsSet());
    TestFalse(TEXT("65 entries decline even with zero query radius"), Input.Query(Full).IsSet());
    TestFalse(TEXT("Negative query radius rejected"), Input.Query({}, -1.0f).IsSet());
    TestFalse(TEXT("Negative query leg rejected"), Input.Query({}, 0.0f, -1).IsSet());
    TestFalse(TEXT("Certified whole-probe duplicate skips only available point"),
        Input.Query({}, 0.0f, 1, FVector{5.0, 0.0, 0.0}).IsSet());
    TestTrue(TEXT("Same XY with different Z does not skip"),
        Input.Query({}, 0.0f, 1, FVector{5.0, 0.0, 1.0}).IsSet());
    auto EqualityInput = Input;
    EqualityInput.Hip.Y = 10.0;
    const auto Equality = EqualityInput.Query();
    TestTrue(TEXT("Own-side equality belongs to raw"),
        Equality.IsSet() && Equality.GetValue() == FVector(5.0, 0.0, 0.0));
    auto CrossingInput = Input;
    CrossingInput.Hip.Y = 25.0;
    CrossingInput.Lead.Y = 20.0;
    const auto Crossing = CrossingInput.Query();
    TestTrue(TEXT("Earlier corrected start wins over later raw start"),
        Crossing.IsSet() && Crossing.GetValue().X == 5.0 && Crossing.GetValue().Y >= 15.0);
    if (Crossing.IsSet())
    {
        TestTrue(TEXT("Corrected candidate passes original own-side predicate"),
            ck::Get_IsFootholdOnOwnSide(Crossing.GetValue() - CrossingInput.Hip, CrossingInput.Outboard, 10.0f));
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ProceduralPlantQuery_InvalidAndFloatCell,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.InvalidAndFloatCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralPlantQuery_InvalidAndFloatCell::RunTest(const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    using namespace ck_procedural_gait;
    auto Input = FInput{};
    Input.Force = 0.0f;
    TestFalse(TEXT("Zero force rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Search = -1.0f;
    TestFalse(TEXT("Negative search rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Reach = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN tolerance rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Outboard = FVector::ZeroVector;
    TestFalse(TEXT("Zero outboard rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Lead = FVector::ZeroVector;
    TestFalse(TEXT("Zero lead rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Lead.Z = 1.0;
    TestFalse(TEXT("Nonhorizontal lead rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Normal = FVector{0.0, 0.1, 1.0};
    TestFalse(TEXT("Noncardinal plane rejected without normalization"), Input.Query().IsSet());
    Input = FInput{};
    Input.Up = -FVector::UpVector;
    TestFalse(TEXT("Unsupported up rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Point.Z = std::nextafter(1.0, 2.0);
    TestFalse(TEXT("Non-binary32 plane height rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Lead.X = std::numeric_limits<double>::max();
    TestFalse(TEXT("Intermediate overflow rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Hip.X = std::numeric_limits<double>::infinity();
    TestTrue(TEXT("Malformed hip fixture retains infinity without component construction"), Input.Hip.ContainsNaN());
    TestFalse(TEXT("Nonfinite input rejected"), Input.Query().IsSet());
    Input = FInput{};
    Input.Plant.Y = 1000.0;
    TestFalse(TEXT("Search radius uses actual plant, not native origin"), Input.Query().IsSet());
    Input = FInput{};
    Input.Outboard = FVector{1.0, 0.0, 0.0};
    Input.Hip.X = 30.0;
    TestFalse(TEXT("Collapsed corrected velocity rejected"), Input.Query().IsSet());

    FPlantQueryInterval Interval;
    TestTrue(TEXT("Zero discriminant rejected"), Get_PlantQuerySphereInterval(
        FVector{0.0, 1.0, 0.0}, FVector{1.0, 0.0, 0.0}, FVector::ZeroVector, 1.0, Interval)
        == EPlantQueryInterval::Invalid);
    TestTrue(TEXT("Negative discriminant is empty"), Get_PlantQuerySphereInterval(
        FVector{0.0, 2.0, 0.0}, FVector{1.0, 0.0, 0.0}, FVector::ZeroVector, 1.0, Interval)
        == EPlantQueryInterval::Empty);
    TestTrue(TEXT("Zero direction rejected before division"), Get_PlantQuerySphereInterval(
        FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, 1.0, Interval)
        == EPlantQueryInterval::Invalid);

    const auto Max = std::numeric_limits<double>::max();
    const FVector PositiveMax{Max, 0.0, 0.0};
    const FVector NegativeMax{-Max, 0.0, 0.0};
    const FVector Sentinel{1.0, 2.0, 3.0};
    auto Output = Sentinel;
    TestFalse(TEXT("Opposite finite subtraction overflow rejected"),
        Get_PlantQueryDifference(PositiveMax, NegativeMax, Output));
    TestTrue(TEXT("Failed subtraction leaves output unchanged"), Output == Sentinel);
    TestTrue(TEXT("Sphere center subtraction overflow rejected before vector construction"),
        Get_PlantQuerySphereInterval(PositiveMax, FVector{1.0, 0.0, 0.0}, NegativeMax, 1.0, Interval)
        == EPlantQueryInterval::Invalid);
    Input = FInput{};
    Input.Point = PositiveMax;
    Input.Hip = NegativeMax;
    TestFalse(TEXT("Native origin minus hip overflow declines without vector diagnostic"), Input.Query().IsSet());
    TestFalse(TEXT("Image multiplication overflow rejected before vector construction"),
        Get_PlantQueryImage(FVector::ZeroVector, PositiveMax, 2.0, Output));
    TestTrue(TEXT("Failed image multiply leaves output unchanged"), Output == Sentinel);
    TestFalse(TEXT("Image addition overflow rejected before vector construction"),
        Get_PlantQueryImage(PositiveMax, PositiveMax, 1.0, Output));
    TestTrue(TEXT("Failed image addition leaves output unchanged"), Output == Sentinel);
    TestTrue(TEXT("Finite image still succeeds"),
        Get_PlantQueryImage(Sentinel, FVector{2.0, 3.0, 4.0}, 2.0, Output));
    TestTrue(TEXT("Finite image preserves ordinary products then sums"), Output == FVector(5.0, 8.0, 11.0));

    double Values[2];
    const auto Next = std::nextafter(1.0f, 2.0f);
    const auto Midpoint = (1.0 + static_cast<double>(Next)) * 0.5;
    TestTrue(TEXT("RNE cell constructed"), Get_PlantQueryAxisCell(Midpoint, Values));
    TestEqual(TEXT("Tie-even first"), Values[0], 1.0);
    TestEqual(TEXT("Opposite adjacent float second"), Values[1], static_cast<double>(Next));
    TestTrue(TEXT("Exact cell constructed"), Get_PlantQueryAxisCell(1.0, Values));
    TestEqual(TEXT("Exact coordinate duplicates for point deduplication"), Values[0], Values[1]);
    TestTrue(TEXT("Signed zero cell constructed"), Get_PlantQueryAxisCell(-0.0, Values));
    TestFalse(TEXT("Signed zero canonicalized"), std::signbit(Values[0]) || std::signbit(Values[1]));
    TestFalse(TEXT("Float overflow declined before cast"), Get_PlantQueryAxisCell(
        static_cast<double>(std::numeric_limits<float>::max()) * 2.0, Values));
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
namespace ck_procedural_gait_query_tests
{
    struct FHipBodyInput
    {
        ck_procedural_gait::FFootholdQuery Foothold;
        ck_procedural_gait::FPlantSeedPlane Plane{FVector::ZeroVector, FVector::UpVector};
        FVector Up = FVector::UpVector;
        float Force = 100.0f;
        float Tolerance = 14.0f;

        FHipBodyInput()
        {
            Foothold.Hip = FVector{0.0, 30.0, 10.0};
            Foothold.Outboard = FVector{0.0, 1.0, 0.0};
            Foothold.MaxHipLateral = 30.0f;
            Foothold.LegIndex = 1;
        }

        auto
            Query(
                const TOptional<FVector>& InIdenticalFinalPoint = {}) const
            -> TOptional<FVector>
        {
            return ck_procedural_gait::Get_HipBodyBoundaryQueryPoint(
                Foothold, Plane, Up, Force, Tolerance, InIdenticalFinalPoint);
        }
    };
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ProceduralPlantQuery_HipBodyGeometry,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.HipBodyGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralPlantQuery_HipBodyGeometry::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    for (const auto Side : {-1.0, 1.0})
    {
        for (const auto Yaw : {0.0, 0.5})
        {
            auto Input = FHipBodyInput{};
            Input.Foothold.Basis = FQuat{FVector::UpVector, Yaw};
            Input.Foothold.BodyLocation = FVector{119000.0, 391000.0, 0.0};
            const auto BodyY = Input.Foothold.Basis.GetAxisY();
            Input.Foothold.Outboard = Side * BodyY;
            Input.Foothold.Hip = Input.Foothold.BodyLocation + Side * 30.0 * BodyY + 10.0 * FVector::UpVector;
            const auto Point = Input.Query();
            TestTrue(TEXT("Both signed sides and horizontal yaw admit geometry"), Point.IsSet());
            if (NOT Point.IsSet())
            {
                continue;
            }
            const auto Q = Point.GetValue();
            const auto Offset = Input.Foothold.Basis.Inverse().RotateVector(Q - Input.Foothold.BodyLocation);
            const auto NativeLateral = static_cast<float>(Offset.Y);
            const auto Boundary = static_cast<double>(Input.Foothold.MaxHipLateral) - 5.0 + 0.1;
            TestEqual(TEXT("Proposal retains exact paid-plane height"), Q.Z, Input.Plane.Point.Z);
            TestEqual(TEXT("Rounded proposal X is binary32"), Q.X, static_cast<double>(static_cast<float>(Q.X)));
            TestEqual(TEXT("Rounded proposal Y is binary32"), Q.Y, static_cast<double>(static_cast<float>(Q.Y)));
            TestTrue(TEXT("Rounded inverse lateral preserves same-side margin"), Side * Offset.Y >= Boundary);
            TestTrue(TEXT("Actual native float lateral preserves same-side margin"), Side * NativeLateral >= Boundary);
            TestTrue(TEXT("Rounded point passes original force squared"),
                FVector::DistSquared(Q, Input.Foothold.Hip) <= FMath::Square(static_cast<double>(Input.Force)));
            TestTrue(TEXT("Rounded point passes original own-side predicate"), ck::Get_IsFootholdOnOwnSide(
                Q - Input.Foothold.Hip, Input.Foothold.Outboard, Input.Tolerance));
            const auto Repeated = Input.Query();
            TestTrue(TEXT("Same input retains deterministic first passing corner"),
                Repeated.IsSet() && Repeated.GetValue() == Q);
        }
    }
    auto Input = FHipBodyInput{};
    Input.Foothold.Hip.X = -0.0;
    const auto SignedZero = Input.Query();
    TestTrue(TEXT("Exact X cell deduplicates signed zero"), SignedZero.IsSet() && NOT std::signbit(SignedZero->X));
    Input.Foothold.Hip.Y = 24.0;
    TestFalse(TEXT("Initially in-band declines instead of outward repair"), Input.Query().IsSet());
    return NOT HasAnyErrors();
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ProceduralPlantQuery_HipBodyInvalid,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.HipBodyInvalid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralPlantQuery_HipBodyInvalid::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    TestFalse(TEXT("Default query declines before geometry"), ck_procedural_gait::Get_HipBodyBoundaryQueryPoint(
        {}, {FVector::ZeroVector, FVector::UpVector}, FVector::UpVector, 100.0f, 14.0f, {}).IsSet());
    auto Input = FHipBodyInput{};
    Input.Foothold.Hip.X = std::numeric_limits<double>::infinity();
    TestFalse(TEXT("Nonfinite hip rejected"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Foothold.Basis.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Malformed basis rejected before rotation"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Foothold.Outboard = FVector::ZeroVector;
    TestFalse(TEXT("Zero outboard rejected before normalization"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Foothold.Outboard.Z = 0.01;
    TestFalse(TEXT("Nonhorizontal outboard rejected"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Foothold.Outboard.Y = -1.0;
    TestFalse(TEXT("Direction away from the body boundary rejected"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Tolerance = 0.0f;
    TestFalse(TEXT("Nonpositive own-side allowance rejected"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Force = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite force rejected"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Plane.Point.Z = std::nextafter(1.0, 2.0);
    TestFalse(TEXT("Nonrepresentable plane height rejected before cell"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Plane.Normal.Y = 0.1;
    TestFalse(TEXT("Nonhorizontal native plane rejected"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Up = -FVector::UpVector;
    TestFalse(TEXT("Unsupported world up rejected"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Foothold.Hip = FVector{0.0, 3.0e38, 0.0};
    Input.Foothold.BodyLocation = FVector{0.0, -3.0e38, 0.0};
    Input.Foothold.MaxHipLateral = 3.0e38f;
    Input.Force = 1.0e30f;
    TestFalse(TEXT("Finite lateral beyond binary32 range declines before cast"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Foothold.Hip.Y = std::numeric_limits<double>::max();
    Input.Foothold.BodyLocation.Y = -std::numeric_limits<double>::max();
    TestFalse(TEXT("Opposite finite offset overflow declines before vector construction"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    Input.Foothold.Reservations = TArrayView<const ck::FProceduralFootReservation>{
        static_cast<const ck::FProceduralFootReservation*>(nullptr), 1};
    TestFalse(TEXT("Nonempty null reservation view declines before access"), Input.Query().IsSet());
    return NOT HasAnyErrors();
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ProceduralPlantQuery_HipBodyReservationsAndIdentity,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.HipBodyReservationsAndIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralPlantQuery_HipBodyReservationsAndIdentity::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    auto Input = FHipBodyInput{};
    const auto Base = Input.Query();
    TestTrue(TEXT("Unreserved body-limited proposal exists"), Base.IsSet());
    if (NOT Base.IsSet())
    {
        return false;
    }
    TestFalse(TEXT("Identical full-signature old final point consumes no new query"), Input.Query(Base).IsSet());
    auto OtherHeight = Base.GetValue();
    OtherHeight.Z += 1.0;
    TestTrue(TEXT("Different point does not certify an identical old final query"), Input.Query(OtherHeight).IsSet());
    Input.Force = 1.0f;
    TestFalse(TEXT("Force bound cannot be rescued by another corner"), Input.Query().IsSet());
    Input = FHipBodyInput{};
    const TArray<ck::FProceduralFootReservation> Covered{{Base.GetValue(), 100.0f, 2}};
    Input.Foothold.Reservations = Covered;
    Input.Foothold.FootContactRadius = 1.0f;
    TestFalse(TEXT("Entire single cell blocked by current reservation"), Input.Query().IsSet());
    Input.Foothold.FootContactRadius = 0.0f;
    TestTrue(TEXT("Zero contact radius retains original contact bypass"), Input.Query().IsSet());
    auto MalformedPosition = FVector::ZeroVector;
    MalformedPosition.X = std::numeric_limits<double>::quiet_NaN();
    const TArray<ck::FProceduralFootReservation> BadSelf{{MalformedPosition, -1.0f, 1}};
    Input.Foothold.Reservations = BadSelf;
    Input.Foothold.FootContactRadius = 1.0f;
    TestFalse(TEXT("Malformed radius rejected before matching self skip"), Input.Query().IsSet());
    const TArray<ck::FProceduralFootReservation> Self{{MalformedPosition, 1.0f, 1}};
    Input.Foothold.Reservations = Self;
    TestTrue(TEXT("Valid self radius ignores unused malformed position"), Input.Query().IsSet());
    const TArray<ck::FProceduralFootReservation> Active{{MalformedPosition, 1.0f, 2}};
    Input.Foothold.Reservations = Active;
    TestFalse(TEXT("Active malformed reservation position rejected"), Input.Query().IsSet());
    TArray<ck::FProceduralFootReservation> Full;
    for (int32 Index = 0; Index < 64; ++Index)
    {
        Full.Emplace(FVector{-1000.0 - 4.0 * Index, 0.0, 0.0}, 1.0f, 2);
    }
    Input.Foothold.Reservations = Full;
    TestTrue(TEXT("Full bounded reservation view supported"), Input.Query().IsSet());
    Full.Emplace(FVector::ZeroVector, 0.0f, 1);
    Input.Foothold.Reservations = Full;
    TestFalse(TEXT("Oversized reservation view declines without truncation"), Input.Query().IsSet());
    return NOT HasAnyErrors();
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ProceduralPlantQuery_SeedFailureMetadata,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.SeedFailureMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralPlantQuery_SeedFailureMetadata::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_procedural_gait;
    const auto Step = FCk_ProceduralGait_Step{}.Set_TargetReachFraction(0.6f).Set_ForceStepReachFraction(0.8f);
    const auto Foothold = FCk_ProceduralGait_Foothold{}.Set_MaxAngle(45.0f);
    auto Query = FFootholdQuery{};
    Query.Step = &Step;
    Query.Foothold = &Foothold;
    Query.Reach = 100.0f;
    Query.RestDrop = 40.0f;
    Query.MaxHipLateral = 30.0f;
    Query.Outboard = FVector{0.0, 1.0, 0.0};
    auto Rays = 7;
    auto Failure = ck::EProceduralHeightSeedFailure::None;
    auto Candidate = FFootholdCandidate{};
    Candidate.Position = FVector{200.0, 0.0, 0.0};
    Candidate.NativeHit = true;
    Candidate.Cast = ck::EProceduralFootholdCast::Down;
    // These are supplied candidate metadata, not observations of a native query or hit.
    DoValidate(Query, Candidate, EFootholdStage::Search, Rays, &Failure);
    TestTrue(TEXT("Down hit metadata beyond force retains Unreachable"),
        Candidate.Verdict == ck::EProceduralFootholdVerdict::Unreachable);
    TestTrue(TEXT("Down hit metadata classifies final force rejection"),
        Failure == ck::EProceduralHeightSeedFailure::TrustedDownFinalForceReject);
    TestEqual(TEXT("Force rejection returns before world rays"), Rays, 7);

    Candidate.NativeHit = false;
    DoValidate(Query, Candidate, EFootholdStage::Search, Rays, &Failure);
    TestTrue(TEXT("No-hit metadata retains force verdict"),
        Candidate.Verdict == ck::EProceduralFootholdVerdict::Unreachable);
    TestTrue(TEXT("No-hit metadata resets reused failure output"), Failure == ck::EProceduralHeightSeedFailure::None);
    TestEqual(TEXT("No-hit force rejection adds no rays"), Rays, 7);

    Candidate.NativeHit = true;
    Candidate.Cast = ck::EProceduralFootholdCast::Face;
    Failure = ck::EProceduralHeightSeedFailure::TrustedDownFinalForceReject;
    DoValidate(Query, Candidate, EFootholdStage::Search, Rays, &Failure);
    TestTrue(TEXT("Face metadata retains target-reach rejection"),
        Candidate.Verdict == ck::EProceduralFootholdVerdict::Unreachable);
    TestTrue(TEXT("Face metadata cannot inherit Down failure cause"), Failure == ck::EProceduralHeightSeedFailure::None);
    TestEqual(TEXT("Face force rejection adds no rays"), Rays, 7);

    Candidate.Cast = ck::EProceduralFootholdCast::Down;
    Candidate.Position = FVector{0.0, 30.0, 0.0};
    Candidate.Normal = FVector{1.0, 0.0, 0.0};
    Failure = ck::EProceduralHeightSeedFailure::TrustedDownFinalForceReject;
    DoValidate(Query, Candidate, EFootholdStage::Search, Rays, &Failure);
    TestTrue(TEXT("Within-force steep metadata retains TooSteep"),
        Candidate.Verdict == ck::EProceduralFootholdVerdict::TooSteep);
    TestTrue(TEXT("TooSteep resets prior failure cause"), Failure == ck::EProceduralHeightSeedFailure::None);
    TestEqual(TEXT("TooSteep returns before visibility rays"), Rays, 7);

    Candidate.Position = FVector{0.0, 0.0, -20.0};
    Candidate.Normal = FVector::UpVector;
    Failure = ck::EProceduralHeightSeedFailure::TrustedDownFinalForceReject;
    DoValidate(Query, Candidate, EFootholdStage::Search, Rays, &Failure);
    TestTrue(TEXT("Within-force level metadata retains UnderBody"),
        Candidate.Verdict == ck::EProceduralFootholdVerdict::UnderBody);
    TestTrue(TEXT("UnderBody resets prior failure cause"), Failure == ck::EProceduralHeightSeedFailure::None);
    TestEqual(TEXT("UnderBody returns before visibility rays"), Rays, 7);
    TestTrue(TEXT("Worldless early-rejection fixture stays worldless"), Query.World == nullptr);
    return NOT HasAnyErrors();
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_ProceduralPlantQuery_ProbeIdentity,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.ProbeIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_ProceduralPlantQuery_ProbeIdentity::
    RunTest(
        const FString&)
    -> bool
{
    using ck_procedural_gait::Get_IsPlantSeedProbeIdentical;
    auto Paid = FCk_ProceduralGait_Probe{};
    Paid.Set_OutwardLean(0.0f);
    TestTrue(TEXT("Exact copied probe has paid signature"), Get_IsPlantSeedProbeIdentical(Paid, Paid));
    auto Changed = Paid;
    Changed.Set_Up(Paid.Get_Up() + 1.0f);
    TestFalse(TEXT("Changed up probe cannot certify identity"), Get_IsPlantSeedProbeIdentical(Paid, Changed));
    Changed = Paid; Changed.Set_Down(Paid.Get_Down() + 1.0f);
    TestFalse(TEXT("Changed down probe cannot certify identity"), Get_IsPlantSeedProbeIdentical(Paid, Changed));
    Changed = Paid; Changed.Set_OutwardLean(0.35f);
    TestFalse(TEXT("Equal point with changed lean cannot certify identity"), Get_IsPlantSeedProbeIdentical(Paid, Changed));
    Changed = Paid; Changed.Set_ContactGrace(FCk_Time{0.5});
    TestFalse(TEXT("Changed grace conservatively declines identity"), Get_IsPlantSeedProbeIdentical(Paid, Changed));
    auto Filter = Paid.Get_QueryFilter();
    Filter.Set_Channel(ECC_Camera); Changed = Paid; Changed.Set_QueryFilter(Filter);
    TestFalse(TEXT("Changed query channel cannot certify identity"), Get_IsPlantSeedProbeIdentical(Paid, Changed));
    Filter = Paid.Get_QueryFilter(); Filter.Set_MinResponse(ECk_Jolt_PairInteraction::Overlap);
    Changed = Paid; Changed.Set_QueryFilter(Filter);
    TestFalse(TEXT("Changed response cannot certify identity"), Get_IsPlantSeedProbeIdentical(Paid, Changed));
    Filter = Paid.Get_QueryFilter(); Filter.Set_BodySelection(ECk_Jolt_QueryBodySelection::StaticNonSensor);
    Changed = Paid; Changed.Set_QueryFilter(Filter);
    TestFalse(TEXT("Changed body selection cannot certify identity"), Get_IsPlantSeedProbeIdentical(Paid, Changed));
    Filter = Paid.Get_QueryFilter(); Filter.Set_CollideWithBackFacesConvex(true);
    Changed = Paid; Changed.Set_QueryFilter(Filter);
    TestFalse(TEXT("Changed back-face policy cannot certify identity"), Get_IsPlantSeedProbeIdentical(Paid, Changed));
    return NOT HasAnyErrors();
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
namespace ck_procedural_gait_query_tests
{
    struct FTiltedInput
    {
        ck_procedural_gait::FFootholdQuery Query;
        FVector Paid{10.0, 50.0, -1.0};
        FVector Plant{0.0, 50.0, 0.0};
        FVector Lead{20.0, 0.0, 0.0};
        ck_procedural_gait::FPlantSeedPlane Plane{Plant, FVector::UpVector};
        ck::FProceduralGaitReachPaceTrial Trial;
        ck_procedural_gait::FTiltedPlantSeedContext Context{true, true, 1, false, false};
        float Force = 92.0f;
        float Search = 30.0f;

        FTiltedInput()
        {
            Query.Basis = FQuat{FVector::ForwardVector, 0.1};
            Query.BodyLocation = FVector{0.0, 0.0, 60.0};
            Query.Hip = FVector{0.0, 30.0, 60.0};
            Query.Outboard = Query.Basis.GetAxisY();
            Query.Reach = 100.0f;
            Query.RestDrop = 60.0f;
            Query.MaxHipLateral = 30.0f;
            Query.LegIndex = 1;
            Trial.Set_Hip(Query.Basis.Inverse().RotateVector(FVector{100.0, 50.0, 20.0}));
            Trial.Set_PosedHip(Trial.Get_Hip());
        }

        auto
            Point(
                const TOptional<FVector>& InIdentical = {}) const
            -> TOptional<FVector>
        {
            return ck_procedural_gait::Get_TiltedPlantSeedQueryPoint(Query, Paid, Plant, Lead, Plane,
                Force, Search, Trial, Context, InIdentical);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkPlantQueryTiltedGeometry,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.TiltedGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkPlantQueryTiltedGeometry::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    auto Input = FTiltedInput{};
    const auto Point = Input.Point();
    TestTrue(TEXT("Tilted production helper admits one rounded paid-XY point on the fresh plant plane"), Point.IsSet());
    if (Point.IsSet())
    {
        TestEqual(TEXT("Only the defined projection is returned"), Point.GetValue(), FVector(10.0, 50.0, 0.0));
        TestTrue(TEXT("Old trial plant is outside physical boundary"), FVector::Dist(
            Input.Query.Basis.Inverse().RotateVector(Input.Plant), Input.Trial.Get_Hip()) > 100.001);
        TestTrue(TEXT("Projected point fits simulation and posed physical trials"), FVector::Dist(
            Input.Query.Basis.Inverse().RotateVector(Point.GetValue()), Input.Trial.Get_Hip()) <= 100.001);
        TestFalse(TEXT("Identical complete paid-query point is not repeated"), Input.Point(Point).IsSet());
    }
    Input.Paid.X = 20.0;
    TestTrue(TEXT("Full original lead dot endpoint is inclusive"), Input.Point().IsSet());
    Input.Paid.X = 9.0;
    TestFalse(TEXT("Less than half original lead dot is rejected"), Input.Point().IsSet());
    Input.Paid.X = 21.0;
    TestFalse(TEXT("Beyond full original lead dot is rejected"), Input.Point().IsSet());
    auto Flat = FInput{};
    Flat.Hip.Z = 50.0;
    const auto OldPoint = Flat.Query();
    TestTrue(TEXT("Compatible flat original constructor still returns its point"), OldPoint.IsSet());
    Input = FTiltedInput{};
    Input.Query.Basis = FQuat::Identity;
    TestFalse(TEXT("New helper leaves flat behavior to original constructors"), Input.Point().IsSet());
    const auto OldAgain = Flat.Query();
    TestTrue(TEXT("Flat original result is exactly unchanged"), OldPoint.IsSet() && OldAgain.IsSet()
        && OldPoint.GetValue() == OldAgain.GetValue());
    return NOT HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkPlantQueryTiltedInvalid,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.TiltedInvalid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkPlantQueryTiltedInvalid::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    TestFalse(TEXT("Default context rejects without geometry or queries"), ck_procedural_gait::Get_TiltedPlantSeedQueryPoint(
        {}, {}, {}, {}, {}, 0.0f, 0.0f, {}, {}, {}).IsSet());
    auto Input = FTiltedInput{};
    Input.Context.PaidNativeDownUnreachable = false;
    TestFalse(TEXT("Miss or Usable-but-trial-failed paid call cannot qualify"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Context.SearchGranted = false;
    TestFalse(TEXT("No actual ordinary grant cannot qualify"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Context.RemainingSlots = 0;
    TestFalse(TEXT("No remaining slot cannot qualify"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Context.StagingAtEntry = true;
    TestFalse(TEXT("Any non-Dormant entry staging excludes whole walker"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Context.StagingNow = true;
    TestFalse(TEXT("Any non-Dormant current staging excludes whole walker"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Query.Basis.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Malformed basis rejects before inverse or rotation"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Paid.X = std::numeric_limits<double>::max();
    TestFalse(TEXT("Finite paid XY outside float range rejects before narrowing"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Plane.Point.Z = std::nextafter(1.0, 2.0);
    TestFalse(TEXT("Plant plane Z must be exactly native-float representable"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Plane.Normal = Input.Query.Basis.GetAxisZ();
    TestFalse(TEXT("Arbitrary tilted native plant plane remains unsupported"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Query.Outboard = FVector::ZeroVector;
    TestFalse(TEXT("Zero outboard rejects before normalization"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Query.Outboard.X = std::numeric_limits<double>::max();
    TestFalse(TEXT("Outboard squared overflow rejects before native predicate"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Query.RestDrop = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Malformed body-band scalar rejects before conversion"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Trial.Set_PosedHip(FVector{1000.0, 0.0, 0.0});
    TestFalse(TEXT("Posed physical trial rejects independently"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Force = 1.0f;
    TestFalse(TEXT("Original native force radius is not enlarged"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Search = 1.0f;
    TestFalse(TEXT("Original plant-centered search ball is retained"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Query.Outboard = FVector{0.0, -1.0, 0.0};
    TestFalse(TEXT("Original own-side predicate rejects the only point"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Query.MaxHipLateral = 100.0f;
    TestFalse(TEXT("Actual tilted native body-band predicate rejects the only point"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Trial.Set_Hip(FVector{1000.0, 0.0, 0.0});
    TestFalse(TEXT("Simulation physical trial rejects independently"), Input.Point().IsSet());
    Input = FTiltedInput{};
    Input.Query.Reservations = TArrayView<const ck::FProceduralFootReservation>{
        static_cast<const ck::FProceduralFootReservation*>(nullptr), 1};
    TestFalse(TEXT("Nonempty null reservation view rejects before access"), Input.Point().IsSet());
    return NOT HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkPlantQueryTiltedReservations,
    "Ck.ProceduralAnimation.Foothold.PlantQuery.TiltedReservations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkPlantQueryTiltedReservations::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_procedural_gait_query_tests;
    auto Input = FTiltedInput{};
    Input.Query.FootContactRadius = 1.0f;
    const auto Point = Input.Point();
    TestTrue(TEXT("Unreserved tilted point is present before reservation controls"), Point.IsSet());
    if (NOT Point.IsSet())
    {
        return false;
    }
    const TArray<ck::FProceduralFootReservation> Blocked{{Point.GetValue(), 1.0f, 2}};
    Input.Query.Reservations = Blocked;
    TestFalse(TEXT("Current foreign reservation blocks the only point"), Input.Point().IsSet());
    Input.Query.FootContactRadius = 0.0f;
    TestTrue(TEXT("Zero own radius keeps the existing bypass"), Input.Point().IsSet());
    Input.Query.FootContactRadius = 1.0f;
    auto BadPosition = FVector::ZeroVector;
    BadPosition.X = std::numeric_limits<double>::quiet_NaN();
    const TArray<ck::FProceduralFootReservation> BadSelf{{BadPosition, -1.0f, 1}};
    Input.Query.Reservations = BadSelf;
    TestFalse(TEXT("Malformed radius rejects before same-leg exclusion"), Input.Point().IsSet());
    const TArray<ck::FProceduralFootReservation> Self{{BadPosition, 1.0f, 1}};
    Input.Query.Reservations = Self;
    TestTrue(TEXT("Valid same-leg radius ignores its unused malformed position"), Input.Point().IsSet());
    const TArray<ck::FProceduralFootReservation> Zero{{BadPosition, 0.0f, -1}};
    Input.Query.Reservations = Zero;
    TestTrue(TEXT("Zero reserved radius keeps the existing bypass"), Input.Point().IsSet());
    const TArray<ck::FProceduralFootReservation> Active{{BadPosition, 1.0f, 2}};
    Input.Query.Reservations = Active;
    TestFalse(TEXT("Malformed active reservation cannot reach native predicate"), Input.Point().IsSet());
    TArray<ck::FProceduralFootReservation> Full;
    for (auto Index = 0; Index < 64; ++Index)
    {
        Full.Emplace(FVector{-1000.0 - Index, 0.0, 0.0}, 1.0f, 2);
    }
    Input.Query.Reservations = Full;
    TestTrue(TEXT("Complete64 reservation view is evaluated without truncation"), Input.Point().IsSet());
    Full.Emplace(FVector::ZeroVector, 0.0f, 1);
    Input.Query.Reservations = Full;
    TestFalse(TEXT("Oversized reservation view rejects before iteration"), Input.Point().IsSet());
    return NOT HasAnyErrors();
}
#endif
