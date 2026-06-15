// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "ProjectileWeapon.generated.h"

class AProjectile;
/**
 * A Weapon that actually spawns a projectile, which will move throughout the world.
 */
UCLASS(Abstract, Blueprintable)
class BLASTER_API AProjectileWeapon : public AWeapon
{
	GENERATED_BODY()
	
public:
	AProjectileWeapon();
	/* Spawn projectile at specified socket and with a rotation that matches the end of the trace hit.*/
	virtual void Fire(const FVector& HitTarget) override;
	
private:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AProjectile> ProjectileClass;
};
