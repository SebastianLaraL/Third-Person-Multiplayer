// Sebastian Lara. All rights reserved.


#include "HealthComponent.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/Controller.h" 

DEFINE_LOG_CATEGORY(LogHealthComponent)

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UHealthComponent, CurrentHealth);
	DOREPLIFETIME(UHealthComponent, CurrentShield);
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
	UE_LOG(LogHealthComponent, Display, TEXT("Called receive damage."))
	
	if (IsDead()) return;
	
	LastInstigatorController = InstigatedBy;
	const float OldHealth = CurrentHealth;
	float DamageToApplyToHealth = Damage;
	
	if (CurrentShield > 0.f)
	{
		const float OldShield = CurrentShield;
		
		if (CurrentShield >= Damage) // Enough shield, health is unaffected.
		{
			CurrentShield = FMath::Clamp(CurrentShield - Damage, 0.f, MaxShield);
			DamageToApplyToHealth = 0.f;
			UE_LOG(LogHealthComponent, Display, TEXT("Current shield set to: %f"), CurrentShield)
		}
		else // Shield is too low, apply health damage as well.
		{
			DamageToApplyToHealth = Damage - CurrentShield;
			CurrentShield = 0.f;
		}
		OnShieldChanged.Broadcast(CurrentShield, CurrentShield - OldShield, LastInstigatorController.Get());
		UE_LOG(LogHealthComponent, Display, TEXT("Current shield set to: %f"), CurrentShield)
	}
	
	if (DamageToApplyToHealth > 0.f)
	{
		CurrentHealth = FMath::Clamp(CurrentHealth - DamageToApplyToHealth, 0.f, MaxHealth);
		OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth - OldHealth, LastInstigatorController.Get());
	}
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
	UE_LOG(LogHealthComponent, Display, TEXT("Health set to: %f"), CurrentHealth)
	OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth - OldHealth, LastInstigatorController.Get());
}

void UHealthComponent::OnRep_Shield(float OldShield)
{
	UE_LOG(LogHealthComponent, Display, TEXT("Shield set to: %f"), CurrentShield)
	OnShieldChanged.Broadcast(CurrentShield, CurrentShield - OldShield, LastInstigatorController.Get());
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
