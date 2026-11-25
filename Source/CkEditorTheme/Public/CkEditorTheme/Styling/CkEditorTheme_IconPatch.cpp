#include "CkEditorTheme_IconPatch.h"

#include "CkEditorTheme/CkEditorTheme_Log.h"

#include "CkCore/Macros/CkMacros.h"

#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateStyleRegistry.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_editor_theme_icon_patch
{
    constexpr auto MaxIconSize = 64.0f;
    constexpr auto DarkLuma = 60.0f;
    constexpr auto LightLuma = 180.0f;
    constexpr auto GlyphValue = uint8{0x3A};

    constexpr auto SurfaceMaxValue = 0x70;
    constexpr auto SurfaceTop = 0xF5;
    constexpr auto SurfaceSlope = 0.45f;

    auto Get_Luma(
        uint8 InB,
        uint8 InG,
        uint8 InR)
        -> float
    {
        return 0.2126f * InR + 0.7152f * InG + 0.0722f * InB;
    }

    auto Get_IsPng(
        const FSlateBrush& InBrush)
        -> bool
    {
        return InBrush.GetResourceName().ToString().EndsWith(TEXT(".png"));
    }

    auto Get_IsOutlineCandidate(
        const FSlateBrush& InBrush)
        -> bool
    {
        return InBrush.DrawAs == ESlateBrushDrawType::Image
            && InBrush.GetTiling() == ESlateBrushTileType::NoTile
            && InBrush.ImageSize.X <= MaxIconSize
            && InBrush.ImageSize.Y <= MaxIconSize
            && Get_IsPng(InBrush);
    }

    auto Get_IsTitleSpill(
        const FSlateBrush& InBrush)
        -> bool
    {
        return InBrush.GetResourceName().ToString().EndsWith(TEXT("/Graph/RegularNode_color_spill.png"));
    }

    auto Get_IsVarSpill(
        const FSlateBrush& InBrush)
        -> bool
    {
        return InBrush.GetResourceName().ToString().EndsWith(TEXT("/Graph/VarNode_color_spill.png"));
    }

    // The node bodies (regular, collapsed, variable) and the title spill. Converted only for light graph nodes, i.e.
    // when pin labels can follow the theme (see FCk_EditorTheme_LightStylePatch::Get_LightGraphNodes).
    auto Get_IsNodeArt(
        const FSlateBrush& InBrush)
        -> bool
    {
        const auto Name = InBrush.GetResourceName().ToString();
        return Name.EndsWith(TEXT("/Graph/RegularNode_body.png")) || Name.EndsWith(TEXT("/Graph/VarNode_body.png"))
            || Get_IsTitleSpill(InBrush) || Get_IsVarSpill(InBrush);
    }

    // Graph node art is handled as node art; shadows are dark on purpose.
    auto Get_IsSurfaceCandidate(
        const FSlateBrush& InBrush)
        -> bool
    {
        const auto IsPanelShaped = InBrush.DrawAs == ESlateBrushDrawType::Box
            || InBrush.DrawAs == ESlateBrushDrawType::Border
            || (InBrush.DrawAs == ESlateBrushDrawType::Image
                && (InBrush.GetTiling() != ESlateBrushTileType::NoTile
                    || InBrush.ImageSize.X > MaxIconSize || InBrush.ImageSize.Y > MaxIconSize));

        if (NOT IsPanelShaped || NOT Get_IsPng(InBrush))
        { return false; }

        const auto Name = InBrush.GetResourceName().ToString();
        // Sequencer sections carry light labels on their gray art, so the track area stays dark like the viewport.
        return (NOT Name.Contains(TEXT("/Graph/")) || Name.Contains(TEXT("/Graph/GraphPanel_")))
            && NOT Name.Contains(TEXT("/Sequencer/")) && NOT Name.Contains(TEXT("Shadow"));
    }

    auto Get_DecodedPng(
        const FString& InFile,
        int32& OutWidth,
        int32& OutHeight)
        -> TOptional<TArray64<uint8>>
    {
        auto Compressed = TArray<uint8>{};
        if (NOT FFileHelper::LoadFileToArray(Compressed, *InFile))
        { return {}; }

        auto& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        const auto Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
        if (NOT Wrapper.IsValid() || NOT Wrapper->SetCompressed(Compressed.GetData(), Compressed.Num()))
        { return {}; }

        auto Raw = TArray64<uint8>{};
        if (NOT Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
        { return {}; }

        OutWidth = static_cast<int32>(Wrapper->GetWidth());
        OutHeight = static_cast<int32>(Wrapper->GetHeight());
        return Raw;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::editor_theme
{
    auto
        Get_OutlineOnlyIcon(
            const TArray64<uint8>& InBGRA,
            int32 InWidth,
            int32 InHeight)
        -> TOptional<TArray64<uint8>>
    {
        using namespace ck_editor_theme_icon_patch;

        const auto PixelCount = static_cast<int64>(InWidth) * InHeight;
        if (PixelCount <= 0 || InBGRA.Num() != PixelCount * 4)
        { return {}; }

        auto Opaque = int64{};
        auto Dark = int64{};
        auto Light = int64{};
        auto Saturation = 0.0;

        for (auto Pixel = int64{}; Pixel < PixelCount; ++Pixel)
        {
            const auto* Bgra = &InBGRA[Pixel * 4];
            if (Bgra[3] <= 128)
            { continue; }

            ++Opaque;
            const auto Luma = Get_Luma(Bgra[0], Bgra[1], Bgra[2]);
            Dark += Luma < DarkLuma ? 1 : 0;
            Light += Luma > LightLuma ? 1 : 0;

            const auto Max = FMath::Max3(Bgra[0], Bgra[1], Bgra[2]);
            const auto Min = FMath::Min3(Bgra[0], Bgra[1], Bgra[2]);
            Saturation += Max > 0 ? static_cast<double>(Max - Min) / Max : 0.0;
        }

        if (Opaque == 0 || Dark * 10 <= Opaque || Light * 100 <= Opaque * 15 || Saturation / Opaque > 0.2)
        { return {}; }

        auto Outline = TArray64<uint8>{};
        Outline.SetNumUninitialized(InBGRA.Num());

        for (auto Pixel = int64{}; Pixel < PixelCount; ++Pixel)
        {
            const auto* Source = &InBGRA[Pixel * 4];
            auto* Target = &Outline[Pixel * 4];

            const auto Coverage = FMath::Clamp((LightLuma - Get_Luma(Source[0], Source[1], Source[2])) / (LightLuma - DarkLuma), 0.0f, 1.0f);

            Target[0] = GlyphValue;
            Target[1] = GlyphValue;
            Target[2] = GlyphValue;
            Target[3] = static_cast<uint8>(FMath::RoundToInt(Source[3] * Coverage));
        }

        return Outline;
    }

    auto
        Get_LightSurface(
            const TArray64<uint8>& InBGRA,
            int32 InWidth,
            int32 InHeight,
            bool InRequireOpaque)
        -> TOptional<TArray64<uint8>>
    {
        using namespace ck_editor_theme_icon_patch;

        const auto PixelCount = static_cast<int64>(InWidth) * InHeight;
        if (PixelCount <= 0 || InBGRA.Num() != PixelCount * 4)
        { return {}; }

        auto Opaque = int64{};
        for (auto Pixel = int64{}; Pixel < PixelCount; ++Pixel)
        {
            const auto* Bgra = &InBGRA[Pixel * 4];
            if (Bgra[3] < 16)
            { continue; }

            const auto Max = FMath::Max3(Bgra[0], Bgra[1], Bgra[2]);
            const auto Min = FMath::Min3(Bgra[0], Bgra[1], Bgra[2]);
            if (Max > SurfaceMaxValue || Max - Min > 12)
            { return {}; }

            Opaque += Bgra[3] >= 200 ? 1 : 0;
        }

        if (InRequireOpaque && Opaque * 2 < PixelCount)
        { return {}; }

        auto Surface = TArray64<uint8>{};
        Surface.SetNumUninitialized(InBGRA.Num());

        for (auto Index = int64{}; Index < InBGRA.Num(); ++Index)
        {
            if (Index % 4 == 3)
            {
                Surface[Index] = InBGRA[Index];
                continue;
            }

            const auto Mapped = SurfaceTop - (static_cast<float>(InBGRA[Index]) - 0x10) * SurfaceSlope;
            Surface[Index] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Mapped), 0, 255));
        }

        return Surface;
    }
}

