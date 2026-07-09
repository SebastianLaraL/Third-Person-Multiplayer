// Sebastian Lara. All rights reserved.


#include "HitScanWeapon.h"

#include "NiagaraFunctionLibrary.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Blaster/BlasterComponents/LagCompensationComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/Utils/DebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

AHitScanWeapon::AHitScanWeapon()
{
	FireType = EFireType::EFT_HitScan;
}

void AHitScanWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	const auto World = GetWorld();
	if (!World) return;

	const auto OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	const auto InstigatorController = OwnerPawn->GetController();

	const USkeletalMeshSocket* MuzzleFlashSocket = GetMesh()->GetSocketByName(MuzzleSocketName);
	if (!MuzzleFlashSocket) return;

	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetMesh());
	const FVector Start = SocketTransform.GetLocation();
	
	TMap<ABlasterCharacter*, uint32> HitMap; // Pair of values for a character and the number of hits they receive.
	for (uint32 Index = 0; Index < NumberOfPellets; Index++)
	{
		// Trace will ignore owner.
		FHitResult Hit;
		WeaponTraceHit(Start, HitTarget, Hit);
		
		ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(Hit.GetActor());

		if (HitCharacter && InstigatorController) // Trace hit something.
		{
			if (HitMap.Contains(HitCharacter))
			{
				HitMap[HitCharacter]++; // Increase the number of hits.
			}
			else
			{
				HitMap.Emplace(HitCharacter,1);
			}
		}
		/* Impact VFX/SFX. */
		if (ImpactEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				ImpactEffect,
				Hit.ImpactPoint,
				Hit.ImpactNormal.Rotation()
				);
		}
		// Hit sound.
	}
	// Apply damage to all actors according to the number of hits received.
	for (const auto& HitPair : HitMap)
	{
		ABlasterCharacter* HitBlasterCharacter = HitPair.Key;
		if (!HitBlasterCharacter || !InstigatorController) continue;
		const bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
		if (HasAuthority() && bCauseAuthDamage)
		{
			UGameplayStatics::ApplyDamage(HitPair.Key,
				Damage * HitPair.Value, // Number of pellets that hit multiplied by damage.
				InstigatorController,
				this,
				UDamageType::StaticClass());
		}
		
		if (!HasAuthority() && bUseServerSideRewind)
		{
			BlasterOwnerCharacter = BlasterOwnerCharacter ? BlasterOwnerCharacter.Get() : Cast<ABlasterCharacter>(OwnerPawn);
			BlasterOwnerController = BlasterOwnerController ? BlasterOwnerController.Get() : Cast<ABlasterPlayerController>(InstigatorController);

			if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensationComponent() && BlasterOwnerCharacter->IsLocallyControlled())
			{
				BlasterOwnerCharacter->GetLagCompensationComponent()->ServerScoreRequest(
					HitBlasterCharacter,
					Start,
					HitTarget,
					BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime,
					this
				);
			}
		}
	}
}

void AHitScanWeapon::FireShotgun(const TArray<FVector_NetQuantize>& HitTargets)
{
	AWeapon::Fire(FVector());
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;
	AController* InstigatorController = OwnerPawn->GetController();

	const USkeletalMeshSocket* MuzzleFlashSocket = GetMesh()->GetSocketByName("MuzzleFlash");
	if (!MuzzleFlashSocket) return;

	const auto World = GetWorld();
	if (!World) return;

	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetMesh());
	const FVector Start = SocketTransform.GetLocation();

	// Maps hit character to number of times hit
	TMap<AActor*, uint32> HitMap;
	for (FVector_NetQuantize HitTarget : HitTargets)
	{
		FHitResult FireHit;
		WeaponTraceHit(Start, HitTarget, FireHit);

		if (AActor* HitActor = FireHit.GetActor())
		{
			if (HitMap.Contains(HitActor))
			{
				HitMap[HitActor]++;
			}
			else
			{
				HitMap.Emplace(HitActor, 1);
			}
			/* Impact VFX/SFX. */
			if (ImpactEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					World,
					ImpactEffect,
					FireHit.ImpactPoint,
					FireHit.ImpactNormal.Rotation()
				);
			}
		}
	}
	
	TArray<ABlasterCharacter*> HitCharacters;
	
	for (const auto& HitPair : HitMap)
	{
		if (HitPair.Key && InstigatorController)
		{
			const bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
			if (HasAuthority() && bCauseAuthDamage)
			{
				UGameplayStatics::ApplyDamage(
					HitPair.Key, // Character that was hit
					Damage * HitPair.Value, // Multiply Damage by number of times hit
					InstigatorController,
					this,
					UDamageType::StaticClass()
				);
			}
			if (ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(HitPair.Key)) // QUICKFIX. I do not like this cast here but I cannot refactor this right now. TODO.
			{
				HitCharacters.Add(HitCharacter);
			}
		}
	}
	if (!HasAuthority() && bUseServerSideRewind)
	{
		if (!BlasterOwnerCharacter || ! BlasterOwnerController) return; // I could make checks for both of these but nah.
		if (BlasterOwnerCharacter->GetLagCompensationComponent() && BlasterOwnerCharacter->IsLocallyControlled())
		{
			BlasterOwnerCharacter->GetLagCompensationComponent()->ShotgunServerScoreRequest(
				HitCharacters, Start, HitTargets,
				BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime
			);
		}
	}
}

void AHitScanWeapon::ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets) const
{
	if(GetWeaponType() != EWeaponType::EWT_Shotgun || FireType != EFireType::EFT_Shotgun)
	{
		UE_LOG(LogTemp, Warning, TEXT("Weapon tried to ShotgunTraceEndWithScatter but it is not setup as a shotgun. %s"), *GetNameSafe(this))
		return;
	}
	const USkeletalMeshSocket* MuzzleFlashSocket = GetMesh()->GetSocketByName(MuzzleSocketName);
	if (!MuzzleFlashSocket) return;

	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetMesh());
	const FVector Start = SocketTransform.GetLocation();
	
	const FVector ToTargetNormalized = (HitTarget - Start).GetSafeNormal();
	const FVector SphereCenter = Start + ToTargetNormalized * DistanceToSphere;
	
	for (uint32 Index = 0; Index <NumberOfPellets; ++Index)
	{
		const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
		const FVector EndLoc = SphereCenter + RandVec;
		FVector ToEndLoc = EndLoc - Start;
		ToEndLoc = Start + ToEndLoc * MaxTraceDistance / ToEndLoc.Size();

		HitTargets.Add(ToEndLoc);
	}
}

void AHitScanWeapon::WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit) const
{
	const auto World = GetWorld();
	if (!World) return;

	const FVector End = TraceStart + (HitTarget - TraceStart) * MaxTraceDistance;
	
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(GetOwner());
	World->LineTraceSingleByChannel(OutHit, TraceStart, End, ECollisionChannel::ECC_Visibility, CollisionParams);

	FVector BeamEnd = End;
	if (OutHit.bBlockingHit)
	{
		BeamEnd = OutHit.ImpactPoint;
	}
	DRAW_DEBUG_SPHERE(GetWorld(), BeamEnd, 16.f, 12, FColor::Orange, true, -1, 0, 0);
	
	if (BeamEffectLegacy)
	{
		UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
			World,
			BeamEffectLegacy,
			TraceStart
		);
		if (Beam)
		{
			Beam->SetVectorParameter(FName("Target"), BeamEnd);
		}
	}
}
