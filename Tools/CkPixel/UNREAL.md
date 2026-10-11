# Using pixel sprites in Unreal

The native modules are `CkPixelSprite` (runtime data, assets, rendering and playback), `CkPixelSpriteEditor` (import, reimport and export), and `CkPixelArtViewer` (the debugger plugin's editor viewer). Dependencies point from viewer to editor/runtime, and from editor to runtime. Runtime headers contain no editor interfaces. Existing `CkPixelArt` is the separate scene-pixelation system.

## Import and reimport

Import an XPM or version-1 animation JSON through the Content Browser. A single XPM produces a one-frame `default` animation. The JSON importer reads and validates every referenced XPM before publishing an asset. It preserves indexed source pixels, frame order, millisecond durations, anchors, events and metadata. It creates no separate visible texture assets.

Keep the manifest and its XPM files in an ordinary local source folder. Native loading rejects symlinks and junctions, including links within that folder. Frame paths must be relative and cannot escape the manifest folder. The loader reports a filename, diagnostic code and source location where available. A rejected reimport preserves the previous working payload and Unreal-side rendering settings.

Reimport the asset after editing either JSON or a referenced XPM. Import data lists the root and all unique frame dependencies for Unreal's source-file tooling. Reimporting a frame reloads its complete manifest atomically. Other assets are not modified. Retarget the manifest root rather than individual frame paths; frame paths are authored in JSON.

Double-click an imported asset to open the native Pixel Art Viewer, or open its tool tab from the Ck launcher. See the viewer module's documentation for controls. Canonical art remains XPM/JSON; preview settings and derived exports do not rewrite it.

## Runtime playback

Use `UCk_PixelSpritePlayer_UE` for an explicitly advanced player. Store its reference in an owner-tracked Unreal property. In Blueprint, construct a player, assign its requested asset and call `Request_ApplyRequestedAsset`, or call `Request_LoadAsset` with a cooked object path. Select a named animation with `Request_SelectAnimation`, then call `Request_Play`.

One designated owner advances a shared player once per tick with `Request_AdvanceMilliseconds(DeltaSeconds * 1000)`. A `UCk_PixelSpriteWidget_UE` passively displays it: assign `_RequestedPlayer`, apply it, and refresh the image after advancing. Several widgets may display one player; none of those passive widgets advances the clock. `UCk_PixelSpritePreview_UE` is the alternative ready-to-use UMG preview that owns and advances its own private player.

AngelScript follows the same reflected API:

```angelscript
private UCk_PixelSpritePlayer_UE _Player;

void InitializeSprite()
{
    _Player = Cast<UCk_PixelSpritePlayer_UE>(NewObject(this, UCk_PixelSpritePlayer_UE));
    if (_Player == nullptr)
    { return; }
    if (!_Player.Request_LoadAsset(FSoftObjectPath("/Game/PixelSpriteExamples/Character.Character")))
    { return; }
    if (!_Player.Request_SelectAnimation("idle"))
    { return; }
    _Player.Request_Play();
}

void AdvanceSprite(float InDeltaSeconds)
{
    if (_Player != nullptr)
    { _Player.Request_AdvanceMilliseconds(double(InDeltaSeconds) * 1000.0); }
}
```

The example path must be cooked into the consuming project. In this development host, `Ck.PixelSprite.Examples.ImportNativeAssets` creates the native Character and Robot example assets; their folder is included in cooking configuration.

Player controls include pause, stop, restart, absolute seek, exact source-frame stepping, direction (`-1` or `1`), positive speed, and loop/once/ping-pong playback. One-shot playback stops at either directional endpoint. `Get_CurrentFrame` reports presentation; `Get_LogicalFrame` reports the authored time selection. Sampling quantizes presentation time and decimation selects every Nth source frame without accelerating the clock. Manual stepping displays the requested source frame even while those presentation options are enabled. It moves the clock to an approximate authored start and keeps an explicit inspected ordinal: an extremely brief frame can have no distinct double timestamp, so its logical clock frame may differ from the displayed frame. Repeated stepping follows the inspected ordinal. Paused or zero-motion advancement preserves it; a successful seek or actual playback movement clears it.

`TryResolveAnchor` returns the displayed frame's authored coordinate. Coordinates start at the upper-left pixel, increase right/down, and stay within the canvas. Event JSON is retained as data; automatic event dispatch is optional and is not implemented. Absolute evaluator boundaries use authored decimal timing; repeated delta accumulation uses a finite binary-double clock.

## Rendering and lifetime

Resources prepare every unique source texture before the generation is published. Frame changes select prepared textures without parsing source files or uploading pixels again. Players and viewer windows also reuse an immutable prepared timing timeline, rebuilding it only for animation, playback mode or asset revision replacement. Assets, revision and rendering settings determine cache identity. Failed preparation retains the previous complete generation.

Nearest rendering preserves source colors. Bilinear interpolates linear premultiplied color, then emits straight-alpha sRGB. Smooth silhouette interpolates coverage while retaining nearest authored RGB: it smooths exterior alpha without blending hard interior colors, and may retain sharp interior transitions. Scale is 1–8; outline, background and palette overrides are derived settings. Textures use sRGB BGRA8, nearest sampling, no streaming and no generated mip chain. The CPU renderer performs the selected filtering before upload.

Per-source textures avoid atlas packing/UV complexity while sharing duplicate manifest references. Preparation is bounded by active-device dimensions, 64 MiB for a generation and 128 MiB for old/new overlap. Large collections exceeding those budgets reject with a diagnostic. Resources and widgets retain their textures through Unreal-tracked references; release widgets normally when their owner closes.

## Export and validation

The editor export API writes a frame PNG or a horizontal animation-sheet PNG with JSON timing, anchors, events and metadata. Choose new absolute output files in existing ordinary folders. Existing files and linked paths reject; sheet publication rolls back newly owned output if its companion fails. Exports use derived pixels and leave source data unchanged.

`Ck.PixelSprite.*` native tests cover parsing, timing, payload validation, imports, rendering and runtime behavior. Shared XPM/manifest conformance fixtures are under `Fixtures`; Python tests cover the CLI. `Ck.PixelArtViewer.*` exercises viewer state and rendered Slate output. Runtime Blueprint/AngelScript and the locally packaged Gauntlet scenario provide separate integration evidence. The host's `Plans/PixelSprites/PROGRESS.md` records which checks actually ran and their results.

For a new animation, keep the canvas/palette conventions in `README.md`, add explicit ordered frames and positive `duration_ms` values, validate with the CLI, inspect native-size/nearest previews and differences, then import or reimport. Missing files, malformed rows, unsupported XPM colors and unsafe paths are errors to correct in source rather than silently substituted artwork. Optional layers and full X11 named-color tables are unsupported and reject explicitly.
