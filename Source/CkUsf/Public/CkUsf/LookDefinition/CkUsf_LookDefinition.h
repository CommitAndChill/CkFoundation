#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/SubsurfaceProfile.h"
#include "Engine/ToonProfile.h"
#include "CkUsf_LookDefinition.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_Usf_Domain : uint8
{
    SurfaceLit,
    SurfaceUnlit,
    PostProcess,
    UI,
    Decal
};

UENUM(BlueprintType)
enum class ECk_Usf_SurfaceGraph : uint8
{
    Legacy,
    NativeSlab,
    NativeToon,
    NativeSimpleClearCoat,
    // Fixed two-Slab topologies; retained closures require Adaptive GBuffer.
    AdaptiveVertical,
    AdaptiveMix,
    // Fixed thin-surface colored-transmittance Slab; Adaptive GBuffer, one closure.
    NativeTranslucentSlab,
    // Same optical Slab with native refraction derived from F0; no root Refraction input.
    // The shared numeric output's RefractionStrength field is unused by this graph.
    NativeRefractiveSlab,
    // Two transmitting Slabs, explicit A/B calibration and thickness; Adaptive, two closures.
    NativeTranslucentVertical,
    // Three transmitting Slabs with nested Vertical operators; Adaptive, three closures.
    NativeTranslucentStack3
};

// Blend-mode override on top of the domain default. `Inherit` keeps the domain's blend.
UENUM(BlueprintType)
enum class ECk_Usf_BlendMode : uint8
{
    Inherit,
    Opaque,
    Masked,
    Translucent,
    Additive,
    Modulate
};

// Shading-model override. `Inherit` keeps the domain default (SurfaceLitâ†’DefaultLit, othersâ†’Unlit).
UENUM(BlueprintType)
enum class ECk_Usf_ShadingModel : uint8
{
    Inherit,
    Unlit,
    DefaultLit,
    Subsurface,
    ClearCoat,
    // Appended: existing look assets retain their serialized shading-model values.
    // Requires the engine's native CkUsf shading-model extension; SurfaceLit/Opaque only.
    Toon,
    PreintegratedSkin,
    SubsurfaceProfile,
    TwoSidedFoliage,
    Hair,
    Cloth,
    Eye,
    SingleLayerWater,
    ThinTranslucent
};

UENUM(BlueprintType)
enum class ECk_Usf_ParamType : uint8
{
    Scalar,
    Vector,
    Texture2D,
    TextureCube,
    Texture2DArray
};

// PostProcess-only: which scene textures a look's Custom node receives (and thereby declares usage for,
// legalizing raw SceneTextureLookup() in its .ush). EMPTY = the default trio (SceneColor/Depth/Normal).
UENUM(BlueprintType)
enum class ECk_Usf_SceneTexture : uint8
{
    SceneColor,     // PPI_PostProcessInput0  -> In.SceneColor
    SceneDepth,     // PPI_SceneDepth         -> In.SceneDepth
    SceneNormal,    // PPI_WorldNormal        -> In.SceneNormal
    CustomDepth,    // PPI_CustomDepth        -> In.CustomDepth
    CustomStencil,  // PPI_CustomStencil      -> In.CustomStencil
    BaseColor,      // PPI_BaseColor          -> In.SceneBaseColor
    Metallic,       // PPI_Metallic           -> In.SceneMetallic
    Roughness,      // PPI_Roughness          -> In.SceneRoughness
    Specular        // PPI_Specular           -> In.SceneSpecular
};

// PostProcess-only: where in the post-processing chain the generated blendable runs (maps to
// EBlendableLocation). Custom Depth/Stencil-derived looks REQUIRE a pre-TAA location or their edges
// shimmer; pre-TAA is also pre-tonemap. Trade-offs: CkUsf/CLAUDE.md Â§ Blendable location.
UENUM(BlueprintType)
enum class ECk_Usf_BlendableLocation : uint8
{
    AfterTonemapping,      // BL_SceneColorAfterTonemapping â€” display res, post-TAA (historical default)
    SceneColorAfterDOF,    // BL_SceneColorAfterDOF         â€” render res, pre-TSR/TAA
    SceneColorBeforeDOF,   // BL_SceneColorBeforeDOF        â€” render res, pre-TSR/TAA, before DOF
    SceneColorBeforeBloom, // BL_SceneColorBeforeBloom      â€” display res, post-TAA, pre-bloom/tonemap

