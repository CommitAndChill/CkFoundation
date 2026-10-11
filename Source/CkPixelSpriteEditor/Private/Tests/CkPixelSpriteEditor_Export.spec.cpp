#include "CkPixelSpriteEditor/CkPixelSpriteEditor_Export.h"

#include "CkCore/Macros/CkMacros.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Export,
    "Ck.PixelSprite.Export.NativePngAndSheet", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_Export::
    RunTest(const FString&)
    -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin is available"), Plugin.IsValid()))
    {
        return false;
    }
    auto Manifest = FCk_PixelSprite_Manifest{};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    const auto Fixture = Plugin->GetBaseDir() / TEXT("Tools/CkPixel/Fixtures/Manifest/valid-loop.json");
    if (NOT TestTrue(TEXT("Animated fixture loads"),
        ck::pixel_sprite::TryLoad_Manifest(Fixture, Manifest, Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    if (NOT TestTrue(TEXT("Animated fixture enters asset"),
        Asset->Request_ReplaceManifest(Manifest, Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }

    const auto Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
        TEXT("PixelSpriteExportTests") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
    if (NOT TestTrue(TEXT("Create test-owned export folder"),
        IFileManager::Get().MakeDirectory(*Directory, true)))
    {
        return false;
    }
    const auto FramePng = Directory / TEXT("frame.png");
    const auto SheetPng = Directory / TEXT("sheet.png");
    const auto SheetJson = Directory / TEXT("sheet.json");
    ON_SCOPE_EXIT
    {
        IFileManager::Get().Delete(*FramePng, false, false, true);
        IFileManager::Get().Delete(*SheetPng, false, false, true);
        IFileManager::Get().Delete(*SheetJson, false, false, true);
        IFileManager::Get().DeleteDirectory(*Directory, false, false);
    };
    auto Settings = FCk_PixelSprite_RenderSettings{};
    if (NOT TestTrue(TEXT("One frame exports to new PNG"),
        ck::pixel_sprite_editor::TryExport_Frame(*Asset, TEXT("walk"), 0,
            Settings, FramePng, Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }
    auto FrameBytes = TArray64<uint8>{};
    auto DecodedFrame = FImage{};
    if (NOT TestTrue(TEXT("Frame PNG loads"), FFileHelper::LoadFileToArray(FrameBytes, *FramePng)) ||
        NOT TestTrue(TEXT("Frame PNG decodes"),
            FImageUtils::DecompressImage(FrameBytes.GetData(), FrameBytes.Num(), DecodedFrame)))
    {
        return false;
    }
    DecodedFrame.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    TestEqual(TEXT("Frame width"), DecodedFrame.SizeX, int32{2});
    TestEqual(TEXT("Frame height"), DecodedFrame.SizeY, int32{2});
    TestEqual(TEXT("Frame authored red pixel"), DecodedFrame.AsBGRA8()[0], FColor::Red);
    TestEqual(TEXT("Frame transparent pixel"), DecodedFrame.AsBGRA8()[1], FColor::Transparent);

    if (NOT TestTrue(TEXT("Sheet PNG and JSON export together"),
        ck::pixel_sprite_editor::TryExport_Sheet(*Asset, TEXT("walk"), Settings,
            SheetPng, SheetJson, Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }
    auto SheetBytes = TArray64<uint8>{};
    auto DecodedSheet = FImage{};
    if (NOT TestTrue(TEXT("Sheet PNG loads"), FFileHelper::LoadFileToArray(SheetBytes, *SheetPng)) ||
        NOT TestTrue(TEXT("Sheet PNG decodes"),
            FImageUtils::DecompressImage(SheetBytes.GetData(), SheetBytes.Num(), DecodedSheet)))
    {
        return false;
    }
    DecodedSheet.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    TestEqual(TEXT("Three frames preserve order in horizontal sheet"), DecodedSheet.SizeX, int32{6});
    TestEqual(TEXT("Sheet height"), DecodedSheet.SizeY, int32{2});
    TestEqual(TEXT("First frame red pixel"), DecodedSheet.AsBGRA8()[0], FColor::Red);
    TestEqual(TEXT("Second frame blue pixel"), DecodedSheet.AsBGRA8()[3], FColor::Blue);
    TestEqual(TEXT("Third frame red pixel"), DecodedSheet.AsBGRA8()[4], FColor::Red);

    auto JsonText = FString{};
    auto JsonRoot = TSharedPtr<FJsonObject>{};
    if (NOT TestTrue(TEXT("Sheet JSON loads"), FFileHelper::LoadFileToString(JsonText, *SheetJson)) ||
        NOT TestTrue(TEXT("Sheet JSON parses"),
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), JsonRoot) && JsonRoot.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("Animation name"), JsonRoot->GetStringField(TEXT("animation")), FString{TEXT("walk")});
    TestEqual(TEXT("Playback"), JsonRoot->GetStringField(TEXT("playback")), FString{TEXT("loop")});
    const auto& Frames = JsonRoot->GetArrayField(TEXT("frames"));
    if (NOT TestEqual(TEXT("Sheet contains each frame"), Frames.Num(), 3))
    {
        return false;
    }
    TestEqual(TEXT("First authored duration in milliseconds"), Frames[0]->AsObject()->GetNumberField(TEXT("duration_ms")), 125.0);
    TestEqual(TEXT("Second authored duration in milliseconds"), Frames[1]->AsObject()->GetNumberField(TEXT("duration_ms")), 250.0);
    TestEqual(TEXT("Third authored duration in milliseconds"), Frames[2]->AsObject()->GetNumberField(TEXT("duration_ms")), 375.0);
    TestEqual(TEXT("Source order preserved"), Frames[1]->AsObject()->GetStringField(TEXT("source")), FString{TEXT("blue.xpm")});
    TestEqual(TEXT("Anchor x retained"),
        Frames[0]->AsObject()->GetObjectField(TEXT("anchors"))->GetArrayField(TEXT("origin"))[0]->AsNumber(), 1.0);
    TestTrue(TEXT("Typed event and metadata retained"),
        Frames[0]->AsObject()->GetArrayField(TEXT("events"))[0]->AsObject()->GetStringField(TEXT("name")) == TEXT("touch") &&
        Frames[2]->AsObject()->GetObjectField(TEXT("metadata"))->GetBoolField(TEXT("repeat")));
    TestTrue(TEXT("Large root metadata integer lexeme retained"),
        JsonText.Contains(TEXT("1234567890123456789012345678901234567890")));

    const auto OriginalBytes = FrameBytes;
    TestFalse(TEXT("Existing PNG cannot be overwritten"),
        ck::pixel_sprite_editor::TryExport_Frame(*Asset, TEXT("walk"), 1,
            Settings, FramePng, Diagnostic));
    auto AfterBytes = TArray64<uint8>{};
    TestTrue(TEXT("Existing PNG remains readable"), FFileHelper::LoadFileToArray(AfterBytes, *FramePng));
    TestTrue(TEXT("Existing PNG bytes are unchanged"), AfterBytes == OriginalBytes);
    TestFalse(TEXT("Source XPM cannot be used as export destination"),
        ck::pixel_sprite_editor::TryExport_Frame(*Asset, TEXT("walk"), 1,
            Settings, Manifest._Sources[0]._Path, Diagnostic));
    auto SourceJsonBefore = TArray64<uint8>{};
    auto SourceJsonAfter = TArray64<uint8>{};
    if (TestTrue(TEXT("Existing manifest source is readable"),
        FFileHelper::LoadFileToArray(SourceJsonBefore, *Fixture)))
    {
        TestFalse(TEXT("Existing manifest cannot be replaced by sheet JSON"),
            ck::pixel_sprite_editor::TryExport_Sheet(*Asset, TEXT("walk"), Settings,
                FPaths::GetPath(Fixture) / TEXT("never-created.png"), Fixture, Diagnostic));
        TestTrue(TEXT("Existing manifest remains readable"),
            FFileHelper::LoadFileToArray(SourceJsonAfter, *Fixture));
        TestTrue(TEXT("Existing manifest bytes are unchanged"), SourceJsonBefore == SourceJsonAfter);
    }
    TestFalse(TEXT("Existing JSON prevents partial PNG pair"),
        ck::pixel_sprite_editor::TryExport_Sheet(*Asset, TEXT("walk"), Settings,
            Directory / TEXT("blocked.png"), SheetJson, Diagnostic));
    TestFalse(TEXT("Blocked pair creates no PNG"), IFileManager::Get().FileExists(*(Directory / TEXT("blocked.png"))));
    return true;
}

#endif
