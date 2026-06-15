// Sebastian Lara. All rights reserved.


#include "HitScanWeapon.h"

#include "NiagaraFunctionLibrary.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

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
	
	TMap<AActor*, uint32> HitMap; // Pair of values for a character and the number of hits they receive.
	for (uint32 Index = 0; Index < NumberOfPellets; Index++)
	{
		// Trace will ignore owner.
		FHitResult Hit;
		WeaponTraceHit(Start, HitTarget, Hit);
		
		auto HitActor = Hit.GetActor();
		
		if (HitActor && HasAuthority() && InstigatorController) // Trace hit something.
		{
			if (HitMap.Contains(HitActor))
			{
				HitMap[HitActor]++; // Increase the number of hits.
			}
			else
			{
				HitMap.Emplace(HitActor,1);
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
	for (auto HitPair : HitMap)
	{
		if (HitPair.Key && HasAuthority() && InstigatorController)
		{
			UGameplayStatics::ApplyDamage(
				HitPair.Key,
				Damage * HitPair.Value, // Number of pellets that hit multiplied by damage.
				InstigatorController,
				this,
				UDamageType::StaticClass()
			);
		}
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
