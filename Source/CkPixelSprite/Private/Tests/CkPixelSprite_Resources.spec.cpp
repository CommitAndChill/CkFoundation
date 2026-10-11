#include "CkPixelSprite/CkPixelSprite_Resources.h"

#include "CkCore/Macros/CkMacros.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "RHIGlobals.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_sprite_resources_tests
{
    auto MakeManifest(int32 InWidth, int32 InHeight, int32 InSourceCount) -> FCk_PixelSprite_Manifest
    {
        auto Manifest = FCk_PixelSprite_Manifest{};
        Manifest._Width = InWidth;
        Manifest._Height = InHeight;
        auto Animation = FCk_PixelSprite_Animation{};
        Animation._Name = TEXT("default");
        for (auto Index = int32{0}; Index < InSourceCount; ++Index)
        {
            auto Source = FCk_PixelSprite_Source{};
            Source._Path = FString::Printf(TEXT("C:/sprites/%d.xpm"), Index);
            Source._Bytes = 100;
            Source._Grid._Width = InWidth;
            Source._Grid._Height = InHeight;
            Source._Grid._Palette.Add({TEXT("."), FColor{static_cast<uint8>(Index), 80, 160, 255}});
            Source._Grid._Indices.Init(uint16{0}, InWidth * InHeight);
            Manifest._Sources.Add(MoveTemp(Source));
            auto Frame = FCk_PixelSprite_Frame{};
            Frame._Source = InSourceCount == 1
                ? Manifest._Sources[Index]._Path : FString::Printf(TEXT("%d.xpm"), Index);
            Frame._SourceIndex = Index;
            Frame._DurationMilliseconds = 100.0;
            Animation._Frames.Add(MoveTemp(Frame));
        }
        Manifest._Animations.Add(MoveTemp(Animation));
        return Manifest;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Resources,
    "Ck.PixelSprite.ResourceGeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTest_PixelSprite_Resources::RunTest(const FString&) -> bool
{
    if (GUsingNullRHI)
    { AddInfo(TEXT("NullRHI validates CPU texture staging only; a rendered GPU result needs a graphics RHI.")); }
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto Resources = TStrongObjectPtr<UCk_PixelSpriteResources_UE>{NewObject<UCk_PixelSpriteResources_UE>()};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    auto Manifest = ck_pixel_sprite_resources_tests::MakeManifest(2, 2, 1);
    if (NOT TestTrue(TEXT("Small manifest accepted"), Asset->Request_ReplaceManifest(Manifest, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }

    auto Settings = FCk_PixelSprite_RenderSettings{};
    if (NOT TestTrue(TEXT("Resource generation succeeds"),
        Resources->TryBuild(Asset.Get(), Settings, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    const auto FirstTexture = Resources->Get_Texture(0);
    if (NOT TestTrue(TEXT("Source zero has a texture"), IsValid(FirstTexture.Get())))
    { return false; }
    TestEqual(TEXT("Texture width"), FirstTexture->GetSizeX(), 2);
    TestEqual(TEXT("Texture height"), FirstTexture->GetSizeY(), 2);
    TestEqual(TEXT("One source uploaded"), Resources->Get_UploadCount(), 1);
    TestTrue(TEXT("Out-of-range source has no texture"), Resources->Get_Texture(1).Get() == nullptr);
    TestTrue(TEXT("Unchanged asset/settings reuse generation"),
        Resources->TryBuild(Asset.Get(), Settings, Diagnostic));
    TestTrue(TEXT("Cache hit preserves texture identity"), Resources->Get_Texture(0) == FirstTexture);
    TestEqual(TEXT("Cache hit performs no upload"), Resources->Get_UploadCount(), 1);

    Settings._Scale = 2;
    if (NOT TestTrue(TEXT("Changed settings rebuild generation"),
        Resources->TryBuild(Asset.Get(), Settings, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    const auto CurrentTexture = Resources->Get_Texture(0);
    TestTrue(TEXT("Changed settings replace texture"), CurrentTexture != FirstTexture);
    TestEqual(TEXT("Changed settings upload once"), Resources->Get_UploadCount(), 2);
    TestEqual(TEXT("Scaled width"), CurrentTexture->GetSizeX(), 4);

    Manifest._Sources[0]._Grid._Palette[0]._Color = FColor::Red;
    if (NOT TestTrue(TEXT("Changed source replaces manifest"),
        Asset->Request_ReplaceManifest(Manifest, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    if (NOT TestTrue(TEXT("Changed source rebuilds resources"),
        Resources->TryBuild(Asset.Get(), Settings, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    const auto RevisedTexture = Resources->Get_Texture(0);
    TestTrue(TEXT("Manifest revision changes texture identity"), RevisedTexture != CurrentTexture);
    TestEqual(TEXT("Manifest revision uploads once"), Resources->Get_UploadCount(), 3);

    auto InvalidSettings = Settings;
    InvalidSettings._Scale = 0;
    TestFalse(TEXT("Invalid settings reject"), Resources->TryBuild(Asset.Get(), InvalidSettings, Diagnostic));
    TestTrue(TEXT("Rejected settings preserve current texture"), Resources->Get_Texture(0) == RevisedTexture);
    TestEqual(TEXT("Rejected settings do not publish uploads"), Resources->Get_UploadCount(), 3);

    auto TooWide = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto WideManifest = ck_pixel_sprite_resources_tests::MakeManifest(4096, 1, 1);
    if (NOT TestTrue(TEXT("Wide source is valid manifest data"),
        TooWide->Request_ReplaceManifest(WideManifest, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    auto WideSettings = FCk_PixelSprite_RenderSettings{};
    WideSettings._Scale = 8;
    TestFalse(TEXT("RHI dimension limit rejects oversized output"),
        Resources->TryBuild(TooWide.Get(), WideSettings, Diagnostic));
    TestEqual(TEXT("RHI dimension diagnostic"), Diagnostic._Code, FString{TEXT("dimensions")});
    TestTrue(TEXT("Rejected dimensions preserve generation"), Resources->Get_Texture(0) == RevisedTexture);

    auto TooMuch = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto LargeManifest = ck_pixel_sprite_resources_tests::MakeManifest(128, 128, 17);
    if (NOT TestTrue(TEXT("Multi-source budget fixture is valid manifest data"),
        TooMuch->Request_ReplaceManifest(LargeManifest, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    auto LargeSettings = FCk_PixelSprite_RenderSettings{};
    LargeSettings._Scale = 8;
    TestFalse(TEXT("Generation budget rejects 68 MiB output"),
        Resources->TryBuild(TooMuch.Get(), LargeSettings, Diagnostic));
    TestEqual(TEXT("Generation budget diagnostic"), Diagnostic._Code, FString{TEXT("budget")});
    TestTrue(TEXT("Rejected budget preserves generation"), Resources->Get_Texture(0) == RevisedTexture);
    TestEqual(TEXT("Rejected budget does not publish uploads"), Resources->Get_UploadCount(), 3);

    auto MultiAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto MultiResources = TStrongObjectPtr<UCk_PixelSpriteResources_UE>{NewObject<UCk_PixelSpriteResources_UE>()};
    auto MultiManifest = ck_pixel_sprite_resources_tests::MakeManifest(2, 2, 2);
    MultiManifest._Sources[0]._Grid._Palette.Add({TEXT("x"), FColor::Green});
    auto LateInvalidSettings = FCk_PixelSprite_RenderSettings{};
    LateInvalidSettings._PaletteOverrides.Add(1, FColor::Red);
    if (NOT TestTrue(TEXT("Two-source manifest accepted"),
        MultiAsset->Request_ReplaceManifest(MultiManifest, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    TestFalse(TEXT("Invalid setting on later source rejects whole generation"),
        MultiResources->TryBuild(MultiAsset.Get(), LateInvalidSettings, Diagnostic));
    TestEqual(TEXT("Late render rejection publishes no uploads"), MultiResources->Get_UploadCount(), 0);
    TestTrue(TEXT("Late render rejection publishes no source textures"),
        MultiResources->Get_Texture(0).Get() == nullptr);
    if (NOT TestTrue(TEXT("Two-source snapshot remains valid"),
        MultiAsset->Get_Snapshot().IsValid()) ||
        NOT TestTrue(TEXT("Every unique source builds before use"),
            MultiResources->TryBuild(MultiAsset.Get(), FCk_PixelSprite_RenderSettings{}, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    TestEqual(TEXT("Two unique sources upload"), MultiResources->Get_UploadCount(), 2);
    TestTrue(TEXT("Source indices resolve to different textures"),
        IsValid(MultiResources->Get_Texture(0).Get()) &&
        IsValid(MultiResources->Get_Texture(1).Get()) &&
        MultiResources->Get_Texture(0) != MultiResources->Get_Texture(1));

    CollectGarbage(RF_NoFlags);
    TestTrue(TEXT("Resource owner keeps active texture alive across GC"),
        IsValid(Resources->Get_Texture(0).Get()));
    return true;
}

#endif
