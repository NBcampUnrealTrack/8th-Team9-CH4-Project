#include "Game/MTGameplayTags.h"

namespace MTGameplayTags
{
	namespace Ability
	{
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Move_Dash, "Skill.Move.Dash");
		UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Attract_Beam, "Skill.Attract.Beam");
	}

	namespace Pedestrian
	{
		UE_DEFINE_GAMEPLAY_TAG(AttractiveInProgress, "State.AttractiveInProgress");
		UE_DEFINE_GAMEPLAY_TAG(Invulnerable, "State.Invulnerable");
		UE_DEFINE_GAMEPLAY_TAG(Attracted, "State.Attracted");
	}
}
