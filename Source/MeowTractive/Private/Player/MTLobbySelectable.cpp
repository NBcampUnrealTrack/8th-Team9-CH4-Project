#include "Player/MTLobbySelectable.h"

#include "Components/MeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"

void MTLobbyOutline::SetActorOutline(AActor* Target, bool bEnable, int32 StencilValue)
{
	if (!Target)
	{
		return;
	}

	// 프롬프트 위젯(UWidgetComponent도 UMeshComponent)은 아웃라인 제외
	TInlineComponentArray<UMeshComponent*> Meshes(Target);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || Mesh->IsA<UWidgetComponent>())
		{
			continue;
		}
		if (bEnable)
		{
			Mesh->SetCustomDepthStencilValue(StencilValue);
			Mesh->SetRenderCustomDepth(true);
		}
		else
		{
			// 끄지 않고 아키타입(BP 기본) 값으로 복원 — 상시 아웃라인(예: 조형물 스텐실 1) 유지
			const UMeshComponent* Archetype = Cast<UMeshComponent>(Mesh->GetArchetype());
			const bool bDefaultRender = Archetype && Archetype->bRenderCustomDepth;
			const int32 DefaultStencil = Archetype ? Archetype->CustomDepthStencilValue : 0;
			Mesh->SetCustomDepthStencilValue(DefaultStencil);
			Mesh->SetRenderCustomDepth(bDefaultRender);
		}
	}
}