    // BL_ReplacingTonemapper â€” the look's output IS the final image; no tonemapper runs after it. APPENDED
    // last on purpose: these values are serialized in look assets, so inserting one renumbers every look
    // declared after it.
    //
    // For a LOOK this is usually the wrong choice â€” you inherit responsibility for the whole tonemap. It is
    // here for the other kind of pass: one whose output is DATA rather than a picture, where a tonemap curve
    // applied afterwards would corrupt the values it is meant to carry (an id, a mask, a depth).
    ReplacingTonemapper
};

// Translucency lighting for LIT translucent-family surface looks. `Inherit` keeps the engine default
// (volumetric non-directional â€” cheap but flat); glass-like surfaces usually want `SurfacePerPixel`.
UENUM(BlueprintType)
enum class ECk_Usf_TranslucencyLighting : uint8
{
    Inherit,
    VolumetricNonDirectional,
    VolumetricDirectional,
    Surface,
    SurfacePerPixel
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKUSF_API FCk_Usf_ParamDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FName _Name = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    ECk_Usf_ParamType _Type = ECk_Usf_ParamType::Scalar;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    float _DefaultScalar = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FLinearColor _DefaultVector = FLinearColor::Black;

    // Scalar/Vector-only: source this param from per-instance custom data (ISM/CkIsmRenderer) instead of a
    // uniform. Safe on non-instanced meshes (the node returns the param default). Runtime writers must NOT
    // count slots by hand â€” query Get_PerInstanceSlotOf / Get_NumPerInstanceFloats.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _PerInstance = false;

    // Per-instance params only: explicit first slot for this param's per-instance data; -1 keeps the auto
    // layout (declaration order). Set it when the instance custom-data layout is owned elsewhere. Explicit
    // slots do NOT advance the auto counter; Get_PerInstanceSlotOf resolves both kinds.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    int32 _PerInstanceSlot = -1;

    // Scalar/Vector-only: source this param from the primitive's CUSTOM PRIMITIVE DATA instead of a
    // uniform â€” the per-component channel a UPrimitiveComponent carries (SetCustomPrimitiveDataFloat),
    // which is what a per-entity SKMC has where an ISM has per-instance custom data. Mutually exclusive
    // with _PerInstance. Safe on any primitive: an unwritten CPD slot reads 0.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _CustomPrimitiveData = false;

    // Custom-primitive-data params only: which CPD index this param reads. The layout is owned by the
    // writer (there is no auto-assignment the way per-instance slots have one), so it must be set
    // whenever _CustomPrimitiveData is.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    int32 _CustomPrimitiveDataIndex = -1;

    // Object path for Texture2D / TextureCube params, e.g.
    // "/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FString _DefaultTexturePath;

    // Parameter-panel group on the generated master (and every MID of it), e.g. "Distortion". None leaves
    // the parameter ungrouped, which is where the engine files it today â€” so existing looks regenerate
    // byte-identically. Purely presentational: it changes no wiring and no shader code.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FName _Group = NAME_None;

    // Ordering WITHIN _Group (ascending). 32 is the engine's own default, so leaving it alone sorts
    // alphabetically alongside anything else that never set it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    int32 _SortPriority = 32;
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(BlueprintType)
class CKUSF_API UCkUsf_LookDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    // Include path resolvable from a Custom node, e.g. "/CkUsf/Looks/Hologram.ush"
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FString _UshIncludePath;

    // HLSL function name inside the include, e.g. "CkUsf_Look_Hologram"
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FName _UshFunctionName = NAME_None;

