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

		// 고등어(범이) 눈빛 키트
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_EyeBeam_Glare);     // 째려보기 (단일 CC)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_EyeBeam_HeartBeam); // 하트광선 (5초 지속 전방 빔)

		// 점박이(나비) 암살 키트
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Spotted_Cling);     // 매달리기 (도약 후 지연 피해)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Spotted_Dominance); // 서열정리 (돌진 + 처형)

		// 뚱냥이(치즈) 골골 키트
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Fatty_Purr);    // 골골거리기 (광역 도트 + 약감속)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Fatty_LieDown); // 드러눕기 (자기속박 광역 도트 + 강감속)
	}

	// --- 쿨다운 식별 태그 (쿨다운 중 부여) ---
	namespace Cooldown
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_EyeBeam_Glare);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_EyeBeam_HeartBeam);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Spotted_Cling);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Spotted_Dominance);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Fatty_Purr);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Fatty_LieDown);
	}

	// --- 고양이 상태 ---
	namespace State
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Stun);    // 피격 스턴 (입력/이동 정지)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Casting); // 액션 스킬 시전 중 (타 액션 상호배제, 이동은 예외)
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
