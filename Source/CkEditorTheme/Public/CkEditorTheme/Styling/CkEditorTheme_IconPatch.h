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
    // Legacy PNG icon (light glyph, baked dark outline) to an outline-only dark glyph; unset for any other art. BGRA8.
    CKEDITORTHEME_API auto Get_OutlineOnlyIcon(
        const TArray64<uint8>& InBGRA,
        int32 InWidth,
        int32 InHeight) -> TOptional<TArray64<uint8>>;

    // Dark-gray legacy panel/border fill remapped onto the Light surfaces; unset for other art, and for mostly
    // translucent art with InRequireOpaque (node bodies are translucent by design, so they skip that check).
    CKEDITORTHEME_API auto Get_LightSurface(
        const TArray64<uint8>& InBGRA,
        int32 InWidth,
        int32 InHeight,
        bool InRequireOpaque = true) -> TOptional<TArray64<uint8>>;

    // Node title spill to a flat header shaded #D8 to #B8: the original bakes dark shading that smudges on a light body.
    CKEDITORTHEME_API auto Get_SolidTitleSpill(
        const TArray64<uint8>& InBGRA,
        int32 InWidth,
        int32 InHeight) -> TOptional<TArray64<uint8>>;

    // Banner stripes (white on transparent) to opaque hazard tape: white stripes over #50 gaps, so the banner tint colours
    // both and the gaps stay dark under the white banner text on a light node.
    CKEDITORTHEME_API auto Get_OpaqueStripes(
        const TArray64<uint8>& InBGRA,
        int32 InWidth,
        int32 InHeight) -> TOptional<TArray64<uint8>>;

    // Variable node colour fill: the body's pill shape, white so the tint is the hue, its interior at 30% so a light body
    // shows through as a pale tint under dark ink, and its outline ring opaque so the hue rims the pill.
    // Resampled to the target size with the corner caps kept and the middle stretched (a 9-slice baked into the texture).
    CKEDITORTHEME_API auto Get_VarPillFill(
        const TArray64<uint8>& InBodyBGRA,
        int32 InWidth,
        int32 InHeight,
        int32 InTargetWidth,
        int32 InTargetHeight,
        int32 InCapX,
        int32 InCapY) -> TOptional<TArray64<uint8>>;
}

// --------------------------------------------------------------------------------------------------------------------

// Light points legacy brushes at generated textures, Dark clears them, in place. Brushes are re-gathered from every
// style set on each apply and never held, because non-Core style sets can be destroyed and rebuilt.
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
        // Slate still alive: brush copies outside the registry may point at a texture, so textures are rooted and leaked.
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
