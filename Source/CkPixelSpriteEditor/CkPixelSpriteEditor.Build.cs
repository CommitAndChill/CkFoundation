using UnrealBuildTool;

public class CkPixelSpriteEditor : CkModuleRules
{
    public CkPixelSpriteEditor(ReadOnlyTargetRules Target) : base(Target, false)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "AssetDefinition",
            "CkPixelSprite",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "BlueprintGraph",
            "DataValidation",
            "ImageCore",
            "Json",
            "Projects",
        });
    }
}
