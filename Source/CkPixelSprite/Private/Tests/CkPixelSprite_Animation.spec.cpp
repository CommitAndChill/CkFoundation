#include "CkPixelSprite/CkPixelSprite_Data.h"
#include "CkCore/Macros/CkMacros.h"
#include "Misc/AutomationTest.h"

#include <cmath>
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_sprite_animation_tests
{
    auto
        MakeAnimation(
            const TArray<double>& InMilliseconds,
            const ECk_PixelSprite_Playback InPlayback = ECk_PixelSprite_Playback::Loop)
        -> FCk_PixelSprite_Animation
    {
        auto Animation = FCk_PixelSprite_Animation{};
        Animation._Playback = InPlayback;
        for (const auto Milliseconds : InMilliseconds)
        {
            auto Frame = FCk_PixelSprite_Frame{};
            Frame._DurationMilliseconds = Milliseconds;
            Animation._Frames.Add(MoveTemp(Frame));
        }
        return Animation;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_AnimationTiming,
    "Ck.PixelSprite.AnimationAuthoringTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_PreparedAnimation,
    "Ck.PixelSprite.PreparedAnimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_PreparedAnimation::
    RunTest(const FString&)
        -> bool
{
    using namespace ck::pixel_sprite;
    auto Animation = ck_pixel_sprite_animation_tests::MakeAnimation({0.1, 0.2, 0.3});
    auto Prepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT TestTrue(TEXT("Prepare valid animation"), TryPrepare_Animation(Animation, Prepared)) ||
        NOT TestTrue(TEXT("Prepared animation retained"), Prepared.IsValid()))
    {
        return false;
    }

    for (const auto Playback : {ECk_PixelSprite_Playback::Loop,
        ECk_PixelSprite_Playback::Once, ECk_PixelSprite_Playback::PingPong})
    {
        Animation._Playback = Playback;
        auto ModePrepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
        if (NOT TestTrue(TEXT("Prepare playback mode"), TryPrepare_Animation(Animation, ModePrepared)) ||
            NOT TestTrue(TEXT("Prepared playback mode valid"), ModePrepared.IsValid()))
        {
            return false;
        }
        for (const auto Milliseconds : {-1e308, -0.8, -0.3, -0.1, 0.0, 0.1,
            std::nextafter(0.3, 0.0), 0.3, std::nextafter(0.3, 1.0),
            0.6, 0.8, 1e308})
        {
            auto FromPrepared = FCk_PixelSprite_FrameSelection{};
            auto FromAuthored = FCk_PixelSprite_FrameSelection{};
            if (NOT TestTrue(TEXT("Prepared timeline evaluates"),
                TryEvaluate_PreparedAnimationMilliseconds(*ModePrepared, Milliseconds, FromPrepared)) ||
                NOT TestTrue(TEXT("Authored timeline evaluates"),
                    TryEvaluate_AnimationMilliseconds(Animation, Milliseconds, FromAuthored)))
            {
                return false;
            }
            TestEqual(TEXT("Prepared frame index matches authored"), FromPrepared._Index, FromAuthored._Index);
            TestEqual(TEXT("Prepared local time matches authored"),
                FromPrepared._FrameTime.Get_Seconds(), FromAuthored._FrameTime.Get_Seconds());
            TestEqual(TEXT("Prepared cycle matches authored"),
                FromPrepared._CycleDuration.Get_Seconds(), FromAuthored._CycleDuration.Get_Seconds());
            TestEqual(TEXT("Prepared completion matches authored"),
                FromPrepared._Complete, FromAuthored._Complete);
        }
    }

    auto Start = double{-7.0};
    TestTrue(TEXT("First frame begins at zero"),
        TryGet_PreparedFrameStartMilliseconds(*Prepared, 0, Start));
    TestEqual(TEXT("First frame prefix"), Start, 0.0);
    TestTrue(TEXT("Third frame prefix converts exact decimal"),
        TryGet_PreparedFrameStartMilliseconds(*Prepared, 2, Start));
    TestEqual(TEXT("Decimal prefix rounds to 0.3 milliseconds"), Start, 0.3);
    TestTrue(TEXT("Decimal prefix differs from binary double addition"), Start != 0.1 + 0.2);
    Start = -7.0;
    TestFalse(TEXT("Negative frame ordinal rejects"),
        TryGet_PreparedFrameStartMilliseconds(*Prepared, -1, Start));
    TestFalse(TEXT("Out-of-range frame ordinal rejects"),
        TryGet_PreparedFrameStartMilliseconds(*Prepared, 3, Start));
    TestEqual(TEXT("Rejected ordinal preserves output"), Start, -7.0);

    const auto SubUlp = ck_pixel_sprite_animation_tests::MakeAnimation(
        {86400000.0, 1e-300, 2e-300, 100.0});
    auto SubUlpPrepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT TestTrue(TEXT("Prepare sub-ULP prefixes"), TryPrepare_Animation(SubUlp, SubUlpPrepared)) ||
        NOT TestTrue(TEXT("Sub-ULP timeline retained"), SubUlpPrepared.IsValid()))
    {
        return false;
    }
    auto FirstTinyStart = double{-1.0};
    auto SecondTinyStart = double{-1.0};
    if (NOT TestTrue(TEXT("First tiny frame start converts"),
        TryGet_PreparedFrameStartMilliseconds(*SubUlpPrepared, 1, FirstTinyStart)) ||
        NOT TestTrue(TEXT("Second tiny frame start converts"),
            TryGet_PreparedFrameStartMilliseconds(*SubUlpPrepared, 2, SecondTinyStart)))
    {
        return false;
    }
    TestEqual(TEXT("First tiny prefix rounds to large start"), FirstTinyStart, 86400000.0);
    TestEqual(TEXT("Distinct exact prefixes may share approximate double"),
        SecondTinyStart, FirstTinyStart);
    auto TinyBoundary = FCk_PixelSprite_FrameSelection{};
    if (TestTrue(TEXT("Exact tiny boundary remains evaluable"),
        TryEvaluate_PreparedAnimationMilliseconds(*SubUlpPrepared, 86400000.0, TinyBoundary)))
    {
        TestEqual(TEXT("Exact large boundary selects first tiny frame"), TinyBoundary._Index, 1);
    }

    auto MaxFrames = FCk_PixelSprite_Animation{};
    MaxFrames._Playback = ECk_PixelSprite_Playback::PingPong;
    MaxFrames._Frames.Reserve(4096);
    for (auto Index = int32{0}; Index < 4096; ++Index)
    {
        auto Frame = FCk_PixelSprite_Frame{};
        Frame._DurationMilliseconds = 1.0;
        MaxFrames._Frames.Add(MoveTemp(Frame));
    }
    auto MaxPrepared = TSharedPtr<const FCk_PixelSprite_PreparedAnimation>{};
    if (NOT TestTrue(TEXT("Prepare maximum bounded ping-pong"),
        TryPrepare_Animation(MaxFrames, MaxPrepared)) ||
        NOT TestTrue(TEXT("Maximum timeline retained"), MaxPrepared.IsValid()))
    {
        return false;
    }
    for (const auto& Expected : {
        TPair<double, int32>{0.0, 0},
        TPair<double, int32>{4095.0, 4095},
        TPair<double, int32>{4096.0, 4094},
        TPair<double, int32>{8189.0, 1},
        TPair<double, int32>{8190.0, 0},
        TPair<double, int32>{-1.0, 1},
        TPair<double, int32>{-8190.0, 0}})
    {
        auto Selection = FCk_PixelSprite_FrameSelection{};
        if (TestTrue(TEXT("Maximum timeline boundary evaluates"),
            TryEvaluate_PreparedAnimationMilliseconds(*MaxPrepared, Expected.Key, Selection)))
        {
            TestEqual(TEXT("Maximum timeline chooses expected visit"), Selection._Index, Expected.Value);
            TestEqual(TEXT("Maximum timeline cycle seconds"), Selection._CycleDuration.Get_Seconds(), 8.19);
        }
    }
    for (const auto HugeMilliseconds : {-1e308, 1e308})
    {
        auto Selection = FCk_PixelSprite_FrameSelection{};
        if (TestTrue(TEXT("Maximum timeline huge signed time evaluates"),
            TryEvaluate_PreparedAnimationMilliseconds(*MaxPrepared, HugeMilliseconds, Selection)))
        {
            TestTrue(TEXT("Maximum timeline huge time selects valid frame"),
                Selection._Index >= 0 && Selection._Index < 4096);
            TestTrue(TEXT("Maximum timeline huge time has bounded local seconds"),
                Selection._FrameTime.Get_Seconds() >= 0.0 &&
                Selection._FrameTime.Get_Seconds() < 0.001);
        }
    }

    auto BeforeMutation = FCk_PixelSprite_FrameSelection{};
    if (NOT TestTrue(TEXT("Prepared snapshot evaluates before authored mutation"),
        TryEvaluate_PreparedAnimationMilliseconds(*Prepared, 0.3, BeforeMutation)))
    {
        return false;
    }
    Animation._Frames[0]._DurationMilliseconds = 100.0;
    Animation._Playback = ECk_PixelSprite_Playback::Once;
    auto AfterMutation = FCk_PixelSprite_FrameSelection{};
    if (NOT TestTrue(TEXT("Prepared snapshot evaluates after authored mutation"),
        TryEvaluate_PreparedAnimationMilliseconds(*Prepared, 0.3, AfterMutation)))
    {
        return false;
    }
    TestEqual(TEXT("Prepared index is immutable"), AfterMutation._Index, BeforeMutation._Index);
    TestEqual(TEXT("Prepared local time is immutable"),
        AfterMutation._FrameTime.Get_Seconds(), BeforeMutation._FrameTime.Get_Seconds());
    TestEqual(TEXT("Prepared cycle is immutable"),
        AfterMutation._CycleDuration.Get_Seconds(), BeforeMutation._CycleDuration.Get_Seconds());
    TestEqual(TEXT("Prepared playback is immutable"), AfterMutation._Complete, BeforeMutation._Complete);

    Animation._Frames[0]._DurationMilliseconds = 0.0;
    TestFalse(TEXT("Invalid preparation rejects"), TryPrepare_Animation(Animation, Prepared));
    TestTrue(TEXT("Failed preparation preserves prepared handle"), Prepared.IsValid());
    auto AfterRejectedPreparation = FCk_PixelSprite_FrameSelection{};
    if (NOT TestTrue(TEXT("Prior prepared handle remains usable"),
        TryEvaluate_PreparedAnimationMilliseconds(*Prepared, 0.3, AfterRejectedPreparation)))
    {
        return false;
    }
    TestEqual(TEXT("Failed preparation preserves old frame"),
        AfterRejectedPreparation._Index, BeforeMutation._Index);

    auto Sentinel = FCk_PixelSprite_FrameSelection{};
    Sentinel._Index = 17;
    Sentinel._FrameTime = FCk_Time{7.0};
    Sentinel._CycleDuration = FCk_Time{11.0};
    Sentinel._Complete = true;
    TestFalse(TEXT("Nonfinite prepared time rejects"),
        TryEvaluate_PreparedAnimationMilliseconds(*Prepared,
            std::numeric_limits<double>::quiet_NaN(), Sentinel));
    TestEqual(TEXT("Rejected prepared time preserves index"), Sentinel._Index, 17);
    TestEqual(TEXT("Rejected prepared time preserves local time"), Sentinel._FrameTime.Get_Seconds(), 7.0);
    TestEqual(TEXT("Rejected prepared time preserves cycle"), Sentinel._CycleDuration.Get_Seconds(), 11.0);
    TestTrue(TEXT("Rejected prepared time preserves completion"), Sentinel._Complete);
    return true;
}

