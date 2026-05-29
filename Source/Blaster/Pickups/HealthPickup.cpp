// Sebastian Lara. All rights reserved.


#include "HealthPickup.h"
#include "Blaster/BlasterComponents/HealthComponent.h"


AHealthPickup::AHealthPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
}

void AHealthPickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (auto HealthComp = OtherActor->FindComponentByClass<UHealthComponent>())
	{
		HealthComp->StartHealing(HealAmount, HealTime);
	}
	Super::OnBeginOverlapCallback(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}