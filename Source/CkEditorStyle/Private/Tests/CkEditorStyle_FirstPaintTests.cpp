#include "CkEditorStyle/CkEditorStyle_Utils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEditorStyle_FirstPaintTest,
    "Ck.EditorStyle.FirstPaint.PNGSVGAndSameKeyReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FCkEditorStyle_FirstPaintTest::RunTest(const FString& Parameters)
{
    if (!FApp::CanEverRender() || GUsingNullRHI || !FSlateApplication::IsInitialized())
    {
        AddError(TEXT("First-paint pixel verification requires an editor process with real Slate and RHI."));
        return false;
    }

    constexpr int32 Size = 64;
    const auto OutputDir = FPaths::ProjectSavedDir() / TEXT("Automation/CkEditorStyleFirstPaint");
    IFileManager::Get().MakeDirectory(*OutputDir, true);
    auto& Slate = FSlateApplication::Get();
    const auto Image = SNew(SImage);
    const auto Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("CK icon first-paint verification")))
        .ClientSize(FVector2D(Size, Size))
        .SizingRule(ESizingRule::FixedSize)
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        .CreateTitleBar(false)
        .UseOSWindowBorder(false)
        [
            SNew(SBorder)
            .Padding(0)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(FLinearColor::Black)
            [Image]
        ];
    Slate.AddWindow(Window);
    // Lay out a blank image first; no tested brush has been painted or registered here.
    TArray<FColor> BlankPixels;
    FIntVector BlankSize = FIntVector::ZeroValue;
    Slate.TakeScreenshot(Window, BlankPixels, BlankSize);
    FlushRenderingCommands();

    // TakeScreenshot uses the real SWindow/SlateRHIRenderer path, including SVG atlas
    // upload. Offscreen FWidgetRenderer does not exercise that production path.
    const auto Paint = [&](const FName Key, const TCHAR* EvidenceName, TArray<FColor>& OutPixels)
    {
        // Requery after replacement: previously returned brush pointers are not retained.
        const auto* Brush = UCk_Utils_EditorStyle_UE::Get().GetBrush(Key);
        if (!TestNotNull(EvidenceName, Brush))
        { return false; }

        Image->SetImage(Brush);
        FIntVector CapturedSize = FIntVector::ZeroValue;
        const bool Read = Slate.TakeScreenshot(Image, OutPixels, CapturedSize);
        if (!TestTrue(TEXT("First paint screenshot has all pixels"), Read &&
            CapturedSize.X > 0 && CapturedSize.Y > 0 && OutPixels.Num() == CapturedSize.X * CapturedSize.Y))
        { return false; }

        // Transparent/black targets cannot pass merely because resource lookup succeeded.
        int32 VisiblePixels = 0;
        for (const auto& Pixel : OutPixels)
        {
            VisiblePixels += Pixel.R > 16 || Pixel.G > 16 || Pixel.B > 16;
        }
        TestTrue(*FString::Printf(TEXT("%s has visible icon pixels"), EvidenceName), VisiblePixels > 64);
        AddInfo(FString::Printf(TEXT("%s first paint: %d visible pixels, resource %s"),
            EvidenceName, VisiblePixels, *Brush->GetResourceName().ToString()));

        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(CapturedSize.X, CapturedSize.Y, OutPixels, Png);
        return TestTrue(TEXT("Saved first-paint PNG evidence"),
            FFileHelper::SaveArrayToFile(Png, *(OutputDir / (FString(EvidenceName) + TEXT(".png")))));
    };

    // Prove startup registrations BEFORE any custom registration. Registering even a
    // test key must not mask missing startup resources with a global texture reload.
    TestTrue(TEXT("Startup PNG lookup is the shipped entity-script brush"),
        UCk_Utils_EditorStyle_UE::Get().GetBrush(TEXT("ClassThumbnail.Ck_EntityScript_UE"))
            ->GetResourceName().ToString().EndsWith(TEXT("Icon_EntityScript_64px.png")));
    TestTrue(TEXT("Startup SVG lookup is the shipped item-definition brush"),
        UCk_Utils_EditorStyle_UE::Get().GetBrush(TEXT("ClassThumbnail.Ck_InventoryItem_Definition"))
            ->GetResourceName().ToString().EndsWith(TEXT("Icon_ItemDefinition.svg")));
    TArray<FColor> ProductionPng, ProductionSvg;
    const bool ProductionPngPaint = Paint(TEXT("ClassThumbnail.Ck_EntityScript_UE"), TEXT("ProductionPNG_FirstPaint"), ProductionPng);
    const bool ProductionSvgPaint = Paint(TEXT("ClassThumbnail.Ck_InventoryItem_Definition"), TEXT("ProductionSVG_FirstPaint"), ProductionSvg);

    const auto RegisterAndPaint = [&](const FName Key, const TCHAR* Icon,
        ECk_CustomIconBrushType Type, const TCHAR* EvidenceName, TArray<FColor>& OutPixels)
    {
        UCk_Utils_EditorStyle_UE::Register_CustomSlateStyle(
            FCk_CustomAssetStyle_Params{ECk_AssetStyleType::Other, Key}
                .Set_IconFileName(Icon)
                .Set_IconBrushType(Type)
                .Set_IconSize(ECk_IconSize::IconSize_64x64));
        TestTrue(TEXT("Fresh same-key lookup selects the newly registered resource"),
            UCk_Utils_EditorStyle_UE::Get().GetBrush(Key)->GetResourceName().ToString().EndsWith(
                FString(Icon) + (Type == ECk_CustomIconBrushType::PNG ? TEXT(".png") : TEXT(".svg"))));
        return Paint(Key, EvidenceName, OutPixels);
    };

    const auto CheckReplacement = [&](const TArray<FColor>& Before, const TArray<FColor>& After,
        const TCHAR* Label)
    {
        if (Before.IsEmpty() || Before.Num() != After.Num())
        { return false; }
        int32 ChangedPixels = 0;
        for (int32 Index = 0; Index < Before.Num(); ++Index)
        {
            ChangedPixels += Before[Index] != After[Index];
        }
        AddInfo(FString::Printf(TEXT("%s replacement changed %d pixels on its first paint"), Label, ChangedPixels));
        return TestTrue(Label, ChangedPixels > 64);
    };

    // Shipped assets differ visibly and exercise both resource formats. Unique test keys
    // avoid changing any class icons used by the user's editor.
    TArray<FColor> PngBefore, PngAfter, SvgBefore, SvgAfter;
    const bool PngInitial = RegisterAndPaint(TEXT("Ck.FirstPaint.PNG"), TEXT("Icon_EntityScript_64px"),
        ECk_CustomIconBrushType::PNG, TEXT("PNG_Initial"), PngBefore);
    const bool PngReplaced = RegisterAndPaint(TEXT("Ck.FirstPaint.PNG"), TEXT("Icon_EntityConstructionScript_64px"),
        ECk_CustomIconBrushType::PNG, TEXT("PNG_Replacement"), PngAfter);
    const bool SvgInitial = RegisterAndPaint(TEXT("Ck.FirstPaint.SVG"), TEXT("Icon_ItemDefinition"),
        ECk_CustomIconBrushType::SVG, TEXT("SVG_Initial"), SvgBefore);
    const bool SvgReplaced = RegisterAndPaint(TEXT("Ck.FirstPaint.SVG"), TEXT("Icon_ItemActor"),
        ECk_CustomIconBrushType::SVG, TEXT("SVG_Replacement"), SvgAfter);
    const bool PngChanged = CheckReplacement(PngBefore, PngAfter, TEXT("PNG same-key replacement"));
    const bool SvgChanged = CheckReplacement(SvgBefore, SvgAfter, TEXT("SVG same-key replacement"));

    Slate.DestroyWindowImmediately(Window);
    FlushRenderingCommands();
    AddInfo(FString::Printf(TEXT("First-paint pixel evidence: %s"), *FPaths::ConvertRelativePathToFull(OutputDir)));
    return ProductionPngPaint && ProductionSvgPaint && PngInitial && PngReplaced && SvgInitial && SvgReplaced && PngChanged && SvgChanged;
}

#endif
