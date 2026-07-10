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
			"StateTreeModule",
			"GameplayStateTreeModule",
			//이펙트용
			"Niagara"
		});

		// 스팀 아바타 원시 API (Steamworks SDK) — 서버 빌드 제외.
		// 빌드가 깨지면 이 블록을 지우고 WITH_STEAM_AVATAR=0으로 두면 됨(아바타만 비활성).
		if (Target.Type != TargetType.Server)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
			PublicDefinitions.Add("WITH_STEAM_AVATAR=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_STEAM_AVATAR=0");
		}
	}


}
