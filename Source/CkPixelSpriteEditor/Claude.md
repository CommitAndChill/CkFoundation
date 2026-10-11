# CkPixelSpriteEditor

Editor-only import support for the runtime `UCk_PixelSpriteAsset_UE` type. The single factory accepts `.xpm` images and version-1 `.json` animation manifests; it constructs owned indexed pixel data without creating separate `UTexture2D` assets. A lone XPM becomes a one-frame looping `default` animation.

The factory stages and validates all referenced sources and source-file hashes before changing an existing asset. On success it replaces the imported manifest, updates the root source path, and publishes `UAssetImportData` for the manifest and every unique referenced XPM. The runtime asset publishes the same source list in its asset-registry tag so Unreal's automatic reimport can discover frame dependencies. A change to any source index reloads the whole authored manifest. A failed ordinary reimport leaves the asset snapshot, source metadata, preview settings, revision, and package dirty state unchanged.

`SetReimportPaths` stages a root-path relocation until successful reimport. Referenced frame paths belong to the manifest and cannot be independently retargeted through that API. Unreal's `FReimportManager::UpdateReimportPath(s)` marks the package dirty itself when explicitly changing a path; that engine behavior is outside the ordinary failed-reimport guarantee.

The asset definition calls the public `ck::pixel_sprite_editor::Get_OpenViewer()` delegate. CkPixelArtViewer binds it from its editor module and unbinds at shutdown/pre-exit. This module has no dependency on `CkGameplayDebugger`; while no viewer is bound, opening is unhandled.

JSON recognition uses the same strict native loader as import, including depth/node/UTF-8 and ordinary-file limits. Recognition may read dependencies; a manifest with unavailable sources is not claimed by this factory. It never feeds unbounded external JSON into Unreal's recursive DOM parser.

CkPixelSpriteEditor_Export.h writes derived frame PNGs and horizontal sequence sheets with JSON duration/anchor/event/metadata sidecars. Destinations must be new absolute files under ordinary existing folders; existing files, links/junctions and aliases cannot be overwritten. Both sheet outputs stage under unique sibling temporary names and roll back owned newly published output if the pair fails. Source files and indexed asset payloads are never modified. Sheet dimensions are bounded to 16384, image/encoded PNG to 64 MiB and JSON to 16 MiB.

Focused automation covers factory/reimport and asset-opening bridge, disk package reload with unavailable sources, real Blueprint VM execution, PNG decoding and sheet metadata, plus production import of Robot and Character examples into /Game/PixelSpriteExamples. The runtime asset's separate tests cover reflected memory serialization. Exact executed verdicts live in host Plans/PixelSprites/PROGRESS.md. Cooked runtime and actual Content Browser double-click require their own integration evidence.
