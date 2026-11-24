# CkEditorTheme

**Editor infrastructure module** (`Editor`, `PostEngineInit`). The Ck Light editor theme, the Follow Windows mode, and the
style, icon and graph patches a light theme needs.

**Depends on:** `CkCore`, `CkLog`, `CkSettings`.

**Light/Dark:** decided by `ck::theme::Get_IsEditorThemeLight` (CkCore), the same rule `CkStyle` uses. Patches apply only
while that is true; Dark values are the captured originals. Unattended runs force Dark.

For context, see [`/Source/EDITOR_MODULES.md`](/Source/EDITOR_MODULES.md).
