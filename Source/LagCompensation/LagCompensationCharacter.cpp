// Copyright Epic Games, Inc. All Rights Reserved.

#include "LagCompensationCharacter.h"

#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "LagCompensationGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "LagCompensationManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ALagCompensationCharacter

ALagCompensationCharacter::ALagCompensationCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(35.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);
}

void ALagCompensationCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

	// Show or hide the two versions of the gun based on whether or not we're using motion controllers.
	
	Mesh1P->SetHiddenInGame(false, true);

	if (GetLocalRole() == ROLE_Authority)
	{
		LagCompensation = Cast<ALagCompensationGameMode> (GetWorld()->GetAuthGameMode())->LagCompensation;
		LagCompensation->RegisterCharacter(this);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ALagCompensationCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ALagCompensationCharacter::OnFire);
	
	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &ALagCompensationCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ALagCompensationCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ALagCompensationCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ALagCompensationCharacter::LookUpAtRate);
}

void ALagCompensationCharacter::ServerFire_Implementation(const FShotInfo& ShotInfo)
{
	LagCompensation->SetState(ShotInfo.Timestamp);

	auto info = ShotInfo;
	info.ShotLocation = GetFirstPersonCameraComponent()->GetComponentLocation();
	
	MulticastFire(info);
}

bool ALagCompensationCharacter::ServerFire_Validate(const FShotInfo& ShotInfo)
{
	return true;
}

void ALagCompensationCharacter::FireImpl(const FShotInfo& ShotInfo)
{
	if (FireAnimation != nullptr)
	{
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}

	MakeTrace(ShotInfo);
}

void ALagCompensationCharacter::MulticastFire_Implementation(const FShotInfo& ShotInfo)
{
	if (!IsLocallyControlled())
	{
		FireImpl(ShotInfo);
	}

	if (GetLocalRole() == ROLE_Authority)
	{
		LagCompensation->ResetState();
	}
	
	::DrawDebugCamera(GetWorld(), ShotInfo.ShotLocation, ShotInfo.ShotDirection.Rotation(), GetFirstPersonCameraComponent()->FieldOfView, 1.f, FColor::White, false, 10.f);
	::DrawDebugLine(GetWorld(), ShotInfo.ShotLocation, ShotInfo.ShotLocation + ShotInfo.ShotDirection * ShootDistance, FColor::Cyan, false, 10.f, 0, 1.f);
}

void ALagCompensationCharacter::MakeTrace(const FShotInfo& ShotInfo)
{
	const auto traceStart = ShotInfo.ShotLocation;
	const auto traceEnd = traceStart + ShotInfo.ShotDirection * ShootDistance;

	FHitResult hitResult;
	UKismetSystemLibrary::LineTraceSingle(this, traceStart, traceEnd, UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_WorldDynamic), false, {this}, EDrawDebugTrace::None, hitResult, true);

	if (IsLocallyControlled())
	{
		const FColor color = FColor::Green;
		::DrawDebugLine(GetWorld(), traceStart, traceEnd, color, false, 10.f, 0, 1.f);
	}

	::DrawDebugSphere(GetWorld(), hitResult.Location, 10.f, 16, FColor::Red, false, 10.f, 0, 1.f);
}

void ALagCompensationCharacter::OnFire()
{
	if (IsLocallyControlled())
	{
		const FShotInfo info = {GetFirstPersonCameraComponent()->GetComponentRotation().Vector(), GetFirstPersonCameraComponent()->GetComponentLocation(), FDateTime::UtcNow().ToUnixTimestamp()};
		ServerFire(info);
		FireImpl(info);
	}
}

void ALagCompensationCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ALagCompensationCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ALagCompensationCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ALagCompensationCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}