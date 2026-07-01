#include "Game/MTGameplayTags.h"

namespace MTGameplayTags
{
	namespace Ability
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Move_Dash, "Skill.Move.Dash");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Attract_Beam, "Skill.Attract.Beam");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Attack_MeowPunch, "Skill.Attack.MeowPunch");
	}

	namespace State
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_State_Stun, "State.Stun");
	}

	namespace Event
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Event_MeowPunch_Hit, "Event.Montage.MeowPunch.Hit");
	}

	namespace GameplayCue
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_GameplayCue_Cat_Hit, "GameplayCue.Cat.Hit");
	}

	namespace Pedestrian
	{
		UE_DEFINE_GAMEPLAY_TAG(AttractiveInProgress, "State.AttractiveInProgress");
		UE_DEFINE_GAMEPLAY_TAG(Invulnerable, "State.Invulnerable");
		UE_DEFINE_GAMEPLAY_TAG(Attracted, "State.Attracted");
	}
}
