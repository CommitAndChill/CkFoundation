#include "CkPixelSprite/CkPixelSprite_Data.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_sprite_tests
{
    auto
        MakeXpm(
            const FString& InBody)
        -> FString
    {
        return TEXT("/* XPM */\nstatic const char *sprite[] = {\n") + InBody + TEXT("\n};");
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Parse,
    "Ck.PixelSprite.Parse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_Parse::RunTest(const FString&) -> bool
{
    auto Grid = FCk_PixelSprite_Grid{};
    auto Error = FCk_PixelSprite_Diagnostic{};
    const auto Source = ck_pixel_sprite_tests::MakeXpm(
        TEXT("\"3 2 3 1 1 1\",\n\"  c None\",\n\"K c #343743\",\n\"Y s gold c #F5BD46\",\n\" KY\",\n\"YK \""));
    TestTrue(TEXT("Valid whitespace/palette/hotspot"), ck::pixel_sprite::TryParse_Xpm(Source, TEXT("test.xpm"), Grid, Error));
    TestEqual(TEXT("Width"), Grid._Width, 3);
    TestEqual(TEXT("Pixel count"), Grid._Indices.Num(), 6);
    if (Grid._Palette.Num() == 3 && Grid._Indices.Num() == 6)
    {
        TestEqual(TEXT("Transparent symbol is a space"), Grid._Palette[0]._Symbol, FString{TEXT(" ")});
        TestEqual(TEXT("Transparency"), Grid._Palette[0]._Color.A, uint8{0});
        TestEqual(TEXT("Exact RGB"), Grid._Palette[2]._Color, FColor{245, 189, 70});
        TestEqual(TEXT("Trailing whitespace survives"), Grid._Indices[5], uint16{0});
    }
    TestTrue(TEXT("Hotspot present"), Grid._Hotspot.IsSet());
    const auto Escaped = ck_pixel_sprite_tests::MakeXpm(
        TEXT("\"2 1 2 2\",\"\\\\\\\" c #fff\",\"Aa c blue\",\"\\\\\\\"Aa\""));
    TestTrue(TEXT("Escapes and cpp2"), ck::pixel_sprite::TryParse_Xpm(Escaped, TEXT("escaped.xpm"), Grid, Error));
    TestEqual(TEXT("Two pixels"), Grid._Indices.Num(), 2);
    const auto CaseDistinct = ck_pixel_sprite_tests::MakeXpm(
        TEXT("\"2 1 2 1\",\"A c red\",\"a c BLUE\",\"Aa\""));
    TestTrue(TEXT("Case-distinct palette symbols parse"),
        ck::pixel_sprite::TryParse_Xpm(CaseDistinct, TEXT("case-distinct.xpm"), Grid, Error));
    TestEqual(TEXT("Case-distinct palette count"), Grid._Palette.Num(), 2);
    TestEqual(TEXT("Case-distinct pixel count"), Grid._Indices.Num(), 2);
    if (Grid._Palette.Num() == 2 && Grid._Indices.Num() == 2)
    {
        TestTrue(TEXT("Uppercase symbol preserved"), Grid._Palette[0]._Symbol.Equals(TEXT("A"), ESearchCase::CaseSensitive));
        TestTrue(TEXT("Lowercase symbol preserved"), Grid._Palette[1]._Symbol.Equals(TEXT("a"), ESearchCase::CaseSensitive));
        TestEqual(TEXT("Uppercase pixel index"), Grid._Indices[0], uint16{0});
        TestEqual(TEXT("Lowercase pixel index"), Grid._Indices[1], uint16{1});
        TestEqual(TEXT("Uppercase symbol color"), Grid._Palette[0]._Color, FColor{255, 0, 0});
        TestEqual(TEXT("Named color remains case-insensitive"), Grid._Palette[1]._Color, FColor{0, 0, 255});
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Rejection,
    "Ck.PixelSprite.RejectionAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_Rejection::RunTest(const FString&) -> bool
{
    const auto Bodies = TArray<FString>{
        TEXT("\"0 1 1 1\",\". c None\",\".\""),
        TEXT("\"9999999999 1 1 1\",\". c None\",\".\""),
        TEXT("\"4096 4096 1 1\",\". c None\",\".\""),
        TEXT("\"2 1 1 1\",\". c None\",\".\""),
        TEXT("\"1 1 1 1\",\". c None\",\"x\""),
        TEXT("\"1 1 1 1\",\". c #ggg\",\".\""),
        TEXT("\"1 1 2 1\",\". c None\",\". c red\",\".\""),
        TEXT("\"1 1 1 1 XPMEXT\",\". c red\",\".\""),
        TEXT("\"1 1 1 1\",\". c red\",\"\\x100\""),
        TEXT("\"1 1 1 1\",\". c red\",\"\\q\""),
        TEXT("\"1 1 1 1\" \"bad\",\". c red\",\".\""),
        TEXT("\"1 1 1 1\",\". c red\"")};
    for (const auto& Body : Bodies)
    {
        auto Grid = FCk_PixelSprite_Grid{};
        Grid._Width = 17;
        Grid._Indices.Add(42);
        auto Error = FCk_PixelSprite_Diagnostic{};
        TestFalse(TEXT("Malformed input rejected"), ck::pixel_sprite::TryParse_Xpm(
            ck_pixel_sprite_tests::MakeXpm(Body), TEXT("bad.xpm"), Grid, Error));
        TestEqual(TEXT("Rejected parse preserves width"), Grid._Width, 17);
        TestEqual(TEXT("Rejected parse preserves indices"), Grid._Indices.Num(), 1);
        TestTrue(TEXT("Actionable diagnostic"), NOT Error._Message.IsEmpty() && Error._Line > 0 && Error._Source == TEXT("bad.xpm"));
    }
    auto Grid = FCk_PixelSprite_Grid{};
    auto Error = FCk_PixelSprite_Diagnostic{};
    TestFalse(TEXT("Quoted qualifier rejected"), ck::pixel_sprite::TryParse_Xpm(
        TEXT("\"static\" char *a[]={\"1 1 1 1\",\"a c #000\",\"a\"};"), TEXT("quoted.xpm"), Grid, Error));
    auto Limits = FCk_PixelSprite_Limits{};
    Limits._Dimension = MAX_int32;
    Limits._Pixels = MAX_int32;
    TestFalse(TEXT("Custom limit count cannot overflow"), ck::pixel_sprite::TryParse_Xpm(
        TEXT("char *a[]={\"1 2147483647 1 1\"};"), TEXT("huge.xpm"), Grid, Error, Limits));
    const auto RejectCaseToken = [this](const FString& InSource, const FString& InExpectedCode) -> void
    {
        auto RejectedGrid = FCk_PixelSprite_Grid{};
        RejectedGrid._Width = 17;
        RejectedGrid._Indices.Add(42);
        auto RejectedError = FCk_PixelSprite_Diagnostic{};
        TestFalse(TEXT("Incorrectly cased XPM token rejected"), ck::pixel_sprite::TryParse_Xpm(
            InSource, TEXT("case-token.xpm"), RejectedGrid, RejectedError));
        TestEqual(TEXT("Incorrectly cased token diagnostic"), RejectedError._Code, InExpectedCode);
        TestEqual(TEXT("Rejected case token preserves width"), RejectedGrid._Width, 17);
        TestEqual(TEXT("Rejected case token preserves pixels"), RejectedGrid._Indices.Num(), 1);
        if (RejectedGrid._Indices.Num() == 1)
        {
            TestEqual(TEXT("Rejected case token preserves pixel value"), RejectedGrid._Indices[0], uint16{42});
        }
    };
    RejectCaseToken(TEXT("/* XPM */\nstatic CHAR *sprite[]={\"1 1 1 1\",\"A c red\",\"A\"};"), TEXT("syntax"));
    RejectCaseToken(TEXT("/* XPM */\nSTATIC char *sprite[]={\"1 1 1 1\",\"A c red\",\"A\"};"), TEXT("syntax"));
    RejectCaseToken(TEXT("/* XPM */\nstatic CONST char *sprite[]={\"1 1 1 1\",\"A c red\",\"A\"};"), TEXT("syntax"));
    RejectCaseToken(ck_pixel_sprite_tests::MakeXpm(TEXT("\"1 1 1 1\",\"A C red\",\"A\"")), TEXT("palette"));
    RejectCaseToken(ck_pixel_sprite_tests::MakeXpm(TEXT("\"1 1 1 1\",\"A c red\",\"a\"")), TEXT("symbol"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Timing,
    "Ck.PixelSprite.Timing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_Timing::RunTest(const FString&) -> bool
{
    auto Timing = FCk_PixelSprite_Timing{{FCk_Time{0.125}, FCk_Time{0.25}, FCk_Time{0.5}}};
    auto Selection = FCk_PixelSprite_FrameSelection{};
    const auto Expect = [this, &Timing, &Selection](double InTime, int32 InFrame) -> void
    {
        TestTrue(TEXT("Valid evaluation"), ck::pixel_sprite::TryEvaluate_Frame(Timing, FCk_Time{InTime}, Selection));
        TestEqual(TEXT("Expected frame"), Selection._Index, InFrame);
    };
    Expect(0.0, 0); Expect(0.125, 1); Expect(0.375, 2); Expect(0.875, 0);
    Expect(-0.01, 2); Expect(875000.125, 1);
    Timing._Playback = ECk_PixelSprite_Playback::PingPong;
    Expect(0.875, 1); Expect(1.125, 0); Expect(-0.01, 1);
    Timing._Playback = ECk_PixelSprite_Playback::Once;
    Expect(-5.0, 0); Expect(10.0, 2);
    TestTrue(TEXT("One shot completes"), Selection._Complete);
    auto Sampled = FCk_Time{};
    TestTrue(TEXT("Sample time"), ck::pixel_sprite::TrySample_Time(FCk_Time{0.249}, FCk_Time{0.125}, Sampled));
    TestEqual(TEXT("Presentation does not accelerate"), Sampled.Get_Seconds(), 0.125);
    Timing._Durations = {FCk_Time{0.125}};
    for (const auto Mode : {ECk_PixelSprite_Playback::Loop, ECk_PixelSprite_Playback::Once, ECk_PixelSprite_Playback::PingPong})
    {
        Timing._Playback = Mode;
        Expect(1234.0, 0);
    }
    Timing._Durations.Reset();
    Selection._Index = 17;
    TestFalse(TEXT("Empty rejects"), ck::pixel_sprite::TryEvaluate_Frame(Timing, FCk_Time{}, Selection));
    TestEqual(TEXT("Invalid timing leaves output intact"), Selection._Index, 17);
    Timing._Durations = {FCk_Time{0.0}};
    TestFalse(TEXT("Zero duration rejects"), ck::pixel_sprite::TryEvaluate_Frame(Timing, FCk_Time{}, Selection));
    Timing._Durations = {FCk_Time{0.2}, FCk_Time{0.5}, FCk_Time{0.3}};
    Timing._Playback = ECk_PixelSprite_Playback::Once;
    Expect(0.2 + 0.5, 2);
    TestEqual(TEXT("Cumulative boundary local time"), Selection._FrameTime.Get_Seconds(), 0.0);
    Timing._Durations = {FCk_Time{1e-10}, FCk_Time{1e-10}};
    Timing._Playback = ECk_PixelSprite_Playback::Loop;
    Expect(1.5e-10, 1);
    Timing._Durations = {FCk_Time{1.0}, FCk_Time{1.0}};
    Expect(-1e-30, 1);
    TestTrue(TEXT("Negative tiny time stays half-open"), Selection._FrameTime.Get_Seconds() < 1.0);
    Sampled = FCk_Time{17.0};
    TestFalse(TEXT("Sampling overflow rejects"), ck::pixel_sprite::TrySample_Time(FCk_Time{-1.7e308}, FCk_Time{1e308}, Sampled));
    TestEqual(TEXT("Sampling rejection is atomic"), Sampled.Get_Seconds(), 17.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Conformance,
    "Ck.PixelSprite.SharedConformance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_Conformance::RunTest(const FString&) -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin located"), Plugin.IsValid()))
    {
        return false;
    }
    const auto Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Tools/CkPixel/Fixtures"));
    auto Text = FString{};
    if (NOT TestTrue(TEXT("Conformance document loads"), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Root, TEXT("conformance.json")))))
    {
        return false;
    }
    auto Document = TSharedPtr<FJsonObject>{};
    if (NOT TestTrue(TEXT("Conformance JSON parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Document) && Document.IsValid()))
    {
        return false;
    }
    for (const auto Category : {FString{TEXT("success")}, FString{TEXT("failure")}})
    {
        if (NOT TestTrue(TEXT("Fixture array exists"), Document->HasTypedField<EJson::Array>(Category)))
        {
            return false;
        }
        for (const auto& Value : Document->GetArrayField(Category))
        {
            if (NOT TestTrue(TEXT("Fixture is an object"), Value.IsValid() && Value->Type == EJson::Object))
            {
                return false;
            }
            const auto Fixture = Value->AsObject();
            auto Name = FString{};
            if (NOT TestTrue(TEXT("Fixture filename exists"), Fixture->TryGetStringField(TEXT("file"), Name)) ||
                NOT TestTrue(TEXT("Fixture file loads"), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Root, Name))))
            {
                return false;
            }
            auto Grid = FCk_PixelSprite_Grid{};
            auto Error = FCk_PixelSprite_Diagnostic{};
            const auto Parsed = ck::pixel_sprite::TryParse_Xpm(Text, Name, Grid, Error);
            TestEqual(Name + TEXT(" acceptance"), Parsed, Category == TEXT("success"));
            if (Category == TEXT("failure"))
            {
                auto Code = FString{};
                if (NOT TestTrue(TEXT("Expected error code"), Fixture->TryGetStringField(TEXT("code"), Code)))
                {
                    return false;
                }
                TestEqual(Name + TEXT(" diagnostic"), Error._Code, Code);
                continue;
            }
            if (NOT Parsed || NOT TestTrue(TEXT("Expected grid exists"), Fixture->HasTypedField<EJson::Object>(TEXT("expected"))))
            {
                continue;
            }
            const auto Expected = Fixture->GetObjectField(TEXT("expected"));
            auto Width = 0.0;
            auto Height = 0.0;
            if (NOT TestTrue(TEXT("Expected dimensions"), Expected->TryGetNumberField(TEXT("width"), Width) && Expected->TryGetNumberField(TEXT("height"), Height)) ||
                NOT TestTrue(TEXT("Expected arrays"), Expected->HasTypedField<EJson::Array>(TEXT("indices")) && Expected->HasTypedField<EJson::Array>(TEXT("palette"))))
            {
                return false;
            }
            TestEqual(Name + TEXT(" width"), Grid._Width, static_cast<int32>(Width));
            TestEqual(Name + TEXT(" height"), Grid._Height, static_cast<int32>(Height));
            const auto& Indices = Expected->GetArrayField(TEXT("indices"));
            TestEqual(Name + TEXT(" indices count"), Grid._Indices.Num(), Indices.Num());
            for (auto Index = 0; Index < FMath::Min(Grid._Indices.Num(), Indices.Num()); ++Index)
            {
                TestEqual(Name + TEXT(" index"), Grid._Indices[Index], static_cast<uint16>(Indices[Index]->AsNumber()));
            }
            const auto& Palette = Expected->GetArrayField(TEXT("palette"));
            TestEqual(Name + TEXT(" palette count"), Grid._Palette.Num(), Palette.Num());
            for (auto Index = 0; Index < FMath::Min(Grid._Palette.Num(), Palette.Num()); ++Index)
            {
                if (NOT TestTrue(TEXT("Expected palette object"), Palette[Index].IsValid() && Palette[Index]->Type == EJson::Object))
                {
                    return false;
                }
                const auto Entry = Palette[Index]->AsObject();
                auto Symbol = FString{};
                if (NOT TestTrue(TEXT("Expected palette symbol"), Entry->TryGetStringField(TEXT("symbol"), Symbol)) ||
                    NOT TestTrue(TEXT("Expected RGBA"), Entry->HasTypedField<EJson::Array>(TEXT("rgba"))))
                {
                    return false;
                }
                const auto& Rgba = Entry->GetArrayField(TEXT("rgba"));
                if (NOT TestEqual(TEXT("RGBA channel count"), Rgba.Num(), 4))
                {
                    return false;
                }
                TestEqual(Name + TEXT(" symbol"), Grid._Palette[Index]._Symbol, Symbol);
                TestEqual(Name + TEXT(" RGBA"), Grid._Palette[Index]._Color, FColor{static_cast<uint8>(Rgba[0]->AsNumber()),
                    static_cast<uint8>(Rgba[1]->AsNumber()), static_cast<uint8>(Rgba[2]->AsNumber()), static_cast<uint8>(Rgba[3]->AsNumber())});
            }
        }
    }
    return true;
}

#endif
