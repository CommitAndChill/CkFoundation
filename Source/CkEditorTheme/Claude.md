# CkEditorTheme

**Editor infrastructure module** (`Editor`, `PostEngineInit`). The Ck Light editor theme, the Follow Windows mode, and the
style, icon and graph patches a light theme needs.

**Depends on:** `CkCore`, `CkLog`, `CkSettings`.

**Light/Dark:** decided by `ck::theme::Get_IsEditorThemeLight` (CkCore), the same rule `CkStyle` uses. Patches apply only
while that is true; Dark values are the captured originals, except node titles and sublines, which use the `User16` /
`User15` theme slots in both because graph nodes copy their text style when built; the controller writes those slots, so
a hot unload without a reload leaves them unwritten. Unattended runs force Dark.

For context, see [`/Source/EDITOR_MODULES.md`](/Source/EDITOR_MODULES.md).
