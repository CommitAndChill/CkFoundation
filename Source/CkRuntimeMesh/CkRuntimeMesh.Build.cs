using UnrealBuildTool;

public class CkRuntimeMesh : CkModuleRules
{
    public CkRuntimeMesh(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "CkCore",
            "CkEcs",
            "CkEcsExt",
            "CkGraphics",
            "CkResourceLoader",
            "CkSettings",
            "GeometryCore",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "MeshConversionEngineTypes",
            "DynamicMesh",
            "GeometryAlgorithms",
            "GeometryFramework",
        });
    }
}
