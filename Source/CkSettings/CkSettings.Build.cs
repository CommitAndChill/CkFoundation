using System.IO;
using UnrealBuildTool;

public class CkSettings : CkModuleRules
{
    public CkSettings(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.AddRange(new string[] {
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
        });
    }
}
