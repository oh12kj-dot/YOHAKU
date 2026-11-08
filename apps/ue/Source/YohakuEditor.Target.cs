using UnrealBuildTool;
public class YohakuEditorTarget : TargetRules {
    public YohakuEditorTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.AddRange(new[] { "YohakuCore", "Yohaku" });
    }
}
