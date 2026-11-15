#pragma once

#include "CkCore/Format/CkFormat.h"

#include "CkSettings/UserSettings/CkUserSettings.h"

#include "CkStyleSettings.generated.h"

// ====================================================================================================================
// Editor-time tunables for Ck editor tooling UIs — Editor Preferences → Ck → Style. Read live via
// GetDefault<UCk_Style_UserSettings_UE>(), normally through CkEditorTools/Style/CkStyle.h. See CLAUDE.md.
// ====================================================================================================================

UENUM()
enum class ECk_Style_PaletteMode : uint8
{
	FollowEditorTheme,
	AlwaysDark,
	AlwaysLight,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Style_PaletteMode);

// --------------------------------------------------------------------------------------------------------------------

// The colour slots of the flat Dark properties on UCk_Style_UserSettings_UE, retuned for a light editor theme.
USTRUCT()
struct CKEDITORTOOLS_API FCk_Style_Palette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor BgRoot = FLinearColor(FColor(0xe5, 0xe5, 0xe5, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Bg1 = FLinearColor(FColor(0xec, 0xec, 0xec, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Bg2 = FLinearColor(FColor(0xf3, 0xf3, 0xf3, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Bg3 = FLinearColor(FColor(0xfa, 0xfa, 0xfa, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Border = FLinearColor(FColor(0xd3, 0xd6, 0xdb, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor BorderStrong = FLinearColor(FColor(0xbc, 0xc1, 0xc9, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Text = FLinearColor(FColor(0x1b, 0x1b, 0x1b, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor TextDim = FLinearColor(FColor(0x5d, 0x5d, 0x5d, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor TextMute = FLinearColor(FColor(0x78, 0x78, 0x78, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor TextStrong = FLinearColor(FColor(0x0f, 0x0f, 0x0f, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Selection = FLinearColor(FColor(0x4a, 0x90, 0xe2, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor SelectionInactive = FLinearColor(FColor(0xd0, 0xd5, 0xde, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Hover = FLinearColor(FColor(0xd6, 0xda, 0xe1, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor None = FLinearColor(FColor(0x8a, 0x8a, 0x8a, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor EntityId = FLinearColor(FColor(0x17, 0x62, 0xcd, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Transform = FLinearColor(FColor(0x4d, 0x70, 0x14, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Network = FLinearColor(FColor(0x70, 0x66, 0x0c, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Relationship = FLinearColor(FColor(0xc2, 0x16, 0x70, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Attribute = FLinearColor(FColor(0x0f, 0x6f, 0x8b, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Reference = FLinearColor(FColor(0x17, 0x62, 0xcd, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PickMarker_Default = FLinearColor(FColor(0x10, 0x6e, 0x91, 166));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PickMarker_Hover = FLinearColor(FColor(0x6f, 0x67, 0x0c, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor AxisX = FLinearColor(FColor(0xcc, 0x17, 0x1f, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor AxisY = FLinearColor(FColor(0x28, 0x74, 0x1b, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor AxisZ = FLinearColor(FColor(0x17, 0x61, 0xcf, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_Bool_True = FLinearColor(FColor(0x0d, 0x76, 0x32, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_Bool_False = FLinearColor(FColor(0xcb, 0x17, 0x17, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_Numeric = FLinearColor(FColor(0x1a, 0x75, 0x1a, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_String = FLinearColor(FColor(0x7f, 0x62, 0x0e, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_Math = FLinearColor(FColor(0x4f, 0x4f, 0xeb, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_Tag = FLinearColor(FColor(0x90, 0x2b, 0xe7, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_Enum = FLinearColor(FColor(0x13, 0x71, 0x71, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_Object = FLinearColor(FColor(0x87, 0x5e, 0x12, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Value_Handle = FLinearColor(FColor(0x10, 0x6e, 0x92, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor State_Enabled = FLinearColor(FColor(0x0d, 0x73, 0x58, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor State_Disabled = FLinearColor(FColor(0xcb, 0x17, 0x17, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor State_Overlapping = FLinearColor(FColor(0x6c, 0x6a, 0x0c, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor State_Config = FLinearColor(FColor(0x70, 0x66, 0x0c, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Status_NotStarted = FLinearColor(FColor(0x66, 0x66, 0x66, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Status_Active = FLinearColor(FColor(0x12, 0x6c, 0xa4, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Status_Completed = FLinearColor(FColor(0x31, 0x74, 0x22, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Status_Failed = FLinearColor(FColor(0xc3, 0x28, 0x16, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Graph_Background = FLinearColor(FColor(0xf3, 0xf4, 0xf7, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Graph_Edge = FLinearColor(FColor(0x8a, 0x8f, 0x99, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Graph_Node_Center = FLinearColor(FColor(0xe6, 0xe9, 0xf2, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Graph_Node_Default = FLinearColor(FColor(0xff, 0xff, 0xff, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Graph_Node_Border_Default = FLinearColor(FColor(0x51, 0x6a, 0x76, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Graph_Node_Border_Center = FLinearColor(FColor(0x33, 0x74, 0x35, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Accent = FLinearColor(FColor(0x11, 0x6c, 0x9a, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Ok = FLinearColor(FColor(0x1d, 0x74, 0x4a, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Err = FLinearColor(FColor(0xca, 0x16, 0x34, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Warn = FLinearColor(FColor(0x87, 0x5d, 0x0f, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Info = FLinearColor(FColor(0x25, 0x6d, 0x89, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor AccentDim = FLinearColor(FColor(0xd9, 0xe7, 0xef, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor OkDim = FLinearColor(FColor(0xdb, 0xe9, 0xe2, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor ErrDim = FLinearColor(FColor(0xf6, 0xda, 0xde, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor WarnDim = FLinearColor(FColor(0xe9, 0xe2, 0xd4, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor InfoDim = FLinearColor(FColor(0xdc, 0xe8, 0xec, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor NeutralDim = FLinearColor(FColor(0xe2, 0xe5, 0xea, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor CategoryGather = FLinearColor(FColor(0x36, 0x74, 0x36, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor CategoryBuild = FLinearColor(FColor(0x85, 0x5f, 0x1d, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor CategoryResearch = FLinearColor(FColor(0x25, 0x6d, 0x89, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor CategoryTrain = FLinearColor(FColor(0xb8, 0x3a, 0x3a, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor CategoryAge = FLinearColor(FColor(0x96, 0x3d, 0xbb, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor CategoryTrade = FLinearColor(FColor(0x7d, 0x62, 0x22, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PaneHeadingColor = FLinearColor(FColor(0x5e, 0x66, 0x79, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Fill_Pending = FLinearColor(FColor(0xf1, 0xf3, 0xf6, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Border_Pending = FLinearColor(FColor(0xd5, 0xda, 0xe1, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Badge_Pending = FLinearColor(FColor(0xe4, 0xe8, 0xee, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Fill_Active = FLinearColor(FColor(0xe0, 0xeb, 0xe8, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Border_Active = FLinearColor(FColor(0x1f, 0x73, 0x5c, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Badge_Active = FLinearColor(FColor(0x1f, 0x73, 0x5c, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Fill_Done = FLinearColor(FColor(0xe1, 0xec, 0xe4, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Border_Done = FLinearColor(FColor(0x28, 0x74, 0x41, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStep_Badge_Done = FLinearColor(FColor(0x28, 0x74, 0x41, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStrip_GoalFill = FLinearColor(FColor(0xea, 0xe6, 0xd8, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor PlanStrip_GoalBorder = FLinearColor(FColor(0x7e, 0x62, 0x0e, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor NodeFill_Inactive = FLinearColor(FColor(0xf1, 0xf3, 0xf6, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor NodeBorder_Inactive = FLinearColor(FColor(0xd5, 0xda, 0xe1, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor NodeFill_InPlan = FLinearColor(FColor(0xe0, 0xeb, 0xef, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor NodeBorder_InPlan = FLinearColor(FColor(0x25, 0x6d, 0x89, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor NodeFill_Goal = FLinearColor(FColor(0xea, 0xe6, 0xd8, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor NodeBorder_Goal = FLinearColor(FColor(0x7e, 0x62, 0x0e, 255));

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor NodeFill_GoalInactive = FLinearColor(FColor(0xf5, 0xf2, 0xec, 255));
};

// ====================================================================================================================

UCLASS(meta = (DisplayName = "Style"))
class CKEDITORTOOLS_API UCk_Style_UserSettings_UE : public UCk_Plugin_UserSettings_UE
{
	GENERATED_BODY()

public:
	virtual auto GetCategoryName() const -> FName override { return TEXT("Ck"); }
	virtual auto GetSectionName()  const -> FName override { return TEXT("Style"); }

	UPROPERTY(Config, EditAnywhere, Category = "Palette")
	ECk_Style_PaletteMode PaletteMode = ECk_Style_PaletteMode::FollowEditorTheme;

	// Off: the in-viewport overlays (input HUD, entity debug cards) stay on the Dark palette in a light editor.
	UPROPERTY(Config, EditAnywhere, Category = "Palette")
	bool bOverlaysFollowEditorTheme = true;

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Light")
	FCk_Style_Palette Light;

	// ----- Palette: Backgrounds — each tier slightly lighter than the last --
	// Defaults follow CkGoapDebugger/Mockups/mockup_d_mission_control.html: inset → bg → panel2 → panel
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor BgRoot = FLinearColor(FColor(0x10, 0x14, 0x1b, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg1 = FLinearColor(FColor(0x14, 0x18, 0x1f, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg2 = FLinearColor(FColor(0x17, 0x1c, 0x25, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Backgrounds")
	FLinearColor Bg3 = FLinearColor(FColor(0x1b, 0x21, 0x2b, 255));

	// ----- Palette: Borders --------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Borders")
	FLinearColor Border = FLinearColor(FColor(0x22, 0x29, 0x37, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Borders")
	FLinearColor BorderStrong = FLinearColor(FColor(0x2a, 0x32, 0x40, 255));

	// ----- Palette: Text -----------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor Text = FLinearColor(FColor(0xe9, 0xed, 0xf4, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor TextDim = FLinearColor(FColor(0x94, 0xa0, 0xb3, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor TextMute = FLinearColor(FColor(0x5d, 0x69, 0x80, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Text")
	FLinearColor TextStrong = FLinearColor(0.95f, 0.95f, 0.95f);

	// ----- Palette: Selection & Hover ---------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Selection")
	FLinearColor Selection = FLinearColor(0.2f, 0.4f, 0.8f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Selection")
	FLinearColor SelectionInactive = FLinearColor(0.15f, 0.15f, 0.2f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Selection")
	FLinearColor Hover = FLinearColor(0.06f, 0.06f, 0.08f);

	// ----- Palette: Domain (ECS / component semantics) ----------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor None = FLinearColor(0.4f, 0.4f, 0.4f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor EntityId = FLinearColor(0.51f, 0.69f, 1.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor Transform = FLinearColor(0.76f, 0.91f, 0.55f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor Network = FLinearColor(1.0f, 0.8f, 0.01f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor Relationship = FLinearColor(0.97f, 0.73f, 0.85f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor Attribute = FLinearColor(0.55f, 0.85f, 0.95f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor Reference = FLinearColor(0.51f, 0.69f, 1.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor PickMarker_Default = FLinearColor(0.35f, 0.75f, 0.95f, 0.65f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor PickMarker_Hover = FLinearColor(1.0f, 0.9f, 0.2f, 1.0f);

	// UE-gizmo-familiar R/G/B, pulled toward the existing Domain/Value brightness so an axis-colored
	// numeric column sits beside Value_Numeric text without shouting: X from the Value_Bool_False red
	// family, Y from the Value_Numeric green family, Z from the EntityId blue family.
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor AxisX = FLinearColor(0.94f, 0.36f, 0.38f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor AxisY = FLinearColor(0.50f, 0.85f, 0.45f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Domain")
	FLinearColor AxisZ = FLinearColor(0.42f, 0.62f, 1.0f);

	// ----- Palette: Value-type colors ---------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_Bool_True = FLinearColor(0.2f, 1.0f, 0.4f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_Bool_False = FLinearColor(1.0f, 0.4f, 0.4f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_Numeric = FLinearColor(0.6f, 0.9f, 0.6f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_String = FLinearColor(1.0f, 0.85f, 0.5f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_Math = FLinearColor(0.7f, 0.7f, 1.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_Tag = FLinearColor(0.8f, 0.6f, 1.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_Enum = FLinearColor(0.5f, 0.9f, 0.9f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_Object = FLinearColor(0.9f, 0.7f, 0.4f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Values")
	FLinearColor Value_Handle = FLinearColor(0.4f, 0.8f, 1.0f);

	// ----- Palette: State colors --------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|States")
	FLinearColor State_Enabled = FLinearColor(0.0f, 1.0f, 0.5f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|States")
	FLinearColor State_Disabled = FLinearColor(1.0f, 0.5f, 0.5f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|States")
	FLinearColor State_Overlapping = FLinearColor(1.0f, 0.95f, 0.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|States")
	FLinearColor State_Config = FLinearColor(1.0f, 0.8f, 0.01f);

	// ----- Palette: Status colors (objective / task progress) --------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Status")
	FLinearColor Status_NotStarted = FLinearColor(0.5f, 0.5f, 0.5f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Status")
	FLinearColor Status_Active = FLinearColor(0.55f, 0.78f, 0.95f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Status")
	FLinearColor Status_Completed = FLinearColor(0.6f, 0.85f, 0.55f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Status")
	FLinearColor Status_Failed = FLinearColor(0.95f, 0.35f, 0.3f);

	// ----- Palette: Graph canvas colors -------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Graph")
	FLinearColor Graph_Background = FLinearColor(FColor(0x0D, 0x0D, 0x14));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Graph")
	FLinearColor Graph_Edge = FLinearColor(0.4f, 0.4f, 0.45f);

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Graph")
	FLinearColor Graph_Node_Center = FLinearColor(FColor(0x2D, 0x2D, 0x3D));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Graph")
	FLinearColor Graph_Node_Default = FLinearColor(FColor(0x1E, 0x1E, 0x2E));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Graph")
	FLinearColor Graph_Node_Border_Default = FLinearColor(FColor(0x60, 0x7D, 0x8B));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Graph")
	FLinearColor Graph_Node_Border_Center = FLinearColor(FColor(0x4C, 0xAF, 0x50));

	// ----- Palette: Semantic -------------------------------------------------
	// Mission Control accent is telemetry cyan; Warn stays amber-family so warnings keep meaning beside it
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Accent = FLinearColor(FColor(0x54, 0xc6, 0xff, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Ok = FLinearColor(FColor(0x46, 0xd0, 0x8d, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Err = FLinearColor(FColor(0xf0, 0x63, 0x7a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Warn = FLinearColor(FColor(0xf2, 0xb3, 0x3d, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic")
	FLinearColor Info = FLinearColor(FColor(0x5f, 0xb3, 0xd4, 255));

	// ----- Palette: Semantic dim backgrounds ---------------------------------
	// Chip/pill fills (mockup *-dim tokens) — dark enough that tone-colored text on top passes contrast
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic Dim")
	FLinearColor AccentDim = FLinearColor(FColor(0x1b, 0x34, 0x48, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic Dim")
	FLinearColor OkDim = FLinearColor(FColor(0x12, 0x36, 0x2a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic Dim")
	FLinearColor ErrDim = FLinearColor(FColor(0x3b, 0x1b, 0x24, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic Dim")
	FLinearColor WarnDim = FLinearColor(FColor(0x3a, 0x2e, 0x12, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic Dim")
	FLinearColor InfoDim = FLinearColor(FColor(0x14, 0x30, 0x3c, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Semantic Dim")
	FLinearColor NeutralDim = FLinearColor(FColor(0x10, 0x14, 0x1b, 255));

	// ----- Palette: Action Categories ---------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryGather = FLinearColor(FColor(0x4e, 0xa8, 0x4e, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryBuild = FLinearColor(FColor(0xc2, 0x8a, 0x2a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryResearch = FLinearColor(FColor(0x5f, 0xb3, 0xd4, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryTrain = FLinearColor(FColor(0xc7, 0x4c, 0x4c, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryAge = FLinearColor(FColor(0xb4, 0x6f, 0xd0, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Palette|Categories")
	FLinearColor CategoryTrade = FLinearColor(FColor(0xd4, 0xb1, 0x5f, 255));

	// ----- Typography --------------------------------------------------------
	// Global scale. Per-widget sizes below override these where needed.
	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH2 = 12;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH3 = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeH4 = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeBody = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeSmall = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 FontSizeMicro = 8;

	// ----- Pane Headings -----------------------------------------------------
	// Every pane header is a separate tunable so nothing sneaks through with a hardcoded size/color
	UPROPERTY(Config, EditAnywhere, Category = "Pane Headings", meta = (ClampMin = 6, ClampMax = 24))
	int32 PaneHeadingFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Pane Headings")
	FLinearColor PaneHeadingColor = FLinearColor(FColor(0x8a, 0x92, 0xa4, 255));

	// ----- Plan Strip --------------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_TitleFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_MetaFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_GoalLabelFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_GoalNameFontSize = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepNameFontSize = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepCostFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip", meta = (ClampMin = 6, ClampMax = 24))
	int32 PlanStrip_StepStateFontSize = 7;

	// Step pills use opaque fills, not tints — a translucent overlay reads as a saturated wash
	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Fill_Pending = FLinearColor(FColor(0x16, 0x1b, 0x24, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Border_Pending = FLinearColor(FColor(0x23, 0x2a, 0x38, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Pending Step")
	FLinearColor PlanStep_Badge_Pending = FLinearColor(FColor(0x1b, 0x22, 0x30, 255));

	// Active step uses a teal family so it reads distinct from goals (amber Accent) and plan (Info blue)
	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Fill_Active = FLinearColor(FColor(0x0a, 0x28, 0x22, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Border_Active = FLinearColor(FColor(0x5f, 0xd4, 0xb3, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Active Step")
	FLinearColor PlanStep_Badge_Active = FLinearColor(FColor(0x5f, 0xd4, 0xb3, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Fill_Done = FLinearColor(FColor(0x10, 0x22, 0x17, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Border_Done = FLinearColor(FColor(0x55, 0xc4, 0x7a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Done Step")
	FLinearColor PlanStep_Badge_Done = FLinearColor(FColor(0x55, 0xc4, 0x7a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Goal Pill")
	FLinearColor PlanStrip_GoalFill = FLinearColor(FColor(0x2a, 0x22, 0x0a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Plan Strip|Goal Pill")
	FLinearColor PlanStrip_GoalBorder = FLinearColor(FColor(0xf5, 0xc8, 0x42, 255));

	// ----- Graph Nodes -------------------------------------------------------
	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Inactive")
	FLinearColor NodeFill_Inactive = FLinearColor(FColor(0x16, 0x1b, 0x24, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Inactive")
	FLinearColor NodeBorder_Inactive = FLinearColor(FColor(0x23, 0x2a, 0x38, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|In Plan")
	FLinearColor NodeFill_InPlan = FLinearColor(FColor(0x14, 0x23, 0x35, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|In Plan")
	FLinearColor NodeBorder_InPlan = FLinearColor(FColor(0x5f, 0xb3, 0xd4, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeFill_Goal = FLinearColor(FColor(0x2a, 0x22, 0x0a, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeBorder_Goal = FLinearColor(FColor(0xf5, 0xc8, 0x42, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Goal")
	FLinearColor NodeFill_GoalInactive = FLinearColor(FColor(0x15, 0x13, 0x08, 255));

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeTitleFontSize = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeCostFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes|Typography", meta = (ClampMin = 6, ClampMax = 24))
	int32 NodeMetaFontSize = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes", meta = (ClampMin = 0.5, ClampMax = 5))
	float NodeBorderThickness = 1.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Graph Nodes", meta = (ClampMin = 0.1, ClampMax = 1))
	float NodeInactiveOpacity = 0.55f;

	// ----- Alpha ladder ------------------------------------------------------
	// The four strengths a translucent overlay is allowed to take. Defaults are the literals that
	// already dominate the debugger suite, so migrating a hardcoded alpha onto a rung is a no-op.
	UPROPERTY(Config, EditAnywhere, Category = "Alphas", meta = (ClampMin = 0, ClampMax = 1))
	float AlphaFaint = 0.15f;

	UPROPERTY(Config, EditAnywhere, Category = "Alphas", meta = (ClampMin = 0, ClampMax = 1))
	float AlphaDim = 0.30f;

	UPROPERTY(Config, EditAnywhere, Category = "Alphas", meta = (ClampMin = 0, ClampMax = 1))
	float AlphaSoft = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category = "Alphas", meta = (ClampMin = 0, ClampMax = 1))
	float AlphaStrong = 0.65f;

	UPROPERTY(Config, EditAnywhere, Category = "Alphas", meta = (ClampMin = 0, ClampMax = 1))
	float SelectionOverlayAlpha = 0.22f;

	UPROPERTY(Config, EditAnywhere, Category = "Alphas", meta = (ClampMin = 0, ClampMax = 1))
	float HoverOverlayAlpha = 0.30f;

	// Backdrop strength for the tinted well behind an icon glyph.
	UPROPERTY(Config, EditAnywhere, Category = "Alphas", meta = (ClampMin = 0, ClampMax = 1))
	float IconWellAlpha = 0.15f;

	// ----- Shape -------------------------------------------------------------
	// Corner radii as VALUES, not brushes: a brush is a Slate object and has to be registered once
	// by whoever owns a style set. These are the numbers those registrations are built from.
	UPROPERTY(Config, EditAnywhere, Category = "Shape", meta = (ClampMin = 0, ClampMax = 32))
	float RadiusS = 3.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Shape", meta = (ClampMin = 0, ClampMax = 32))
	float RadiusM = 6.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Shape", meta = (ClampMin = 0, ClampMax = 32))
	float RadiusL = 8.0f;

	// Large enough that the rounded-box shader clamps to a full half-capsule at any height.
	UPROPERTY(Config, EditAnywhere, Category = "Shape", meta = (ClampMin = 0, ClampMax = 999))
	float RadiusPill = 99.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Shape", meta = (ClampMin = 0, ClampMax = 8))
	float RingWidth = 1.0f;
};

// ====================================================================================================================
