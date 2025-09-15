// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WallRunningCMC.generated.h"

class AWallRunningCharacter;

UENUM(BlueprintType)
enum ECustomMovementMode : int
{
	MOVE_CustomNone UMETA(Hidden),
	MOVE_WallRunning UMETA(DisplayName = "Wall Running")
};

/**
 * 
 */
UCLASS()
class WALLRUNNINGDEMO_API UWallRunningCMC : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	virtual float GetMaxSpeed() const override;

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

	bool IsCustomMovementMode(uint8 TestCustomMovementMode) const;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

protected:
	virtual void PhysWallRun(float deltaTime, int32 Iterations);

	virtual bool TryWallRun();

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

	UFUNCTION(BlueprintNativeEvent, Category = "Wall Running")
	void EnterWallRun();
	virtual void EnterWallRun_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Wall Running")
	void ExitWallRun();
	virtual void ExitWallRun_Implementation();

private:
	float OwnerCapsuleRadius() const;
	float OwnerCapsuleHalfHeight() const;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall Running")
	bool bWallRunIsRight;

	UPROPERTY(EditDefaultsOnly)
	float MinWallRunSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly)
	float MaxWallRunSpeed = 800.f;

	UPROPERTY(EditDefaultsOnly)
	float MaxVerticalWallRunSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly)
	float WallRunPullAwayAngle = 75;

	UPROPERTY(EditDefaultsOnly)
	float WallAttractionForce = 200.f;

	UPROPERTY(EditDefaultsOnly)
	float MinWallRunHeight = 200.f;

	UPROPERTY(EditDefaultsOnly)
	UCurveFloat* WallRunGravityScaleCurve;

	UPROPERTY(EditDefaultsOnly)
	float WallJumpForce = 300.f;

	UFUNCTION(BlueprintPure)
	bool IsWallRunning() const { return IsCustomMovementMode(MOVE_WallRunning); }

protected:
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<AWallRunningCharacter> WallRunningCharacter;
};
