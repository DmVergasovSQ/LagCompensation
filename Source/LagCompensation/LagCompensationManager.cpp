// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationManager.h"

#include "DrawDebugHelpers.h"
#include "LagCompensationCharacter.h"

#include "Components/CapsuleComponent.h"

ALagCompensationManager::ALagCompensationManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
}

void ALagCompensationManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GetWorld()->IsServer())
	{
		return;
	}
	
	if (TimeStamps.Num() > MaxStamps)
	{
		TimeStamps.RemoveAt(0);
	}

	TimeStamps.Add( FDateTime::UtcNow().ToUnixTimestamp());
	
	for (auto& character : CharacterData)
	{
		if (character.Value.Num() > MaxStamps)
		{
			character.Value.RemoveAt(0);
		}

		character.Value.Add({character.Key->GetActorLocation(), character.Key->GetActorRotation()});
	}
}

void ALagCompensationManager::RegisterCharacter(ALagCompensationCharacter* Character)
{
	auto& val = CharacterData.Add(Character);
	val.Init({Character->GetActorLocation(), Character->GetActorRotation()}, TimeStamps.Num());
	Character->OnEndPlay.AddUniqueDynamic(this, &ALagCompensationManager::OnCharacterEndPlay);
}

void ALagCompensationManager::BeginPlay()
{
	Super::BeginPlay();
	
	TimeStamps.Reserve(MaxStamps);
}

void ALagCompensationManager::SetState(int64 ToState)
{
	auto bestValue = TNumericLimits<int64>::Max();
	auto bestIndex = 0;
	
	for (int32 i = TimeStamps.Num() - 1; i > 0; --i)
	{
		const auto delta =  FMath::Abs(TimeStamps[i]- ToState);

		if (delta < bestValue)
		{
			bestValue = delta;
			bestIndex = i;
		}
		else
		{
			break;
		}
	}
	
	TArray<FCachedLagCompensationData> debugData;
	
	for (const auto& character : CharacterData)
	{
		check(character.Value.IsValidIndex(bestIndex));
		
		const auto& stampData = character.Value[bestIndex];
		
		CachedCharacters.Add(character.Key, {character.Key->GetActorLocation(), character.Key->GetActorRotation()});
		
		character.Key->SetActorLocation(stampData.CharacterLocation);
		character.Key->SetActorRotation(stampData.CharacterRotation);
		
		debugData.Add({stampData.CharacterLocation,
			stampData.CharacterRotation});
	}

	DrawCompensation(debugData);
}

void ALagCompensationManager::ResetState()
{
	for (const auto& character : CachedCharacters)
	{
		character.Key->SetActorLocation(character.Value.CharacterLocation);
		character.Key->SetActorRotation(character.Value.CharacterRotation);
	}

	CachedCharacters.Empty();
}

void ALagCompensationManager::OnCharacterEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	CharacterData.Remove(Cast<ALagCompensationCharacter>(Actor));
}

void ALagCompensationManager::DrawCompensation_Implementation(const TArray<FCachedLagCompensationData>& Data)
{
	const auto charCDO = ALagCompensationCharacter::StaticClass()->GetDefaultObject<ALagCompensationCharacter>();
	
	const auto height = charCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const auto radius = charCDO->GetCapsuleComponent()->GetScaledCapsuleRadius();
	
	for (const auto& data : Data)
	{
		::DrawDebugCapsule(GetWorld(), data.CharacterLocation, height, radius, 	data.CharacterRotation.Quaternion(), FColor::Cyan, false, 10.f, 0, 1.f);
	}
}
