// Sebastian Lara. All rights reserved.


#include "CombatComponent.h"


#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/Weapon/Weapon.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundCue.h"
#include "Blaster/Weapon/ProjectileGrenade.h"
#include "Components/BoxComponent.h"
#include "Blaster/Utils/DebugHelpers.h"
#include "Blaster/Weapon/HitScanWeapon.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed = 600.f;
	AimWalkSpeed = 450.f;
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate overlapping weapon.
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	// Replicate secondary weapon.
	DOREPLIFETIME(ThisClass, SecondaryWeapon);
	// Replicate aiming state.
	DOREPLIFETIME(UCombatComponent, bIsAiming);
	// Carried ammo replicated only on owner character (other objects do not need to know about the ammo we are carrying.
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly);
	// Combat state.
	DOREPLIFETIME(UCombatComponent, CombatState);
	// Grenades.
	DOREPLIFETIME_CONDITION(UCombatComponent, Grenades, COND_OwnerOnly); // Lecture replicates without condition, but I do not think it is necessary to replicate to other clients.
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Character)
	{
		// Setup character base walk speed according to this component value.
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
		// Save Default Camera FOV.
		if (Character->GetCamera())
		{
			DefaultFOV = Character->GetCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}
		if (Character->HasAuthority())
		{
			InitializeCarriedAmmo();
		}
		if (Character->IsLocallyControlled())
		{
			UpdateHUDGrenades();
		}
	}
	if (bShouldSpawnDefaultWeapon)
	{
		SpawnDefaultWeapon();
	}
}

void UCombatComponent::SetSpeeds(const float InBaseSpeed, const float InCrouchSpeed)
{
	if (!Character || !Character->GetCharacterMovement()) return;
	BaseWalkSpeed = InBaseSpeed;
	AimWalkSpeed = InBaseSpeed;

	Character->GetCharacterMovement()->MaxWalkSpeedCrouched = InCrouchSpeed;
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character && Character->IsLocallyControlled())
	{
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;
		SetHUDCrosshairs(DeltaTime);
		InterpFOV(DeltaTime);
	}
}

