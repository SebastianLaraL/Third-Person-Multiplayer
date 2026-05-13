// Sebastian Lara. All rights reserved.


#include "LobbyGameMode.h"

#include "GameFramework/GameStateBase.h"

void ADEPRECATED_LobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	// Change from lobby to Gameplay level if we have enough players./
	int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();
	if (NumberOfPlayers == 3) // TODO: 2 is not strictly necessary.
	{
		if (UWorld* const World = GetWorld())
		{
			bUseSeamlessTravel = true;
			World->ServerTravel(FString(/*"/Game/Maps/BlasterMap?listen"*/ /*"/Game/Maps/BlasterMapLowPoly?listen"*/"/Game/Maps/LowPolyOnly?listen"));
		}
	}
}
