using UnrealBuildTool;

public class CkOnline : CkModuleRules
{
    public CkOnline(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "CkCore", "CkEcs", "CkEcsExt", "CkLog"
        });
        PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Networking", "Json", "GameplayTags", "OnlineSubsystemUtils" });
    }
}
