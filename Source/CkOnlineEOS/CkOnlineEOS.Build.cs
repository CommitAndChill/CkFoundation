using UnrealBuildTool;

public class CkOnlineEOS : CkModuleRules
{
    public CkOnlineEOS(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "SocketSubsystemEOS", "OnlineSubsystemUtils" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "CkCore", "OnlineServicesInterface", "OnlineServicesCommon",
            "OnlineSubsystem", "EOSShared", "EOSSDK"
        });
    }
}
