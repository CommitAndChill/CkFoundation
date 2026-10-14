using UnrealBuildTool;

public class CkProceduralAnimationEditor : CkModuleRules
{
    public CkProceduralAnimationEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "CkCore",
            "CkEcs",
            "CkEcsExt",
            "CkJolt",
            "CkProceduralAnimation",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AdvancedPreviewScene",
            "AssetDefinition",
            "AssetRegistry",
            "AssetTools",
            "CurveEditor",
            "EditorFramework",
            "EditorWidgets",
            "InputCore",
            "KismetWidgets",
            "PropertyEditor",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "ToolWidgets",
            "UnrealEd",
            "CkEditorTools",
            "CkTween",
        });
    }
}
