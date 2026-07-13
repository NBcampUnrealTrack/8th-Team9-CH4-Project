#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "MTGameUserSettings.generated.h"

/** 확장 유저 설정: 볼륨 영속화 (GameUserSettings.ini). 그래픽은 부모 기본 기능 사용.
 *  볼륨의 사운드 믹스 반영은 UMTGameInstance::ApplyAudioSettings가 수행. */
UCLASS()
class MEOWTRACTIVE_API UMTGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "MT|Settings")
	static UMTGameUserSettings* GetMTSettings();

	UFUNCTION(BlueprintCallable, Category = "MT|Settings")
	void SetMasterVolume(float Volume) { MasterVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "MT|Settings")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintCallable, Category = "MT|Settings")
	void SetBGMVolume(float Volume) { BGMVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "MT|Settings")
	float GetBGMVolume() const { return BGMVolume; }

	UFUNCTION(BlueprintCallable, Category = "MT|Settings")
	void SetSFXVolume(float Volume) { SFXVolume = FMath::Clamp(Volume, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "MT|Settings")
	float GetSFXVolume() const { return SFXVolume; }

protected:
	// 0~1. SaveSettings() 시 GameUserSettings.ini에 저장됨
	UPROPERTY(Config)
	float MasterVolume = 1.f;

	UPROPERTY(Config)
	float BGMVolume = 1.f;

	UPROPERTY(Config)
	float SFXVolume = 1.f;
};
