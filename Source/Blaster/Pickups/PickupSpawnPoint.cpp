// Sebastian Lara. All rights reserved.


#include "PickupSpawnPoint.h"

#include "AmmoPickup.h"
#include "Pickup.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpawnPoint, Log, All)

APickupSpawnPoint::APickupSpawnPoint()
{
 	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
}

void APickupSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	
	StartSpawnPickupTimer(nullptr);
}

void APickupSpawnPoint::StartSpawnPickupTimer(AActor* DestroyedActor)
{
	const float SpawnTime = bRandomSpawnTime
		                        ? FMath::RandRange(SpawnPickupTimeMin, SpawnPickupTimeMax)
		                        : SpawnPickupTimeMax;
	
	GetWorldTimerManager().SetTimer(
		SpawnPickupTimer,
		this,
		&ThisClass::SpawnPickupTimerFinished,
		SpawnTime
		);
}

void APickupSpawnPoint::SpawnPickupTimerFinished()
{
	if (HasAuthority())
	{
		SpawnPickup();
	}
}

void APickupSpawnPoint::SpawnPickup()
{
	// Prevent empty slots in blueprint editor. An empty slot can cause no spawning a pickup and make useless this spawn point.
	const auto& ValidClasses = PickupClasses.FilterByPredicate(
		[](const TSubclassOf<APickup>& Class) { return IsValid(Class); });

	const int32 NumPickupClasses = ValidClasses.Num();
	
	if (NumPickupClasses < 1) return;

	
	const int32 Selection = FMath::RandRange(0, NumPickupClasses - 1);

	UE_LOG(LogSpawnPoint, Display, TEXT("Generated Selection: %s"), *PickupClasses[Selection]->GetName())

	/*
	 * OK, so basically I have to do all this because in this project I did not create one blueprint for each ammo pickup,
	 * instead I created one single Ammo pickup with all its attributes set to public.
	 * And I have to set them here. Using SpawnActorDeferred, then setting the attributes for example the most important is the
	 * WeaponType.
	 * Finally I have to FinishSpawning to tell Unreal I finished spawning a new actor and it should run the construction script.
	 */
	if (PickupClasses[Selection]->IsChildOf(AAmmoPickup::StaticClass()))
	{
		AAmmoPickup* AmmoPickup = GetWorld()->SpawnActorDeferred<AAmmoPickup>(
			PickupClasses[Selection], GetActorTransform());

		UE_LOG(LogSpawnPoint, Display, TEXT("AmmoPickup after deferred spawn: %s"), *GetNameSafe(AmmoPickup));
		
		if (AmmoPickup)
		{
			const int32 RandomWeaponType = FMath::RandRange(0, static_cast<int32>(EWeaponType::EWT_MAX) - 1); // Exclude EWT_MAX (no valid weapon to spawn).
			UE_LOG(LogSpawnPoint, Display, TEXT("Generated random ammo: %d"), RandomWeaponType);
			AmmoPickup->WeaponType = static_cast<EWeaponType>(RandomWeaponType);
			UE_LOG(LogSpawnPoint, Display, TEXT("Weapon type as uint8: %d .Weapon type generated: %d"),
			       static_cast<uint8>(RandomWeaponType), AmmoPickup->WeaponType)
			
			AmmoPickup->FinishSpawning(GetTransform());

			UE_LOG(LogSpawnPoint, Display, TEXT("AmmoPickup after FinishSpawning: %s"), *GetNameSafe(AmmoPickup));

			SpawnedPickup = AmmoPickup;
			UE_LOG(LogSpawnPoint, Display, TEXT("SpawnedPickup after assign: %s"), *GetNameSafe(SpawnedPickup.Get()));
		}
	}
	
	// Spawn any other type that is not an AmmoPickup.
	else
	{
		SpawnedPickup = GetWorld()->SpawnActor<APickup>(PickupClasses[Selection], GetActorTransform());
	}

	if (HasAuthority() && SpawnedPickup.Get())
	{
		UE_LOG(LogSpawnPoint, Display, TEXT("Binding OnDestroyed for: %s"), *GetNameSafe(SpawnedPickup.Get()));
		SpawnedPickup->OnDestroyed.AddUniqueDynamic(this, &ThisClass::StartSpawnPickupTimer);
	}
	else
	{
		UE_LOG(LogSpawnPoint, Warning, TEXT("SpawnedPickup is null, cannot bind OnDestroyed"));
	}
}
