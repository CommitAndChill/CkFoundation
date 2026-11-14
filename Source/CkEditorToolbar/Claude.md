# CkEditorToolbar

**Editor infrastructure module.** The main CkFoundation editor toolbar — code generator, data viewer, Insights capture. Extend by adding an entry via the toolbar extension point; don't add tools to UE's main menu directly.

**Depends on:** `CkCore`, `CkEcs`, `CkEditorTools`, `CkLog`, `CkResourceLoader`, `CkSettings`, `CkUI`.

**Light/Dark:** toolbar widgets are UMG assets, which hold only concrete colours. Text inside a button or combo uses
Use Foreground; any other coloured text binds its colour to `[Ck][EditorToolbar] Get Theme Ink` (`CkStyle::InkOf` of its Dark hue).

For context, see [`/Source/EDITOR_MODULES.md`](/Source/EDITOR_MODULES.md).
