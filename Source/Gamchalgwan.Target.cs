using UnrealBuildTool;

public class GamchalgwanTarget : TargetRules
{
	public GamchalgwanTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Gamchalgwan");
	}
}
