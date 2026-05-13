// Sebastian Lara


#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "Components/TextBlock.h"

void UMenu::MenuSetupByPath(FString LobbyPath, int32 NumberOfPublicConnections, FString TypeOfMatch)
{
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);

	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;

	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if (UWorld* const World = GetWorld())
	{
		if (APlayerController* const PlayerController = World->GetFirstPlayerController())
		{
			FInputModeUIOnly InputModeData;
			// Focus on this widget
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	if (UGameInstance* const GameInstance = GetGameInstance())
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	if (OnlineSubsystemWarningText)
	{
		OnlineSubsystemWarningText->SetVisibility(ESlateVisibility::Hidden);
	}
	BindMultiplayerSessionsSubsystemCallbacks();
}

void UMenu::MenuSetupByLevel(TSoftObjectPtr<UWorld> Level, int32 NumberOfPublicConnections, FString TypeOfMatch)
{
	UE_LOG(LogSlate, Warning, TEXT("Level path: %s | IsNull: %d"),
	*Level.ToString(),
	Level.IsNull() ? 1 : 0);
	UE_LOG(LogTemp, Warning, TEXT("PackageName: %s"),
	*Level.ToSoftObjectPath().GetLongPackageName());
	
	// Make sure Level is valid, otherwise log and return.
	if (Level.IsNull())
	{
		const FString Message = FString::Printf(
			TEXT("Error: Level soft object ptr is not valid. Make sure the level pin has a valid UWorld asset."
		"\n Internal error: Level soft object ptr is not valid in %s. Line: %d"), *GetNameSafe(this), __LINE__);
		UE_LOG(LogLevelStreaming, Error, TEXT("%s"), *Message);
		GEngine->AddOnScreenDebugMessage(1, 15.f, FColor::Red, *Message);
		return;
	}
	
	PathToLobby = FString::Printf(TEXT("%s?listen"), *Level.ToSoftObjectPath().GetLongPackageName());

	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;

	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if (UWorld* const World = GetWorld())
	{
		if (APlayerController* const PlayerController = World->GetFirstPlayerController())
		{
			FInputModeUIOnly InputModeData;
			// Focus on this widget
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	if (UGameInstance* const GameInstance = GetGameInstance())
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	if (OnlineSubsystemWarningText)
	{
		OnlineSubsystemWarningText->SetVisibility(ESlateVisibility::Hidden);
	}
	BindMultiplayerSessionsSubsystemCallbacks();
}


bool UMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// Bind buttons and click events
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}

	return true;
}

void UMenu::NativeDestruct()
{
	MenuTearDown();

	Super::NativeDestruct();
}

void UMenu::HostButtonClicked()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 15.0f,
			FColor::MakeRandomColor(),TEXT("Host button clicked")
		);
	}
	HostButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);
	}
}

void UMenu::JoinButtonClicked()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 15.0f,
			FColor::MakeRandomColor(),TEXT("Join button clicked")
		);
	}
	JoinButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->FindSessions(10000); // A large number since we're using a steam popular devID
	}
}

void UMenu::OnOnlineSubsystemNotAvailable()
{
	if (OnlineSubsystemWarningText)
	{
		OnlineSubsystemWarningText->SetText(
			FText::FromString(TEXT("No online subsystem available. Make sure Steam or Epic Games is running."))
			);
		OnlineSubsystemWarningText->SetVisibility(ESlateVisibility::Visible);
	}
	if (HostButton) HostButton->SetIsEnabled(true);
	if (JoinButton) JoinButton->SetIsEnabled(true);
}

void UMenu::MenuTearDown()
{
	RemoveFromParent();
	if (UWorld* const World = GetWorld())
	{
		if (APlayerController* const PlayerController = World->GetFirstPlayerController())
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}

void UMenu::BindMultiplayerSessionsSubsystemCallbacks()
{
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnOnlineSubsystemNotAvailable.AddDynamic(this, &ThisClass::OnOnlineSubsystemNotAvailable);
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.
									  AddDynamic(this, &ThisClass::OnDestroySession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
	}
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 15.0f,
				FColor::MakeRandomColor(),TEXT("Session created successfully")
			);
		}

		if (UWorld* const World = GetWorld())
		{
			World->ServerTravel(PathToLobby);
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 15.0f,
				FColor::Red,TEXT("Failed to create session")
			);
		}
		HostButton->SetIsEnabled(true);
	}
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (!MultiplayerSessionsSubsystem)
	{
		return;
	}

	for (auto Result : SessionResults)
	{
		FString SettingsValue;
		Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		if (SettingsValue == MatchType)
		{
			MultiplayerSessionsSubsystem->JoinSession(Result);
			return;
		}
	}

	// Enable the Join button again
	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	// Get session interface from online subsystem
	if (const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString Address;
			SessionInterface->GetResolvedConnectString(NAME_GameSession, Address);

			if (const UGameInstance* GameInstance = GetGameInstance())
			{
				if (APlayerController* PlayerController = GameInstance->GetFirstLocalPlayerController())
				{
					PlayerController->ClientTravel(Address, TRAVEL_Absolute);
				}
			}
		}
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		JoinButton->SetIsEnabled(true);
	}
}

	
void UMenu::OnDestroySession(bool bWasSuccessful)
{
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
}