    // Optional WorldPositionOffset entry point (vertex shader), e.g. "CkUsf_Look_Displace_WPO"; None = no WPO.
    // Surface domains only. Takes FCkUsf_VertexInput + the same params as the pixel fn, returns a world offset.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FName _WpoFunctionName = NAME_None;

    // Surface-only: wire mesh TexCoord1/TexCoord2 into the PIXEL Custom node (In.UV1/In.UV2). Opt-in
    // because every wired coordinate costs interpolators on the look's master. The WPO node receives
    // UV1/UV2 unconditionally; this flag concerns the pixel node only.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _PixelDataChannels = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    ECk_Usf_Domain _Domain = ECk_Usf_Domain::SurfaceLit;

    // PostProcess-only (see ECk_Usf_SceneTexture): EMPTY = the default trio; opt into CustomDepth/
    // CustomStencil explicitly (e.g. the SolidOutline look). Ignored for non-PostProcess domains.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    TArray<ECk_Usf_SceneTexture> _SceneTextures;

    // PostProcess-only: wire the engine's WorldPosition expression into the Custom node (In.WorldPosition).
    // In a PostProcess material that expression reconstructs the SCENE SURFACE position from depth
    // (PostProcessMaterialShaders.usf overwrites SvPosition.z with the scene's device Z before the material
    // parameters are built), so it is the world point the pixel is looking at, not the quad's own position.
    // Opt-in because it costs the reconstruction on every pixel, and because at after-tonemap / SSR
    // placements that reconstruction is dynamic-resolution scaled â€” intended for the pre-TAA locations.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _PostProcessWorldPosition = false;

    // PostProcess-only: chain placement of the generated blendable (see the enum for the pre- vs post-TAA
    // trade-off â€” Custom Depth/Stencil looks need a pre-TAA location). Ignored for non-PostProcess domains.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    ECk_Usf_BlendableLocation _BlendableLocation = ECk_Usf_BlendableLocation::AfterTonemapping;

    // Native graphs are explicit opt-ins; existing serialized shading models retain their meaning.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    ECk_Usf_SurfaceGraph _SurfaceGraph = ECk_Usf_SurfaceGraph::Legacy;

    // NativeSlab only. Connected fuzz changes the compiled feature even at zero weight.
    // Blendable preserves amount/color, fixes fuzz roughness, and cannot combine it with anisotropy.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _SlabFuzz = false;

    // AdaptiveVertical only; explicitly connects native glints on the lower B Slab.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _AdaptiveBaseGlints = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    TObjectPtr<UToonProfile> _ToonProfile;

    // Surface-domain overrides (ignored for PostProcess/UI/Decal, which keep their domain config).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    ECk_Usf_BlendMode _BlendMode = ECk_Usf_BlendMode::Inherit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    ECk_Usf_ShadingModel _ShadingModel = ECk_Usf_ShadingModel::Inherit;

    // Wire O.Anisotropy and tangent-space O.Tangent for native graphs or
    // Opaque/Masked SurfaceLit legacy DefaultLit/ClearCoat.
    // Opt-in because a connected Custom output enables the anisotropic shader feature even when its
    // HLSL result is zero. Changing this setting requires regenerating the master outside PIE.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _DirectionalShading = false;

    // Translucent SurfaceLit DefaultLit/ThinTranslucent only. Adds the opaque scene-color copy
    // and viewport coordinates to the pixel node; changing this requires regeneration.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _SurfaceSceneColor = false;

    // Copied opaque color already contains fog. Replacement compositing must suppress a
    // second fog application on this material; ordinary scene-color consumers keep fogging.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _SceneColorReplacesBackground = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    TObjectPtr<USubsurfaceProfile> _SubsurfaceProfile;

    // Lit translucent-family surface looks only (see the enum). `Inherit` keeps the engine default.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    ECk_Usf_TranslucencyLighting _TranslucencyLighting = ECk_Usf_TranslucencyLighting::Inherit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _TwoSided = false;

    // Preprocessor defines injected into the look's Custom nodes (pixel AND WPO), each "NAME" or
    // "NAME=VALUE". A define change requires regenerating the master.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    TArray<FString> _Defines;

