// Sebastian Lara. All rights reserved.


#include "BlasterGameMode.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"

namespace MatchState
{
	const FName Cooldown = FName("Cooldown");
}

ABlasterGameMode::ABlasterGameMode()
{
	bDelayedStart = true;
	bUseSeamlessTravel = true;
}

void ABlasterGameMode::BeginPlay()
{
	Super::BeginPlay();
	LevelStartingTime = GetWorld()->GetTimeSeconds();
}

void ABlasterGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogGameMode, Warning, TEXT("End-Play-Reason: %s"), *UEnum::GetValueAsString(EndPlayReason))
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void ABlasterGameMode::HandleMatchIsWaitingToStart()
{
	
	// Setup and start a timer when level starts (no gameplay).
	
	Super::HandleMatchIsWaitingToStart();

	// Lambda
	FTimerDelegate TimerCallback = FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		// Run only the first time the match starts.
		if (!HasMatchStarted())
		{
			// Start match and clear timer.
			StartMatch();
			GetWorldTimerManager().ClearTimer(WarmupStateTimer);
		}
	});
	// Run a timer to start match.
	GetWorldTimerManager().SetTimer(WarmupStateTimer, TimerCallback, WarmupTime, false);
}

void ABlasterGameMode::HandleMatchHasStarted()
{
	/** Setup and start a timer when match begins to transition to State Cooldown.*/
	
	Super::HandleMatchHasStarted();

	// Lambda
	FTimerDelegate TimerCallback = FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		// Set state to Cooldown to display winner.
		SetMatchState(MatchState::Cooldown);
	});
	
	GetWorldTimerManager().SetTimer(MatchStartedTimer, TimerCallback, MatchTime, false);
}

void ABlasterGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();
	
	// Add overlay to all player controllers.
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (auto BlasterPlayerController = Cast<ABlasterPlayerController>(*It))
		{
			BlasterPlayerController->OnMatchStateSet(MatchState);
		}
	}
	// When changing to Cooldown state, start a timer to restart game.
	if (MatchState == MatchState::Cooldown)
	{
		 // Lambda 
		FTimerDelegate TimerCallback = FTimerDelegate::CreateWeakLambda(this,[this]()
		{
			UE_LOG(LogGameMode, Display, TEXT("RESTARTING GAME in %f"), CooldownTime)
			RestartGame();
		});
		GetWorldTimerManager().SetTimer(RestartGameTimer, TimerCallback, CooldownTime, false);
	}
}

void ABlasterGameMode::PlayerEliminated(ABlasterCharacter* ElimmedCharacter, ABlasterPlayerController* VictimController,
                                        ABlasterPlayerController* AttackerController) const
{
	if (!AttackerController || !AttackerController->PlayerState)
	{
		return;
	}
	if (!VictimController || !VictimController->PlayerState)
	{
		return;
	}
	
	ABlasterPlayerState* AttackerPlayerState = AttackerController ? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
	ABlasterPlayerState* VictimPlayerState = VictimController ? Cast<ABlasterPlayerState>(VictimController->PlayerState) : nullptr;
	
	auto BlasterGameState = GetGameState<ABlasterGameState>();
	
	// Make sure Attacker is not the victim. Add score then.
	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && BlasterGameState)
	{
		TArray<ABlasterPlayerState*> PlayersCurrentlyInTheLead;
		for (const auto& LeadPlayer : BlasterGameState->TopScoringPlayers)
		{
			PlayersCurrentlyInTheLead.Add(LeadPlayer);
		}
		
		AttackerPlayerState->AddToScore(1.f);
		BlasterGameState->UpdateTopScore(AttackerPlayerState);
		
		if (BlasterGameState->TopScoringPlayers.Contains(AttackerPlayerState))
		{
			if (ABlasterCharacter* Leader = Cast<ABlasterCharacter>(AttackerPlayerState->GetPawn()))
			{
				Leader->MulticastGainedTheLead();
			}
		}
		
		for (int32 Index = 0; Index < PlayersCurrentlyInTheLead.Num(); Index++)
		{
			if (!BlasterGameState->TopScoringPlayers.Contains(PlayersCurrentlyInTheLead[Index]))
			{
				if (ABlasterCharacter* Loser = Cast<ABlasterCharacter>(PlayersCurrentlyInTheLead[Index]->GetPawn()))
				{
					Loser->MulticastLostTheLead();
				}
			}
		}
	}
	// Add victim defeat.
	if (VictimPlayerState)
	{
		VictimPlayerState->AddToDefeats(1);
	}
	
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Elim();
	}
	
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const auto BlasterPlayerController = Cast<ABlasterPlayerController>(*It);
		if (BlasterPlayerController && AttackerPlayerState && VictimPlayerState)
		{
			BlasterPlayerController->BroadcastElim(AttackerPlayerState, VictimPlayerState);
		}
	}
}

void ABlasterGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
{
	// Destroy pawn.
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Reset();
		ElimmedCharacter->Destroy();
	}
	// Choose a random player start in the level and respawn.
	if (ElimmedController)
	{
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(),PlayerStarts);
		const int32 SelectionIndex = FMath::RandRange(0, PlayerStarts.Num() - 1); // A random index to choose a player start. TODO: implement
																						// an algorithm to spawn at a location far away from all players.
		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[SelectionIndex]);
	}
}

void ABlasterGameMode::Logout(AController* Exiting)
{
	if (const auto BlasterGameState = GetGameState<ABlasterGameState>())
	{
		BlasterGameState->RemoveLeavingPlayer(Cast<ABlasterPlayerState>(Exiting->PlayerState));
	}
	
	Super::Logout(Exiting);
}
