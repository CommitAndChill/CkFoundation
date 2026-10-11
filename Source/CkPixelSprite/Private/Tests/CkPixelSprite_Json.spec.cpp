// Focused tests for the private JSON recognizer; file and schema tests are separate.

#include "CkPixelSprite_Json.h"

#include "Misc/AutomationTest.h"

#include <cfloat>

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_sprite_json_tests
{
    using ck::pixel_sprite::json_private::EKind;
    using ck::pixel_sprite::json_private::FNode;

    auto
        NestedArrays(
            int32 InDepth)
        -> FString
    {
        auto Text = FString{};
        Text.Reserve(InDepth * 2);
        for (auto Index = int32{0}; Index < InDepth; ++Index)
        {
            Text.AppendChar(TEXT('['));
        }
        for (auto Index = int32{0}; Index < InDepth; ++Index)
        {
            Text.AppendChar(TEXT(']'));
        }
        return Text;
    }

    auto
        FlatArray(
            int32 InElements)
        -> FString
    {
        auto Text = FString{};
        Text.Reserve(InElements * 2 + 2);
        Text.AppendChar(TEXT('['));
        for (auto Index = int32{0}; Index < InElements; ++Index)
        {
            if (Index > 0)
            {
                Text.AppendChar(TEXT(','));
            }
            Text.AppendChar(TEXT('0'));
        }
        Text.AppendChar(TEXT(']'));
        return Text;
    }

    auto
        FlatObject(
            int32 InPairs)
        -> FString
    {
        auto Text = FString{};
        Text.Reserve(InPairs * 12 + 2);
        Text.AppendChar(TEXT('{'));
        for (auto Index = int32{0}; Index < InPairs; ++Index)
        {
            if (Index > 0)
            {
                Text.AppendChar(TEXT(','));
            }
            Text += TEXT("\"k");
            Text += FString::FromInt(Index);
            Text += TEXT("\":0");
        }
        Text.AppendChar(TEXT('}'));
        return Text;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_JsonNullIdentity,
    "Ck.PixelSprite.Json.NullIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_JsonNullIdentity::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_pixel_sprite_json_tests;
    using ck::pixel_sprite::json_private::TryParse;

    // Each pair decodes to exactly the same Unicode sequence despite different
    // escape spellings. The duplicate check must not hash decoded FString
    // through its first NUL or confuse a NUL suffix with its shorter prefix.
    const auto Duplicates = TArray<FString>{
        TEXT("{\"a\":1,\"\\u0061\":2}"),
        TEXT("{\"\\u0000A\":1,\"\\u0000\\u0041\":2}"),
        TEXT("{\"A\\u0000B\":1,\"\\u0041\\u0000\\u0042\":2}"),
        TEXT("{\"A\\u0000\":1,\"\\u0041\\u0000\":2}"),
        TEXT("{\"\U0001F600\":1,\"\\uD83D\\uDE00\":2}")};
    for (const auto& Text : Duplicates)
    {
        auto Root = FNode{};
        Root._Kind = EKind::String;
        Root._Value = TEXT("sentinel");
        auto Error = FCk_PixelSprite_Diagnostic{};
        TestFalse(TEXT("Equivalent decoded keys reject"), TryParse(Text, Root, Error));
        TestEqual(TEXT("Duplicate code"), Error._Code, FString{TEXT("json")});
        TestTrue(TEXT("Duplicate rejection preserves output"),
            Root._Kind == EKind::String && Root._Value == TEXT("sentinel"));
    }

    const auto Distinct = FString{
        TEXT("{\"\\u0000a\":1,\"\\u0000b\":2,\"a\\u0000\":3,\"a\":4,")
        TEXT("\"metadata\":{\"leading\":\"\\u0000x\",")
        TEXT("\"interior\":\"x\\u0000y\",\"trailing\":\"x\\u0000\"}}")};
    auto Root = FNode{};
    auto Error = FCk_PixelSprite_Diagnostic{};
    if (NOT TestTrue(TEXT("Distinct NUL keys accepted"), TryParse(Distinct, Root, Error)))
    {
        return false;
    }
    if (NOT TestEqual(TEXT("Five root fields"), Root._Children.Num(), 5))
    {
        return false;
    }
    if (NOT TestTrue(TEXT("NUL flags parallel keys"),
        Root._KeyContainsNull.Num() == Root._Keys.Num() && Root._Keys.Num() == 5))
    {
        return false;
    }
    TestTrue(TEXT("Leading NUL decoded, not dropped"),
        Root._KeyContainsNull[0] && Root._Keys[0].Len() == 2 && Root._Keys[0][0] == 0 &&
        Root._Keys[0][1] == TEXT('a'));
    TestTrue(TEXT("Distinct leading NUL keys"),
        Root._KeyContainsNull[1] && Root._Keys[1].Len() == 2 &&
        Root._Keys[1][0] == 0 && Root._Keys[1][1] == TEXT('b'));
    TestTrue(TEXT("Trailing NUL distinguished from prefix"),
        Root._KeyContainsNull[2] && NOT Root._KeyContainsNull[3] &&
        Root._Keys[2].Len() == 2 && Root._Keys[2][1] == 0);

    const auto& Metadata = Root._Children[4];
    if (NOT TestTrue(TEXT("Metadata object"), Metadata._Kind == EKind::Object &&
        Metadata._Children.Num() == 3))
    {
        return false;
    }
    for (const auto& Value : Metadata._Children)
    {
        TestTrue(TEXT("Decoded metadata string records NUL"),
            Value._Kind == EKind::String && Value._ContainsNull);
        TestTrue(TEXT("Raw metadata string span survives"),
            Distinct.Mid(Value._Start, Value._End - Value._Start).Contains(TEXT("\\u0000")));
    }
    TestTrue(TEXT("Leading value NUL preserved"),
        Metadata._Children[0]._Value.Len() == 2 && Metadata._Children[0]._Value[0] == 0);
    TestTrue(TEXT("Interior value NUL preserved"),
        Metadata._Children[1]._Value.Len() == 3 && Metadata._Children[1]._Value[1] == 0);
    TestTrue(TEXT("Trailing value NUL preserved"),
        Metadata._Children[2]._Value.Len() == 2 && Metadata._Children[2]._Value[1] == 0);
    TestTrue(TEXT("Case-distinct keys remain distinct"),
        TryParse(TEXT("{\"a\":1,\"A\":2}"), Root, Error));
    for (const auto& Literal : {FString{TEXT("TRUE")}, FString{TEXT("False")}, FString{TEXT("NULL")}})
    {
        TestFalse(TEXT("JSON literals are case sensitive"), TryParse(Literal, Root, Error));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_JsonGrammarAtomic,
    "Ck.PixelSprite.Json.GrammarAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_JsonGrammarAtomic::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_pixel_sprite_json_tests;
    using ck::pixel_sprite::json_private::TryParse;

    auto RawControl = FString{TEXT("{\"x\":\"a")};
    RawControl.AppendChar(TEXT('\n'));
    RawControl += TEXT("b\"}");
    const auto Invalid = TArray<FString>{
        TEXT(""), TEXT("  "), TEXT("{\"a\":"), TEXT("{\"a\":1,}"),
        TEXT("[1,]"), TEXT("01"), TEXT("-"), TEXT("1."), TEXT("1e+"),
        TEXT("true false"), TEXT("null trailing"), TEXT("{\"x\":\"\\q\"}"),
        TEXT("{\"x\":\"\\u12G4\"}"), TEXT("{\"x\":\"\\uD800\"}"),
        TEXT("{\"x\":\"\\uDC00\"}"), TEXT("{\"x\":\"\\uD800\\u0041\"}"),
        RawControl};
    for (const auto& Text : Invalid)
    {
        auto Root = FNode{};
        Root._Kind = EKind::Number;
        Root._Value = TEXT("17");
        Root._Start = 41;
        Root._End = 43;
        auto Error = FCk_PixelSprite_Diagnostic{};
        TestFalse(TEXT("Malformed JSON rejected"), TryParse(Text, Root, Error));
        TestTrue(TEXT("Parse failure is atomic"),
            Root._Kind == EKind::Number && Root._Value == TEXT("17") &&
            Root._Start == 41 && Root._End == 43);
        TestTrue(TEXT("Structured JSON diagnostic"),
            Error._Code == TEXT("json") && Error._Line >= 1 && Error._Column >= 1);
    }
    auto Root = FNode{};
    auto Error = FCk_PixelSprite_Diagnostic{TEXT("old"), TEXT("old"), TEXT("old"), 9, 9};
    TestTrue(TEXT("Valid JSON accepts"), TryParse(TEXT(" { \"x\" : [true,null] } "), Root, Error));
    TestTrue(TEXT("Success clears stale diagnostic"), Error._Code.IsEmpty());
    TestTrue(TEXT("Root span excludes outer whitespace"),
        Root._Start == 1 && Root._End == 22);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_JsonLimits,
    "Ck.PixelSprite.Json.Limits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_JsonLimits::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_pixel_sprite_json_tests;
    using ck::pixel_sprite::json_private::TryParse;

    auto Root = FNode{};
    auto Error = FCk_PixelSprite_Diagnostic{};
    TestTrue(TEXT("64 nested empty arrays accepted"),
        TryParse(NestedArrays(64), Root, Error));
    Root._Kind = EKind::String;
    Root._Value = TEXT("sentinel");
    TestFalse(TEXT("Depth 65 rejected"),
        TryParse(NestedArrays(65), Root, Error));
    TestEqual(TEXT("Depth limit code"), Error._Code, FString{TEXT("limits")});
    TestTrue(TEXT("Depth failure preserves root"),
        Root._Kind == EKind::String && Root._Value == TEXT("sentinel"));

    // Array root counts as one node; each scalar element counts as one.
    // The exact boundary is therefore 99,999 elements.
    TestTrue(TEXT("100000 nodes accepted"),
        TryParse(FlatArray(99999), Root, Error));
    Root._Kind = EKind::String;
    Root._Value = TEXT("sentinel");
    TestFalse(TEXT("100001 nodes rejected"),
        TryParse(FlatArray(100000), Root, Error));
    TestEqual(TEXT("Node limit code"), Error._Code, FString{TEXT("limits")});
    TestTrue(TEXT("Node failure preserves root"),
        Root._Kind == EKind::String && Root._Value == TEXT("sentinel"));
    TestFalse(TEXT("Object keys also count toward node budget"),
        TryParse(FlatObject(50000), Root, Error));
    TestEqual(TEXT("Object key budget code"), Error._Code, FString{TEXT("limits")});
    TestTrue(TEXT("Object budget failure preserves root"),
        Root._Kind == EKind::String && Root._Value == TEXT("sentinel"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_JsonNumbers,
    "Ck.PixelSprite.Json.Numbers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_JsonNumbers::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_pixel_sprite_json_tests;
    using ck::pixel_sprite::json_private::TryGetFiniteNumber;
    using ck::pixel_sprite::json_private::TryParse;

    for (const auto& Lexeme : {FString{TEXT("0e999999")}, FString{TEXT("1e-999999")},
         FString{TEXT("5e-324")}, FString{TEXT("1.7976931348623157e308")}})
    {
        auto Root = FNode{};
        auto Error = FCk_PixelSprite_Diagnostic{};
        TestTrue(TEXT("Finite JSON float accepted"), TryParse(Lexeme, Root, Error));
        TestTrue(TEXT("Raw float lexeme preserved"),
            Root._Kind == EKind::Number && Root._Value == Lexeme);
        auto Number = double{17.0};
        TestTrue(TEXT("Finite helper accepts"), TryGetFiniteNumber(Lexeme, Number));
        TestTrue(TEXT("Converted value finite"), FMath::IsFinite(Number));
    }
    auto Number = double{17.0};
    TestTrue(TEXT("Zero with huge exponent"), TryGetFiniteNumber(TEXT("0e999999"), Number));
    TestEqual(TEXT("Exact zero"), Number, 0.0);
    TestTrue(TEXT("Maximum finite double"), TryGetFiniteNumber(
        TEXT("1.7976931348623157e308"), Number));
    TestEqual(TEXT("DBL_MAX"), Number, DBL_MAX);
    Number = 17.0;
    TestFalse(TEXT("Float overflow rejects"), TryGetFiniteNumber(TEXT("1.8e308"), Number));
    TestEqual(TEXT("Numeric conversion failure atomic"), Number, 17.0);
    auto Root = FNode{};
    Root._Kind = EKind::String;
    Root._Value = TEXT("sentinel");
    auto Error = FCk_PixelSprite_Diagnostic{};
    TestFalse(TEXT("Overflow JSON lexeme rejected"), TryParse(TEXT("1.8e308"), Root, Error));
    TestEqual(TEXT("Overflow error code"), Error._Code, FString{TEXT("json")});
    TestTrue(TEXT("Overflow parse atomic"),
        Root._Kind == EKind::String && Root._Value == TEXT("sentinel"));

    // This stresses a long mantissa canceled by the exponent. It is finite
    // and should convert to 1.0, even when the mantissa alone exceeds double.
    auto Cancellation = FString{TEXT("1")};
    for (auto Index = int32{0}; Index < 1024; ++Index)
    {
        Cancellation.AppendChar(TEXT('0'));
    }
    Cancellation += TEXT("e-1024");
    TestTrue(TEXT("Canceled exponent JSON accepts"), TryParse(Cancellation, Root, Error));
    Number = 17.0;
    TestTrue(TEXT("Canceled exponent numeric helper accepts"),
        TryGetFiniteNumber(Cancellation, Number));
    TestEqual(TEXT("Canceled exponent value"), Number, 1.0);

    const auto HugeInteger = FString{
        TEXT("12345678901234567890123456789012345678901234567890")};
    TestTrue(TEXT("Integer metadata token accepts without double conversion"),
        TryParse(HugeInteger, Root, Error));
    TestTrue(TEXT("Integer raw lexeme stays exact"),
        Root._Kind == EKind::Number && Root._Value == HugeInteger);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
