// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

using UnrealBuildTool;

public class DigitalHumanRuntime : ModuleRules
{
	public DigitalHumanRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Stage 3 adds Json for parsing backend control messages (audio format headers, etc).
		// Still to come, added only when the corresponding system is built:
		//   AnimGraphRuntime, ControlRig, RigLogicModule -> Stage 4+ (facial animation)
		//   NNE, NNERuntimeORT (if/when a model is introduced) -> optional, later
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"WebSockets",
			"Json"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects"
		});
	}
}
