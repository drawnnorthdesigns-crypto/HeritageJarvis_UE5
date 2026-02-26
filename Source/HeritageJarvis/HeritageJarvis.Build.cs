using UnrealBuildTool;
using System.IO;

public class HeritageJarvis : ModuleRules
{
	public HeritageJarvis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Allow subdirectory includes (e.g. #include "Core/HJGameInstance.h")
		string ModuleDir = ModuleDirectory;
		PublicIncludePaths.AddRange(new string[]
		{
			ModuleDir,
			Path.Combine(ModuleDir, "Core"),
			Path.Combine(ModuleDir, "UI"),
			Path.Combine(ModuleDir, "Game")
		});

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",                    // Unreal Motion Graphics (UI)
			"Slate",
			"SlateCore",
			"HTTP",                   // For Flask API calls
			"Json",                   // JSON parsing
			"JsonUtilities"           // JSON helpers
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"WebBrowserWidget",  // UWebBrowser dashboard embed
			"WebBrowser"
		});
	}
}
