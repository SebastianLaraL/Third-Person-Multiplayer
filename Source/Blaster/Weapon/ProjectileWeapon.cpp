// Sebastian Lara. All rights reserved.


#include "ProjectileWeapon.h"

#include "Engine/SkeletalMeshSocket.h"
#include "Projectile.h"

AProjectileWeapon::AProjectileWeapon()
{
	FireType = EFireType::EFT_Projectile;
}

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	APawn* const InstigatorPawn = Cast<APawn>(GetOwner());
	if (!ProjectileClass || !InstigatorPawn) return;
	
	USkeletalMeshSocket const* MuzzleFlashSocket = GetMesh()->GetSocketByName(MuzzleSocketName);
	if (!MuzzleFlashSocket) return;

	UWorld* const World = GetWorld();
	if (!World) return;
	
	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetMesh());
	// From muzzle flash socket to hit location from TraceUnderCrosshairs.
	const FVector ToTarget = HitTarget - SocketTransform.GetLocation();
	const FRotator TargetRotation = ToTarget.Rotation();
	
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = InstigatorPawn;
	
	AProjectile* SpawnedProjectile; // = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
	
	if (bUseServerSideRewind)
	{
		if (InstigatorPawn->HasAuthority())
		{
			if (InstigatorPawn->IsLocallyControlled()) // Server host.
			{
				SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
				SpawnedProjectile->bUseServerSideRewind = false;
				SpawnedProjectile->Damage = Damage;
			}
			else // Server, controlled by a remote client.
			{
				SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
				SpawnedProjectile->SetReplicates(false);
				SpawnedProjectile->bUseServerSideRewind = false;
			}
		}
		else
		{
			if (InstigatorPawn->IsLocallyControlled()) // Local client with SSR.
			{
				SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
				SpawnedProjectile->SetReplicates(false);
				SpawnedProjectile->bUseServerSideRewind = true;
				SpawnedProjectile->TraceStart = SocketTransform.GetLocation();
				SpawnedProjectile->InitialVelocity = SpawnedProjectile->GetActorForwardVector() * SpawnedProjectile->InitialSpeed;
				SpawnedProjectile->Damage = Damage;
			}
			else // Remote client without SSR.
			{
				SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
				SpawnedProjectile->SetReplicates(false);
				SpawnedProjectile->bUseServerSideRewind = false;
			}
		}
	}
	else // No SSR, only server spawns and replicates as usual.
	{
		if (InstigatorPawn->HasAuthority())
		{
			SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
			SpawnedProjectile->bUseServerSideRewind = false;
			SpawnedProjectile->Damage = Damage;
		}
	}
	// TODO: object pooling?
}
