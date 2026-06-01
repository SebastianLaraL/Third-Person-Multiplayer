// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AController;

DECLARE_LOG_CATEGORY_EXTERN(LogHealthComponent, Log, All);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FHealthChangedSignature, float NewHealth, float DeltaHealth, AController* InstigatorController);

/*
 * A health component. It is designed to be
 * reusable in different projects.
 * Supports replication.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHealthComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	FHealthChangedSignature OnHealthChanged;
	FHealthChangedSignature OnShieldChanged;
	
	void StartHealing(const float HealAmount, const float HealTime);
	void StartReplenishingShield(const float ShieldAmount, const float ShieldTime);
	
protected:
	
	UFUNCTION()
	virtual void ReceiveDamage(AActor* DamagedActor, float Damage, const /*class*/ UDamageType* DamageType, /*class*/ AController* InstigatedBy, AActor* DamageCauser);
	
	UFUNCTION()
	virtual void OnRep_Health(float OldHealth);
	
	UFUNCTION()
	virtual void OnRep_Shield(float OldShield);
	
	void HealTick();
	
	void ReplenishShieldTick();
	
private:
	
	/*
	 * Player health.
	 */
	
	UPROPERTY(ReplicatedUsing = OnRep_Health, EditDefaultsOnly, Category = Health, meta = (ClampMin = 0.1f))
	float CurrentHealth = 100.f;
	
	UPROPERTY(EditAnywhere, Category = Health, meta = (ClampMin = 0.00001))
	float MaxHealth = 100.f;
	
	// Shield.
	// Originally I did not design this component to support shields, but it would complicate things to create a separate Shield component and a new damage system.
	UPROPERTY(ReplicatedUsing = OnRep_Shield, EditDefaultsOnly, Category = Shield, meta = (ClampMin = 0.1f))
	float CurrentShield = 100.f;
	
	UPROPERTY(EditAnywhere, Category = Shield, meta = (ClampMin = 0.1f))
	float MaxShield = 100.f;
	
	FTimerHandle HealTimerHandle;
	float RemainingHealAmount = 0.f;
	float HealingRate = 0.f;
	float EndResultingHealth = 0.f;
	
	FTimerHandle ReplenishShieldTimerHandle;
	float RemainingReplenishShieldAmount = 0.f;
	float ReplenishingShieldRate = 0.f;
	float EndResultingShield = 0.f;
	
	// Speed at which we update the health when healing (using timers).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health|Healing", meta = (AllowPrivateAccess = true, ClampMin = 0.0166, ForceUnits = "Seconds"))
	float HealTickInterval = 0.1;
	
	// Speed at which we update the shield when Replenishing (using timers).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield|Replanish", meta = (AllowPrivateAccess = true, ClampMin = 0.0166, ForceUnits = "Seconds"))
	float ReplenishShieldTickInterval = 0.1;
	
	// The instigator controller of the last damage taken event.
	// Saved to call on OnRep_Health.
	TWeakObjectPtr<AController> LastInstigatorController = nullptr;
public:
	
	UFUNCTION(BlueprintPure, Category = Health)
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }
	
	UFUNCTION(BlueprintPure, Category = Health)
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	
	UFUNCTION(BlueprintPure, Category = Health)
	FORCEINLINE float GetCurrentShield() const { return CurrentShield; }
	
	UFUNCTION(BlueprintPure, Category = Health)
	FORCEINLINE float GetMaxShield() const { return MaxShield; }
	
	UFUNCTION(BlueprintPure, Category = Health)
	FORCEINLINE bool IsDead() const { return CurrentHealth <= 0.f; }
};
