// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Projectile.generated.h"


class UProjectileMovementComponent;
class UBoxComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundCue;

USTRUCT(BlueprintType)
struct FImpactEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> ImpactParticle;

	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundCue> ImpactSound;
};


/*
 *
 */
UCLASS()
class BLASTER_API AProjectile : public AActor
{
	GENERATED_BODY()

public:
	AProjectile();
	virtual void Destroyed() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	//
	// Used with server-side rewind.
	//
	bool bUseServerSideRewind = false;
	FVector_NetQuantize TraceStart;
	FVector_NetQuantize100 InitialVelocity;
	
	UPROPERTY(EditAnywhere)
	float InitialSpeed = 15000.f;
	
	// Set by ProjectileWeapon::Fire() for rockets and bullets. 
	// For grenades, set this directly since they are thrown by hand.
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0.000001f, ToolTip = "Only edit this for ProjectileGrenade (since it is hand thrown). For rockets and bullets this value is overwritten by the weapon at spawn time."))
	float Damage = 1.f;

protected:
	virtual void BeginPlay() override;
	// Useful for rockets and grenades. It is not convenient for bullets.
	void StartDestroyTimer();
	void SpawnTrailSystem();

	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* HitOther, UPrimitiveComponent* OtherComp,
	                   FVector NormalImpulse, const FHitResult& Hit);
	
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastImpactEffects(UPhysicalMaterial* HitMaterial);
	
	// The particle to spawn on a hit.
	UPROPERTY(VisibleAnywhere, Category=ImpactEffects)
	TObjectPtr<UNiagaraSystem> ImpactParticle;
	
	UPROPERTY(VisibleAnywhere, Category=ImpactEffects)
	TObjectPtr<USoundCue> ImpactSound;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UBoxComponent> CollisionBox;
	
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Projectile")
	TObjectPtr<UNiagaraSystem> TrailSystem;
	
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> TrailSystemComponent;
	
	// Default particle to spawn if hit surface does not have a physics material set.
	UPROPERTY(EditDefaultsOnly, Category=ImpactEffects)
	TObjectPtr<UNiagaraSystem> DefaultImpactParticle;
	
	// Default sound to play if hit surface does not have a physics material set.
	UPROPERTY(EditAnywhere, Category=ImpactEffects)
	TObjectPtr<USoundCue> DefaultImpactSound;

	// Creation must be implemented in every class constructor.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
	
	// The amount of damage to apply when an actor is on the outer radius. Use only with grenades and missiles.
	UPROPERTY(EditDefaultsOnly, Category = "Damage|Explosion Area",
		meta = (AllowPrivateAccess = true, ClampMin = 0.000001))
	float MinimumDamage = 0.1f;

	// Radius of the full damage area. Use only with grenades and missiles.
	UPROPERTY(EditDefaultsOnly, Category = "Damage|Explosion Area",
		meta = (AllowPrivateAccess = true, ClampMin = 0.000001))
	float DamageInnerRadius = 10.f;

	// Radius of the minimum damage area. Use only with grenades and missiles.
	UPROPERTY(EditDefaultsOnly, Category = "Damage|Explosion Area",
		meta = (AllowPrivateAccess = true, ClampMin = 0.000001))
	float DamageOuterRadius = 25.f;

	// Falloff exponent of damage from inner to outer radius. Use only with grenades and missiles.
	UPROPERTY(EditDefaultsOnly, Category = "Damage|Explosion Area",
		meta = (AllowPrivateAccess = true, ClampMin = 0.000001))
	float DamageFalloff = 1.f;
	
private:

	/* VFX. */
	UPROPERTY(EditDefaultsOnly, Category=VFX)
	TObjectPtr<UNiagaraSystem> TracerAsset;

	UPROPERTY(EditAnywhere, Category=VFX)
	TObjectPtr<UNiagaraComponent> TracerNiagaraComponent;
	
	// Different impact effects depending on surface hit.
	UPROPERTY(EditDefaultsOnly, Category=ImpactEffects)
	TMap<TEnumAsByte<EPhysicalSurface>, FImpactEffect> ImpactEffects;
	
	
	UPROPERTY(EditAnywhere, meta = (Units = "Seconds", ClampMin = 0.000001f))
	float DestroyTime = 3.f;
	
	FTimerHandle DestroyTimer;
};
