using UnrealBuildTool;

public class CkOnlineLobby : CkModuleRules
{
    public CkOnlineLobby(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "CkCore" });
        PrivateDependencyModuleNames.Add("OnlineSubsystem");
        bool WithSteam = Target.Type != TargetType.Server &&
            (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux ||
             Target.Platform == UnrealTargetPlatform.Mac);
        PrivateDefinitions.Add("CK_WITH_STEAM_LOBBY=" + (WithSteam ? "1" : "0"));
        if (WithSteam)
        {
            PrivateDependencyModuleNames.Add("SteamShared");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
        }
    }
}
