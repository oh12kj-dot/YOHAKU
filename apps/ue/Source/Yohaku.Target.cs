using UnrealBuildTool;
public class YohakuTarget : TargetRules {
    public YohakuTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Game; DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.AddRange(new[] { "YohakuCore", "Yohaku" });
    }
}