namespace ck::editor_theme
{
    auto
        Get_SolidTitleSpill(
            const TArray64<uint8>& InBGRA,
            int32 InWidth,
            int32 InHeight)
        -> TOptional<TArray64<uint8>>
    {
        const auto PixelCount = static_cast<int64>(InWidth) * InHeight;
        if (PixelCount <= 0 || InBGRA.Num() != PixelCount * 4)
        { return {}; }

        // The art bakes dark shading into its RGB (down to ~78 mid-band) that only reads as depth over a dark body.
        // Light gets a flat header: a gentle top-to-bottom shade, fully opaque except the rounded left edge.
        constexpr auto TopShade = 0xD8;
        constexpr auto BottomShade = 0xB8;
        auto Solid = TArray64<uint8>{};
        Solid.SetNumUninitialized(InBGRA.Num());

        for (auto Y = 0; Y < InHeight; ++Y)
        {
            const auto Row = static_cast<int64>(Y) * InWidth;
            auto PeakX = 0;
            for (auto X = 0; X < InWidth; ++X)
            {
                if (InBGRA[(Row + X) * 4 + 3] > InBGRA[(Row + PeakX) * 4 + 3])
                { PeakX = X; }
            }

            // Near-transparent feather rows (peak alpha 5 and 8 on the stock art) stay transparent: as opaque rows they
            // would make the header taller and square off the node's rounded top-right corner.
            const auto Peak = InBGRA[(Row + PeakX) * 4 + 3] < 16 ? uint8{0} : InBGRA[(Row + PeakX) * 4 + 3];
            const auto OpaqueFrom = Peak * 0.6f;
            const auto Shade = static_cast<uint8>(FMath::RoundToInt(
                FMath::Lerp(static_cast<float>(TopShade), static_cast<float>(BottomShade), InHeight > 1 ? static_cast<float>(Y) / (InHeight - 1) : 0.0f)));

            for (auto X = 0; X < InWidth; ++X)
            {
                auto* Target = &Solid[(Row + X) * 4];
                Target[0] = Shade;
                Target[1] = Shade;
                Target[2] = Shade;
                // The rounded left edge keeps its ramp up to 60% of the peak; past that the art's noisy plateau would
                // leave the header unevenly translucent, so it is opaque.
                Target[3] = Peak == 0 ? uint8{0}
                    : X < PeakX ? static_cast<uint8>(FMath::Min(255, FMath::RoundToInt(255.0f * InBGRA[(Row + X) * 4 + 3] / OpaqueFrom)))
                    : uint8{255};
            }
        }

        return Solid;
    }

