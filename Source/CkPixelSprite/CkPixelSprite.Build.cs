using UnrealBuildTool;

public class CkPixelSprite : CkModuleRules
{
    public CkPixelSprite(ReadOnlyTargetRules Target) : base(Target, false)
    {
        PublicDependencyModuleNames.Add("CkCore");
        PublicDependencyModuleNames.AddRange(new string[] { "UMG", "SlateCore" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Json", "Projects", "RHI", "Slate" });
    }
}
