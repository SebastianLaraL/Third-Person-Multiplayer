// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BlasterGameMode.generated.h"

namespace MatchState
{
	extern BLASTER_API const FName Cooldown; // Match duration has been reached. Display winner and begin cooldown timer.
}


class ABlasterCharacter;
class ABlasterPlayerController;

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	ABlasterGameMode();
	virtual void PlayerEliminated(ABlasterCharacter* ElimmedCharacter, ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController) const;
	virtual void RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController);
	virtual void Logout(AController* Exiting) override;
	
	// How long the player will control an invisible
	// flying pawn before match begins.
	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;
	
	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;
	
	UPROPERTY(EditDefaultsOnly)
	float CooldownTime = 10.f;

	float LevelStartingTime = 0.f;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/** Setup and start a timer when level starts (no gameplay).*/
	virtual void HandleMatchIsWaitingToStart() override;
	
	/** Setup and start a timer when match begins to transition to State Cooldown.*/
	virtual void HandleMatchHasStarted() override;
	virtual void OnMatchStateSet() override;
private:
	FTimerHandle WarmupStateTimer;
	FTimerHandle RestartGameTimer;
	FTimerHandle MatchStartedTimer;
};