void UCombatComponent::SetAiming(bool NewAiming)
{
	if (!Character) return;

	bIsAiming = NewAiming;
	/*
	 * We set the value even before calling the ServerSetAiming() function for cosmetic (animation) effects.
	 * It will be quicker for the client to change the animation this way.
	 */
	ServerSetAiming(NewAiming);

	// Change walk speed locally.
	Character->GetCharacterMovement()->MaxWalkSpeed = NewAiming ? AimWalkSpeed : BaseWalkSpeed;

	// Play aiming animation only with sniper rifle equipped.
	if (Character->IsLocallyControlled() && EquippedWeapon && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle)
	{
		Controller = Controller ? Controller.Get() : Cast<ABlasterPlayerController>(Character->Controller);
		Controller->SetHUDSniperScope(NewAiming);
		if (NewAiming)
		{
			if (ZoomInSniperRifle)
				UGameplayStatics::PlaySound2D(this, ZoomInSniperRifle);
		}
		else
		{
			if (ZoomOutSniperRifle)
				UGameplayStatics::PlaySound2D(this, ZoomOutSniperRifle);
		}
	}
	if (Character->IsLocallyControlled())
	{
		bAimButtonPressed = bIsAiming;
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool NewAiming)
{
	bIsAiming = NewAiming;
	if (Character)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = NewAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::OnRep_EquippedWeapon()
{
	if (EquippedWeapon && Character)
	{
		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
		/* We are calling this so that these properties are set here first before
			   AttachActor gets called to avoid issues with attaching the weapon while it is marked to simulate physics or something like that.		
			   */
		// Attach to character's hand socket.
		AttachActorToRightHand(EquippedWeapon);
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
		// Play sound when equipped.
		PlayEquipWeaponSound(EquippedWeapon);
		EquippedWeapon->EnableCustomDepth(false);
		if (Controller)
		{
			Controller->SetHUDEquippedWeaponName(EquippedWeapon->GetWeaponType());
			Controller->SetHUDWeaponAmmo(EquippedWeapon->GetAmmo());
			UpdateCarriedAmmo();
		}
	}
}

void UCombatComponent::OnRep_SecondaryWeapon()
{
	if (SecondaryWeapon && Character)
	{
		SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
		AttachActorToBack(SecondaryWeapon);
		PlayEquipWeaponSound(EquippedWeapon);

		if (!EquippedWeapon) return;
		EquippedWeapon->SetOwner(Character);
	}
}

void UCombatComponent::ServerThrowGrenade_Implementation()
{
	if (Grenades < 1) return;
	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (Character)
	{
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		ShowAttachedGrenade(true);
	}
	Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{
	bFireButtonPressed = bPressed;

	if (bFireButtonPressed && EquippedWeapon)
	{
		Fire();
	}
}

void UCombatComponent::PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount)
{
	if (CarriedAmmoMap.Contains(WeaponType))
	{
		CarriedAmmoMap[WeaponType] = FMath::Clamp(CarriedAmmoMap[WeaponType] + AmmoAmount, 0, MaxAmmo);
		UpdateCarriedAmmo();
	}
	// Automatic reload if just picked up ammo when the weapon was empty.
	if (EquippedWeapon && EquippedWeapon->IsEmpty() && EquippedWeapon->GetWeaponType() == WeaponType)
	{
		Reload();
	}
}

void UCombatComponent::Fire()
{
	if (!CanFire()) return;

	bCanFire = false;

	if (EquippedWeapon)
	{
		CrosshairShootingFactor += CrosshairsShootingFactorIncrement;
		switch (EquippedWeapon->FireType)
		{
		case EFireType::EFT_Projectile:
		case EFireType::EFT_HitScan:
			FireWeapon();
			break;
		case EFireType::EFT_Shotgun:
			FireShotgun();
			break;
		default: break;
		}
	}
	StartFireTimer();
}

void UCombatComponent::FireWeapon()
{
	if (Character && EquippedWeapon)
	{
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		if (!Character->HasAuthority()) LocalFire(HitTarget);
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireShotgun()
{
	const AHitScanWeapon* HitScanWeapon = Cast<AHitScanWeapon>(EquippedWeapon);
	if (Character && HitScanWeapon && HitScanWeapon->FireType == EFireType::EFT_Shotgun)
	{
		TArray<FVector_NetQuantize> HitTargets;
		// TODO: could optimize a bit calling HitTargets.Reserve(NumberOfPellets).
		HitScanWeapon->ShotgunTraceEndWithScatter(HitTarget, HitTargets);
		if (!Character->HasAuthority()) ShotgunLocalFire(HitTargets);
		ServerShotgunFire(HitTargets, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::LocalFire(const FVector_NetQuantize& TraceHitTarget)
{
	if (!EquippedWeapon || !Character) return;
	// Allow characters with a shotgun to fire when they are reloading.
	if (CombatState == ECombatState::ECS_Reloading || CombatState == ECombatState::ECS_Unoccupied && EquippedWeapon->
		GetWeaponType() ==
		EWeaponType::EWT_Shotgun)
	{
		Character->PlayFireMontage(bIsAiming);
		EquippedWeapon->Fire(TraceHitTarget);
		CombatState = ECombatState::ECS_Unoccupied;
		return;
	}
	// Fire only when we are not reloading.
	if (CombatState == ECombatState::ECS_Unoccupied)
	{
		Character->PlayFireMontage(bIsAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::ShotgunLocalFire(const TArray<FVector_NetQuantize>& TraceHitTargets)
{
	AHitScanWeapon* Shotgun = Cast<AHitScanWeapon>(EquippedWeapon);
	if (Shotgun == nullptr || Character == nullptr) return;
	if (CombatState == ECombatState::ECS_Reloading || CombatState == ECombatState::ECS_Unoccupied)
	{
		bLocallyReloading = false;
		Character->PlayFireMontage(bIsAiming);
		Shotgun->FireShotgun(TraceHitTargets);
		CombatState = ECombatState::ECS_Unoccupied;
	}
}

void UCombatComponent::StartFireTimer()
{
	if (!EquippedWeapon || !Character) return;

	Character->GetWorldTimerManager().SetTimer(
		FireTimer, this, &ThisClass::FireTimerFinished, EquippedWeapon->FireDelay
	);
}

void UCombatComponent::FireTimerFinished()
{
	if (!EquippedWeapon) return;
	bCanFire = true;
	if (bFireButtonPressed && EquippedWeapon->bAutomatic)
	{
		Fire();
	}
	// Auto-reload (if specified).
	if (EquippedWeapon->bAutomaticReload && EquippedWeapon->IsEmpty())
	{
		Reload();
	}
}


void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget, const float FireDelay)
{
	MulticastFire(TraceHitTarget);
}

// Validate Fire Delay. Kick the player if it is not.
bool UCombatComponent::ServerFire_Validate(const FVector_NetQuantize& TraceHitTarget, const float FireDelay)
{
	if (!EquippedWeapon) return true;
	return FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	LocalFire(TraceHitTarget);
}

void UCombatComponent::ServerShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets,
                                                        const float FireDelay)
{
	MulticastShotgunFire(TraceHitTargets);
}

bool UCombatComponent::ServerShotgunFire_Validate(const TArray<FVector_NetQuantize>& TraceHitTargets,
                                                  const float FireDelay)
{
	if (!EquippedWeapon) return true;
	return FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
}

void UCombatComponent::MulticastShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets)
{
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	ShotgunLocalFire(TraceHitTargets);
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	FVector2D ViewportSize{};
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	const FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition, CrosshairWorldDirection;
	const bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation, CrosshairWorldPosition, CrosshairWorldDirection
	);
	if (bScreenToWorld)
	{
		FVector Start = CrosshairWorldPosition;
		// Add an offset to Start location. This is because starting the trace from the camera position can
		// lead to inaccurate shots and hand rotation.
		if (Character)
		{
			const float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			Start += CrosshairWorldDirection * (DistanceToCharacter + StartOfTraceOffset);
			DRAW_DEBUG_SPHERE(GetWorld(), Start, 12.f, 12, FColor::Blue, false, -1, 0, 0);
		}

		const FVector End = CrosshairWorldPosition + CrosshairWorldDirection * TraceLength;
		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult, Start, End, ECollisionChannel::ECC_Visibility
		);
		// Make crosshairs red if actor interacts with interface. Otherwise, white.
		if (TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>())
		{
			HUDPackage.CrosshairsColor = FLinearColor::Red;
			bAimingAtEnemy = true;
		}
		else
		{
			HUDPackage.CrosshairsColor = FLinearColor::White;
			bAimingAtEnemy = false;
		}

		if (!TraceHitResult.bBlockingHit)
		{
			TraceHitResult.ImpactPoint = End;
		}

		DRAW_DEBUG_SPHERE(GetWorld(), TraceHitResult.ImpactPoint, 12.f, 12, FColor::Red, false, -1, 0, 0);
	}
}

void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
	if (!Character || !Character->Controller) return;

	if (!Controller)
	{
		Controller = Cast<ABlasterPlayerController>(Character->GetController());
	}
	if (!Controller) return;
	// Abort if controller is null. This could still happen due to server travel. TODO: improve readability.

	if (!HUD)
	{
		HUD = Cast<ABlasterHUD>(Controller->GetHUD());
	}
	if (!HUD) return; // Abort if HUD is null. This could still happen due to server travel.  TODO: improve readability.

	if (EquippedWeapon)
	{
		HUDPackage.CrosshairCenter = EquippedWeapon->CrosshairsCenter;
		HUDPackage.CrosshairBottom = EquippedWeapon->CrosshairsBottom;
		HUDPackage.CrosshairTop = EquippedWeapon->CrosshairsTop;
		HUDPackage.CrosshairLeft = EquippedWeapon->CrosshairsLeft;
		HUDPackage.CrosshairRight = EquippedWeapon->CrosshairsRight;
	}
	// Else case should set every field to nullptr, that way HUD will not draw anything.

	// Calculate crosshair spread (including jump).
	const FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
	const FVector2D VelocityMultiplierRange(0.f, 1.f);
	FVector Velocity = Character->GetVelocity();
	Velocity.Z = 0.f;
	CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange,
	                                                            Velocity.Size());
	if (Character->GetCharacterMovement()->IsFalling())
	{
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
		// TODO: interp speed and Target to variable?
	}
	else
	{
		// Interpolate very quickly when landing.
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
	}
	// Include aiming factor.
	if (bIsAiming)
	{
		CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, CrosshairsDisplacementWhileAiming, DeltaTime,
		                                      30.f /*TODO: convert to param? */);
	}
	else
	{
		CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
	}
	// Shrink crosshairs when aiming at enemies.
	if (bAimingAtEnemy)
	{
		CrosshairEnemyFactor = FMath::FInterpTo(CrosshairEnemyFactor, AimAtEnemyShrinkFactor, DeltaTime, 30.f);
	}
	else
	{
		CrosshairEnemyFactor = FMath::FInterpTo(CrosshairEnemyFactor, 0.0f, DeltaTime, 30.f);
	}

	// Shooting factor should always interpolate back to zero.
	CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime,
	                                           CrosshairShootingFactorBackToZeroInterpSpeed);

	HUDPackage.CrosshairSpread = 0.5f
		/* Hard coded value to spread crosshairs a little by default.  TODO: convert to param.*/ +
		CrosshairVelocityFactor + CrosshairInAirFactor + CrosshairAimFactor +
		CrosshairShootingFactor - CrosshairEnemyFactor;


	HUD->SetHudPackage(HUDPackage);
}

void UCombatComponent::InterpFOV(const float DeltaTime)
{
	/*
	 * NOTE: to avoid the blur effect when aiming at large or very short distances
	 * you must set the Camera's Depth of Field: Focal Distance and the Aperture (F-stop) 
	 * attributes to large values.
	 */

	/*
		 * 32 and 10000 are the limit values the inspector allows for these settings.
		 * Hardcoded. Change as you think it is more convenient.
		 These settings patch are due to a multiplayer (only build) issue where the screen turned all blurry
		 when zooming in.
	*/
	if (!EquippedWeapon) return;
	if (!Character || !Character->GetCamera()) return;

	// Zoom in.
	if (bIsAiming)
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->ZoomedFOV, DeltaTime,
		                              EquippedWeapon->ZoomInterpSpeed);


		Character->GetCamera()->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
		Character->GetCamera()->PostProcessSettings.DepthOfFieldFstop = 32.f;
		Character->GetCamera()->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
		Character->GetCamera()->PostProcessSettings.DepthOfFieldFocalDistance = 10000.f;
	}
	// Stopped aiming. Go back to default zoom.
	else
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);

		Character->GetCamera()->PostProcessSettings.bOverride_DepthOfFieldFstop = false;
		// Character->GetCamera()->PostProcessSettings.DepthOfFieldFstop = 4.f; // Reset to default values
		Character->GetCamera()->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = false;
		// Character->GetCamera()->PostProcessSettings.DepthOfFieldFocalDistance = 0.f; // Reset to default values
	}

	Character->GetCamera()->SetFieldOfView(CurrentFOV);
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (!Character || !WeaponToEquip) return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (EquippedWeapon && !SecondaryWeapon)
	{
		EquipSecondaryWeapon(WeaponToEquip);
	}
	else
	{
		EquipPrimaryWeapon(WeaponToEquip);
	}

	// Stop orienting rotation to movement. This is to allow leaning animations in animation blueprint.
	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
}

