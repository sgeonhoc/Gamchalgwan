using UnrealBuildTool;

public class GamchalgwanEditorTarget : TargetRules
{
	public GamchalgwanEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Gamchalgwan");
	}
}
