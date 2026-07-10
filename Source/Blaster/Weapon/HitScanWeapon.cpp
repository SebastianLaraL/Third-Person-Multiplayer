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

	FVector Start;
	APawn* OwnerPawn;
	AController* InstigatorController;
	if (!GetFireSetup(Start, OwnerPawn, InstigatorController)) return;
	
	
	TMap<ABlasterCharacter*, uint32> HitMap; // Pair of values for a character and the number of hits they receive.
	TMap<ABlasterCharacter*, uint32> HeadShotHitMap;
	
	for (uint32 Index = 0; Index < NumberOfPellets; Index++)
	{
		// Trace will ignore owner.
		FHitResult Hit;
		WeaponTraceHit(Start, HitTarget, Hit);
		
		ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(Hit.GetActor());

		if (HitCharacter && InstigatorController) // Trace hit something.
		{
			const bool bHeadShot = Hit.BoneName == HeadBone;
			FString String = FString::Printf(TEXT("HitBone: %s"), *Hit.BoneName.ToString());
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, String);
			if (bHeadShot)
			{
				HeadShotHitMap.Contains(HitCharacter) ? HeadShotHitMap[HitCharacter]++ : HeadShotHitMap.Emplace(HitCharacter, 1);
			}
			else
			{
				HitMap.Contains(HitCharacter) ? HitMap[HitCharacter]++ : HitMap.Emplace(HitCharacter, 1);
			}
		}
		SpawnImpactEffect(Hit);
		
		// Hit sound.
	}
	
	TMap<ABlasterCharacter*, float> DamageMap;
	for (const auto& HitPair : HitMap)
	{
		if (HitPair.Key) DamageMap.Emplace(HitPair.Key, HitPair.Value * Damage);
	}
	for (const auto& HeadShotPair : HeadShotHitMap)
	{
		if (HeadShotPair.Key)
		{
			if (DamageMap.Contains(HeadShotPair.Key)) DamageMap[HeadShotPair.Key] += HeadShotPair.Value * HeadShotDamage;
			else DamageMap.Emplace(HeadShotPair.Key, HeadShotPair.Value * HeadShotDamage);
		}
	}
	
	// Apply damage to all actors according to the number of hits received, including headshots.
	for (const auto& DamagePair : DamageMap)
	{
		ABlasterCharacter* HitBlasterCharacter = DamagePair.Key;
		if (!HitBlasterCharacter || !InstigatorController) continue;
		
		ApplyHitDamage(HitBlasterCharacter, DamagePair.Value, InstigatorController, OwnerPawn);
		
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
	
	const auto World = GetWorld();
	if (!World) return;
	
	FVector Start;
	APawn* OwnerPawn;
	AController* InstigatorController;
	if (!GetFireSetup(Start, OwnerPawn, InstigatorController)) return;

	// Maps hit character to number of times hit
	TMap<ABlasterCharacter*, uint32> HitMap;
	TMap<ABlasterCharacter*, uint32> HeadShotHitMap;
	
	for (FVector_NetQuantize HitTarget : HitTargets)
	{
		FHitResult FireHit;
		WeaponTraceHit(Start, HitTarget, FireHit);

		if (ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(FireHit.GetActor()))
		{
			const bool bHeadShot = FireHit.BoneName == HeadBone;
			if (bHeadShot)
			{
				HeadShotHitMap.Contains(HitCharacter) ? HeadShotHitMap[HitCharacter]++ : HeadShotHitMap.Emplace(HitCharacter, 1);
			}
			else
			{
				HitMap.Contains(HitCharacter) ? HitMap[HitCharacter]++ : HitMap.Emplace(HitCharacter, 1);
			}
			SpawnImpactEffect(FireHit);
		}
	}
	
	TArray<ABlasterCharacter*> HitCharacters;
	TMap<AActor*, float> DamageMap;
	
	for (const auto& HitPair : HitMap)
	{
		if (HitPair.Key)
		{
			DamageMap.Emplace(HitPair.Key, HitPair.Value * Damage);
			HitCharacters.AddUnique(HitPair.Key);
		}
	}
	for (const auto& HeadShotPair : HeadShotHitMap)
	{
		if (HeadShotPair.Key)
		{
			if (DamageMap.Contains(HeadShotPair.Key)) DamageMap[HeadShotPair.Key] += HeadShotPair.Value * HeadShotDamage;
			else DamageMap.Emplace(HeadShotPair.Key, HeadShotPair.Value * HeadShotDamage);
			HitCharacters.AddUnique(HeadShotPair.Key);
		}
	}
	
	for (const auto& DamagePair : DamageMap)
	{
		if (!DamagePair.Key || !InstigatorController) continue;
		ApplyHitDamage(DamagePair.Key, DamagePair.Value, InstigatorController, OwnerPawn);
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
	else
	{
		OutHit.ImpactPoint = End;
	}
	DRAW_DEBUG_SPHERE(GetWorld(), BeamEnd, 16.f, 12, FColor::Orange, false, 8.0f, 0, 0);
	
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

bool AHitScanWeapon::GetFireSetup(FVector& OutStart, APawn*& OutOwnerPawn, AController*& OutInstigatorController) const
{
	OutOwnerPawn = Cast<APawn>(GetOwner());
	if (!OutOwnerPawn) return false;
	
	OutInstigatorController = OutOwnerPawn->GetController();
	
	const USkeletalMeshSocket* MuzzleFlashSocket = GetMesh()->GetSocketByName(MuzzleSocketName);
	if (!MuzzleFlashSocket) return false;
	
	OutStart = MuzzleFlashSocket->GetSocketTransform(GetMesh()).GetLocation();
	return true;
}

void AHitScanWeapon::SpawnImpactEffect(const FHitResult& Hit) const
{
	if (ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ImpactEffect,
			Hit.ImpactPoint,
			Hit.ImpactNormal.Rotation()
		);
	}
}

void AHitScanWeapon::ApplyHitDamage(AActor* HitActor, const float DamageAmount, AController* InstigatorController, const APawn* OwnerPawn) const
{
	const bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
	if (HasAuthority() && bCauseAuthDamage)
	{
		UGameplayStatics::ApplyDamage(HitActor, DamageAmount, InstigatorController, const_cast<AHitScanWeapon*>(this), UDamageType::StaticClass());
	}
}
