// Copyright 2026 RainButterfly. All Rights Reserved. 
using UnrealBuildTool; 

public class GraphWeaverPlugin : ModuleRules { 
	public GraphWeaverPlugin(ReadOnlyTargetRules Target) : base(Target) 
	{ 
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs; 
		PublicIncludePaths.AddRange( new string[] {  } ); 
		PrivateIncludePaths.AddRange( new string[] {  } ); 
		PublicDependencyModuleNames.AddRange( new string[] { "Core", "GameplayTags", } ); 
		PrivateDependencyModuleNames.AddRange( new string[] { "CoreUObject", "Engine", "Slate", "SlateCore", } ); 
		DynamicallyLoadedModuleNames.AddRange( new string[] {  } ); 
	} 
}