void UCombatComponent::SwapWeapons()
{
	if (!CanSwapWeapons() || !Character) return;

	Character->PlaySwapMontage();
	CombatState = ECombatState::ECS_SwappingWeapons;
}

void UCombatComponent::EquipPrimaryWeapon(AWeapon* WeaponToEquip)
{
	if (!WeaponToEquip) return;

	DropEquippedWeapon();
	// In this game picking up a weapon drops the previous primary weapon and replaces it with the new one.
	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

	AttachActorToRightHand(EquippedWeapon);
	EquippedWeapon->SetOwner(Character);
	EquippedWeapon->SetHUDAmmo();

	if (Controller)
		Controller->SetHUDWeaponAmmo(EquippedWeapon->GetAmmo());

	UpdateCarriedAmmo();
	PlayEquipWeaponSound(EquippedWeapon);

	ReloadEmptyWeapon();
}

void UCombatComponent::EquipSecondaryWeapon(AWeapon* WeaponToEquip)
{
	if (!WeaponToEquip) return;

	SecondaryWeapon = WeaponToEquip;
	SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
	SecondaryWeapon->SetOwner(Character);
	AttachActorToBack(WeaponToEquip);
	PlayEquipWeaponSound(WeaponToEquip);
}

void UCombatComponent::DropEquippedWeapon()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->Drop();
	}
}

