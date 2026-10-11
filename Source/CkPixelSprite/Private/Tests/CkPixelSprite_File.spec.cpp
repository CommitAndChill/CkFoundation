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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_NativeFileGate,
    "Ck.PixelSprite.NativeFileGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_NativeFileGate::RunTest(const FString&) -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin available"), Plugin.IsValid()))
    {
        return false;
    }

    const auto SavedIndexPath = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("PixelSpriteNativeFixtures/native-gate-v1/index.json"));
    const auto CheckedInIndexPath = FPaths::Combine(
        Plugin->GetBaseDir(), TEXT("Tools/CkPixel/Fixtures/NativeFile/conformance.json"));
    auto Indices = TArray<FString>{CheckedInIndexPath};
    if (FPaths::FileExists(SavedIndexPath))
    {
        Indices.Add(SavedIndexPath);
    }
    for (const auto& IndexPath : Indices)
    {
        AddInfo(TEXT("Native gate fixture index: ") + IndexPath);
        auto IndexText = FString{};
        if (NOT TestTrue(TEXT("Native gate fixture index loads"), FFileHelper::LoadFileToString(IndexText, *IndexPath)))
        {
            return false;
        }
        auto Index = TSharedPtr<FJsonObject>{};
        if (NOT TestTrue(TEXT("Native gate fixture index parses"),
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(IndexText), Index) && Index.IsValid()))
        {
            return false;
        }
        if (NOT TestTrue(TEXT("Native gate fixture index has cases"),
            Index->HasTypedField<EJson::Array>(TEXT("cases"))))
        {
            return false;
        }

        const auto& Cases = Index->GetArrayField(TEXT("cases"));
        if (NOT TestEqual(TEXT("Known native gate case count"), Cases.Num(), 12))
        {
            return false;
        }
        auto Seen = TSet<FString>{};
        auto ExecutedNames = TSet<FString>{};
        auto Executed = int32{0};
        auto Skipped = int32{0};
        for (const auto& Value : Cases)
        {
            if (NOT TestTrue(TEXT("Native gate case is an object"), Value.IsValid() && Value->Type == EJson::Object))
            {
                return false;
            }
            const auto Case = Value->AsObject();
            auto Name = FString{};
            auto ManifestPath = FString{};
            auto ExpectedValid = false;
            auto Supported = false;
            auto SkipReason = FString{};
            if (NOT TestTrue(TEXT("Native gate case fields"),
                Case->TryGetStringField(TEXT("name"), Name) &&
                Case->TryGetStringField(TEXT("manifest"), ManifestPath) &&
                Case->TryGetBoolField(TEXT("expected_valid"), ExpectedValid) &&
                Case->TryGetBoolField(TEXT("supported"), Supported) &&
                Case->TryGetStringField(TEXT("skip_reason"), SkipReason)))
            {
                return false;
            }
            if (NOT TestTrue(Name + TEXT(" unique case name"), NOT Seen.Contains(Name)))
            {
                return false;
            }
            Seen.Add(Name);

            // The Saved index uses absolute paths; the checked-in index is portable.
            if (FPaths::IsRelative(ManifestPath))
            {
                ManifestPath = FPaths::Combine(FPaths::GetPath(IndexPath), ManifestPath);
            }

            if (NOT Supported)
            {
                ++Skipped;
                AddWarning(FString::Printf(TEXT("Native gate fixture skipped: %s (%s)"), *Name, *SkipReason));
                continue;
            }

            ++Executed;
            ExecutedNames.Add(Name);
            AddInfo(FString::Printf(TEXT("Native gate fixture executed: %s"), *Name));
            auto Manifest = FCk_PixelSprite_Manifest{};
            Manifest._Width = 79;
            Manifest._Height = 83;
            Manifest._MetadataJson = TEXT("sentinel-metadata");
            Manifest._EventsJson = TEXT("sentinel-events");
            auto SourceSentinel = FCk_PixelSprite_Source{};
            SourceSentinel._Path = TEXT("sentinel-source");
            Manifest._Sources.Add(MoveTemp(SourceSentinel));
            auto AnimationSentinel = FCk_PixelSprite_Animation{};
            AnimationSentinel._Name = TEXT("sentinel-animation");
            Manifest._Animations.Add(MoveTemp(AnimationSentinel));
            auto Diagnostic = FCk_PixelSprite_Diagnostic{};
            const auto Loaded = ck::pixel_sprite::TryLoad_Manifest(ManifestPath, Manifest, Diagnostic);
            TestEqual(Name + TEXT(" acceptance"), Loaded, ExpectedValid);
            if (ExpectedValid)
            {
                if (NOT Loaded)
                {
                    AddError(Diagnostic.Get_Text());
                    continue;
                }
                TestEqual(Name + TEXT(" width"), Manifest._Width, 2);
                TestEqual(Name + TEXT(" height"), Manifest._Height, 2);
                TestEqual(Name + TEXT(" source count"), Manifest._Sources.Num(), 2);
                TestEqual(Name + TEXT(" animation count"), Manifest._Animations.Num(), 1);
                if (Manifest._Animations.Num() == 1)
                {
                    TestEqual(Name + TEXT(" animation name"), Manifest._Animations[0]._Name, FString{TEXT("idle")});
                    TestEqual(Name + TEXT(" frame count"), Manifest._Animations[0]._Frames.Num(), 2);
                    if (Manifest._Animations[0]._Frames.Num() == 2)
                    {
                        TestEqual(Name + TEXT(" first source index"), Manifest._Animations[0]._Frames[0]._SourceIndex, 0);
                        TestEqual(Name + TEXT(" second source index"), Manifest._Animations[0]._Frames[1]._SourceIndex, 1);
                    }
                }
                TestTrue(Name + TEXT(" diagnostic cleared"), Diagnostic._Code.IsEmpty() && Diagnostic._Message.IsEmpty());
                continue;
            }

            auto ExpectedCode = FString{};
            if (NOT TestTrue(Name + TEXT(" expected code present"),
                Case->TryGetStringField(TEXT("expected_code"), ExpectedCode)))
            {
                return false;
            }
            TestEqual(Name + TEXT(" diagnostic code"), Diagnostic._Code, ExpectedCode);
            TestTrue(Name + TEXT(" diagnostic source"), NOT Diagnostic._Source.IsEmpty());
            TestEqual(Name + TEXT(" sentinel width"), Manifest._Width, 79);
            TestEqual(Name + TEXT(" sentinel height"), Manifest._Height, 83);
            TestEqual(Name + TEXT(" sentinel metadata"), Manifest._MetadataJson, FString{TEXT("sentinel-metadata")});
            TestEqual(Name + TEXT(" sentinel events"), Manifest._EventsJson, FString{TEXT("sentinel-events")});
            TestEqual(Name + TEXT(" sentinel source count"), Manifest._Sources.Num(), 1);
            if (Manifest._Sources.Num() == 1)
            {
                TestEqual(Name + TEXT(" sentinel source"), Manifest._Sources[0]._Path, FString{TEXT("sentinel-source")});
            }
            TestEqual(Name + TEXT(" sentinel animation count"), Manifest._Animations.Num(), 1);
            if (Manifest._Animations.Num() == 1)
            {
                TestEqual(Name + TEXT(" sentinel animation"), Manifest._Animations[0]._Name, FString{TEXT("sentinel-animation")});
            }
        }

        for (const auto& Required : {FString{TEXT("ordinary_valid")},
            FString{TEXT("late_missing_second_source")}, FString{TEXT("dimension_mismatch")},
            FString{TEXT("utf8_bom_manifest")}, FString{TEXT("malformed_utf8_manifest")},
            FString{TEXT("raw_nul_json")}, FString{TEXT("malformed_utf8_xpm")},
            FString{TEXT("directory_link")}, FString{TEXT("file_link")}, FString{TEXT("linked_root")},
            FString{TEXT("junction_inside")}, FString{TEXT("junction_outside")}})
        {
            TestTrue(Required + TEXT(" indexed"), Seen.Contains(Required));
        }
        for (const auto& Required : {FString{TEXT("ordinary_valid")},
            FString{TEXT("late_missing_second_source")}, FString{TEXT("dimension_mismatch")},
            FString{TEXT("utf8_bom_manifest")}, FString{TEXT("malformed_utf8_manifest")},
            FString{TEXT("raw_nul_json")}, FString{TEXT("malformed_utf8_xpm")}})
        {
            TestTrue(Required + TEXT(" executed"), ExecutedNames.Contains(Required));
        }
        TestEqual(TEXT("Every indexed case executed or explicitly skipped"), Executed + Skipped, Cases.Num());
        AddInfo(FString::Printf(TEXT("Native gate: %d executed, %d skipped of %d indexed fixtures"),
            Executed, Skipped, Cases.Num()));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
