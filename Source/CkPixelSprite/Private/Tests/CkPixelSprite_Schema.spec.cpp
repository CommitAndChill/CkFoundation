// Pure schema tests use the production JSON recognizer and schema parser.

#include "CkPixelSprite_Schema.h"
#include "CkPixelSprite_Json.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_sprite_schema_tests
{
    auto
        TrySchema(
            const FString& InText,
            FCk_PixelSprite_Manifest& OutManifest,
            FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
        auto Root = ck::pixel_sprite::json_private::FNode{};
        if (NOT ck::pixel_sprite::json_private::TryParse(InText, Root, OutDiagnostic))
        {
            return false;
        }
        auto Schema = ck::pixel_sprite::manifest_private::FSchema{
            InText, TEXT("schema-test.json"), OutDiagnostic};
        return Schema.Parse(Root, OutManifest);
    }

    auto
        Frame(
            const FString& InDuration = TEXT("1"),
            const FString& InSource = TEXT("frame.xpm"), const FString& InExtra = TEXT(""))
        -> FString
    {
        return TEXT("{\"source\":\"") + InSource + TEXT("\",\"duration_ms\":") +
            InDuration + InExtra + TEXT("}");
    }

    auto
        Frames(
            int32 InCount)
        -> FString
    {
        auto Text = FString{TEXT("[")};
        const auto Value = Frame();
        Text.Reserve(InCount * (Value.Len() + 1) + 2);
        for (auto Index = int32{0}; Index < InCount; ++Index)
        {
            if (Index > 0)
            {
                Text.AppendChar(TEXT(','));
            }
            Text += Value;
        }
        Text.AppendChar(TEXT(']'));
        return Text;
    }

    auto
        Animation(
            const FString& InFrames = Frames(1),
            const FString& InPlayback = TEXT("loop"), const FString& InExtra = TEXT(""))
        -> FString
    {
        return TEXT("{\"playback\":\"") + InPlayback + TEXT("\",\"frames\":") +
            InFrames + InExtra + TEXT("}");
    }

    auto
        Animations(
            int32 InCount)
        -> FString
    {
        auto Text = FString{TEXT("{")};
        const auto Value = Animation();
        for (auto Index = int32{0}; Index < InCount; ++Index)
        {
            if (Index > 0)
            {
                Text.AppendChar(TEXT(','));
            }
            Text += TEXT("\"a");
            Text += FString::FromInt(Index);
            Text += TEXT("\":");
            Text += Value;
        }
        Text.AppendChar(TEXT('}'));
        return Text;
    }

    auto
        Repeated(
            TCHAR InCharacter,
            int32 InCount)
        -> FString
    {
        auto Text = FString{};
        Text.Reserve(InCount);
        for (auto Index = int32{0}; Index < InCount; ++Index)
        {
            Text.AppendChar(InCharacter);
        }
        return Text;
    }

    auto
        Anchors(
            int32 InCount)
        -> FString
    {
        auto Text = FString{TEXT("{")};
        for (auto Index = int32{0}; Index < InCount; ++Index)
        {
            if (Index > 0)
            {
                Text.AppendChar(TEXT(','));
            }
            Text += TEXT("\"a");
            Text += FString::FromInt(Index);
            Text += TEXT("\":[0,0]");
        }
        Text.AppendChar(TEXT('}'));
        return Text;
    }

    auto
        Manifest(
            const FString& InVersion = TEXT("1"),
            const FString& InCanvas = TEXT("{\"width\":2,\"height\":2}"),
            const FString& InAnimations = TEXT("{\"idle\":{\"playback\":\"loop\",\"frames\":[{\"source\":\"frame.xpm\",\"duration_ms\":1}]}}"),
            const FString& InExtra = TEXT(""))
        -> FString
    {
        return TEXT("{\"version\":") + InVersion + TEXT(",\"canvas\":") +
            InCanvas + TEXT(",\"animations\":") + InAnimations + InExtra + TEXT("}");
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_SchemaFields,
    "Ck.PixelSprite.Schema.Fields",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_SchemaFields::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_pixel_sprite_schema_tests;
    const auto Reject = [this](const FString& InText, const FString& InCode) -> void
    {
        auto Value = FCk_PixelSprite_Manifest{};
        Value._Width = 17;
        Value._EventsJson = TEXT("sentinel");
        auto Error = FCk_PixelSprite_Diagnostic{};
        TestFalse(TEXT("Invalid schema rejected"), TrySchema(InText, Value, Error));
        TestEqual(TEXT("Expected diagnostic code"), Error._Code, InCode);
        TestTrue(TEXT("Schema rejection preserves caller value"),
            Value._Width == 17 && Value._EventsJson == TEXT("sentinel"));
    };

    Reject(TEXT("{}"), TEXT("schema"));
    Reject(TEXT("{\"version\":1,\"canvas\":{\"width\":2,\"height\":2}}"), TEXT("schema"));
    Reject(Manifest(TEXT("true")), TEXT("version"));
    Reject(Manifest(TEXT("1.0")), TEXT("version"));
    Reject(Manifest(TEXT("1e0")), TEXT("version"));
    Reject(Manifest(TEXT("\"1\"")), TEXT("version"));
    Reject(Manifest(TEXT("2")), TEXT("version"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":1.0,\"height\":2}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":1e0,\"height\":2}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":true,\"height\":2}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2,\"extra\":1}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":{\"playback\":\"loop\",\"frames\":[]}}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":{\"playback\":\"loop\",\"frames\":[{\"source\":\"frame.xpm\",\"duration_ms\":1,\"unknown\":0}]}}")),
        TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":{\"playback\":\"loop\",\"frames\":[{\"source\":\"frame.xpm\",\"duration_ms\":1,\"layers\":[]}]}}")),
        TEXT("layers_unsupported"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":{\"playback\":\"loop\",\"frames\":[{\"source\":\"frame.xpm\",\"duration_ms\":1}]}}"),
        TEXT(",\"unknown\":true")), TEXT("schema"));
    Reject(TEXT("{\"Version\":1,\"canvas\":{\"width\":2,\"height\":2},\"animations\":{}}"), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"Width\":2,\"height\":2}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":{\"Playback\":\"loop\",\"frames\":[{\"source\":\"frame.xpm\",\"duration_ms\":1}]}}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":{\"playback\":\"Loop\",\"frames\":[{\"source\":\"frame.xpm\",\"duration_ms\":1}]}}")), TEXT("playback"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":{\"playback\":\"loop\",\"frames\":[{\"Source\":\"frame.xpm\",\"duration_ms\":1}]}}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":{\"playback\":\"loop\",\"frames\":[{\"source\":\"frame.xpm\",\"duration_ms\":1,\"Anchors\":{}}]}}")), TEXT("schema"));

    auto Value = FCk_PixelSprite_Manifest{};
    auto Error = FCk_PixelSprite_Diagnostic{};
    TestTrue(TEXT("Minimal schema accepts"), TrySchema(Manifest(), Value, Error));
    TestTrue(TEXT("Defaults and ordered animation retained"),
        Value._Width == 2 && Value._Height == 2 &&
        Value._Animations.Num() == 1 && Value._Animations[0]._Name == TEXT("idle") &&
        Value._EventsJson == TEXT("[]") && Value._MetadataJson == TEXT("{}"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_SchemaBounds,
    "Ck.PixelSprite.Schema.Bounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_SchemaBounds::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_pixel_sprite_schema_tests;
    const auto Check = [this](const FString& InText, bool InExpected,
        const FString& InCode = TEXT("")) -> void
    {
        auto Value = FCk_PixelSprite_Manifest{};
        Value._Width = 17;
        auto Error = FCk_PixelSprite_Diagnostic{};
        const auto Accepted = TrySchema(InText, Value, Error);
        TestEqual(TEXT("Schema acceptance"), Accepted, InExpected);
        if (NOT InExpected)
        {
            TestEqual(TEXT("Schema limit diagnostic"), Error._Code, InCode);
            TestEqual(TEXT("Failure keeps sentinel width"), Value._Width, 17);
        }
    };

    Check(Manifest(TEXT("1"), TEXT("{\"width\":4096,\"height\":256}")), true);
    Check(Manifest(TEXT("1"), TEXT("{\"width\":4096,\"height\":257}")), false, TEXT("limits"));
    Check(Manifest(TEXT("1"), TEXT("{\"width\":4097,\"height\":1}")), false, TEXT("schema"));
    Check(Manifest(TEXT("1"), TEXT("{\"width\":0,\"height\":1}")), false, TEXT("schema"));
    Check(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"), TEXT("{}")), false, TEXT("limits"));
    Check(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"), Animations(1024)), true);
    Check(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"), Animations(1025)), false, TEXT("limits"));

    const auto OneFrame = Frame();
    Check(Manifest(TEXT("1"), TEXT("{\"width\":64,\"height\":64}"),
        TEXT("{\"idle\":") + Animation(Frames(4096)) + TEXT("}")), true);
    Check(Manifest(TEXT("1"), TEXT("{\"width\":64,\"height\":64}"),
        TEXT("{\"idle\":") + Animation(Frames(4097)) + TEXT("}")), false, TEXT("limits"));
    // 1024*1024 canvas: sixteen frames exactly consume 16,777,216 pixels.
    Check(Manifest(TEXT("1"), TEXT("{\"width\":1024,\"height\":1024}"),
        TEXT("{\"idle\":") + Animation(Frames(16)) + TEXT("}")), true);
    Check(Manifest(TEXT("1"), TEXT("{\"width\":1024,\"height\":1024}"),
        TEXT("{\"idle\":") + Animation(Frames(17)) + TEXT("}")), false, TEXT("limits"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_SchemaDurationAnchors,
    "Ck.PixelSprite.Schema.DurationAnchors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_SchemaDurationAnchors::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_pixel_sprite_schema_tests;
    const auto WithFrame = [](const FString& InFrame) -> FString
    { return Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":") + Animation(TEXT("[") + InFrame + TEXT("]")) + TEXT("}")); };
    const auto Check = [this, &WithFrame](const FString& InFrame, bool InExpected,
        const FString& InCode = TEXT("")) -> void
    {
        auto Value = FCk_PixelSprite_Manifest{};
        Value._Width = 17;
        auto Error = FCk_PixelSprite_Diagnostic{};
        const auto Accepted = TrySchema(WithFrame(InFrame), Value, Error);
        TestEqual(TEXT("Duration/anchor acceptance"), Accepted, InExpected);
        if (NOT InExpected)
        {
            TestEqual(TEXT("Duration/anchor diagnostic"), Error._Code, InCode);
            TestEqual(TEXT("Failure preserves sentinel"), Value._Width, 17);
        }
    };
    Check(Frame(TEXT("0.001")), true);
    Check(Frame(TEXT("86400000")), true);
    Check(Frame(TEXT("0")), false, TEXT("duration"));
    Check(Frame(TEXT("-1")), false, TEXT("duration"));
    Check(Frame(TEXT("86400001")), false, TEXT("duration"));
    Check(Frame(TEXT("true")), false, TEXT("duration"));
    Check(Frame(TEXT("\"1\"")), false, TEXT("duration"));
    Check(Frame(TEXT("1e999")), false, TEXT("json")); // Tokenizer rejects overflow before schema.
    Check(Frame(TEXT("1"), TEXT("frame.xpm"), TEXT(",\"anchors\":{\"origin\":[-0,1]}")), true);
    Check(Frame(TEXT("1"), TEXT("frame.xpm"), TEXT(",\"anchors\":{\"origin\":[1.0,1]}")), false, TEXT("schema"));
    Check(Frame(TEXT("1"), TEXT("frame.xpm"), TEXT(",\"anchors\":{\"origin\":[1e0,1]}")), false, TEXT("schema"));
    Check(Frame(TEXT("1"), TEXT("frame.xpm"), TEXT(",\"anchors\":{\"origin\":[2,1]}")), false, TEXT("schema"));
    Check(Frame(TEXT("1"), TEXT("frame.xpm"), TEXT(",\"anchors\":{\"origin\":[0]}")), false, TEXT("anchors"));
    Check(Frame(TEXT("1"), TEXT("frame.xpm"), TEXT(",\"anchors\":{\"\\u0000hand\":[0,0]}")), false, TEXT("anchors"));
    Check(Frame(TEXT("1"), TEXT("frame.xpm"), TEXT(",\"anchors\":") + Anchors(256)), true);
    Check(Frame(TEXT("1"), TEXT("frame.xpm"), TEXT(",\"anchors\":") + Anchors(257)), false, TEXT("anchors"));
    Check(Frame(TEXT("1"), TEXT("frame\\u0000.xpm")), false, TEXT("source_path"));
    const auto DistinctAnchors = WithFrame(Frame(TEXT("1"), TEXT("frame.xpm"),
        TEXT(",\"anchors\":{\"hand\":[0,0],\"Hand\":[1,1]}")));
    auto Named = FCk_PixelSprite_Manifest{};
    auto NamedError = FCk_PixelSprite_Diagnostic{};
    if (TestTrue(TEXT("Case-distinct anchors accepted"), TrySchema(DistinctAnchors, Named, NamedError)) &&
        TestTrue(TEXT("One frame retained"), Named._Animations.Num() == 1 && Named._Animations[0]._Frames.Num() == 1))
    {
        const auto& AnchorMap = Named._Animations[0]._Frames[0]._Anchors;
        TestEqual(TEXT("Case-distinct anchors retained"), AnchorMap.Num(), 2);
        auto FoundLower = false;
        auto FoundUpper = false;
        for (const auto& Pair : AnchorMap)
        {
            if (Pair.Key.Equals(TEXT("hand"), ESearchCase::CaseSensitive))
            {
                FoundLower = Pair.Value == FIntPoint{0, 0};
            }
            if (Pair.Key.Equals(TEXT("Hand"), ESearchCase::CaseSensitive))
            {
                FoundUpper = Pair.Value == FIntPoint{1, 1};
            }
        }
        TestTrue(TEXT("Lowercase anchor coordinates"), FoundLower);
        TestTrue(TEXT("Uppercase anchor coordinates"), FoundUpper);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_SchemaExtras,
    "Ck.PixelSprite.Schema.Extras",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_SchemaExtras::
    RunTest(
        const FString&)
    -> bool
{
    using namespace ck_pixel_sprite_schema_tests;
    const auto RootMetadata = FString{
        TEXT("{ \"huge\" : 1234567890123456789012345678901234567890,")
        TEXT(" \"nested\" : [null,true,{\"\\u0000key\":\"\\u0000x\"}] }")};
    const auto RootEvents = FString{TEXT("[{\"name\":\"ready\",\"at\":1e-3}]")};
    const auto AnimationEvents = FString{TEXT("[{\"name\":\"step\",\"data\":[1,false]}]")};
    const auto FrameMetadata = FString{TEXT("{\"value\":1.234567890123456789e30,\"nil\":null}")};
    const auto Text = Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":") + Animation(
            TEXT("[") + Frame(TEXT("125"), TEXT("frame.xpm"),
                TEXT(",\"metadata\":") + FrameMetadata) + TEXT("]"),
            TEXT("loop"), TEXT(",\"events\":") + AnimationEvents) + TEXT("}"),
        TEXT(",\"metadata\":") + RootMetadata + TEXT(",\"events\":") + RootEvents);
    auto Value = FCk_PixelSprite_Manifest{};
    auto Error = FCk_PixelSprite_Diagnostic{};
    if (NOT TestTrue(TEXT("Nested typed extras accepted"), TrySchema(Text, Value, Error)))
    {
        return false;
    }
    if (NOT TestTrue(TEXT("Animation and frame present"),
        Value._Animations.Num() == 1 && Value._Animations[0]._Frames.Num() == 1))
    {
        return false;
    }
    TestEqual(TEXT("Root metadata raw lexemes and spacing"), Value._MetadataJson, RootMetadata);
    TestEqual(TEXT("Root events raw lexemes"), Value._EventsJson, RootEvents);
    TestEqual(TEXT("Animation events raw lexemes"), Value._Animations[0]._EventsJson, AnimationEvents);
    TestEqual(TEXT("Frame metadata raw lexemes"), Value._Animations[0]._Frames[0]._MetadataJson,
        FrameMetadata);

    const auto Reject = [this](const FString& InText, const FString& InCode) -> void
    {
        auto Candidate = FCk_PixelSprite_Manifest{};
        Candidate._Width = 17;
        auto Diagnostic = FCk_PixelSprite_Diagnostic{};
        TestFalse(TEXT("Structural NUL or bad name rejected"), TrySchema(InText, Candidate, Diagnostic));
        TestEqual(TEXT("Expected diagnostic"), Diagnostic._Code, InCode);
        TestEqual(TEXT("Rejection atomic"), Candidate._Width, 17);
    };
    Reject(TEXT("{\"ver\\u0000sion\":1,\"canvas\":{\"width\":2,\"height\":2},\"animations\":{}}"),
        TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"\\u0000idle\":") + Animation() + TEXT("}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"idle\":") + Animation(Frames(1), TEXT("lo\\u0000op")) + TEXT("}")), TEXT("playback"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"\":") + Animation() + TEXT("}")), TEXT("schema"));
    Reject(Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
        TEXT("{\"") + Repeated(TEXT('x'), 129) + TEXT("\":") + Animation() + TEXT("}")),
        TEXT("schema"));
    auto Named = FCk_PixelSprite_Manifest{};
    auto NameDiagnostic = FCk_PixelSprite_Diagnostic{};
    TestTrue(TEXT("128-character animation name accepted"), TrySchema(
        Manifest(TEXT("1"), TEXT("{\"width\":2,\"height\":2}"),
            TEXT("{\"") + Repeated(TEXT('x'), 128) + TEXT("\":") + Animation() + TEXT("}")),
        Named, NameDiagnostic));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
