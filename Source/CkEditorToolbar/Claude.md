# CkEditorToolbar

**Editor infrastructure module.** The main CkFoundation editor toolbar — code generator, data viewer, Insights capture. Extend by adding an entry via the toolbar extension point; don't add tools to UE's main menu directly.

**Depends on:** `CkCore`, `CkEcs`, `CkEditorTools`, `CkLog`, `CkResourceLoader`, `CkSettings`, `CkUI`.

**Light/Dark:** toolbar widgets are UMG assets, which hold only concrete colours. Text inside a button or combo uses
Use Foreground. Any other text with a coloured (not grey) colour is bound at creation to `CkStyle::InkOf` of that colour
(`UCk_EditorToolbar_CaptionInk_UE`), so it follows a live switch with no binding in the asset. `[Ck][EditorToolbar] Get
Theme Ink` is the same ink for widgets the toolbar subsystem does not create.

For context, see [`/Source/EDITOR_MODULES.md`](/Source/EDITOR_MODULES.md).
