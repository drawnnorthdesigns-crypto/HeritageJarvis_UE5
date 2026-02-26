using UnrealBuildTool;
using System.Collections.Generic;

public class HeritageJarvisEditorTarget : TargetRules
{
	public HeritageJarvisEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HeritageJarvis");
	}
}
