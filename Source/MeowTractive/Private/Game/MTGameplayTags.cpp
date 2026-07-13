#include "Game/MTGameplayTags.h"

namespace MTGameplayTags
{
	namespace Ability
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Move_Dash, "Skill.Move.Dash");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Attract_Beam, "Skill.Attract.Beam");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Attack_MeowPunch, "Skill.Attack.MeowPunch");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_EyeBeam_Glare, "Skill.EyeBeam.Glare");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_EyeBeam_HeartBeam, "Skill.EyeBeam.HeartBeam");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Spotted_Cling, "Skill.Spotted.Cling");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Spotted_Dominance, "Skill.Spotted.Dominance");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Fatty_Purr, "Skill.Fatty.Purr");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Fatty_LieDown, "Skill.Fatty.LieDown");
	}

	namespace Cooldown
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_EyeBeam_Glare, "Cooldown.EyeBeam.Glare");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_EyeBeam_HeartBeam, "Cooldown.EyeBeam.HeartBeam");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Spotted_Cling, "Cooldown.Spotted.Cling");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Spotted_Dominance, "Cooldown.Spotted.Dominance");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Fatty_Purr, "Cooldown.Fatty.Purr");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Fatty_LieDown, "Cooldown.Fatty.LieDown");
	}

	namespace State
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_State_Stun, "State.Stun");
		UE_DEFINE_GAMEPLAY_TAG(TAG_State_Casting, "State.Casting");
	}

	namespace Event
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Event_MeowPunch_Hit, "Event.Montage.MeowPunch.Hit");
	}

	namespace GameplayCue
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Cat_Hit, "GameplayCue.Cat.Hit");
		UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Cat_Punch, "GameplayCue.Cat.Punch");
	}

	namespace Pedestrian
	{
		UE_DEFINE_GAMEPLAY_TAG(AttractiveInProgress, "State.AttractiveInProgress");
		UE_DEFINE_GAMEPLAY_TAG(Invulnerable, "State.Invulnerable");
		UE_DEFINE_GAMEPLAY_TAG(Attracted, "State.Attracted");
	}
}
