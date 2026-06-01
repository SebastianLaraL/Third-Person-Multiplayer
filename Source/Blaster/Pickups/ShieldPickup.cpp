// Sebastian Lara. All rights reserved.


#include "ShieldPickup.h"
#include "Blaster/BlasterComponents/HealthComponent.h"

AShieldPickup::AShieldPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
}

void AShieldPickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (auto HealthComp = OtherActor->FindComponentByClass<UHealthComponent>())
	{
		HealthComp->StartReplenishingShield(ShieldReplenishAmount, ShieldReplenishTime);
	}
	Super::OnBeginOverlapCallback(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}