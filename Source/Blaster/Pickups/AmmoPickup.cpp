// Sebastian Lara. All rights reserved.


#include "AmmoPickup.h"

#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

void AAmmoPickup::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, WeaponType)
}

void AAmmoPickup::BeginPlay()
{
	Super::BeginPlay();
	
	OnRep_WeaponType();
}

void AAmmoPickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (auto OtherCombatComponent = OtherActor->FindComponentByClass<UCombatComponent>())
	{
		OtherCombatComponent->PickupAmmo(WeaponType, AmmoAmount);
	}
	if (AmmoDataMap.Contains(WeaponType))
	{
		SoftPickupSound = AmmoDataMap[WeaponType].Sound;
	}
	
	Super::OnBeginOverlapCallback(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void AAmmoPickup::OnRep_WeaponType()
{
	if (!AmmoDataMap.Contains(WeaponType)) return;
	
	const FAmmoPickupData& Data = AmmoDataMap[WeaponType];
	
	// Load mesh.
	if (!Data.Mesh.IsNull())
	{
		const auto LoadedMesh = Data.Mesh.LoadSynchronous();
		if (LoadedMesh && Mesh)
		{
			Mesh->SetStaticMesh(LoadedMesh);
		}
	}
	// Load sound.
	if (!Data.Sound.IsNull())
	{
		SoftPickupSound = Data.Sound;
	}

	// Set ammo amount.
	AmmoAmount = Data.Amount;
}

// TODO: rotating movement component here or in parent?