void UCombatComponent::AttachActorToRightHand(const AActor* ActorToAttach) const
{
	if (!Character || !Character->GetMesh() || !ActorToAttach) return;

	if (const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(HandSocketName))
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
}

// Hardcoded "PistolLeftHandSocket" and "LeftHandSocket"
void UCombatComponent::AttachActorToLeftHand(const AActor* ActorToAttach) const
{
	if (!Character || !Character->GetMesh() || !ActorToAttach || !EquippedWeapon) return;
	const bool bUsePistolSocket =
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol || EquippedWeapon->GetWeaponType() ==
		EWeaponType::EWT_SubmachineGun;

	const FName SocketName = bUsePistolSocket ? FName("PistolLeftHandSocket") : FName("LeftHandSocket");

	if (const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(SocketName))
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
}

// If this causes a crash, make sure the skeletal mesh has the "BackSocket".
void UCombatComponent::AttachActorToBack(AActor* ActorToAttach) const
{
	if (!Character || !Character->GetMesh() || !ActorToAttach) return;
	if (const USkeletalMeshSocket* BackSocket = Character->GetMesh()->GetSocketByName(FName("BackSocket")))
	{
		BackSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
	else
	{
		UE_LOG(LogSkeletalMesh, Warning,
		       TEXT("Combat component tried to attach actor %s to back of %s but BackSocket does not exist."),
		       *ActorToAttach->GetName(), *Character->GetName());
	}
}

void UCombatComponent::UpdateCarriedAmmo()
{
	if (!EquippedWeapon) return;
	// Check if we have ammo of the current weapon type and display that amount.
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	if (Controller = Controller ? Controller.Get() : Cast<ABlasterPlayerController>(Character->Controller); Controller)
	{
		Controller->SetHUDWeaponCarriedAmmo(CarriedAmmo);
		Controller->SetHUDEquippedWeaponName(EquippedWeapon->GetWeaponType());
	}
}

void UCombatComponent::PlayEquipWeaponSound(AWeapon* WeaponToEquip) const
{
	if (Character && WeaponToEquip && WeaponToEquip->EquipSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, WeaponToEquip->EquipSound, Character->GetActorLocation());
	}
}

