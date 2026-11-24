"""Generate the Ck Light Slate theme (CkFoundation/Resources/Themes/Ck Light.json) for UE 5.7/5.8.
Bump REVISION on every palette change: CkEditorTheme re-stages a user-dir copy only when its CkRevision is lower.

Slot names = EStyleColor (Engine/Source/Runtime/SlateCore/Public/Styling/StyleColors.h:31-77).
Values are sRGB hex; USlateThemeManager::LoadThemeColors reads linear FLinearColor strings
(StyleColors.cpp:509 InitFromString), so convert sRGB -> linear here.
"""
import json, os, sys

ID = "6C1A9E1F-4B7D-4E0B-9A51-2C0FDA11C0DE"
NAME = "Ck Light"
REVISION = 3

# Windows 11 light (Fluent) palette mapped onto Starship slots.
COLORS = {
    "Black":            "#000000FF",  # used literally: borders + foreground on light chips (31 refs)
    "Background":       "#EAEAEAFF",  # Mica-ish base
    "Title":            "#E6E6E6FF",
    "WindowBorder":     "#C8C8C8FF",
    "Foldout":          "#F0F0F0FF",
    "Input":            "#FFFFFFFF",  # text boxes
    "InputOutline":     "#D1D1D1FF",
    "Recessed":         "#E8E8E8FF",
    "Panel":            "#F0F0F0FF",  # cards / tab bodies. MUST stay <= #F5 (use <= #F0 for float round-trip margin): PropertyEditorConstants.cpp adds up to +10 per uint8 channel
    "Header":           "#EAEAEAFF",  # also <= #F5 (Details hover base)
    "Dropdown":         "#FFFFFFFF",
    "DropdownOutline":  "#D1D1D1FF",
    "Hover":            "#E3E3E3FF",
    "Hover2":           "#CFCFCFFF",
    "White":            "#FFFFFFFF",
    "White25":          "#00000040",  # used as a subtle overlay; must darken on light bg
    "Highlight":        "#005FB8FF",
    "Primary":          "#005FB8FF",  # Win11 default accent (light)
    "PrimaryHover":     "#196EBFFF",
    "PrimaryPress":     "#3381C9FF",
    "Secondary":        "#FBFBFBFF",  # Win11 standard button fill
    "Foreground":       "#2E2E2EFF",  # ~ Win11 primary ink (black @ 89% over the light base)
    "ForegroundHover":  "#000000FF",
    "ForegroundInverted": "#FFFFFFFF",
    "ForegroundHeader": "#2E2E2EFF",
    "Select":           "#005FB8FF",
    "SelectInactive":   "#C5D7EAFF",
    "SelectParent":     "#E1EAF3FF",
    "SelectHover":      "#EAEAEAFF",
    "Notifications":    "#FFFFFFFF",
    "AccentBlue":       "#0078D4FF",
    "AccentPurple":     "#8764B8FF",
    "AccentPink":       "#C239B3FF",
    "AccentRed":        "#C42B1CFF",
    "AccentOrange":     "#CA5010FF",
    "AccentYellow":     "#9D5D00FF",
    "AccentGreen":      "#0F7B0FFF",
    "AccentBrown":      "#8E562EFF",
    "AccentBlack":      "#242424FF",
    "AccentGray":       "#8A8A8AFF",
    "AccentWhite":      "#FFFFFFFF",
    "AccentFolder":     "#E8B33AFF",  # Win11-like folder yellow; the folder art is shaded, so the tint must be bright
    "Warning":          "#9D5D00FF",
    "Error":            "#C42B1CFF",
    "Success":          "#0F7B0FFF",
}

def lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def to_linear_str(h):
    h = h.lstrip("#")
    r, g, b, a = (int(h[i:i+2], 16) for i in (0, 2, 4, 6))
    return f"(R={lin(r):.6f},G={lin(g):.6f},B={lin(b):.6f},A={a/255.0:.6f})"

theme = {"Version": 1, "Id": ID, "DisplayName": NAME, "CkRevision": REVISION,
         "Colors": {"EStyleColor::" + k: to_linear_str(v) for k, v in COLORS.items()}}  # keys must be enum-qualified (UEnum::GetNameByIndex)
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "Resources", "Themes", NAME + ".json")
with open(out, "w", newline="\r\n") as f:
    json.dump(theme, f, indent="\t")
    f.write("\n")
print("wrote", out)
