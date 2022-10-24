// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LagCompensationManager.h"
#include "GameFramework/GameModeBase.h"
#include "LagCompensationGameMode.generated.h"

UCLASS(minimalapi)
class ALagCompensationGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALagCompensationGameMode();
 
	virtual  void PreInitializeComponents() override;
	UPROPERTY()
	ALagCompensationManager* LagCompensation;
};



