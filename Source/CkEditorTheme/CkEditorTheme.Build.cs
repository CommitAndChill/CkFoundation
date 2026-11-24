using UnrealBuildTool;

public class CkEditorTheme : CkModuleRules
{
    public CkEditorTheme(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",

            "CkCore",
            "CkLog",
            "CkSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "ContentBrowser",
            "GraphEditor",
            "ImageWrapper",
            "Json",
            "Projects",
            "Settings",
            "Slate",
            "SlateCore",
            "UnrealEd"
        });
    }
}
