// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MeowTractive : ModuleRules
{
	public MeowTractive(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"NetCore",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineSubsystemSteam",
			"CommonUI",
			"CommonInput",
			"UMG",
			"Slate",
			"SlateCore",
			"SkeletalMerging",
			//행인용 모듈
			"NavigationSystem", "AIModule",
			//StateTree (C++ 태스크용)
			"StateTreeModule", "GameplayStateTreeModule" });

	}


}
