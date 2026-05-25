// Sebastian Lara. All rights reserved.


#include "HealthComponent.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/Controller.h" 


UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UHealthComponent, CurrentHealth);
}


void UHealthComponent::StartHealing(const float HealAmount, const float HealTime)
{
	if (!GetOwner()->HasAuthority() || IsDead())
	{
		return;
	}
	if (CurrentHealth >= MaxHealth)
	{
		return;
	}
	
	RemainingHealAmount = HealAmount;
	HealingRate = HealAmount / HealTime;
	EndResultingHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.f, MaxHealth);
	
	GetWorld()->GetTimerManager().SetTimer(HealTimerHandle, this, &ThisClass::HealTick, HealTickInterval, true);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Bind owners OnTakeAnyDamage (only on server)
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &ThisClass::ReceiveDamage);
	}
}

void UHealthComponent::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	if (IsDead()) return;
	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.f, MaxHealth);
	LastInstigatorController = InstigatedBy;
	OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth - OldHealth, LastInstigatorController.Get());
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth - OldHealth, LastInstigatorController.Get());
}

void UHealthComponent::HealTick()
{
	if (!GetOwner()->HasAuthority()) return;
	
	if (IsDead() || RemainingHealAmount <= 0.f)
	{
		GetWorld()->GetTimerManager().ClearTimer(HealTimerHandle);
		CurrentHealth = EndResultingHealth; // Make sure the health is set to the right final value. This is necessary due to floating-point precision issues.
		return;
	}
	const float OldHealth = CurrentHealth;
	
	const float HealThisTick = HealingRate * HealTickInterval;
	const float ActualHealAmount = FMath::Min(HealThisTick,RemainingHealAmount);
	
	CurrentHealth = FMath::Clamp(CurrentHealth + ActualHealAmount, 0.f, MaxHealth);
	
	RemainingHealAmount -= ActualHealAmount;
	
	OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth - OldHealth, nullptr);
	
}
