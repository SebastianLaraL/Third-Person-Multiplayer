// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "HostLobbyGameMode.generated.h"

/**
 * Game mode that allows the host player to start a match
 * using a button on screen. 
 * Uses seamless travel in TryStartMatch.
 */
UCLASS()
class BLASTER_API AHostLobbyGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	AHostLobbyGameMode();
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	// Uses seamless travel.
	void TryStartMatch();
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bHasShownHostWidget = false;
	
	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UWorld> Level;
};
