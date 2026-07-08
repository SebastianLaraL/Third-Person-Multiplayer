// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "BlasterGameState.generated.h"

class ABlasterPlayerState;

/**
 * Blaster game state.
 * Supports up to 3 players with top score in the same match.
 * (The 3 value is hardcoded in the constructor)
 */
UCLASS()
class BLASTER_API ABlasterGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	ABlasterGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void UpdateTopScore(ABlasterPlayerState* const ScoringPlayer);
	void RemoveLeavingPlayer(ABlasterPlayerState* LeavingPlayer);
	
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ABlasterPlayerState>> TopScoringPlayers;	
private:
	float TopScore = 0.f;
};
