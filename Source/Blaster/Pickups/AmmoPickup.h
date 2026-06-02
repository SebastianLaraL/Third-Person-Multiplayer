// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "AmmoPickup.generated.h"

USTRUCT(BlueprintType, Blueprintable)
struct FAmmoPickupData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	TSoftObjectPtr<USoundCue> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	int32 Amount;
};

/**
 *
 */
UCLASS()
class BLASTER_API AAmmoPickup : public APickup
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	virtual void BeginPlay() override;
	virtual void OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
public:
	UPROPERTY(ReplicatedUsing = OnRep_WeaponType, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0, ExposeOnSpawn))
	EWeaponType WeaponType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0))
	int32 AmmoAmount;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<EWeaponType, FAmmoPickupData> AmmoDataMap;
private:
	UFUNCTION()
	void OnRep_WeaponType();
};