// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

/**
 * Game mode for a lobby level.
 */
UCLASS(Deprecated, meta = (DeprecationMessage = "This Lobby is deprecated, use HostLobbyGameMode instead"))
class BLASTER_API ADEPRECATED_LobbyGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	// Always sets bUseSeamlessTravel = true
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
