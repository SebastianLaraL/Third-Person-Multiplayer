// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenu.generated.h"

class ABlasterPlayerController;
class UButton;

DECLARE_MULTICAST_DELEGATE(FOnCloseMenuRequested)
DECLARE_MULTICAST_DELEGATE(FOnQuitRequested)
DECLARE_MULTICAST_DELEGATE(FOnReturnToMenuRequested)

/**
 * 
 */
UCLASS()
class BLASTER_API UPauseMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	
	// Buttons.
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseMenuButton;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ReturnToMainMenuButton;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuitGameButton;
	
	// Delegates.
	FOnCloseMenuRequested OnCloseMenuRequested;
	FOnQuitRequested OnQuitGameRequested;
	FOnReturnToMenuRequested OnReturnToMenuRequested;
	
private:
	UPROPERTY()
	TObjectPtr<ABlasterPlayerController> BlasterPlayerController;
	
	UFUNCTION()
	void CloseMenuButtonClicked();
	
	UFUNCTION()
	void ReturnToMainMenuButtonClicked();
	
	UFUNCTION()
	void QuitGameButtonClicked();
};
