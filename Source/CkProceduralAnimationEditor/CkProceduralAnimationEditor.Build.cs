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
            "AssetDefinition",
            "AssetRegistry",
            "AssetTools",
            "CurveEditor",
            "EditorFramework",
            "InputCore",
            "PropertyEditor",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd",
            "CkTween",
        });
    }
}
