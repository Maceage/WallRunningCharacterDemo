// Fill out your copyright notice in the Description page of Project Settings.


#include "WallRunningCMC.h"

#include "WallRunningCharacter.h"
#include "Components/CapsuleComponent.h"

void UWallRunningCMC::BeginPlay()
{
	Super::BeginPlay();

	WallRunningCharacter = Cast<AWallRunningCharacter>(PawnOwner);
}

float UWallRunningCMC::GetMaxSpeed() const
{
	if (MovementMode == EMovementMode::MOVE_Custom)
	{
		if (CustomMovementMode == ECustomMovementMode::MOVE_WallRunning)
		{
			return MaxWallRunSpeed;
		}
	}

	return Super::GetMaxSpeed();
}

void UWallRunningCMC::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	if (IsFalling())
	{
		TryWallRun();
	}
}

void UWallRunningCMC::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
}

void UWallRunningCMC::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

bool UWallRunningCMC::CanAttemptJump() const
{
	return Super::CanAttemptJump() || IsWallRunning();
}

bool UWallRunningCMC::DoJump(bool bReplayingMoves, float DeltaTime)
{
	bool bWasWallRunning = IsWallRunning();

	if (Super::DoJump(bReplayingMoves, DeltaTime))
	{
		if (bWasWallRunning && WallRunningCharacter)
		{
			FVector Start = UpdatedComponent->GetComponentLocation();
			FVector CastDelta = UpdatedComponent->GetRightVector() * OwnerCapsuleRadius() * 2;
			FVector End = bWallRunIsRight ? Start + CastDelta : Start - CastDelta;

			auto Params = WallRunningCharacter->GetIgnoredCharacterParams();

			FHitResult WallHit;

			GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params);

			Velocity += WallHit.Normal * WallJumpForce;
		}

		return true;
	}

	return false;
}

void UWallRunningCMC::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	Super::ProcessLanded(Hit, remainingTime, Iterations);
}

bool UWallRunningCMC::IsCustomMovementMode(uint8 TestCustomMovementMode) const
{
	return MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == TestCustomMovementMode;
}

void UWallRunningCMC::PhysCustom(float deltaTime, int32 Iterations)
{
	switch (CustomMovementMode)
	{
	case MOVE_WallRunning:
		PhysWallRun(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Unknown Custom Movement Mode %d"), CustomMovementMode)
		break;
	}

	Super::PhysCustom(deltaTime, Iterations);
}

void UWallRunningCMC::PhysWallRun(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!WallRunningCharacter || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity()))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	bJustTeleported = false;
	float remainingTime = deltaTime;

	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations)
		&& CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController))
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();

		FVector Start = UpdatedComponent->GetComponentLocation();
		FVector CastDelta = UpdatedComponent->GetRightVector() * OwnerCapsuleRadius() * 2;
		FVector End = bWallRunIsRight ? Start + CastDelta : Start - CastDelta;
		auto Params = WallRunningCharacter->GetIgnoredCharacterParams();
		float SinPullAwayAngle = FMath::Sin(FMath::DegreesToRadians(WallRunPullAwayAngle));
		FHitResult WallHit;
		GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params);

		bool bWantsToPullAway = WallHit.IsValidBlockingHit() && !Acceleration.IsNearlyZero() && (Acceleration.GetSafeNormal() | WallHit.Normal) > SinPullAwayAngle;

		if (!WallHit.IsValidBlockingHit() || bWantsToPullAway)
		{
			SetMovementMode(EMovementMode::MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}

		Acceleration = FVector::VectorPlaneProject(Acceleration, WallHit.Normal);
		Acceleration.Z = 0.f;

		CalcVelocity(timeTick, 0.f, false, GetMaxBrakingDeceleration());
		Velocity = FVector::VectorPlaneProject(Velocity, WallHit.Normal);

		float TangentAccel = Acceleration.GetSafeNormal() | Velocity.GetSafeNormal2D();
		bool bVelUp = Velocity.Z > 0.f;
		Velocity.Z += GetGravityZ() * (WallRunGravityScaleCurve ? WallRunGravityScaleCurve->GetFloatValue(bVelUp ? 0.f : TangentAccel) * timeTick : 0.f);

		if (Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2) || Velocity.Z < -MaxVerticalWallRunSpeed)
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}

		const FVector Delta = timeTick * Velocity;

		if (Delta.IsNearlyZero())
		{
			remainingTime = 0.f;
		}
		else
		{
			FHitResult Hit;
			SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

			FVector WallAttractionDelta = -WallHit.Normal * WallAttractionForce * timeTick;
			SafeMoveUpdatedComponent(WallAttractionDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
		}

		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}

		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
	}

	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector CastDelta = UpdatedComponent->GetRightVector() * OwnerCapsuleRadius() * 2;
	FVector End = bWallRunIsRight ? Start + CastDelta : Start - CastDelta;
	auto Params = WallRunningCharacter->GetIgnoredCharacterParams();

	FHitResult FloorHit;
	FHitResult WallHit;
	
	GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params);
	GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (OwnerCapsuleHalfHeight() + MinWallRunHeight * 5.f), "BlockAll", Params);
	
	if (FloorHit.IsValidBlockingHit() || !WallHit.IsValidBlockingHit() || Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2))
	{
		SetMovementMode(MOVE_Falling);
	}
}

