// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LagCompensationManager.generated.h"

USTRUCT()
struct FCachedLagCompensationData
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FVector_NetQuantize CharacterLocation;
	UPROPERTY()
	FRotator CharacterRotation;
};

class ALagCompensationCharacter;

UCLASS()
class LAGCOMPENSATION_API ALagCompensationManager : public AInfo
{
	GENERATED_BODY()

public:
	ALagCompensationManager();
	
	virtual void Tick( float DeltaTime ) override;
	void RegisterCharacter(ALagCompensationCharacter* Character);

	virtual  void BeginPlay() override;

	void SetState(int64 ToStat);
	void ResetState();

	UFUNCTION(NetMulticast, Unreliable)
	void DrawCompensation(const TArray<FCachedLagCompensationData>& Data);

	int32 MaxStamps = 120;

	TArray<int64> TimeStamps;
	
	TMap<TWeakObjectPtr<ALagCompensationCharacter>, TArray<FCachedLagCompensationData>> CharacterData;

	TMap<TWeakObjectPtr<ALagCompensationCharacter>, FCachedLagCompensationData> CachedCharacters;

protected:
	UFUNCTION()
	void OnCharacterEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);
};
