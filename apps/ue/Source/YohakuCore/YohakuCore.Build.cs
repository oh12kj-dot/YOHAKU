using UnrealBuildTool;
public class YohakuCore : ModuleRules {
    public YohakuCore(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "Json" });
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
    }
}