void UCombatComponent::ReloadEmptyWeapon()
{
	// Auto-reload (if specified).
	if (EquippedWeapon->bAutomaticReload && EquippedWeapon->IsEmpty())
	{
		Reload();
	}
}

void UCombatComponent::ShowAttachedGrenade(const bool bShowGrenade) const
{
	if (Character && Character->GetGrenadeMesh())
	{
		Character->GetGrenadeMesh()->SetVisibility(bShowGrenade);
	}
}

void UCombatComponent::OnRep_Aiming()
{
	if (Character && Character->IsLocallyControlled())
	{
		bIsAiming = bAimButtonPressed;
	}
}

bool UCombatComponent::CanFire() const
{
	if (!EquippedWeapon) return false;
	if (bLocallyReloading)
	{
		// Allow shotgun shot while reloading.

		/*
	* Weapon is not empty, can fire, is reloading, and it is a shotgun.
	*/
		if (!EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Reloading &&
			EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun)
		{
			bLocallyReloading = false;
			return true;
		}
		return false;
	}
	return !EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Unoccupied;
}

void UCombatComponent::Reload()
{
	// We can reload only if we have enough ammo, we have fired at least one projectile of the current magazine, and we are unoccupied.
	if (CarriedAmmo > 0 && EquippedWeapon && !EquippedWeapon->IsFull() && CombatState == ECombatState::ECS_Unoccupied
		&& !bLocallyReloading)
	{
		ServerReload();
		HandleReload();
		bLocallyReloading = true;
	}
}

