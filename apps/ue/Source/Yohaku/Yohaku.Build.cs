using UnrealBuildTool;
public class Yohaku : ModuleRules {
    public Yohaku(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "YohakuCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "Json", "ApplicationCore", "RenderCore", "ProceduralMeshComponent" });
        if (Target.Platform == UnrealTargetPlatform.Win64) PublicSystemLibraries.Add("winhttp.lib");
    }
}
