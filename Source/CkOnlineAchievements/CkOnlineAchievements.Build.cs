using UnrealBuildTool;

public class CkOnlineAchievements : CkModuleRules
{
    public CkOnlineAchievements(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "OnlineSubsystem" });
        bool HasSteam = Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac;
        PrivateDefinitions.Add("WITH_CK_ACHIEVEMENTS_STEAM=" + (HasSteam ? "1" : "0"));
        if (HasSteam)
        {
            PrivateDependencyModuleNames.Add("SteamShared");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
        }
    }
}
