// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterOverlay.generated.h"

class UProgressBar;
class UTextBlock;
class UWidgetAnimation;
class UImage;

/**
 * For displaying players information such as health, ammo, and defeats.
 */
UCLASS()
class BLASTER_API UCharacterOverlay : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ShieldBar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ShieldText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DefeatsAmount;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ScoreAmount;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WeaponAmmoAmount;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CarriedAmmoAmount;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EquippedWeaponName;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GrenadesText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MatchCountDownText;
	
	// E.g. blinking animation.
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	TObjectPtr<UWidgetAnimation> CountdownAnimation;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> HighPingImage;
	
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	TObjectPtr<UWidgetAnimation> HighPingAnimation;
};