    // Usage flags baked into the generated master at generation time; hand-set flags on a generated master
    // are wiped by the next regeneration, and a missing flag falls back to the default material in packaged
    // builds. CkIsm needs InstancedStaticMeshes; CkIskm needs SkeletalMesh (+ MorphTargets if morphed);
    // Nanite meshes need Nanite.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _UsedWithInstancedStaticMeshes = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _UsedWithSkeletalMesh = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _UsedWithMorphTargets = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _UsedWithNanite = false;

    // Niagara SPRITE renderers (MATUSAGE_NiagaraSprites). Without it a sprite renderer using this master
    // falls back to the default material in a packaged build.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _UsedWithNiagaraSprites = false;

    // Niagara MESH-PARTICLE renderers (MATUSAGE_NiagaraMeshParticles) â€” the same silent packaged-build
    // fallback as the sprite flag, on the renderer that draws a carrier mesh per particle.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _UsedWithNiagaraMeshParticles = false;

    // Niagara RIBBON renderers (MATUSAGE_NiagaraRibbons) â€” the trail renderer that links particles into a strip.
    // The three Niagara usages are INDEPENDENT engine bits, so a material drawn on a ribbon in one system and on
    // a sprite in another needs two generated masters, not one carrying both flags.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _UsedWithNiagaraRibbons = false;

    // Surface-only: wire the renderer's per-particle color into the PIXEL Custom node (In.ParticleColor).
    // Opt-in because it costs an interpolator and reads as opaque white outside a particle renderer.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _ParticleColor = false;

    // Surface-only: wire the Niagara Dynamic Material Parameter float4 into the PIXEL Custom node
    // (In.DynamicParameter). The emitter writes it as Particles.DynamicMaterialParameter; channel meanings
    // are the effect's own contract. Opt-in for the same reason as _ParticleColor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    bool _ParticleDynamicParameter = false;

    // Names for the four DynamicParameter channels, purely for readability in the generated master
    // (the .ush reads In.DynamicParameter.xyzw positionally). Empty entries fall back to Param1..4.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    TArray<FString> _ParticleDynamicParameterNames;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    TArray<FCk_Usf_ParamDesc> _Parameters;

    // Logical look name; defaults to asset name if None. Drives the generated master path.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FName _LookName = NAME_None;

    // Content root the generated master is saved under and loaded from, e.g. "/Game/MyGame/Materials/GeneratedLooks".
    // Empty keeps the framework root, which is where every look that ships with CkFoundation lives. A look that
    // belongs to a game sets this so its master is saved in the game's own content, not in the plugin's.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FString _GeneratedPackageRoot;

    UFUNCTION(BlueprintPure, Category = "CkUsf",
              DisplayName = "[Ck][Usf] Get Effective Generated Package Root")
    FString Get_EffectiveGeneratedPackageRoot() const;

    // InPackageRootOverride is the tests-only seam described in CkUsf_LookDefinition_Naming.h; when set it wins.
    auto
    Get_GeneratedMasterPackagePath(
        const FString& InPackageRootOverride = {}) const -> FString;

    auto
    Get_GeneratedMasterObjectPath(
        const FString& InPackageRootOverride = {}) const -> FString;

    UFUNCTION(BlueprintCallable, Category = "CkUsf",
              DisplayName = "[Ck][Usf] Get Effective Look Name")
    FName Get_EffectiveLookName() const;

    // First slot of InParamName's per-instance data, or -1 if the param is not per-instance.
    UFUNCTION(BlueprintPure, Category = "CkUsf",
              DisplayName = "[Ck][Usf] Get Per-Instance Slot Of")
    int32 Get_PerInstanceSlotOf(FName InParamName) const;

    // Total per-instance floats this look consumes (the NumCustomDataFloats an ISM must allocate).
    UFUNCTION(BlueprintPure, Category = "CkUsf",
              DisplayName = "[Ck][Usf] Get Num Per-Instance Floats")
    int32 Get_NumPerInstanceFloats() const;
};
