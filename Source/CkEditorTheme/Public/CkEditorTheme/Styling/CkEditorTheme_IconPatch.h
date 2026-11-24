#pragma once

#include "CkEditorTheme/Styling/CkEditorTheme_LightStylePatch.h"

#include "CkCore/Enums/CkEnums.h"

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class ISlateStyle;
class UTexture2D;
struct FSlateBrush;

// --------------------------------------------------------------------------------------------------------------------

namespace ck::editor_theme
{
    // Legacy PNG icons are a light glyph with a baked dark outline: on Dark the outline disappears, on Light the
    // foreground-tinted body and the outline merge into a dark blob. Keeps only the outline as a dark glyph, which
    // reads as a line icon. Unset for anything that is not such an icon (colorful, no outline, no light body).
    // InBGRA is 8-bit BGRA, InWidth * InHeight pixels.
    CKEDITORTHEME_API auto Get_OutlineOnlyIcon(
        const TArray64<uint8>& InBGRA,
        int32 InWidth,
        int32 InHeight) -> TOptional<TArray64<uint8>>;

    // Legacy PNG panels and borders (Common/GroupBorder, TableViewHeader, ...) bake a dark gray fill. Maps the gray
    // ramp onto the Light surfaces (#1B -> #F0, #60 -> #D1), alpha kept. Unset for anything that is not a dark gray
    // surface (glyphs, colored art) or, with InRequireOpaque, mostly translucent (shadows). Node bodies are
    // translucent by design, so they are converted without that check.
    CKEDITORTHEME_API auto Get_LightSurface(
        const TArray64<uint8>& InBGRA,
        int32 InWidth,
        int32 InHeight,
        bool InRequireOpaque = true) -> TOptional<TArray64<uint8>>;

    // The node title spill (Graph/RegularNode_color_spill) fades into the node body and bakes dark shading into its
    // RGB: depth on a dark body, a dark smudge and a washed-out end on a light one. Produces a flat header instead:
    // opaque from each row's peak to the right edge (the rounded left edge kept), shaded #D8 top to #B8 bottom.
    CKEDITORTHEME_API auto Get_SolidTitleSpill(
        const TArray64<uint8>& InBGRA,
        int32 InWidth,
        int32 InHeight) -> TOptional<TArray64<uint8>>;

    // The variable node spill (Graph/VarNode_color_spill) is a soft glow: its strong top rows carry the SET/GET title,
    // which is white and vanishes on a light body. The top half's visible rows (peak alpha >= 16) become a solid strip
    // at #C8; the faint lower glow is kept as drawn.
    CKEDITORTHEME_API auto Get_SolidVarSpill(
        const TArray64<uint8>& InBGRA,
        int32 InWidth,
        int32 InHeight) -> TOptional<TArray64<uint8>>;
}

// --------------------------------------------------------------------------------------------------------------------

// Light points legacy PNG brushes at generated textures (FSlateBrush::SetResourceObject), Dark clears them so each
// brush draws its original file again. In place, so live widgets follow without a rebuild. Brushes are gathered from
// every registered style set (including the brushes inside widget styles) on each apply and never held, because
// style sets other than Core/Editor can be destroyed and rebuilt; textures are cached per file.
class CKEDITORTHEME_API FCk_EditorTheme_IconPatch
{
public:
    auto Request_Apply(
        ECk_EditorTheme_Brightness InBrightness,
        ECk_EnableDisable InOutlineLegacyIcons,
        bool InLightGraphNodes) -> bool;

    // Re-applies when a style set was registered or replaced since the last apply (modules load their styles lazily).
    auto Request_RefreshStyleSets() -> bool;

    enum class ECk_TextureRelease : uint8
    {
        // Slate pre-shutdown: nothing draws any more.
        Free,
        // Module unload with Slate alive: brush copies outside the registry may still point at a texture, so the
        // textures are rooted and leaked rather than freed under them.
        KeepAlive
    };

    // Brushes back to their files, textures dropped.
    auto Request_Release(
        ECk_TextureRelease InRelease) -> void;

private:
    enum class ECk_PatchKind : uint8
    {
        OutlineIcon,
        Surface,
        NodeArt
    };

    auto DoApply_ToAllBrushes() -> bool;

    auto Get_Texture(
        const FSlateBrush& InBrush,
        ECk_PatchKind InKind) -> UTexture2D*;

    auto Get_StyleSetSignature() const -> TArray<const ISlateStyle*>;

    TMap<FName, TStrongObjectPtr<UTexture2D>> _OutlineIcons;
    TMap<FName, TStrongObjectPtr<UTexture2D>> _Surfaces;
    TMap<FName, TStrongObjectPtr<UTexture2D>> _NodeArt;
    TSet<const UObject*> _OwnTextures;
    TArray<const ISlateStyle*> _AppliedStyleSets;

    bool _UseSurfaces = false;
    bool _UseOutlineIcons = false;
    bool _UseNodeArt = false;
    bool _HasApplied = false;
};

// --------------------------------------------------------------------------------------------------------------------