void UCombatComponent::ServerReload_Implementation()
{
	if (!Character) return;
	CombatState = ECombatState::ECS_Reloading;
	if (!Character->IsLocallyControlled())
	{
		HandleReload(); // Handle Reload for server.
	}
}

void UCombatComponent::FinishReloading()
{
	bLocallyReloading = false; // No need to check if character is locally controlled to set this, we'll always make sure this is set to false here.
	if (Character && Character->HasAuthority())
	{
		CombatState = ECombatState::ECS_Unoccupied;
		UpdateAmmoValues();
	}
	// Allow to fire immediately when reload finishes and we are pressing the fire button.
	if (bFireButtonPressed)
	{
		Fire();
	}
}

void UCombatComponent::FinishSwap()
{
	if (Character)
	{
		Character->bFinishedSwapping = true;
		if (Character->HasAuthority())
		{
			CombatState = ECombatState::ECS_Unoccupied;
		}
	}
}

void UCombatComponent::FinishSwapAttachWeapons()
{
	if(!Character || !Character->HasAuthority()) return;
	
	AWeapon* const TempWeapon = EquippedWeapon;	// This swapping section used to be performed at the beginning of SwapWeapons, but since EquippedWeapon and SecondaryWeapon are replicated
	EquippedWeapon = SecondaryWeapon;				// clients saw the weapon swap instantly instead of seeing it when this function (in an anim notify) is called.
	SecondaryWeapon = TempWeapon;
	
	
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	AttachActorToRightHand(EquippedWeapon);

	EquippedWeapon->SetHUDAmmo();
	UpdateCarriedAmmo();
	PlayEquipWeaponSound(EquippedWeapon);

	SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
	AttachActorToBack(SecondaryWeapon);
}

void UCombatComponent::OnRep_CombatState()
{
	switch (CombatState)
	{
	case ECombatState::ECS_Reloading:
		if (Character && !Character->IsLocallyControlled())
		{
			HandleReload();
		}
		break;

	// Allow to fire immediately when not reloading (or in any busy state) and we are pressing the fire button.
	case ECombatState::ECS_Unoccupied:
		if (bFireButtonPressed)
		{
			Fire();
		}
		break;
	case ECombatState::ECS_ThrowingGrenade:
		if (Character && !Character->IsLocallyControlled())
		{
			Character->PlayThrowGrenadeMontage();
			AttachActorToLeftHand(EquippedWeapon);
			ShowAttachedGrenade(true); // During Throw grenade animation, mesh should be visible.
		}
		break;
	case ECombatState::ECS_SwappingWeapons:
		if (Character && !Character->IsLocallyControlled())
		{
			Character->PlaySwapMontage();
		}
		break;
	}
}

void UCombatComponent::HandleReload() const
{
	if (Character)
		Character->PlayReloadMontage();
}

