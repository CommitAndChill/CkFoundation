#include "CkPixelSprite/CkPixelSprite_Data.h"
#include "CkCore/Macros/CkMacros.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Manifest,
    "Ck.PixelSprite.ManifestConformance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_Manifest::RunTest(const FString&) -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin is available"), Plugin.IsValid()))
    {
        return false;
    }
    const auto Folder = Plugin->GetBaseDir() / TEXT("Tools/CkPixel/Fixtures/Manifest");
    auto IndexText = FString{};
    if (NOT TestTrue(TEXT("Manifest index loads"), FFileHelper::LoadFileToString(IndexText, *(Folder / TEXT("conformance.json")))))
    {
        return false;
    }
    auto Index = TSharedPtr<FJsonObject>{};
    if (NOT TestTrue(TEXT("Manifest index decodes"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(IndexText), Index)))
    {
        return false;
    }
    for (const auto& Entry : Index->GetArrayField(TEXT("valid")))
    {
        const auto Object = Entry->AsObject();
        const auto Filename = Object->GetStringField(TEXT("file"));
        auto Manifest = FCk_PixelSprite_Manifest{};
        auto Diagnostic = FCk_PixelSprite_Diagnostic{};
        if (NOT TestTrue(Filename, ck::pixel_sprite::TryLoad_Manifest(Folder / Filename, Manifest, Diagnostic)))
        {
            AddError(Diagnostic.Get_Text());
            continue;
        }
        const auto& Canvas = Object->GetArrayField(TEXT("canvas"));
        TestEqual(TEXT("Canvas width"), Manifest._Width, static_cast<int32>(Canvas[0]->AsNumber()));
        TestEqual(TEXT("Canvas height"), Manifest._Height, static_cast<int32>(Canvas[1]->AsNumber()));
        TestEqual(TEXT("One grid per unique source"), Manifest._Sources.Num(), static_cast<int32>(Object->GetNumberField(TEXT("source_count"))));
        const auto& ExpectedAnimations = Object->GetArrayField(TEXT("animations"));
        if (NOT TestEqual(TEXT("Animation count"), Manifest._Animations.Num(), ExpectedAnimations.Num()))
        {
            continue;
        }
        for (auto AnimationIndex = int32{0}; AnimationIndex < Manifest._Animations.Num(); ++AnimationIndex)
        {
            const auto& Animation = Manifest._Animations[AnimationIndex];
            const auto Expected = ExpectedAnimations[AnimationIndex]->AsObject();
            TestEqual(TEXT("Animation order/name"), Animation._Name, Expected->GetStringField(TEXT("name")));
            const auto PlaybackName = Expected->GetStringField(TEXT("playback"));
            TestTrue(TEXT("Known fixture playback"), PlaybackName == TEXT("loop") ||
                PlaybackName == TEXT("once") || PlaybackName == TEXT("ping-pong"));
            const auto ExpectedPlayback = PlaybackName == TEXT("once") ? ECk_PixelSprite_Playback::Once
                : PlaybackName == TEXT("ping-pong") ? ECk_PixelSprite_Playback::PingPong
                : ECk_PixelSprite_Playback::Loop;
            TestEqual(TEXT("Playback mode"), Animation._Playback, ExpectedPlayback);
            const auto& Indices = Expected->GetArrayField(TEXT("source_indices"));
            if (NOT TestEqual(TEXT("Frame count"), Animation._Frames.Num(), Indices.Num()))
            {
                continue;
            }
            const auto& Durations = Expected->GetArrayField(TEXT("durations_ms"));
            const auto& Sources = Expected->GetArrayField(TEXT("frame_sources"));
            if (NOT TestEqual(TEXT("Source name count"), Sources.Num(), Animation._Frames.Num()) ||
                NOT TestEqual(TEXT("Duration count"), Durations.Num(), Animation._Frames.Num()))
            {
                continue;
            }
            const auto HasAnchors = Expected->HasField(TEXT("anchors"));
            if (HasAnchors && NOT TestEqual(TEXT("Anchor frame count"),
                Expected->GetArrayField(TEXT("anchors")).Num(), Animation._Frames.Num()))
            {
                continue;
            }
            for (auto FrameIndex = int32{0}; FrameIndex < Animation._Frames.Num(); ++FrameIndex)
            {
                const auto& Frame = Animation._Frames[FrameIndex];
                TestEqual(TEXT("Stable cached source index"), Frame._SourceIndex, static_cast<int32>(Indices[FrameIndex]->AsNumber()));
                TestEqual(TEXT("Frame source name"), Frame._Source, Sources[FrameIndex]->AsString());
                TestEqual(TEXT("Explicit authored duration"), Frame._DurationMilliseconds, Durations[FrameIndex]->AsNumber());
                if (HasAnchors)
                {
                    const auto ExpectedAnchors = Expected->GetArrayField(TEXT("anchors"))[FrameIndex]->AsObject();
                    TestEqual(TEXT("Anchor count"), Frame._Anchors.Num(), ExpectedAnchors->Values.Num());
                    for (const auto& Anchor : ExpectedAnchors->Values)
                    {
                        // UE 5.8 keys FJsonObject by UE::FSharedString; operator* is the TCHAR* of either key type
                        const auto AnchorName = FString{*Anchor.Key};
                        if (NOT TestTrue(TEXT("Anchor name"), Frame._Anchors.Contains(AnchorName)))
                        {
                            continue;
                        }
                        const auto& Coordinates = Anchor.Value->AsArray();
                        TestEqual(TEXT("Anchor position"), Frame._Anchors.FindChecked(AnchorName),
                            FIntPoint{static_cast<int32>(Coordinates[0]->AsNumber()), static_cast<int32>(Coordinates[1]->AsNumber())});
                    }
                }
            }
            for (const auto& Sample : Expected->GetArrayField(TEXT("samples")))
            {
                const auto& Values = Sample->AsArray();
                auto Selection = FCk_PixelSprite_FrameSelection{};
                const auto Label = FString::Printf(TEXT("%s/%s/%gms"), *Filename, *Animation._Name, Values[0]->AsNumber());
                if (TestTrue(Label + TEXT(" evaluates"), ck::pixel_sprite::TryEvaluate_AnimationMilliseconds(
                    Animation, Values[0]->AsNumber(), Selection)))
                {
                    TestEqual(Label + TEXT(" selects frame"), Selection._Index, static_cast<int32>(Values[1]->AsNumber()));
                }
            }
        }
        if (Filename == TEXT("valid-loop.json"))
        {
            TestTrue(TEXT("Huge integer extras preserve exact lexeme"), Manifest._MetadataJson.Contains(TEXT("1234567890123456789012345678901234567890")));
            TestTrue(TEXT("Nested extras preserve JSON types"), Manifest._MetadataJson.Contains(TEXT("true")) && Manifest._MetadataJson.Contains(TEXT("null")));
        }
        if (Filename == TEXT("valid-escaped-extras.json"))
        {
            TestTrue(TEXT("NUL-containing extras survive"), Manifest._MetadataJson.Contains(TEXT("\\u0000")));
        }
    }
    for (const auto& Entry : Index->GetArrayField(TEXT("invalid")))
    {
        const auto Object = Entry->AsObject();
        auto Manifest = FCk_PixelSprite_Manifest{};
        Manifest._Width = 79;
        Manifest._MetadataJson = TEXT("sentinel");
        auto Diagnostic = FCk_PixelSprite_Diagnostic{};
        TestFalse(Object->GetStringField(TEXT("file")), ck::pixel_sprite::TryLoad_Manifest(
            Folder / Object->GetStringField(TEXT("file")), Manifest, Diagnostic));
        TestEqual(TEXT("Structured failure code"), Diagnostic._Code, Object->GetStringField(TEXT("code")));
        TestEqual(TEXT("Rejected manifest width unchanged"), Manifest._Width, 79);
        TestEqual(TEXT("Rejected extras unchanged"), Manifest._MetadataJson, FString{TEXT("sentinel")});
        TestTrue(TEXT("No partially loaded source table"), Manifest._Sources.IsEmpty());
        TestTrue(TEXT("No runnable animation subset"), Manifest._Animations.IsEmpty());
    }
    return true;
}


#endif
