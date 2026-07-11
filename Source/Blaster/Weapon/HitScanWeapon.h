// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "HitScanWeapon.generated.h"

class UNiagaraSystem;
class UParticleSystem;
/*
 * A type of weapon that casts line traces to emulate projectile hits.
 * NOTE: this type of weapon applies damage to the actor it hits, no matter
 * the subclass.
 */
UCLASS()
class BLASTER_API AHitScanWeapon : public AWeapon
{
	GENERATED_BODY()

public:
	AHitScanWeapon();
	virtual void Fire(const FVector& HitTarget) override;
	virtual void FireShotgun(const TArray<FVector_NetQuantize>& HitTargets);
	void ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets) const;
protected:
	// Performs a line trace depending on whether this weapon uses scatter or not and spawns a beam effect.
	// Outs the result of the line trace.
	void WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit) const;
	
	
	
	// Used for detecting headshots.
	UPROPERTY(EditDefaultsOnly)
	FName HeadBone = FName("Head");
private:
		
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1), Category = "Weapon Scatter")
	uint32 NumberOfPellets = 1;
	
	
	/* In the future, to implement different impact particles and sounds I could use the same approach
	* I used in the Projectile class.
	* For now, I will use only one impact particle.
	 */
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UNiagaraSystem> ImpactEffect;
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UParticleSystem> BeamEffectLegacy; // A legacy particle because I need to set a vector parameter and converting to niagara does not support making that parameter (I would have to make the parameter setup from scratch).
	
	// Returns false if setup is invalid (no pawn, or socket).
	bool GetFireSetup(FVector& OutStart, APawn*& OutOwnerPawn, AController*& OutInstigatorController) const;
	void SpawnImpactEffect(const FHitResult& Hit) const;
	// Valid only on server.
	void ApplyHitDamage(AActor* HitActor, const float DamageAmount, AController* InstigatorController, const APawn* OwnerPawn) const;
};
