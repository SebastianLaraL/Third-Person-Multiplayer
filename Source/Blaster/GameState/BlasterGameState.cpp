// Sebastian Lara. All rights reserved.


#include "BlasterGameState.h"

#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Net/UnrealNetwork.h"

ABlasterGameState::ABlasterGameState()
{
	/* This reserves space for 3 winners by default, but it is allowed to add over 3 TopScoring players.*/
	TopScoringPlayers.Reserve(3);
}

void ABlasterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ABlasterGameState, TopScoringPlayers)
}

void ABlasterGameState::UpdateTopScore(ABlasterPlayerState* const ScoringPlayer)
{
	if (TopScoringPlayers.Num() == 0) // No top scoring players: add the first top scoring player.
	{
		TopScoringPlayers.Add(ScoringPlayer);
		TopScore = ScoringPlayer->GetScore();
	}
	else if (ScoringPlayer->GetScore() == TopScore) // At least another player with the same score.
	{
		TopScoringPlayers.AddUnique(ScoringPlayer);
	}
	else if (ScoringPlayer->GetScore() > TopScore) // Player has the highest score on the match, empty array and only add this player.
	{
		TopScoringPlayers.Empty();
		TopScoringPlayers.AddUnique(ScoringPlayer);
		TopScore = ScoringPlayer->GetScore();
	}
}

void ABlasterGameState::RemoveLeavingPlayer(ABlasterPlayerState* LeavingPlayer) 
{
	if (!LeavingPlayer) return;
	if (TopScoringPlayers.Contains(LeavingPlayer))
	{
		TopScoringPlayers.Remove(LeavingPlayer);
	}
}
