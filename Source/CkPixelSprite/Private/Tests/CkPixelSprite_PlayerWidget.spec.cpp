#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkPixelSprite/CkPixelSprite_Player.h"
#include "CkPixelSprite/CkPixelSprite_Resources.h"
#include "CkPixelSprite/CkPixelSprite_Widget.h"

#include "CkCore/Macros/CkMacros.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_sprite_player_tests
{
    auto
        MakeManifest()
        -> FCk_PixelSprite_Manifest
    {
        auto Manifest = FCk_PixelSprite_Manifest{};
        Manifest._Width = 2;
        Manifest._Height = 2;
        auto Animation = FCk_PixelSprite_Animation{};
        Animation._Name = TEXT("walk");
        for (auto Index = int32{0}; Index < 3; ++Index)
        {
            auto Source = FCk_PixelSprite_Source{};
            Source._Path = FString::Printf(TEXT("C:/sprites/%d.xpm"), Index);
            Source._Bytes = 100;
            Source._Grid._Width = 2;
            Source._Grid._Height = 2;
            Source._Grid._Palette.Add({TEXT("."), FColor{static_cast<uint8>(Index), 80, 160, 255}});
            Source._Grid._Indices.Init(uint16{0}, 4);
            Manifest._Sources.Add(MoveTemp(Source));
            auto Frame = FCk_PixelSprite_Frame{};
            Frame._Source = FString::Printf(TEXT("%d.xpm"), Index);
            Frame._SourceIndex = Index;
            Frame._DurationMilliseconds = Index == 0 ? 125.0 : Index == 1 ? 250.0 : 375.0;
            Frame._Anchors.Add(TEXT("origin"), FIntPoint{Index % 2, Index / 2});
            Animation._Frames.Add(MoveTemp(Frame));
        }
        Manifest._Animations.Add(MoveTemp(Animation));
        return Manifest;
    }

    auto
        MakeSubUlpManifest()
        -> FCk_PixelSprite_Manifest
    {
        auto Manifest = MakeManifest();
        auto& Frames = Manifest._Animations[0]._Frames;
        Frames[0]._DurationMilliseconds = 86400000.0;
        Frames[1]._DurationMilliseconds = 1e-300;
        Frames[2]._DurationMilliseconds = 2e-300;

        auto Source = Manifest._Sources[0];
        Source._Path = TEXT("C:/sprites/3.xpm");
        Manifest._Sources.Add(MoveTemp(Source));
        auto Frame = Frames[2];
        Frame._Source = TEXT("3.xpm");
        Frame._SourceIndex = 3;
        Frame._DurationMilliseconds = 100.0;
        Frame._Anchors.FindOrAdd(TEXT("origin")) = FIntPoint{1, 1};
        Frames.Add(MoveTemp(Frame));
        return Manifest;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_PlayerWidget,
    "Ck.PixelSprite.PlayerWidget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_PlayerWidget::
    RunTest(
        const FString&)
    -> bool
{
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto Player = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    auto Manifest = ck_pixel_sprite_player_tests::MakeManifest();
    if (NOT TestTrue(TEXT("Three-frame manifest accepted"),
        Asset->Request_ReplaceManifest(Manifest, Diagnostic)) ||
        NOT TestTrue(TEXT("Player builds all source textures before use"),
        Player->TrySelectAsset(Asset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    TestEqual(TEXT("Three unique textures preuploaded"), Player->Get_Resources()->Get_UploadCount(), 3);
    TestEqual(TEXT("Initial frame"), Player->Get_CurrentFrame(), 0);

    auto ReusedDiagnostic = FCk_PixelSprite_Diagnostic{};
    ReusedDiagnostic._Source = TEXT("stale source");
    ReusedDiagnostic._Code = TEXT("stale code");
    ReusedDiagnostic._Message = TEXT("stale message");
    ReusedDiagnostic._Line = 17;
    ReusedDiagnostic._Column = 29;
    const auto OriginalResources = Player->Get_Resources();
    const auto OriginalRevision = Player->Get_Revision();
    const auto OriginalTexture = Player->Get_CurrentTexture();
    TestFalse(TEXT("Null asset rejected with reused diagnostic"),
        Player->TrySelectAsset(TObjectPtr<UCk_PixelSpriteAsset_UE>{}, ReusedDiagnostic));
    TestEqual(TEXT("Diagnostic source replaced"), ReusedDiagnostic._Source, FString{TEXT("PixelSpritePlayer")});
    TestEqual(TEXT("Diagnostic code replaced"), ReusedDiagnostic._Code, FString{TEXT("asset")});
    TestEqual(TEXT("Diagnostic message replaced"), ReusedDiagnostic._Message,
        FString{TEXT("Pixel sprite asset is invalid")});
    TestEqual(TEXT("Diagnostic line reset"), ReusedDiagnostic._Line, 1);
    TestEqual(TEXT("Diagnostic column reset"), ReusedDiagnostic._Column, 1);
    TestTrue(TEXT("Rejected asset retains resources"), Player->Get_Resources() == OriginalResources);
    TestEqual(TEXT("Rejected asset retains revision"), Player->Get_Revision(), OriginalRevision);
    TestTrue(TEXT("Rejected asset retains current texture"), Player->Get_CurrentTexture() == OriginalTexture);

    Player->Request_Play();
    TestTrue(TEXT("Advance to exact authored boundary"), Player->Request_AdvanceMilliseconds(125.0));
    TestEqual(TEXT("Boundary selects second frame"), Player->Get_CurrentFrame(), 1);
    const auto FrameOneTexture = Player->Get_CurrentTexture();
    Player->Request_Pause();
    TestTrue(TEXT("Paused advance is accepted"), Player->Request_AdvanceMilliseconds(250.0));
    TestEqual(TEXT("Paused advance does not move clock"), Player->Get_ElapsedMilliseconds(), 125.0);
    TestTrue(TEXT("Seek evaluates authored durations"), Player->Request_SeekMilliseconds(375.0));
    TestEqual(TEXT("Seek selects third frame"), Player->Get_CurrentFrame(), 2);
    TestTrue(TEXT("Reverse direction accepted"), Player->Request_SetDirection(-1));
    Player->Request_Play();
    TestTrue(TEXT("Reverse advance succeeds"), Player->Request_AdvanceMilliseconds(250.0));
    TestEqual(TEXT("Reverse reaches second frame"), Player->Get_CurrentFrame(), 1);
    TestTrue(TEXT("Reverse reuses uploaded texture"), Player->Get_CurrentTexture() == FrameOneTexture);
    TestTrue(TEXT("Large signed loop delta succeeds"), Player->Request_AdvanceMilliseconds(7500000.0));
    TestEqual(TEXT("Large signed loop retains exact phase"), Player->Get_CurrentFrame(), 1);

    TestTrue(TEXT("Ping-pong setting accepted"),
        Player->Request_SetPlayback(ECk_PixelSprite_PlayerPlayback::PingPong));
    TestTrue(TEXT("Ping-pong seek accepted"), Player->Request_SeekMilliseconds(700.0));
    TestEqual(TEXT("Ping-pong reverse visit is third frame"), Player->Get_CurrentFrame(), 2);
    TestTrue(TEXT("Once setting accepted"),
        Player->Request_SetPlayback(ECk_PixelSprite_PlayerPlayback::Once));
    TestTrue(TEXT("Forward once direction accepted"), Player->Request_SetDirection(1));
    TestTrue(TEXT("Once completion seek accepted"), Player->Request_SeekMilliseconds(750.0));
    TestTrue(TEXT("Once completes at cycle end"), Player->Get_IsComplete());
    TestEqual(TEXT("Once holds final frame"), Player->Get_CurrentFrame(), 2);
    TestEqual(TEXT("Once normalized time reaches one"), Player->Get_NormalizedTime(), 1.0);
    TestTrue(TEXT("Reverse direction can leave completed forward endpoint"),
        Player->Request_SetDirection(-1));
    TestFalse(TEXT("Reverse direction clears forward completion"), Player->Get_IsComplete());
    Player->Request_Play();
    TestTrue(TEXT("Reverse once reaches start"), Player->Request_AdvanceMilliseconds(750.0));
    TestEqual(TEXT("Reverse once clamps at zero"), Player->Get_ElapsedMilliseconds(), 0.0);
    TestTrue(TEXT("Reverse once reports completion"), Player->Get_IsComplete());
    TestFalse(TEXT("Reverse once stops"), Player->Get_IsPlaying());
    TestTrue(TEXT("Reverse restart succeeds"), Player->Request_Restart());
    TestEqual(TEXT("Reverse restart begins at end"), Player->Get_ElapsedMilliseconds(), 750.0);
    TestFalse(TEXT("Reverse restart is not complete"), Player->Get_IsComplete());
    TestTrue(TEXT("Forward direction can leave reverse start"), Player->Request_SetDirection(1));
    TestTrue(TEXT("Forward seek to once end"), Player->Request_SeekMilliseconds(750.0));
    TestTrue(TEXT("Coarse sampling interval accepted"),
        Player->Request_SetSamplingIntervalMilliseconds(1000.0));
    TestTrue(TEXT("Completion survives display sampling"), Player->Get_IsComplete());
    TestEqual(TEXT("Sampled display may show earlier frame"), Player->Get_CurrentFrame(), 0);
    TestTrue(TEXT("Sampling disabled"), Player->Request_SetSamplingIntervalMilliseconds(0.0));
    TestTrue(TEXT("Restart rewinds and plays"), Player->Request_Restart());
    TestEqual(TEXT("Restart returns first frame"), Player->Get_CurrentFrame(), 0);
    TestTrue(TEXT("Forward direction accepted"), Player->Request_SetDirection(1));
    TestTrue(TEXT("Double speed accepted"), Player->Request_SetSpeed(2.0));
    TestTrue(TEXT("Speed scales the explicit advance"), Player->Request_AdvanceMilliseconds(62.5));
    TestEqual(TEXT("Double speed reaches authored boundary"), Player->Get_CurrentFrame(), 1);
    TestTrue(TEXT("Normal speed restored"), Player->Request_SetSpeed(1.0));

    TestTrue(TEXT("Loop restore accepted"),
        Player->Request_SetPlayback(ECk_PixelSprite_PlayerPlayback::Loop));
    TestTrue(TEXT("Sample interval accepted"), Player->Request_SetSamplingIntervalMilliseconds(300.0));
    TestTrue(TEXT("Seek keeps authored logical time"), Player->Request_SeekMilliseconds(400.0));
    TestEqual(TEXT("Logical frame remains third"), Player->Get_LogicalFrame(), 2);
    TestEqual(TEXT("Sampled presentation frame is second"), Player->Get_CurrentFrame(), 1);
    TestEqual(TEXT("Sampled presentation does not change clock"), Player->Get_ElapsedMilliseconds(), 400.0);
    TestTrue(TEXT("Sampling reset accepted"), Player->Request_SetSamplingIntervalMilliseconds(0.0));
    TestTrue(TEXT("Decimation accepted"), Player->Request_SetSourceFrameDecimation(2));
    TestTrue(TEXT("Seek into middle frame"), Player->Request_SeekMilliseconds(125.0));
    TestEqual(TEXT("Logical middle frame retained"), Player->Get_LogicalFrame(), 1);
    TestEqual(TEXT("Decimated display uses prior source frame"), Player->Get_CurrentFrame(), 0);
    TestTrue(TEXT("Coarse frame decimation accepted"), Player->Request_SetSourceFrameDecimation(3));
    TestTrue(TEXT("Coarse sample interval accepted"),
        Player->Request_SetSamplingIntervalMilliseconds(1000.0));
    TestTrue(TEXT("Seek to first frame for explicit stepping"), Player->Request_SeekMilliseconds(0.0));
    TestTrue(TEXT("Step can inspect next authored frame"), Player->Request_StepFrames(1));
    TestEqual(TEXT("Step bypasses display sampling and decimation"), Player->Get_CurrentFrame(), 1);
    TestTrue(TEXT("Seek resumes display sampling"), Player->Request_SeekMilliseconds(125.0));
    TestEqual(TEXT("Seek restores sampled display"), Player->Get_CurrentFrame(), 0);
    TestTrue(TEXT("Coarse sample interval reset"),
        Player->Request_SetSamplingIntervalMilliseconds(0.0));
    TestTrue(TEXT("Decimation reset accepted"), Player->Request_SetSourceFrameDecimation(1));
    TestTrue(TEXT("Frame step succeeds"), Player->Request_StepFrames(1));
    TestEqual(TEXT("Step reaches next authored frame"), Player->Get_CurrentFrame(), 2);
    auto Anchor = FIntPoint{};
    TestTrue(TEXT("Displayed frame anchor resolves"), Player->TryResolveAnchor(TEXT("origin"), Anchor));
    TestEqual(TEXT("Anchor belongs to stepped frame"), Anchor, FIntPoint(0, 1));

    const auto BeforeBadTime = Player->Get_ElapsedMilliseconds();
    const auto BeforeBadTexture = Player->Get_CurrentTexture();
    TestFalse(TEXT("NaN advance rejects"), Player->Request_AdvanceMilliseconds(
        std::numeric_limits<double>::quiet_NaN()));
    TestFalse(TEXT("Infinite speed rejects"), Player->Request_SetSpeed(
        std::numeric_limits<double>::infinity()));
    TestFalse(TEXT("Zero speed rejects"), Player->Request_SetSpeed(0.0));
    TestFalse(TEXT("Negative sampling rejects"), Player->Request_SetSamplingIntervalMilliseconds(-1.0));
    TestFalse(TEXT("Zero decimation rejects"), Player->Request_SetSourceFrameDecimation(0));
    TestFalse(TEXT("Invalid direction rejects"), Player->Request_SetDirection(0));
    TestEqual(TEXT("Invalid controls keep clock"), Player->Get_ElapsedMilliseconds(), BeforeBadTime);
    TestTrue(TEXT("Invalid controls keep texture"), Player->Get_CurrentTexture() == BeforeBadTexture);
    TestFalse(TEXT("Invalid soft asset path rejects"), Player->Request_LoadAsset(FSoftObjectPath{}));
    TestTrue(TEXT("Rejected asset path keeps generation"), Player->Get_CurrentTexture() == BeforeBadTexture);
    TestTrue(TEXT("Finite large speed is permitted"),
        Player->Request_SetSpeed(std::numeric_limits<double>::max()));
    Player->Request_Play();
    TestFalse(TEXT("Overflowing advance rejects atomically"),
        Player->Request_AdvanceMilliseconds(std::numeric_limits<double>::max()));
    TestEqual(TEXT("Overflowing advance keeps clock"), Player->Get_ElapsedMilliseconds(), BeforeBadTime);
    TestTrue(TEXT("Overflowing advance keeps texture"), Player->Get_CurrentTexture() == BeforeBadTexture);
    TestTrue(TEXT("Speed restores after rejected advance"), Player->Request_SetSpeed(1.0));

    auto WidgetA = TStrongObjectPtr<UCk_PixelSpriteWidget_UE>{NewObject<UCk_PixelSpriteWidget_UE>()};
    auto WidgetB = TStrongObjectPtr<UCk_PixelSpriteWidget_UE>{NewObject<UCk_PixelSpriteWidget_UE>()};
    WidgetA->Request_SetPlayer(Player.Get());
    WidgetB->_RequestedPlayer = Player.Get();
    WidgetB->Request_ApplyRequestedPlayer();
    TestTrue(TEXT("Both widgets display the same generation"),
        WidgetA->Get_DisplayedTexture() == WidgetB->Get_DisplayedTexture());
    const auto BeforeRefresh = Player->Get_ElapsedMilliseconds();
    TestTrue(TEXT("Owner advances shared player once"), Player->Request_AdvanceMilliseconds(10.0));
    TestEqual(TEXT("One owner advance moves clock once"),
        Player->Get_ElapsedMilliseconds(), BeforeRefresh + 10.0);
    const auto AfterAdvance = Player->Get_ElapsedMilliseconds();
    WidgetA->Request_RefreshImage();
    WidgetB->Request_RefreshImage();
    TestEqual(TEXT("Two widget refreshes never advance shared player"),
        Player->Get_ElapsedMilliseconds(), AfterAdvance);
    CollectGarbage(RF_NoFlags);
    TestTrue(TEXT("Player keeps resource texture alive through GC"),
        IsValid(Player->Get_CurrentTexture().Get()));
    TestTrue(TEXT("Widget keeps displayed texture alive through GC"),
        IsValid(WidgetA->Get_DisplayedTexture().Get()));

    const auto OldResource = Player->Get_Resources();
    const auto OldRevision = Player->Get_Revision();
    Manifest._Animations[0]._Frames[0]._DurationMilliseconds = 200.0;
    TestTrue(TEXT("Valid revision publishes"), Asset->Request_ReplaceManifest(Manifest, Diagnostic));
    TestFalse(TEXT("Pending revision plus NaN seek rejects atomically"),
        Player->Request_SeekMilliseconds(std::numeric_limits<double>::quiet_NaN()));
    TestEqual(TEXT("Invalid seek does not publish pending revision"), Player->Get_Revision(), OldRevision);
    TestTrue(TEXT("Invalid seek keeps old resources"), Player->Get_Resources() == OldResource);
    TestTrue(TEXT("Large finite speed accepted for overflow preflight"),
        Player->Request_SetSpeed(std::numeric_limits<double>::max()));
    TestFalse(TEXT("Pending revision plus overflowing advance rejects atomically"),
        Player->Request_AdvanceMilliseconds(std::numeric_limits<double>::max()));
    TestEqual(TEXT("Overflow does not publish pending revision"), Player->Get_Revision(), OldRevision);
    TestTrue(TEXT("Overflow keeps old resources"), Player->Get_Resources() == OldResource);
    TestTrue(TEXT("Speed restored after pending-revision rejection"), Player->Request_SetSpeed(1.0));
    TestTrue(TEXT("Player refreshes named animation"), Player->TryRefreshGeneration(Diagnostic));
    TestEqual(TEXT("Player sees new revision"), Player->Get_Revision(), OldRevision + 1);
    TestTrue(TEXT("Revision replaces coherent resource generation"), Player->Get_Resources() != OldResource);
    WidgetA->Request_RefreshImage();
    TestTrue(TEXT("Widget adopts refreshed texture"),
        WidgetA->Get_DisplayedTexture() == Player->Get_CurrentTexture());
    const auto NewTexture = Player->Get_CurrentTexture();
    const auto NewRevision = Player->Get_Revision();
    Manifest._Animations[0]._Name = TEXT("replacement");
    TestTrue(TEXT("Replacement removing selection publishes"),
        Asset->Request_ReplaceManifest(Manifest, Diagnostic));
    TestFalse(TEXT("Missing named selection rejects refresh"),
        Player->TryRefreshGeneration(Diagnostic));
    TestEqual(TEXT("Failed refresh preserves pinned revision"), Player->Get_Revision(), NewRevision);
    TestTrue(TEXT("Failed refresh preserves old texture"), Player->Get_CurrentTexture() == NewTexture);
    Player->Request_Stop();
    TestFalse(TEXT("Stop pauses the old coherent generation"), Player->Get_IsPlaying());
    TestEqual(TEXT("Stop rewinds retained generation"), Player->Get_CurrentFrame(), 0);

    auto PrecisionAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto PrecisionPlayer = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto PrecisionManifest = ck_pixel_sprite_player_tests::MakeSubUlpManifest();
    if (NOT TestTrue(TEXT("Sub-ULP manifest accepted"),
        PrecisionAsset->Request_ReplaceManifest(PrecisionManifest, Diagnostic)) ||
        NOT TestTrue(TEXT("Sub-ULP player prepares generation"),
        PrecisionPlayer->TrySelectAsset(PrecisionAsset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    const auto Prepared = PrecisionPlayer->Get_PreparedAnimation();
    const auto PrecisionResources = PrecisionPlayer->Get_Resources();
    TestTrue(TEXT("Prepared timeline retained"), Prepared.IsValid());
    TestTrue(TEXT("Sub-ULP sampling accepts one second"),
        PrecisionPlayer->Request_SetSamplingIntervalMilliseconds(1000.0));
    TestTrue(TEXT("Sub-ULP decimation accepts three"),
        PrecisionPlayer->Request_SetSourceFrameDecimation(3));
    for (auto Index = int32{1}; Index <= 3; ++Index)
    {
        TestTrue(TEXT("Forward step reaches each authored frame"), PrecisionPlayer->Request_StepFrames(1));
        TestEqual(TEXT("Manual frame is exact under sampling and decimation"),
            PrecisionPlayer->Get_CurrentFrame(), Index);
        TestTrue(TEXT("Manual frame selects its own preuploaded texture"),
            PrecisionPlayer->Get_CurrentTexture() == PrecisionResources->Get_Texture(Index));
    }
    TestEqual(TEXT("Sub-ULP prefix clock rounds to large first frame"),
        PrecisionPlayer->Get_ElapsedMilliseconds(), 86400000.0);
    TestTrue(TEXT("Manual frame differs from logical clock at sub-ULP boundary"),
        PrecisionPlayer->Get_LogicalFrame() != PrecisionPlayer->Get_CurrentFrame());
    auto PrecisionAnchor = FIntPoint{};
    TestTrue(TEXT("Manual frame anchor resolves"),
        PrecisionPlayer->TryResolveAnchor(TEXT("origin"), PrecisionAnchor));
    TestEqual(TEXT("Manual fourth-frame anchor belongs to inspected frame"),
        PrecisionAnchor, FIntPoint(1, 1));
    for (auto Index = int32{2}; Index >= 0; --Index)
    {
        TestTrue(TEXT("Reverse step uses manual frame as base"), PrecisionPlayer->Request_StepFrames(-1));
        TestEqual(TEXT("Reverse manual inspection reaches authored frame"),
            PrecisionPlayer->Get_CurrentFrame(), Index);
    }
    TestTrue(TEXT("Step returns to tiny second frame"), PrecisionPlayer->Request_StepFrames(1));
    TestTrue(TEXT("Repeated step reaches tiny third frame"), PrecisionPlayer->Request_StepFrames(1));
    PrecisionPlayer->Request_Pause();
    TestTrue(TEXT("Paused advance accepts elapsed input"), PrecisionPlayer->Request_AdvanceMilliseconds(1.0));
    TestEqual(TEXT("Paused advance preserves manual inspection"), PrecisionPlayer->Get_CurrentFrame(), 2);
    PrecisionPlayer->Request_Play();
    TestTrue(TEXT("Zero-motion advance accepted"), PrecisionPlayer->Request_AdvanceMilliseconds(0.0));
    TestEqual(TEXT("Zero-motion advance preserves manual inspection"), PrecisionPlayer->Get_CurrentFrame(), 2);
    TestTrue(TEXT("Advances reuse prepared timeline"), PrecisionPlayer->Get_PreparedAnimation() == Prepared);
    TestFalse(TEXT("Invalid seek rejects during manual inspection"),
        PrecisionPlayer->Request_SeekMilliseconds(std::numeric_limits<double>::quiet_NaN()));
    TestEqual(TEXT("Invalid seek preserves manual frame"), PrecisionPlayer->Get_CurrentFrame(), 2);
    TestTrue(TEXT("Invalid seek preserves resources"), PrecisionPlayer->Get_Resources() == PrecisionResources);
    TestTrue(TEXT("Invalid seek preserves prepared timeline"), PrecisionPlayer->Get_PreparedAnimation() == Prepared);
    TestTrue(TEXT("Actual motion resumes time-driven presentation"),
        PrecisionPlayer->Request_AdvanceMilliseconds(1.0));
    TestEqual(TEXT("Actual motion clears manual inspection"), PrecisionPlayer->Get_CurrentFrame(), 0);
    TestTrue(TEXT("Actual motion reuses prepared timeline"), PrecisionPlayer->Get_PreparedAnimation() == Prepared);
    TestTrue(TEXT("Seek rewinds for another manual inspection"), PrecisionPlayer->Request_SeekMilliseconds(0.0));
    TestTrue(TEXT("Manual inspection reaches third frame again"), PrecisionPlayer->Request_StepFrames(2));
    TestEqual(TEXT("Manual frame restored before refresh rejection"), PrecisionPlayer->Get_CurrentFrame(), 2);

    const auto PinnedRevision = PrecisionPlayer->Get_Revision();
    PrecisionManifest._Animations[0]._Name = TEXT("removed");
    TestTrue(TEXT("Removing selected animation publishes to asset"),
        PrecisionAsset->Request_ReplaceManifest(PrecisionManifest, Diagnostic));
    TestFalse(TEXT("Missing selected animation rejects refresh"),
        PrecisionPlayer->TryRefreshGeneration(Diagnostic));
    TestEqual(TEXT("Failed refresh preserves manual frame"), PrecisionPlayer->Get_CurrentFrame(), 2);
    TestTrue(TEXT("Failed refresh preserves prepared timeline"), PrecisionPlayer->Get_PreparedAnimation() == Prepared);
    TestTrue(TEXT("Failed refresh preserves resources"), PrecisionPlayer->Get_Resources() == PrecisionResources);
    TestEqual(TEXT("Failed refresh preserves revision"), PrecisionPlayer->Get_Revision(), PinnedRevision);
    TestFalse(TEXT("Missing replacement selection rejects without publication"),
        PrecisionPlayer->Request_SelectAnimation(TEXT("missing")));
    TestEqual(TEXT("Rejected selection preserves manual frame"), PrecisionPlayer->Get_CurrentFrame(), 2);
    TestTrue(TEXT("Rejected selection preserves prepared timeline"),
        PrecisionPlayer->Get_PreparedAnimation() == Prepared);
    TestTrue(TEXT("Rejected selection preserves resources"), PrecisionPlayer->Get_Resources() == PrecisionResources);
    TestTrue(TEXT("Large finite speed accepted during manual inspection"),
        PrecisionPlayer->Request_SetSpeed(std::numeric_limits<double>::max()));
    PrecisionPlayer->Request_Play();
    TestFalse(TEXT("Overflowing advance rejects before pending refresh"),
        PrecisionPlayer->Request_AdvanceMilliseconds(std::numeric_limits<double>::max()));
    TestEqual(TEXT("Overflow rejection preserves manual frame"), PrecisionPlayer->Get_CurrentFrame(), 2);
    TestTrue(TEXT("Overflow rejection preserves prepared timeline"),
        PrecisionPlayer->Get_PreparedAnimation() == Prepared);
    TestTrue(TEXT("Overflow rejection preserves resources"), PrecisionPlayer->Get_Resources() == PrecisionResources);
    TestEqual(TEXT("Overflow rejection preserves revision"), PrecisionPlayer->Get_Revision(), PinnedRevision);
    TestTrue(TEXT("Speed restores after overflow rejection"), PrecisionPlayer->Request_SetSpeed(1.0));
    PrecisionManifest._Animations[0]._Name = TEXT("walk");
    TestTrue(TEXT("Restored selected animation publishes"),
        PrecisionAsset->Request_ReplaceManifest(PrecisionManifest, Diagnostic));
    TestTrue(TEXT("Successful refresh atomically replaces generation"),
        PrecisionPlayer->TryRefreshGeneration(Diagnostic));
    TestTrue(TEXT("Refresh replaces prepared timeline"), PrecisionPlayer->Get_PreparedAnimation() != Prepared);
    TestTrue(TEXT("Refresh replaces resources"), PrecisionPlayer->Get_Resources() != PrecisionResources);
    TestEqual(TEXT("Successful animation replacement clears manual frame"),
        PrecisionPlayer->Get_CurrentFrame(), 0);
    PrecisionPlayer->Request_Play();
    TestTrue(TEXT("Actual advance resumes sampled presentation"),
        PrecisionPlayer->Request_AdvanceMilliseconds(1.0));
    TestEqual(TEXT("Advancing retains sampled display"), PrecisionPlayer->Get_CurrentFrame(), 0);
    TestTrue(TEXT("Seek to first frame succeeds"), PrecisionPlayer->Request_SeekMilliseconds(0.0));
    TestEqual(TEXT("Seek restores time-driven display"), PrecisionPlayer->Get_CurrentFrame(), 0);

    TestTrue(TEXT("Tiny sampling interval is valid at zero"),
        PrecisionPlayer->Request_SetSamplingIntervalMilliseconds(1e-302));
    const auto BeforeFiniteRejectedResources = PrecisionPlayer->Get_Resources();
    const auto BeforeFiniteRejectedPrepared = PrecisionPlayer->Get_PreparedAnimation();
    const auto BeforeFiniteRejectedRevision = PrecisionPlayer->Get_Revision();
    PrecisionManifest._Animations[0]._Frames[3]._DurationMilliseconds = 101.0;
    TestTrue(TEXT("Pending revision for finite presentation overflow"),
        PrecisionAsset->Request_ReplaceManifest(PrecisionManifest, Diagnostic));
    TestFalse(TEXT("Finite seek rejects overflowing sampling quotient before refresh"),
        PrecisionPlayer->Request_SeekMilliseconds(std::numeric_limits<double>::max()));
    TestEqual(TEXT("Finite seek preserves zero clock"), PrecisionPlayer->Get_ElapsedMilliseconds(), 0.0);
    TestEqual(TEXT("Finite seek preserves current frame"), PrecisionPlayer->Get_CurrentFrame(), 0);
    TestEqual(TEXT("Finite seek preserves revision"), PrecisionPlayer->Get_Revision(), BeforeFiniteRejectedRevision);
    TestTrue(TEXT("Finite seek preserves resources"),
        PrecisionPlayer->Get_Resources() == BeforeFiniteRejectedResources);
    TestTrue(TEXT("Finite seek preserves prepared timeline"),
        PrecisionPlayer->Get_PreparedAnimation() == BeforeFiniteRejectedPrepared);
    PrecisionPlayer->Request_Play();
    TestFalse(TEXT("Finite advance rejects overflowing sampling quotient before refresh"),
        PrecisionPlayer->Request_AdvanceMilliseconds(std::numeric_limits<double>::max()));
    TestEqual(TEXT("Finite advance preserves zero clock"), PrecisionPlayer->Get_ElapsedMilliseconds(), 0.0);
    TestEqual(TEXT("Finite advance preserves current frame"), PrecisionPlayer->Get_CurrentFrame(), 0);
    TestEqual(TEXT("Finite advance preserves revision"), PrecisionPlayer->Get_Revision(), BeforeFiniteRejectedRevision);
    TestTrue(TEXT("Finite advance preserves resources"),
        PrecisionPlayer->Get_Resources() == BeforeFiniteRejectedResources);
    TestTrue(TEXT("Finite advance preserves prepared timeline"),
        PrecisionPlayer->Get_PreparedAnimation() == BeforeFiniteRejectedPrepared);

    TestTrue(TEXT("Refresh pending revision after rejection"), PrecisionPlayer->TryRefreshGeneration(Diagnostic));
    TestTrue(TEXT("Manual step bypasses tiny sampling interval"), PrecisionPlayer->Request_StepFrames(2));
    TestEqual(TEXT("Manual third frame is inspectable"), PrecisionPlayer->Get_CurrentFrame(), 2);
    const auto BeforeManualRejectedResources = PrecisionPlayer->Get_Resources();
    const auto BeforeManualRejectedPrepared = PrecisionPlayer->Get_PreparedAnimation();
    const auto BeforeManualRejectedRevision = PrecisionPlayer->Get_Revision();
    const auto BeforeManualRejectedClock = PrecisionPlayer->Get_ElapsedMilliseconds();
    PrecisionPlayer->Request_Play();
    PrecisionManifest._Animations[0]._Frames[3]._DurationMilliseconds = 102.0;
    TestTrue(TEXT("Second pending revision for manual inspection"),
        PrecisionAsset->Request_ReplaceManifest(PrecisionManifest, Diagnostic));
    TestFalse(TEXT("Finite seek rejects while manual inspection is active"),
        PrecisionPlayer->Request_SeekMilliseconds(std::numeric_limits<double>::max()));
    TestFalse(TEXT("Finite advance rejects while manual inspection is active"),
        PrecisionPlayer->Request_AdvanceMilliseconds(std::numeric_limits<double>::max()));
    TestEqual(TEXT("Finite failures preserve manual frame"), PrecisionPlayer->Get_CurrentFrame(), 2);
    TestEqual(TEXT("Finite failures preserve manual clock"),
        PrecisionPlayer->Get_ElapsedMilliseconds(), BeforeManualRejectedClock);
    TestEqual(TEXT("Finite failures preserve pinned revision"),
        PrecisionPlayer->Get_Revision(), BeforeManualRejectedRevision);
    TestTrue(TEXT("Finite failures preserve pinned resources"),
        PrecisionPlayer->Get_Resources() == BeforeManualRejectedResources);
    TestTrue(TEXT("Finite failures preserve pinned prepared timeline"),
        PrecisionPlayer->Get_PreparedAnimation() == BeforeManualRejectedPrepared);
    TestTrue(TEXT("Seek zero evaluates pending generation before old unrepresentable sample"),
        PrecisionPlayer->Request_SeekMilliseconds(0.0));
    TestEqual(TEXT("Seek recovery clears manual frame"), PrecisionPlayer->Get_CurrentFrame(), 0);
    TestEqual(TEXT("Seek recovery publishes pending revision"),
        PrecisionPlayer->Get_Revision(), BeforeManualRejectedRevision + 1);
    TestTrue(TEXT("Seek recovery replaces prepared timeline"),
        PrecisionPlayer->Get_PreparedAnimation() != BeforeManualRejectedPrepared);

    auto LoopToOnceAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto LoopToOncePlayer = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto LoopToOnceManifest = ck_pixel_sprite_player_tests::MakeManifest();
    if (NOT TestTrue(TEXT("Loop-to-once asset accepted"),
        LoopToOnceAsset->Request_ReplaceManifest(LoopToOnceManifest, Diagnostic)) ||
        NOT TestTrue(TEXT("Loop-to-once player selected"),
        LoopToOncePlayer->TrySelectAsset(LoopToOnceAsset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    TestTrue(TEXT("Loop-to-once reverse direction accepted"), LoopToOncePlayer->Request_SetDirection(-1));
    TestTrue(TEXT("Loop-to-once reverse start seek accepted"), LoopToOncePlayer->Request_SeekMilliseconds(125.0));
    LoopToOncePlayer->Request_Play();
    LoopToOnceManifest._Animations[0]._Playback = ECk_PixelSprite_Playback::Once;
    TestTrue(TEXT("Loop-to-once revision published"),
        LoopToOnceAsset->Request_ReplaceManifest(LoopToOnceManifest, Diagnostic));
    TestTrue(TEXT("Reverse advance follows replacement once mode"),
        LoopToOncePlayer->Request_AdvanceMilliseconds(250.0));
    TestEqual(TEXT("Replacement once clamps reverse clock to zero"),
        LoopToOncePlayer->Get_ElapsedMilliseconds(), 0.0);
    TestTrue(TEXT("Replacement once reports reverse completion"), LoopToOncePlayer->Get_IsComplete());
    TestFalse(TEXT("Replacement once stops at reverse endpoint"), LoopToOncePlayer->Get_IsPlaying());

    auto OnceToLoopAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto OnceToLoopPlayer = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto OnceToLoopManifest = ck_pixel_sprite_player_tests::MakeManifest();
    OnceToLoopManifest._Animations[0]._Playback = ECk_PixelSprite_Playback::Once;
    if (NOT TestTrue(TEXT("Once-to-loop asset accepted"),
        OnceToLoopAsset->Request_ReplaceManifest(OnceToLoopManifest, Diagnostic)) ||
        NOT TestTrue(TEXT("Once-to-loop player selected"),
        OnceToLoopPlayer->TrySelectAsset(OnceToLoopAsset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    TestTrue(TEXT("Once-to-loop reverse direction accepted"), OnceToLoopPlayer->Request_SetDirection(-1));
    TestTrue(TEXT("Once-to-loop reverse start seek accepted"), OnceToLoopPlayer->Request_SeekMilliseconds(125.0));
    OnceToLoopPlayer->Request_Play();
    OnceToLoopManifest._Animations[0]._Playback = ECk_PixelSprite_Playback::Loop;
    TestTrue(TEXT("Once-to-loop revision published"),
        OnceToLoopAsset->Request_ReplaceManifest(OnceToLoopManifest, Diagnostic));
    TestTrue(TEXT("Reverse advance follows replacement loop mode"),
        OnceToLoopPlayer->Request_AdvanceMilliseconds(250.0));
    TestEqual(TEXT("Replacement loop retains signed reverse clock"),
        OnceToLoopPlayer->Get_ElapsedMilliseconds(), -125.0);
    TestEqual(TEXT("Replacement loop wraps to authored final frame"), OnceToLoopPlayer->Get_CurrentFrame(), 2);
    TestTrue(TEXT("Replacement loop keeps playback active"), OnceToLoopPlayer->Get_IsPlaying());

    auto TinyModeAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto TinyModePlayer = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto TinyModeManifest = ck_pixel_sprite_player_tests::MakeManifest();
    TinyModeManifest._Animations[0]._Playback = ECk_PixelSprite_Playback::Once;
    if (NOT TestTrue(TEXT("Tiny-sampling mode asset accepted"),
        TinyModeAsset->Request_ReplaceManifest(TinyModeManifest, Diagnostic)) ||
        NOT TestTrue(TEXT("Tiny-sampling mode player selected"),
        TinyModePlayer->TrySelectAsset(TinyModeAsset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    TestTrue(TEXT("Tiny-sampling interval accepted at zero"),
        TinyModePlayer->Request_SetSamplingIntervalMilliseconds(1e-300));
    TestTrue(TEXT("Tiny positive seek accepted"), TinyModePlayer->Request_SeekMilliseconds(1e-300));
    TestTrue(TEXT("Tiny-sampling reverse direction accepted"), TinyModePlayer->Request_SetDirection(-1));
    TinyModePlayer->Request_Play();
    const auto TinyModeResources = TinyModePlayer->Get_Resources();
    const auto TinyModePrepared = TinyModePlayer->Get_PreparedAnimation();
    const auto TinyModeRevision = TinyModePlayer->Get_Revision();
    TinyModeManifest._Animations[0]._Playback = ECk_PixelSprite_Playback::Loop;
    TestTrue(TEXT("Tiny-sampling loop revision published"),
        TinyModeAsset->Request_ReplaceManifest(TinyModeManifest, Diagnostic));
    TestFalse(TEXT("New loop mode rejects overflowing reverse sample atomically"),
        TinyModePlayer->Request_AdvanceMilliseconds(std::numeric_limits<double>::max()));
    TestEqual(TEXT("New-mode rejection preserves tiny clock"),
        TinyModePlayer->Get_ElapsedMilliseconds(), 1e-300);
    TestEqual(TEXT("New-mode rejection preserves pinned revision"),
        TinyModePlayer->Get_Revision(), TinyModeRevision);
    TestTrue(TEXT("New-mode rejection preserves resources"), TinyModePlayer->Get_Resources() == TinyModeResources);
    TestTrue(TEXT("New-mode rejection preserves prepared timeline"),
        TinyModePlayer->Get_PreparedAnimation() == TinyModePrepared);
    TestTrue(TEXT("New-mode rejection preserves playback state"), TinyModePlayer->Get_IsPlaying());

    auto OverrideAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto OverridePlayer = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto OverrideManifest = ck_pixel_sprite_player_tests::MakeManifest();
    if (NOT TestTrue(TEXT("Playback override asset accepted"),
        OverrideAsset->Request_ReplaceManifest(OverrideManifest, Diagnostic)) ||
        NOT TestTrue(TEXT("Playback override player selected"),
        OverridePlayer->TrySelectAsset(OverrideAsset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    TestTrue(TEXT("Once playback override accepted"),
        OverridePlayer->Request_SetPlayback(ECk_PixelSprite_PlayerPlayback::Once));
    TestTrue(TEXT("Override reverse direction accepted"), OverridePlayer->Request_SetDirection(-1));
    TestTrue(TEXT("Override reverse start seek accepted"), OverridePlayer->Request_SeekMilliseconds(125.0));
    OverridePlayer->Request_Play();
    OverrideManifest._Animations[0]._Playback = ECk_PixelSprite_Playback::PingPong;
    TestTrue(TEXT("Authored playback revision published under override"),
        OverrideAsset->Request_ReplaceManifest(OverrideManifest, Diagnostic));
    TestTrue(TEXT("Reverse advance retains requested once override"),
        OverridePlayer->Request_AdvanceMilliseconds(250.0));
    TestEqual(TEXT("Once override clamps reverse endpoint"), OverridePlayer->Get_ElapsedMilliseconds(), 0.0);
    TestTrue(TEXT("Once override completes despite authored ping-pong"), OverridePlayer->Get_IsComplete());

    auto RestartAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto RestartPlayer = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto RestartManifest = ck_pixel_sprite_player_tests::MakeManifest();
    RestartManifest._Animations[0]._Playback = ECk_PixelSprite_Playback::Once;
    if (NOT TestTrue(TEXT("Reverse restart asset accepted"),
        RestartAsset->Request_ReplaceManifest(RestartManifest, Diagnostic)) ||
        NOT TestTrue(TEXT("Reverse restart player selected"),
        RestartPlayer->TrySelectAsset(RestartAsset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    TestTrue(TEXT("Reverse restart direction accepted"), RestartPlayer->Request_SetDirection(-1));
    RestartManifest._Animations[0]._Frames[2]._DurationMilliseconds = 475.0;
    TestTrue(TEXT("Longer replacement cycle published"),
        RestartAsset->Request_ReplaceManifest(RestartManifest, Diagnostic));
    TestTrue(TEXT("Reverse restart evaluates replacement cycle"), RestartPlayer->Request_Restart());
    TestEqual(TEXT("Reverse restart uses replacement 850 ms duration"),
        RestartPlayer->Get_ElapsedMilliseconds(), 850.0);
    TestTrue(TEXT("Reverse restart starts playback"), RestartPlayer->Get_IsPlaying());
    TestFalse(TEXT("Reverse restart is not complete"), RestartPlayer->Get_IsComplete());

    auto EmptyPlayer = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    TestFalse(TEXT("Unselected player rejects zero advance"), EmptyPlayer->Request_AdvanceMilliseconds(0.0));

    auto PausedAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    auto PausedPlayer = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>()};
    auto PausedManifest = ck_pixel_sprite_player_tests::MakeManifest();
    if (NOT TestTrue(TEXT("Paused refresh asset accepted"),
        PausedAsset->Request_ReplaceManifest(PausedManifest, Diagnostic)) ||
        NOT TestTrue(TEXT("Paused refresh player selected"),
        PausedPlayer->TrySelectAsset(PausedAsset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text());
        return false;
    }
    TestTrue(TEXT("Paused player can inspect second frame"), PausedPlayer->Request_StepFrames(1));
    TestEqual(TEXT("Paused manual frame selected"), PausedPlayer->Get_CurrentFrame(), 1);
    TestTrue(TEXT("Zero advance with unchanged revision succeeds"),
        PausedPlayer->Request_AdvanceMilliseconds(0.0));
    TestEqual(TEXT("Unchanged paused advance preserves manual frame"), PausedPlayer->Get_CurrentFrame(), 1);
    const auto PausedResources = PausedPlayer->Get_Resources();
    const auto PausedPrepared = PausedPlayer->Get_PreparedAnimation();
    const auto PausedRevision = PausedPlayer->Get_Revision();
    const auto PausedClock = PausedPlayer->Get_ElapsedMilliseconds();
    PausedManifest._Animations[0]._Frames[0]._DurationMilliseconds = 200.0;
    TestTrue(TEXT("Paused player replacement revision published"),
        PausedAsset->Request_ReplaceManifest(PausedManifest, Diagnostic));
    TestTrue(TEXT("Paused zero advance adopts pending revision"),
        PausedPlayer->Request_AdvanceMilliseconds(0.0));
    TestEqual(TEXT("Paused refresh keeps authored clock"), PausedPlayer->Get_ElapsedMilliseconds(), PausedClock);
    TestEqual(TEXT("Paused refresh adopts new logical display"), PausedPlayer->Get_CurrentFrame(), 0);
    TestEqual(TEXT("Paused refresh publishes revision"), PausedPlayer->Get_Revision(), PausedRevision + 1);
    TestTrue(TEXT("Paused refresh replaces resources"), PausedPlayer->Get_Resources() != PausedResources);
    TestTrue(TEXT("Paused refresh replaces prepared timeline"),
        PausedPlayer->Get_PreparedAnimation() != PausedPrepared);
    TestFalse(TEXT("Paused refresh does not start playback"), PausedPlayer->Get_IsPlaying());
    return true;
}

#endif
