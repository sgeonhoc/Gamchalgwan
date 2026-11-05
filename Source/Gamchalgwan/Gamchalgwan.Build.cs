using UnrealBuildTool;

public class Gamchalgwan : ModuleRules
{
	public Gamchalgwan(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Few files; keep each .cpp its own unit so file-local helpers never collide.
		bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "RenderCore", "ImageCore"
		});
	}
}
