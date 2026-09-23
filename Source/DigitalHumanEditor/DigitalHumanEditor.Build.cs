// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

using UnrealBuildTool;

public class DigitalHumanEditor : ModuleRules
{
	public DigitalHumanEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// NOTE: Stage 1 keeps this minimal. Editor tool systems (Connection Monitor,
		// Curve Debugger, Latency Monitor, Viseme Viewer, Blueprint/MetaHuman Setup
		// Wizards) are added once the runtime systems they visualize actually exist -
		// building a debugger for a system that doesn't exist yet isn't useful.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DigitalHumanRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}
