// Sebastian Lara. All rights reserved.


#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "Blaster/BlasterComponents/HealthComponent.h"
#include "Blaster/Weapon/Weapon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blaster/BlasterTypes/CombatState.h"

void UBlasterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
}

void UBlasterAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (BlasterCharacter == nullptr)
	{
		BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
	}
	if (BlasterCharacter == nullptr) return;

	FVector Velocity = BlasterCharacter->GetVelocity();
	Velocity.Z = 0.0f;
	Speed = Velocity.Size();

	// In air or grounded.
	bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling();
	// 
	bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false;
	// Weapon equipped.
	bWeaponEquipped = BlasterCharacter->IsWeaponEquipped();
	EquippedWeapon = BlasterCharacter->GetEquippedWeapon();
	// Crouched.
	bIsCrouched = BlasterCharacter->IsCrouched();
	// Is aiming weapon.
	bIsAiming = BlasterCharacter->IsAiming();
	// Turning in place.
	TurningInPlace = BlasterCharacter->GetTurningInPlace();
	// Rotate root bone.
	bRotateRootBone = BlasterCharacter->ShouldRotateRootBone();

	// Offset Yaw for strafing.
	FRotator AimRotation = BlasterCharacter->GetBaseAimRotation();
	FRotator YawRotation = UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity());
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(YawRotation, AimRotation);
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, StrafingInterpSpeed);
	YawOffset = DeltaRotation.Yaw;

	// Lean.
	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = BlasterCharacter->GetActorRotation();
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
	const float Target = Delta.Yaw / DeltaSeconds;
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaSeconds, LeanInterpSpeed);
	Lean = FMath::Clamp(Interp, -90.f, 90.f);

	AO_Yaw = BlasterCharacter->GetAO_Yaw();
	AO_Pitch = BlasterCharacter->GetAO_Pitch();

	if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetMesh() && BlasterCharacter->GetMesh())
	{
		LeftHandTransform = EquippedWeapon->GetMesh()->GetSocketTransform(
			FName("LeftHandSocket"), RTS_World // TODO: Make left hand socket a uproperty?
		);

		FVector OutPosition;
		FRotator OutRotation;

		BlasterCharacter->GetMesh()->TransformToBoneSpace(RightHandBoneName, LeftHandTransform.GetLocation(),
		                                                  FRotator::ZeroRotator, OutPosition, OutRotation);
		LeftHandTransform.SetLocation(OutPosition);
		LeftHandTransform.SetRotation(FQuat(OutRotation));

		// Calculate a rotation to align hand and weapon more precisely to
		// aiming point (only locally to avoid consuming bandwidth unnecessarily).
		if (BlasterCharacter->IsLocallyControlled())
		{
			bLocallyControlled = true;
			const FTransform RightHandTransform = BlasterCharacter->GetMesh()->GetSocketTransform(
				FName("RightHand"), RTS_World);
			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(RightHandTransform.GetLocation(), BlasterCharacter->GetHitTarget());
			

 // Hand bone rotation to match weapon muzzle flash with hit target. DISCARDED.
 // 2 Solutions were found: 
 // First one actually rotated the weapon to look at the hit target but right hand was wrongly rotated so it looked broken.
 // The second (this was the chosen one) was that the weapon will not match the hit target but the hand will look normal.
 // 
			/*
			RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaSeconds, 30.f);
			
			const FTransform MuzzleTipTransform = EquippedWeapon->GetMesh()->GetSocketTransform(FName("MuzzleFlash"), RTS_World);
			const FVector MuzzleX(FRotationMatrix(MuzzleTipTransform.GetRotation().Rotator()).GetUnitAxis(EAxis::X));			
			
			// The direction muzzle flash is facing.
			DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(),
			              MuzzleTipTransform.GetLocation() + MuzzleX * 1000000.f, FColor::Red);
			// The trace impact location (center of the viewport).
			DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), BlasterCharacter->GetHitTarget(),
			              FColor::Purple);
			   /*           
 * In case you want to use this, remember you still need to use the transform node in the anim blueprint and set the
 * rotation param with in World Space and Replace Existing options for the right hand bone (or the one you want to modify).
*/
		}
	}

	bIsDead = BlasterCharacter ? BlasterCharacter->GetHealthComponent()->IsDead() : false;
	// Only use FABRIK (adjusting left hand algorithm) when character is not reloading.
	bUseFabrik = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied;
	const bool bFABRIKOverride = BlasterCharacter->IsLocallyControlled() &&
		BlasterCharacter->GetCombatState() != ECombatState::ECS_ThrowingGrenade &&
			BlasterCharacter->bFinishedSwapping;
			
	if (bFABRIKOverride)
	{
		bUseFabrik = !BlasterCharacter->IsLocallyReloading() && BlasterCharacter->bFinishedSwapping;
	}
	bUseAimOffsets = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied && !BlasterCharacter->GetDisableGameplay(); // NOT reloading and NOT disabled gameplay.
	bTransformRightHand = BlasterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied && !BlasterCharacter->GetDisableGameplay(); // NOT reloading and NOT disabled gameplay.
}
