// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupSpawnPoint.generated.h"

class APickup;

UCLASS()
class BLASTER_API APickupSpawnPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	APickupSpawnPoint();

protected:
	virtual void BeginPlay() override;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSubclassOf<APickup>> PickupClasses;
	
	UFUNCTION()
	void StartSpawnPickupTimer(AActor* DestroyedActor);
	void SpawnPickupTimerFinished();
	void SpawnPickup();
	
	// If set to false, the Pickup item will be spawned every SpawnPickupTimeMax seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bRandomSpawnTime = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0.1f, ForceUnits = Seconds))
	float SpawnPickupTimeMin = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0.1f, ForceUnits = Seconds))
	float SpawnPickupTimeMax = 5.f;
	
private:
	FTimerHandle SpawnPickupTimer;
	
	TWeakObjectPtr<AActor>	SpawnedPickup;
};
