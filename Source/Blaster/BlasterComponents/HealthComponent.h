// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AController;


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
	
	void StartHealing(const float HealAmount, const float HealTime);
	
protected:
	
	UFUNCTION()
	virtual void ReceiveDamage(AActor* DamagedActor, float Damage, const /*class*/ UDamageType* DamageType, /*class*/ AController* InstigatedBy, AActor* DamageCauser);
	
	UFUNCTION()
	virtual void OnRep_Health(float OldHealth);
	
	void HealTick();
	
private:
	
	/*
	 * Player health.
	 */
	
	UPROPERTY(ReplicatedUsing = OnRep_Health, EditDefaultsOnly, Category = Health, meta = (ClampMin = 0.1f))
	float CurrentHealth = 100.f;
	
	UPROPERTY(EditAnywhere, Category = Health, meta = (ClampMin = 0.00001))
	float MaxHealth = 100.f;
	
	FTimerHandle HealTimerHandle;
	float RemainingHealAmount = 0.f;
	float HealingRate = 0.f;
	float EndResultingHealth = 0.f;
	
	// Speed at which we update the health when healing (using timers).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health|Healing", meta = (AllowPrivateAccess = true, ClampMin = 0.0166, ForceUnits = "Seconds"))
	float HealTickInterval = 0.1;
	
	// The instigator controller of the last damage taken event.
	// Saved to call on OnRep_Health.
	TWeakObjectPtr<AController> LastInstigatorController = nullptr;
public:
	
	UFUNCTION(BlueprintPure, Category = Health)
	FORCEINLINE float GetCurrentHealth() const { return CurrentHealth; }
	
	UFUNCTION(BlueprintPure, Category = Health)
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	
	UFUNCTION(BlueprintPure, Category = Health)
	FORCEINLINE bool IsDead() const { return CurrentHealth <= 0.f; }
};
