// Sebastian Lara. All rights reserved.


#include "PauseMenu.h"
#include "Components/Button.h"

bool UPauseMenu::Initialize()
{
	if (!Super::Initialize())
	{
		UE_LOG(LogTemp, Warning, TEXT("PauseMenu::Initialize - Super::Initialize failed"));
		return false;
	}
	
	if (CloseMenuButton)
	{
		CloseMenuButton->OnClicked.AddDynamic(this, &ThisClass::UPauseMenu::CloseMenuButtonClicked);
	}
	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->OnClicked.AddDynamic(this, &ThisClass::ReturnToMainMenuButtonClicked);
	}
	if (QuitGameButton)
	{
		QuitGameButton->OnClicked.AddDynamic(this, &ThisClass::QuitGameButtonClicked);
	}
	return true;
}

void UPauseMenu::CloseMenuButtonClicked()
{
	OnCloseMenuRequested.Broadcast();
}

void UPauseMenu::ReturnToMainMenuButtonClicked()
{
	OnReturnToMenuRequested.Broadcast();
}

void UPauseMenu::QuitGameButtonClicked()
{
	OnQuitGameRequested.Broadcast();
}
