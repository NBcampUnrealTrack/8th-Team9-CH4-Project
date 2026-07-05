#pragma once

#include "NativeGameplayTags.h"

namespace MTGameplayTags
{
	// --- 고양이 스킬 ---
	namespace Ability
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Move_Dash);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Attract_Beam);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Attack_MeowPunch);
	}

	// --- 고양이 상태 ---
	namespace State
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Stun); // 피격 스턴 (입력/이동 정지)
	}

	// --- 몽타주 AnimNotify 이벤트 ---
	namespace Event
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_MeowPunch_Hit); // 펀치 데미지 창
	}

	// --- GameplayCue (연출 전용) ---
	namespace GameplayCue
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Cat_Hit); // 고양이 피격 연출
	}

	// --- 행인 상태 ---
	namespace Pedestrian
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttractiveInProgress); // State.AttractiveInProgress (피격 7초 → 회복 정지)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invulnerable);         // State.Invulnerable (매료 직후 15초 무적)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attracted);            // State.Attracted (매료 완료)
	}
}
