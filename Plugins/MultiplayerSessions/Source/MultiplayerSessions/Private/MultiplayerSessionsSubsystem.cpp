// Sebastian Lara


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem() :
	// Bind Session related delegates
	CreateSessionCompleteDelegate(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))
{
	// Get session interface from online subsystem
	if (const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		SessionInterface = Subsystem->GetSessionInterface();
	}
}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType)
{
	if (!OnlineSubsystemIsAvailable())
	{
		return;
	}
	
	if (!IsValidSessionInterface())
	{
		return;
	}

	// Check if a session already exists and destroy it in that case
	if (auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession))
	{
		// Store information about last session in case of future creation
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;
		//				SessionInterface->DestroySession(NAME_GameSession);
		DestroySession();
	}

	// Store the delegate in a FDelegateHandle so we can later remove it from the delegate list
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	// Is LAN if steam subsystem is used, otherwise false
	if (IOnlineSubsystem::Get())
	{
		LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	}
	LastSessionSettings->NumPublicConnections = NumPublicConnections; // Number of connections for a particular session
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true; // Recommendation from a student in the comments of class 22
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1;
	
	
	if (GEngine && IOnlineSubsystem::Get()->GetSubsystemName() == "STEAM")
	{
		GEngine->AddOnScreenDebugMessage(1, 7.f, FColor::Green, FString("Steam subsystem found"));
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 7.f, FColor::Red, FString("Steam subsystem NOT found. Make sure Steam is running."));
	}

	if (const UWorld* World = GetWorld())
	{
		const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
		// If session could not be created, remove delegate
		if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession,
		                                     *LastSessionSettings))
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);

			// Broadcast our own custom delegate
			MultiplayerOnCreateSessionComplete.Broadcast(false);
		}
	}
}

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
	if (!OnlineSubsystemIsAvailable())
	{
		return;
	}
	
	if (!IsValidSessionInterface())
	{
		return;
	}
	
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FindSessionsCompleteDelegate);
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	if (IOnlineSubsystem::Get())
	{
		LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	}
	LastSessionSearch->QuerySettings.
	                   Set(SEARCH_LOBBIES /*FName("SEARCH_LOBBIES")*//*SEARCH_PRESENCE*//*FName("SEARCH_PRESENCE")*/, true,
	                       EOnlineComparisonOp::Equals);

	if (const UWorld* World = GetWorld())
	{
		if (const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
		{
			// Could not find a session
			if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(),
			                                    LastSessionSearch.ToSharedRef()))
			{
				SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
				MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
			}
		}
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionSearchResult)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		JoinSessionCompleteDelegate);
	if (const UWorld* World = GetWorld())
	{
		if (const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
		{			
			if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession,
			                                   SessionSearchResult))
			{
				SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
				MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
			}
		}
	}
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		DestroySessionCompleteDelegate);

	// Destroying the session has failed
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::StartSession()
{
}

bool UMultiplayerSessionsSubsystem::IsValidSessionInterface()
{
	if (!SessionInterface.IsValid())
	{
		if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
		{
			SessionInterface = Subsystem->GetSessionInterface();
		}
	}
	return SessionInterface.IsValid();
}

void UMultiplayerSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMultiplayerSessionsSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		// Remove delegate since creation was successful
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
		
	// Broadcast session was created successfully
	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	
	if (LastSessionSearch.IsValid())
	{
		if (LastSessionSearch->SearchResults.Num() <= 0)
		{
			MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
			return;
		}
	}
	
	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
//		MultiplayerOnJoinSessionComplete.Broadcast(Result);
	}
	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// TODO: Implement?
}

bool UMultiplayerSessionsSubsystem::OnlineSubsystemIsAvailable() const
{
	if (IOnlineSubsystem::Get() && IOnlineSubsystem::Get()->GetSubsystemName() == "STEAM")
	{
		return true;
	}
	// Add Epic games subsystem case here...
	
	MultiplayerOnOnlineSubsystemNotAvailable.Broadcast();
	return false;
}