void UCombatComponent::UpdateAmmoValues()
{
	if (!Character || !EquippedWeapon) return;
	const int32 ReloadAmount = AmountToReload();
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		// Subtract reloaded ammo amount.
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	if (Controller = Controller ? Controller.Get() : Cast<ABlasterPlayerController>(Character->Controller); Controller)
	{
		Controller->SetHUDWeaponCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(ReloadAmount);
}

void UCombatComponent::UpdateShotgunAmmoValues()
{
	if (!Character || !EquippedWeapon) return;

	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= 1;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller ? Controller.Get() : Cast<ABlasterPlayerController>(Character->Controller);
	if (Controller)
	{
		Controller->SetHUDWeaponCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(1);
	bCanFire = true;
	// In case the shotgun is fully reloaded or there is no more ammo left to reload, end reload anim.
	if (EquippedWeapon->IsFull() || CarriedAmmo == 0)
	{
		JumpToShotgunEnd();
	}
}

void UCombatComponent::OnRep_Grenades()
{
	if (Character && Character->IsLocallyControlled())
	{
		UpdateHUDGrenades();
	}
}

void UCombatComponent::UpdateHUDGrenades()
{
	if (Controller = Controller ? Controller.Get() : Cast<ABlasterPlayerController>(Character->Controller); Controller)
	{
		Controller->SetHUDGrenades(Grenades);
	}
}

void UCombatComponent::SpawnDefaultWeapon()
{
	if (!GetOwner()->HasAuthority()) return;

	const auto World = GetWorld();
	if (!World) return;

	if (DefaultWeaponClass)
	{
		const auto StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
		StartingWeapon->bDestroyWeapon = true;
		EquipWeapon(StartingWeapon);
	}
}

void UCombatComponent::JumpToShotgunEnd()
{
	if (!Character || !Character->GetMesh()) return;
	auto AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (AnimInstance && Character->GetReloadMontage())
	{
		AnimInstance->Montage_JumpToSection(FName("ShotgunEnd"));
	}
}

// CombatState = ECombatState::ECS_Unoccupied;
void UCombatComponent::ThrowGrenadeFinished()
{
	CombatState = ECombatState::ECS_Unoccupied;
	AttachActorToRightHand(EquippedWeapon);
}

void UCombatComponent::LaunchGrenade()
{
	// Hide mesh.
	ShowAttachedGrenade(false);
	if (Character && Character->IsLocallyControlled())
	{
		ServerLaunchGrenade(HitTarget);

		UpdateHUDGrenades();
	}
}

void UCombatComponent::ShotgunShellReload()
{
	if (Character && Character->HasAuthority())
	{
		UpdateShotgunAmmoValues();
	}
}

int32 UCombatComponent::AmountToReload() const
{
	if (!EquippedWeapon) return 0;

	const int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();

	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		// Ammo amount carried of this weapon type. 
		const int32 AmountCarried = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
		const int32 Least = FMath::Min(RoomInMag, AmountCarried);
		return FMath::Clamp(RoomInMag, 0, Least);
	}
	return 0;
}

void UCombatComponent::OnRep_CarriedAmmo()
{
	Controller = Controller ? Controller.Get() : Cast<ABlasterPlayerController>(Character->Controller);
	if (Controller)
	{
		Controller->SetHUDWeaponCarriedAmmo(CarriedAmmo);
	}
	const bool bJumpToShotgunEnd = CombatState == ECombatState::ECS_Reloading && EquippedWeapon != nullptr &&
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun && CarriedAmmo == 0;
	if (bJumpToShotgunEnd)
	{
		JumpToShotgunEnd();
	}
}

void UCombatComponent::InitializeCarriedAmmo()
{
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingSniperAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
}

void UCombatComponent::ThrowGrenade()
{
	if (Grenades == 0) return;
	if (CombatState != ECombatState::ECS_Unoccupied || !EquippedWeapon) return;

	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (Character)
	{
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		if (!Character->HasAuthority())
		{
			ServerThrowGrenade();
		}
		else
		{
			Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
		}
	}
}

void UCombatComponent::ServerLaunchGrenade_Implementation(const FVector_NetQuantize& Target)
{
	// Spawn actual grenade.
	if (Character && Character->GetGrenadeMesh() && GrenadeClass)
	{
		const FVector StartingLocation = Character->GetGrenadeMesh()->GetComponentLocation();
		const FVector ToTarget = Target - StartingLocation;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Instigator = Character;
		SpawnParams.Owner = Character;
		if (const auto World = GetWorld())
		{
			const auto Grenade = World->SpawnActor<AProjectile>(GrenadeClass, StartingLocation, ToTarget.Rotation(),
			                                                    SpawnParams);
			if (const auto GrenadeCollider = Grenade->GetComponentByClass<UBoxComponent>())
			{
				GrenadeCollider->IgnoreActorWhenMoving(Character, true);
				// This is already set in AProjectile::BeginPlay but for some reason it was not working for BP_ThrowGrenade.
			}
		}
	}
}
