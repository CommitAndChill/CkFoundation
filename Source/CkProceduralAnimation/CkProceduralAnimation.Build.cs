using System.IO;
using UnrealBuildTool;

public class CkProceduralAnimation : CkModuleRules
{
    public CkProceduralAnimation(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "CkCore",
            "CkEcs",
            "CkEcsExt",
            "CkJolt",
            "CkLabel",
            "CkLog",
            "CkRecord",
            "CkResourceLoader",
            "CkTween",
        });

        PrivateDependencyModuleNames.Add("AnimationCore");
    }
}
