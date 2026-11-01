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