auto
    FCkTest_PixelSprite_AnimationTiming::
    RunTest(const FString&)
        -> bool
{
    const auto SelectMilliseconds = [this](
        const FCk_PixelSprite_Animation& InAnimation,
        const double InMilliseconds,
        const int32 InExpectedIndex,
        const bool bInExpectedComplete = false) -> void
    {
        auto Selection = FCk_PixelSprite_FrameSelection{};
        if (NOT TestTrue(TEXT("Animation milliseconds evaluate"),
            ck::pixel_sprite::TryEvaluate_AnimationMilliseconds(InAnimation, InMilliseconds, Selection)))
        {
            return;
        }
        TestEqual(TEXT("Animation milliseconds choose authored frame"), Selection._Index, InExpectedIndex);
        TestEqual(TEXT("Animation completion"), Selection._Complete, bInExpectedComplete);
        if (InAnimation._Frames.IsValidIndex(Selection._Index))
        {
            const auto FrameSeconds = Selection._FrameTime.Get_Seconds();
            const auto DurationSeconds = InAnimation._Frames[Selection._Index]._DurationMilliseconds / 1000.0;
            TestTrue(TEXT("Frame time is nonnegative"), FrameSeconds >= 0.0);
            TestTrue(TEXT("Frame time does not exceed authored duration"), FrameSeconds <= DurationSeconds);
        }
    };

    auto Integer = ck_pixel_sprite_animation_tests::MakeAnimation({100.0, 200.0, 300.0});
    SelectMilliseconds(Integer, 0.0, 0);
    SelectMilliseconds(Integer, 100.0, 1);
    SelectMilliseconds(Integer, 300.0, 2);
    SelectMilliseconds(Integer, 600.0, 0);
    SelectMilliseconds(Integer, 800.0, 1);
    Integer._Playback = ECk_PixelSprite_Playback::PingPong;
    SelectMilliseconds(Integer, 300.0, 2);
    SelectMilliseconds(Integer, 600.0, 1);
    SelectMilliseconds(Integer, 800.0, 0);

    auto Fractional = ck_pixel_sprite_animation_tests::MakeAnimation({0.1, 0.2, 0.3});
    SelectMilliseconds(Fractional, std::nextafter(0.3, 0.0), 1);
    SelectMilliseconds(Fractional, 0.3, 2);
    SelectMilliseconds(Fractional, std::nextafter(0.3, 1.0), 2);
    SelectMilliseconds(Fractional, std::nextafter(0.6, 0.0), 2);
    SelectMilliseconds(Fractional, 0.6, 0);
    SelectMilliseconds(Fractional, std::nextafter(0.6, 1.0), 0);
    SelectMilliseconds(Fractional, 0.8, 1);
    Fractional._Playback = ECk_PixelSprite_Playback::PingPong;
    SelectMilliseconds(Fractional, 0.3, 2);
    SelectMilliseconds(Fractional, 0.6, 1);
    SelectMilliseconds(Fractional, std::nextafter(0.8, 0.0), 1);
    SelectMilliseconds(Fractional, 0.8, 0);
    SelectMilliseconds(Fractional, std::nextafter(0.8, 1.0), 0);

    auto Once = ck_pixel_sprite_animation_tests::MakeAnimation(
        {0.1, 0.2}, ECk_PixelSprite_Playback::Once);
    SelectMilliseconds(Once, std::nextafter(0.3, 0.0), 1);
    SelectMilliseconds(Once, 0.3, 1, true);
    SelectMilliseconds(Once, std::nextafter(0.3, 1.0), 1, true);
    auto Completed = FCk_PixelSprite_FrameSelection{};
    if (TestTrue(TEXT("One-shot decimal end evaluates"),
        ck::pixel_sprite::TryEvaluate_AnimationMilliseconds(Once, 0.3, Completed)))
    { TestEqual(TEXT("One-shot holds full final frame duration"),
        Completed._FrameTime.Get_Seconds(), Once._Frames.Last().Get_Duration().Get_Seconds()); }

    auto Tiny = ck_pixel_sprite_animation_tests::MakeAnimation({1e-10, 1e-10});
    SelectMilliseconds(Tiny, 1e-10, 1);
    SelectMilliseconds(Tiny, -1e-20, 1);
    SelectMilliseconds(Tiny, -2e-10, 0);
    SelectMilliseconds(Tiny, 2e-10, 0);
    const auto Subnormal = ck_pixel_sprite_animation_tests::MakeAnimation({5e-321, 5e-321});
    SelectMilliseconds(Subnormal, 5e-321, 1);
    SelectMilliseconds(Subnormal, 1e-320, 0);
    auto TinyNegative = FCk_PixelSprite_FrameSelection{};
    if (TestTrue(TEXT("Tiny negative loop time evaluates"),
        ck::pixel_sprite::TryEvaluate_AnimationMilliseconds(Tiny, -1e-20, TinyNegative)))
    {
        TestTrue(TEXT("Tiny negative loop local time stays half-open"),
            TinyNegative._FrameTime.Get_Seconds() < Tiny._Frames[1].Get_Duration().Get_Seconds());
    }

    const auto SingleSecond = ck_pixel_sprite_animation_tests::MakeAnimation({1000.0});
    SelectMilliseconds(SingleSecond, 1e308, 0);
    SelectMilliseconds(SingleSecond, -1e308, 0);
    const auto HugeModulo = ck_pixel_sprite_animation_tests::MakeAnimation({100.0, 200.0});
    SelectMilliseconds(HugeModulo, 1e308, 1);
    SelectMilliseconds(HugeModulo, -1e308, 1);
    SelectMilliseconds(HugeModulo, -3e307, 0);
    for (const auto Seconds : {1e308, -1e308})
    {
        auto Selection = FCk_PixelSprite_FrameSelection{};
        if (NOT TestTrue(TEXT("Huge finite seconds evaluate"),
            ck::pixel_sprite::TryEvaluate_Animation(SingleSecond, FCk_Time{Seconds}, Selection)))
        {
            continue;
        }
        TestEqual(TEXT("Single-frame seconds select frame zero"), Selection._Index, 0);
        TestTrue(TEXT("Single-frame seconds local time is in range"),
            Selection._FrameTime.Get_Seconds() >= 0.0 && Selection._FrameTime.Get_Seconds() < 1.0);
        if (TestTrue(TEXT("Huge seconds preserve decimal modulo without multiplying doubles"),
            ck::pixel_sprite::TryEvaluate_Animation(HugeModulo, FCk_Time{Seconds}, Selection)))
        {
            TestEqual(TEXT("Huge seconds choose nontrivial frame"), Selection._Index, 1);
            TestEqual(TEXT("Huge seconds local time"), Selection._FrameTime.Get_Seconds(),
                Seconds < 0.0 ? 0.1 : 0.0);
        }
    }

    auto Sentinel = FCk_PixelSprite_FrameSelection{};
    Sentinel._Index = 17;
    Sentinel._FrameTime = FCk_Time{7.0};
    Sentinel._CycleDuration = FCk_Time{11.0};
    Sentinel._Complete = true;
    const auto Reject = [this, &Sentinel](const FCk_PixelSprite_Animation& InAnimation,
        const double InMilliseconds) -> void
    {
        TestFalse(TEXT("Invalid authored animation rejects"),
            ck::pixel_sprite::TryEvaluate_AnimationMilliseconds(InAnimation, InMilliseconds, Sentinel));
        TestEqual(TEXT("Rejected index unchanged"), Sentinel._Index, 17);
        TestEqual(TEXT("Rejected frame time unchanged"), Sentinel._FrameTime.Get_Seconds(), 7.0);
        TestEqual(TEXT("Rejected cycle duration unchanged"), Sentinel._CycleDuration.Get_Seconds(), 11.0);
        TestTrue(TEXT("Rejected completion unchanged"), Sentinel._Complete);
    };
    Reject(ck_pixel_sprite_animation_tests::MakeAnimation({}), 0.0);
    Reject(ck_pixel_sprite_animation_tests::MakeAnimation({0.0}), 0.0);
    Reject(ck_pixel_sprite_animation_tests::MakeAnimation({-1.0}), 0.0);
    Reject(ck_pixel_sprite_animation_tests::MakeAnimation({86400001.0}), 0.0);
    Reject(ck_pixel_sprite_animation_tests::MakeAnimation({std::numeric_limits<double>::denorm_min()}), 0.0);
    Reject(ck_pixel_sprite_animation_tests::MakeAnimation({std::numeric_limits<double>::quiet_NaN()}), 0.0);
    Reject(ck_pixel_sprite_animation_tests::MakeAnimation({std::numeric_limits<double>::infinity()}), 0.0);
    Reject(SingleSecond, std::numeric_limits<double>::quiet_NaN());
    Reject(SingleSecond, std::numeric_limits<double>::infinity());
    for (const auto Seconds : {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()})
    {
        TestFalse(TEXT("Nonfinite seconds reject"),
            ck::pixel_sprite::TryEvaluate_Animation(SingleSecond, FCk_Time{Seconds}, Sentinel));
        TestEqual(TEXT("Rejected seconds preserve index"), Sentinel._Index, 17);
        TestEqual(TEXT("Rejected seconds preserve frame time"), Sentinel._FrameTime.Get_Seconds(), 7.0);
        TestEqual(TEXT("Rejected seconds preserve cycle"), Sentinel._CycleDuration.Get_Seconds(), 11.0);
        TestTrue(TEXT("Rejected seconds preserve completion"), Sentinel._Complete);
    }
    auto TooMany = ck_pixel_sprite_animation_tests::MakeAnimation({});
    for (auto Index = 0; Index < 4097; ++Index)
    {
        auto Frame = FCk_PixelSprite_Frame{};
        Frame._DurationMilliseconds = 100.0;
        TooMany._Frames.Add(MoveTemp(Frame));
    }
    Reject(TooMany, 0.0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