    auto
        Get_VarPillFill(
            const TArray64<uint8>& InBodyBGRA,
            int32 InWidth,
            int32 InHeight,
            int32 InTargetWidth,
            int32 InTargetHeight,
            int32 InCapX,
            int32 InCapY)
        -> TOptional<TArray64<uint8>>
    {
        const auto PixelCount = static_cast<int64>(InWidth) * InHeight;
        if (PixelCount <= 0 || InBodyBGRA.Num() != PixelCount * 4
            || InCapX * 2 >= InWidth || InCapY * 2 >= InHeight
            || InTargetWidth < InCapX * 2 + 1 || InTargetHeight < InCapY * 2 + 1)
        { return {}; }

        // The body's outline ring is more opaque than its interior (209 vs 105 in VarNode_body.png): the interior is the reference.
        const auto Interior = InBodyBGRA[(static_cast<int64>(InHeight / 2) * InWidth + InWidth / 2) * 4 + 3];
        if (Interior == 0)
        { return {}; }

        // Caps map 1:1; the middle samples the source's middle.
        const auto Source = [](int32 InTarget, int32 InTargetSize, int32 InSourceSize, int32 InCap)
        {
            if (InTarget < InCap)
            { return InTarget; }
            if (InTarget >= InTargetSize - InCap)
            { return InSourceSize - (InTargetSize - InTarget); }
            const auto Middle = InSourceSize - InCap * 2;
            return InCap + (InTarget - InCap) * Middle / (InTargetSize - InCap * 2);
        };

        // The interior and the ring become opaque; the anti-aliased edge keeps its falloff.
        constexpr auto Shade = uint8{0xC8};
        auto Fill = TArray64<uint8>{};
        Fill.SetNumUninitialized(static_cast<int64>(InTargetWidth) * InTargetHeight * 4);
        for (auto Y = 0; Y < InTargetHeight; ++Y)
        {
            const auto SourceY = Source(Y, InTargetHeight, InHeight, InCapY);
            for (auto X = 0; X < InTargetWidth; ++X)
            {
                const auto SourceX = Source(X, InTargetWidth, InWidth, InCapX);
                const auto Alpha = InBodyBGRA[(static_cast<int64>(SourceY) * InWidth + SourceX) * 4 + 3];
                auto* Target = &Fill[(static_cast<int64>(Y) * InTargetWidth + X) * 4];
                Target[0] = Shade;
                Target[1] = Shade;
                Target[2] = Shade;
                Target[3] = static_cast<uint8>(FMath::Min(255, FMath::RoundToInt(255.0f * Alpha / Interior)));
            }
        }

        return Fill;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_EditorTheme_IconPatch::
    Request_Apply(
        ECk_EditorTheme_Brightness InBrightness,
        ECk_EnableDisable InOutlineLegacyIcons,
        bool InLightGraphNodes)
    -> bool
{
    const auto IsLight = InBrightness == ECk_EditorTheme_Brightness::Light;
    const auto UseOutlineIcons = IsLight && InOutlineLegacyIcons == ECk_EnableDisable::Enable;
    const auto UseNodeArt = IsLight && InLightGraphNodes;

    if (_HasApplied && _UseSurfaces == IsLight && _UseOutlineIcons == UseOutlineIcons && _UseNodeArt == UseNodeArt)
    { return false; }

    _UseSurfaces = IsLight;
    _UseOutlineIcons = UseOutlineIcons;
    _UseNodeArt = UseNodeArt;
    _HasApplied = true;

    const auto StartSeconds = FPlatformTime::Seconds();
    DoApply_ToAllBrushes();

    if (IsLight)
    {
        const auto Count = [](const TMap<FName, TStrongObjectPtr<UTexture2D>>& InCache)
        {
            auto Converted = 0;
            for (const auto& Entry : InCache)
            { Converted += Entry.Value.IsValid() ? 1 : 0; }
            return Converted;
        };

        ck::editor_theme::Log(TEXT("Light textures: [{}] outlined icons, [{}] surfaces, [{}] graph node bodies ({} ms)"),
            UseOutlineIcons ? Count(_OutlineIcons) : 0, Count(_Surfaces), UseNodeArt ? Count(_NodeArt) : 0,
            FMath::RoundToInt((FPlatformTime::Seconds() - StartSeconds) * 1000.0));
    }

    return true;
}

auto
    FCk_EditorTheme_IconPatch::
    Request_RefreshStyleSets()
    -> bool
{
    if (NOT _HasApplied || NOT _UseSurfaces || Get_StyleSetSignature() == _AppliedStyleSets)
    { return false; }

    return DoApply_ToAllBrushes();
}

auto
    FCk_EditorTheme_IconPatch::
    Request_Release(
        ECk_TextureRelease InRelease)
    -> void
{
    _UseSurfaces = false;
    _UseOutlineIcons = false;
    _UseNodeArt = false;

    if (_HasApplied)
    { DoApply_ToAllBrushes(); }

    if (InRelease == ECk_TextureRelease::KeepAlive)
    {
        for (const auto* Texture : _OwnTextures)
        { const_cast<UObject*>(Texture)->AddToRoot(); }
    }

    _OutlineIcons.Reset();
    _Surfaces.Reset();
    _NodeArt.Reset();
    _OwnTextures.Reset();
    _AppliedStyleSets.Reset();
    _HasApplied = false;
}

auto
    FCk_EditorTheme_IconPatch::
    DoApply_ToAllBrushes()
    -> bool
{
    using namespace ck_editor_theme_icon_patch;

    auto Brushes = TArray<const FSlateBrush*>{};
    FSlateStyleRegistry::IterateAllStyles([&](const ISlateStyle& InStyle)
    {
        InStyle.GetResources(Brushes);
        return true;
    });

    auto AnyChanged = false;

    for (const auto* ConstBrush : Brushes)
    {
        if (ConstBrush == nullptr)
        { continue; }

        // Style sets own their brushes as mutable heap objects; GetResources only hands them out as const.
        auto* Brush = const_cast<FSlateBrush*>(ConstBrush);
        const auto* Current = Brush->GetResourceObject();
        const auto IsOwn = Current != nullptr && _OwnTextures.Contains(Current);

        if (Current != nullptr && NOT IsOwn)
        { continue; }

        auto* Wanted = static_cast<UTexture2D*>(nullptr);
        if (_UseNodeArt && Get_IsNodeArt(*Brush))
        { Wanted = Get_Texture(*Brush, ECk_PatchKind::NodeArt); }
        else if (_UseSurfaces && Get_IsSurfaceCandidate(*Brush))
        { Wanted = Get_Texture(*Brush, ECk_PatchKind::Surface); }
        if (Wanted == nullptr && _UseOutlineIcons && Get_IsOutlineCandidate(*Brush))
        { Wanted = Get_Texture(*Brush, ECk_PatchKind::OutlineIcon); }

        if (Current == Wanted)
        { continue; }

        Brush->SetResourceObject(Wanted);
        AnyChanged = true;
    }

    _AppliedStyleSets = Get_StyleSetSignature();
    return AnyChanged;
}

auto
    FCk_EditorTheme_IconPatch::
    Get_Texture(
        const FSlateBrush& InBrush,
        ECk_PatchKind InKind)
    -> UTexture2D*
{
    using namespace ck_editor_theme_icon_patch;

    auto& Cache = InKind == ECk_PatchKind::Surface ? _Surfaces : InKind == ECk_PatchKind::NodeArt ? _NodeArt : _OutlineIcons;
    const auto ResourceName = InBrush.GetResourceName();

    if (const auto* Cached = Cache.Find(ResourceName))
    { return Cached->Get(); }

    // A failed conversion is cached as null so each file is decoded at most once per kind.
    auto& Entry = Cache.Add(ResourceName);

    auto Width = int32{};
    auto Height = int32{};
    // The variable spill takes the body's pill shape (FCk_EditorTheme_LightStylePatch gives it the body's 9-slice).
    const auto SourceFile = InKind == ECk_PatchKind::NodeArt && Get_IsVarSpill(InBrush)
        ? ResourceName.ToString().Replace(TEXT("VarNode_color_spill"), TEXT("VarNode_body"))
        : ResourceName.ToString();
    const auto Decoded = Get_DecodedPng(SourceFile, Width, Height);
    if (NOT Decoded.IsSet())
    { return nullptr; }

    const auto Converted = InKind == ECk_PatchKind::OutlineIcon
        ? ck::editor_theme::Get_OutlineOnlyIcon(Decoded.GetValue(), Width, Height)
        : InKind == ECk_PatchKind::NodeArt && Get_IsTitleSpill(InBrush)
        ? ck::editor_theme::Get_SolidTitleSpill(Decoded.GetValue(), Width, Height)
        : InKind == ECk_PatchKind::NodeArt && Get_IsVarSpill(InBrush)
        ? ck::editor_theme::Get_VarPillFill(Decoded.GetValue(), Width, Height,
            FMath::RoundToInt(InBrush.GetImageSize().X), FMath::RoundToInt(InBrush.GetImageSize().Y),
            FMath::RoundToInt(InBrush.GetMargin().Left * InBrush.GetImageSize().X),
            FMath::RoundToInt(InBrush.GetMargin().Top * InBrush.GetImageSize().Y))
        : ck::editor_theme::Get_LightSurface(Decoded.GetValue(), Width, Height, InKind == ECk_PatchKind::Surface);
    if (NOT Converted.IsSet())
    { return nullptr; }

    if (InKind == ECk_PatchKind::NodeArt && Get_IsVarSpill(InBrush))
    {
        Width = FMath::RoundToInt(InBrush.GetImageSize().X);
        Height = FMath::RoundToInt(InBrush.GetImageSize().Y);
    }

    auto* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, NAME_None, Converted.GetValue());
    if (Texture == nullptr)
    { return nullptr; }

    Texture->SRGB = true;
    Texture->UpdateResource();

    Entry = TStrongObjectPtr<UTexture2D>{Texture};
    _OwnTextures.Add(Texture);
    return Texture;
}

auto
    FCk_EditorTheme_IconPatch::
    Get_StyleSetSignature() const
    -> TArray<const ISlateStyle*>
{
    auto Signature = TArray<const ISlateStyle*>{};
    FSlateStyleRegistry::IterateAllStyles([&](const ISlateStyle& InStyle)
    {
        Signature.Add(&InStyle);
        return true;
    });

    Signature.Sort([](const ISlateStyle& InA, const ISlateStyle& InB) { return &InA < &InB; });
    return Signature;
}

// --------------------------------------------------------------------------------------------------------------------
