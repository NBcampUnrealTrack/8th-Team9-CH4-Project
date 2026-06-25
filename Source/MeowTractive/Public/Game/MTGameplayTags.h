#pragma once

#include "NativeGameplayTags.h"

namespace MTGameplayTags
{
	// --- 고양이 스킬 ---
	namespace Ability
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Move_Dash);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Attract_Beam);
	}

	// --- 행인 상태 ---
	namespace Pedestrian
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttractiveInProgress); // State.AttractiveInProgress (피격 7초 → 회복 정지)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invulnerable);         // State.Invulnerable (매료 직후 15초 무적)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attracted);            // State.Attracted (매료 완료)
	}
}