bool UWallRunningCMC::TryWallRun()
{
	if (!IsFalling())
	{
		return false;
	}

	if (Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2))
	{
		return false;
	}

	if (Velocity.Z < -MaxVerticalWallRunSpeed)
	{
		return false;
	}

	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector LeftEnd = Start - UpdatedComponent->GetRightVector() * OwnerCapsuleRadius() * 2;
	FVector RightEnd = Start + UpdatedComponent->GetRightVector() * OwnerCapsuleRadius() * 2;
	auto Params = WallRunningCharacter->GetIgnoredCharacterParams();
	FHitResult FloorHit;
	FHitResult WallHit;

	if (GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (OwnerCapsuleHalfHeight() + MinWallRunHeight * 5.f), "BlockAll", Params))
	{
		return false;
	}

	GetWorld()->LineTraceSingleByProfile(WallHit, Start, LeftEnd, "BlockAll", Params);

	if (WallHit.IsValidBlockingHit() && (Velocity | WallHit.Normal) < 0.f)
	{
		bWallRunIsRight = false;
	}
	else
	{
		GetWorld()->LineTraceSingleByProfile(WallHit, Start, RightEnd, "BlockAll", Params);

		if (WallHit.IsValidBlockingHit() && (Velocity | WallHit.Normal) > 0.f)
		{
			bWallRunIsRight = true;
		}
		else
		{
			return false;
		}
	}

	FVector ProjectedVelocity = FVector::VectorPlaneProject(Velocity, WallHit.Normal);

	if (ProjectedVelocity.SizeSquared2D() < pow(MinWallRunSpeed, 2)) return false;

	Velocity = ProjectedVelocity;
	Velocity.Z = FMath::Clamp(Velocity.Z, 0.f, MaxVerticalWallRunSpeed);

	SetMovementMode(MOVE_Custom, MOVE_WallRunning);
	
	return true;
}

void UWallRunningCMC::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	if (!HasValidData())
	{
		return;
	}

	if (PreviousCustomMode == MOVE_Custom)
	{
		if (PreviousCustomMode == MOVE_WallRunning)
		{
			ExitWallRun();
		}
	}

	if (MovementMode == MOVE_Custom)
	{
		if (CustomMovementMode == MOVE_WallRunning)
		{
			EnterWallRun();
		}
	}
	
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
}

void UWallRunningCMC::EnterWallRun_Implementation()
{
	// Own logic here
}

void UWallRunningCMC::ExitWallRun_Implementation()
{
	// Own logic here
}

float UWallRunningCMC::OwnerCapsuleRadius() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UWallRunningCMC::OwnerCapsuleHalfHeight() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}
