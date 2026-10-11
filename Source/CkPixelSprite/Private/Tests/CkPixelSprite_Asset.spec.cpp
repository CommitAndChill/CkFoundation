#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkCore/Macros/CkMacros.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_sprite_asset_tests
{
    auto SameManifest(const FCk_PixelSprite_Manifest& InLeft,
        const FCk_PixelSprite_Manifest& InRight) -> bool
    {
        if (InLeft._Width != InRight._Width || InLeft._Height != InRight._Height ||
            InLeft._EventsJson != InRight._EventsJson || InLeft._MetadataJson != InRight._MetadataJson ||
            InLeft._Sources.Num() != InRight._Sources.Num() ||
            InLeft._Animations.Num() != InRight._Animations.Num())
        { return false; }
        for (auto Index = int32{0}; Index < InLeft._Sources.Num(); ++Index)
        {
            const auto& Left = InLeft._Sources[Index];
            const auto& Right = InRight._Sources[Index];
            if (Left._Path != Right._Path || Left._Bytes != Right._Bytes ||
                Left._Grid._Width != Right._Grid._Width || Left._Grid._Height != Right._Grid._Height ||
                Left._Grid._Hotspot != Right._Grid._Hotspot ||
                Left._Grid._Indices != Right._Grid._Indices ||
                Left._Grid._Palette.Num() != Right._Grid._Palette.Num())
            { return false; }
            for (auto PaletteIndex = int32{0}; PaletteIndex < Left._Grid._Palette.Num(); ++PaletteIndex)
            {
                if (Left._Grid._Palette[PaletteIndex]._Symbol != Right._Grid._Palette[PaletteIndex]._Symbol ||
                    Left._Grid._Palette[PaletteIndex]._Color != Right._Grid._Palette[PaletteIndex]._Color)
                { return false; }
            }
        }
        for (auto Index = int32{0}; Index < InLeft._Animations.Num(); ++Index)
        {
            const auto& Left = InLeft._Animations[Index];
            const auto& Right = InRight._Animations[Index];
            if (Left._Name != Right._Name || Left._Playback != Right._Playback ||
                Left._EventsJson != Right._EventsJson || Left._MetadataJson != Right._MetadataJson ||
                Left._Frames.Num() != Right._Frames.Num())
            { return false; }
            for (auto FrameIndex = int32{0}; FrameIndex < Left._Frames.Num(); ++FrameIndex)
            {
                const auto& LeftFrame = Left._Frames[FrameIndex];
                const auto& RightFrame = Right._Frames[FrameIndex];
                if (LeftFrame._Source != RightFrame._Source ||
                    LeftFrame._SourceIndex != RightFrame._SourceIndex ||
                    LeftFrame._DurationMilliseconds != RightFrame._DurationMilliseconds ||
                    LeftFrame._EventsJson != RightFrame._EventsJson ||
                    LeftFrame._MetadataJson != RightFrame._MetadataJson ||
                    LeftFrame._Anchors.Num() != RightFrame._Anchors.Num())
                { return false; }
                for (const auto& Anchor : LeftFrame._Anchors)
                {
                    if (NOT RightFrame._Anchors.Contains(Anchor.Key) ||
                        RightFrame._Anchors.FindChecked(Anchor.Key) != Anchor.Value)
                    { return false; }
                }
            }
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_Asset,
    "Ck.PixelSprite.AssetRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTest_PixelSprite_Asset::RunTest(const FString&) -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin is available"), Plugin.IsValid()))
    { return false; }
    auto Manifest = FCk_PixelSprite_Manifest{};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    const auto Fixture = Plugin->GetBaseDir() / TEXT("Tools/CkPixel/Fixtures/Manifest/valid-loop.json");
    if (NOT TestTrue(TEXT("Character fixture loads"),
        ck::pixel_sprite::TryLoad_Manifest(Fixture, Manifest, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }

    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    TestFalse(TEXT("Default asset has no valid snapshot"), Asset->Get_Snapshot().IsValid());
    auto Settings = FCk_PixelSprite_RenderSettings{};
    Settings._Mode = ECk_PixelSprite_RenderMode::SmoothSilhouette;
    Settings._Scale = 3;
    Settings._Outline = true;
    Settings._Background = FColor::Blue;
    Settings._PaletteOverrides.Add(1, FColor::Green);
    Asset->Request_SetRenderSettings(Settings);
    Asset->Set_SourceFilename(Fixture);
    if (NOT TestTrue(TEXT("Fixture imports into reflected asset"),
        Asset->Request_ReplaceManifest(Manifest, Diagnostic)))
    { AddError(Diagnostic.Get_Text()); return false; }
    const auto OriginalSnapshot = Asset->Get_Snapshot();
    if (NOT TestTrue(TEXT("Imported snapshot is valid"), OriginalSnapshot.IsValid()))
    { return false; }
    TestTrue(TEXT("Imported source, pixels, frames, anchors and extras match"),
        ck_pixel_sprite_asset_tests::SameManifest(Manifest, *OriginalSnapshot));
    TestEqual(TEXT("Width query"), Asset->Get_Width(), Manifest._Width);
    TestEqual(TEXT("Height query"), Asset->Get_Height(), Manifest._Height);
    TestEqual(TEXT("Animation names query"), Asset->Get_AnimationNames().Num(), Manifest._Animations.Num());
    TestEqual(TEXT("Frame count query"), Asset->Get_FrameCount(Manifest._Animations[0]._Name),
        Manifest._Animations[0]._Frames.Num());

    auto Synthetic = FCk_PixelSprite_Manifest{};
    Synthetic._Width = Manifest._Width;
    Synthetic._Height = Manifest._Height;
    Synthetic._Sources.Add(Manifest._Sources[0]);
    auto DefaultAnimation = FCk_PixelSprite_Animation{};
    DefaultAnimation._Name = TEXT("default");
    auto DefaultFrame = FCk_PixelSprite_Frame{};
    DefaultFrame._Source = Synthetic._Sources[0]._Path;
    DefaultFrame._SourceIndex = 0;
    DefaultFrame._DurationMilliseconds = 100.0;
    DefaultAnimation._Frames.Add(MoveTemp(DefaultFrame));
    Synthetic._Animations.Add(MoveTemp(DefaultAnimation));
    auto SyntheticAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    TestTrue(TEXT("Standalone image accepts its normalized absolute source path"),
        SyntheticAsset->Request_ReplaceManifest(Synthetic, Diagnostic));
    TestTrue(TEXT("Standalone image snapshot is available"), SyntheticAsset->Get_Snapshot().IsValid());

    auto Serialized = TArray<uint8>{};
    FObjectWriter{Asset.Get(), Serialized};
    auto Restored = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    FObjectReader{Restored.Get(), Serialized};
    const auto RestoredSnapshot = Restored->Get_Snapshot();
    if (NOT TestTrue(TEXT("Serialized asset restores a valid snapshot"), RestoredSnapshot.IsValid()))
    { return false; }
    TestTrue(TEXT("Every native manifest field survives UObject serialization"),
        ck_pixel_sprite_asset_tests::SameManifest(Manifest, *RestoredSnapshot));
    TestEqual(TEXT("Source filename survives serialization"), Restored->Get_SourceFilename(), Fixture);
    TestEqual(TEXT("Render scale survives serialization"), Restored->Get_RenderSettings()._Scale, 3);
    TestEqual(TEXT("Render mode survives serialization"), Restored->Get_RenderSettings()._Mode,
        ECk_PixelSprite_RenderMode::SmoothSilhouette);
    if (TestTrue(TEXT("Palette override survives serialization"),
        Restored->Get_RenderSettings()._PaletteOverrides.Contains(1)))
    {
        TestEqual(TEXT("Palette override color survives serialization"),
            Restored->Get_RenderSettings()._PaletteOverrides.FindChecked(1), FColor::Green);
    }

    auto UnsupportedVersion = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    FObjectReader{UnsupportedVersion.Get(), Serialized};
    UnsupportedVersion->_DataFormatVersion = 2;
    TestFalse(TEXT("Unsupported serialized payload version has no runnable snapshot"),
        UnsupportedVersion->Get_Snapshot().IsValid());

    auto CorruptPixels = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    FObjectReader{CorruptPixels.Get(), Serialized};
    CorruptPixels->_ManifestData._Sources[0]._Grid._Indices[0] = 65535;
    TestFalse(TEXT("Corrupt serialized palette index has no runnable snapshot"),
        CorruptPixels->Get_Snapshot().IsValid());

    auto DuplicateAnchors = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    FObjectReader{DuplicateAnchors.Get(), Serialized};
    auto& StoredAnchors = DuplicateAnchors->_ManifestData._Animations[0]._Frames[0]._Anchors;
    if (TestTrue(TEXT("Fixture contains an anchor"), NOT StoredAnchors.IsEmpty()))
    {
        const auto Duplicate = StoredAnchors[0];
        StoredAnchors.Add(Duplicate);
        TestFalse(TEXT("Duplicate serialized anchor names cannot produce a snapshot"),
            DuplicateAnchors->Get_Snapshot().IsValid());
    }

    auto Duplicated = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{
        DuplicateObject<UCk_PixelSpriteAsset_UE>(Asset.Get(), GetTransientPackage())};
    TestTrue(TEXT("Duplicate owns an independent valid snapshot"),
        Duplicated->Get_Snapshot().IsValid() &&
        ck_pixel_sprite_asset_tests::SameManifest(Manifest, *Duplicated->Get_Snapshot()) &&
        Duplicated->Get_Snapshot() != OriginalSnapshot);

    const auto BeforeRevision = Asset->Get_Revision();
    auto Invalid = Manifest;
    Invalid._Sources[0]._Grid._Indices[0] = 65535;
    TestFalse(TEXT("Invalid palette index rejects"), Asset->Request_ReplaceManifest(Invalid, Diagnostic));
    TestFalse(TEXT("Invalid replacement reports a reason"), Diagnostic._Code.IsEmpty());
    TestEqual(TEXT("Invalid replacement preserves revision"), Asset->Get_Revision(), BeforeRevision);
    TestTrue(TEXT("Invalid replacement preserves snapshot identity"),
        Asset->Get_Snapshot() == OriginalSnapshot);
    TestTrue(TEXT("Invalid replacement preserves render settings"), Asset->Get_RenderSettings()._Outline);

    Invalid = Manifest;
    Invalid._Animations[0]._Frames[0]._SourceIndex = Invalid._Sources.Num();
    TestFalse(TEXT("Invalid source index rejects"), Asset->Request_ReplaceManifest(Invalid, Diagnostic));
    Invalid = Manifest;
    Invalid._Animations[0]._Frames[0]._DurationMilliseconds = 0.0;
    TestFalse(TEXT("Invalid duration rejects"), Asset->Request_ReplaceManifest(Invalid, Diagnostic));
    Invalid = Manifest;
    Invalid._Animations[0]._Frames[0]._MetadataJson = TEXT("[]");
    TestFalse(TEXT("Invalid metadata type rejects"), Asset->Request_ReplaceManifest(Invalid, Diagnostic));
    TestEqual(TEXT("All invalid replacements preserve revision"), Asset->Get_Revision(), BeforeRevision);
    TestTrue(TEXT("All invalid replacements preserve the runnable snapshot"),
        Asset->Get_Snapshot() == OriginalSnapshot);

    auto Changed = Manifest;
    Changed._Animations[0]._Frames[0]._DurationMilliseconds += 1.0;
    if (NOT TestTrue(TEXT("Valid replacement succeeds"), Asset->Request_ReplaceManifest(Changed, Diagnostic)))
    { return false; }
    TestEqual(TEXT("Valid replacement increments revision"), Asset->Get_Revision(), BeforeRevision + 1);
    TestTrue(TEXT("Earlier snapshot remains intact after replacement"),
        ck_pixel_sprite_asset_tests::SameManifest(Manifest, *OriginalSnapshot));
    TestTrue(TEXT("Replacement publishes changed snapshot"),
        ck_pixel_sprite_asset_tests::SameManifest(Changed, *Asset->Get_Snapshot()));
    TestEqual(TEXT("Replacement preserves source filename"), Asset->Get_SourceFilename(), Fixture);
    TestEqual(TEXT("Replacement preserves render settings"), Asset->Get_RenderSettings()._Scale, 3);
    return true;
}

#endif
