#include "CkPixelSprite/CkPixelSprite_MaterialPreview.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkCore/Macros/CkMacros.h"

#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_WorldPresentation,
    "Ck.PixelSprite.WorldPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_WorldPresentation::
    RunTest(const FString&)
    -> bool
{
    auto Player = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto Pixels = TArray<FColor>{FColor::Red};
    auto Canvas = FIntPoint{9, 9};
    TestFalse(TEXT("Unselected player rejects pixel query"), Player->TryGet_CurrentPixels(Pixels, Canvas));
    TestTrue(TEXT("Rejected query preserves both outputs"), Pixels.Num() == 1 && Pixels[0] == FColor::Red && Canvas == FIntPoint{9, 9});
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto Manifest = FCk_PixelSprite_Manifest{};
    Manifest._Width = 2;
    Manifest._Height = 2;
    auto Animation = FCk_PixelSprite_Animation{};
    Animation._Name = TEXT("idle");
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto Source = FCk_PixelSprite_Source{};
        Source._Bytes = 1;
        Source._Path = FString::Printf(TEXT("pose%d.xpm"), Index);
        Source._Grid._Width = 2;
        Source._Grid._Height = 2;
        Source._Grid._Palette.Add({TEXT("."), FColor::Transparent});
        Source._Grid._Palette.Add({TEXT("K"), Index == 0 ? FColor::Red : FColor::Green});
        Source._Grid._Indices = {0, 1, 1, 0};
        Manifest._Sources.Add(MoveTemp(Source));
        auto Frame = FCk_PixelSprite_Frame{};
        Frame._Source = FString::Printf(TEXT("pose%d.xpm"), Index);
        Frame._SourceIndex = Index;
        Frame._DurationMilliseconds = 100;
        Animation._Frames.Add(MoveTemp(Frame));
    }
    Manifest._Animations.Add(MoveTemp(Animation));
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    if (NOT TestTrue(TEXT("Indexed world source selects"), Asset->Request_ReplaceManifest(Manifest, Diagnostic)
        && Player->TrySelectAsset(Asset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    TestTrue(TEXT("Query returns authored RGBA and dimensions"), Player->TryGet_CurrentPixels(Pixels, Canvas)
        && Canvas == FIntPoint{2, 2} && Pixels == TArray<FColor>{FColor::Transparent, FColor::Red, FColor::Red, FColor::Transparent});
    TestTrue(TEXT("Stepping changes queried source"), Player->Request_StepFrames(1)
        && Player->TryGet_CurrentPixels(Pixels, Canvas) && Pixels[1] == FColor::Green);
    auto Presenter = TStrongObjectPtr<UCk_PixelSpriteMaterialPreview_UE>{NewObject<UCk_PixelSpriteMaterialPreview_UE>()};
    Presenter->Player = Player.Get();
    auto A = TStrongObjectPtr<UMaterialInstanceDynamic>{UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), Presenter.Get())};
    auto B = TStrongObjectPtr<UMaterialInstanceDynamic>{UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), Presenter.Get())};
    Presenter->Materials = {A.Get(), B.Get()};
    TestTrue(TEXT("All material targets bind one source generation"), Presenter->Request_Refresh());
    TestEqual(TEXT("Presenter does not advance clock"), Player->Get_ElapsedMilliseconds(), 100.0);
    Presenter->Materials.Add(nullptr);
    TestFalse(TEXT("One invalid target rejects the full material set"), Presenter->Request_Refresh());
    TestEqual(TEXT("First target cleared on partial invalidation"), A->K2_GetScalarParameterValue(TEXT("SpriteReady")), 0.0f);
    TestEqual(TEXT("Second target cleared on partial invalidation"), B->K2_GetScalarParameterValue(TEXT("SpriteReady")), 0.0f);
    Presenter->Materials = {A.Get(), B.Get()};
    auto Settings = Asset->Get_RenderSettings();
    Settings._Scale = 2;
    Asset->Request_SetRenderSettings(Settings);
    TestFalse(TEXT("Derived CPU artwork cannot masquerade as raw shader texels"), Presenter->Request_Refresh());
    TestTrue(TEXT("Pixel query remains authored independently of render settings"),
        Player->TryGet_CurrentPixels(Pixels, Canvas) && Canvas == FIntPoint{2, 2});
    return true;
}
#endif
