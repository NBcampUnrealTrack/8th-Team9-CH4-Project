// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EMTItemType : uint8
{
	None    UMETA(DisplayName = "None"),
	Speed   UMETA(DisplayName = "Speed Boost"),
	Charm   UMETA(DisplayName = "Charm Beam"),
	Shield  UMETA(DisplayName = "Shield"),
};
