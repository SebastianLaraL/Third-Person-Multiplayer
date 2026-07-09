// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BlasterHUD.generated.h"

class UElimAnnouncement;
/*
 * A struct with all crosshair positions.
 */
USTRUCT(BlueprintType)
struct FHUDPackage
{
	GENERATED_BODY()
	// Initialize as nullptr every field explicitly.
	//FHUDPackage() : CrosshairCenter(nullptr), CrosshairLeft(nullptr), CrosshairRight(nullptr), CrosshairTop(nullptr), CrosshairBottom(nullptr)
	//{
	// }
	FHUDPackage() = default;
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> CrosshairCenter;
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> CrosshairLeft;
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> CrosshairRight;
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> CrosshairTop;
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> CrosshairBottom;
	float CrosshairSpread;
	FLinearColor CrosshairsColor;
};

class USniperScope;
class UPauseMenu;
class UCharacterOverlay;
class UAnnouncement;

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	void AddCharacterOverlay();
	
	UPROPERTY(EditAnywhere, Category="Player Stats")
	TSubclassOf<UUserWidget> CharacterOverlayClass;
	
	UPROPERTY()
	TObjectPtr<UCharacterOverlay> CharacterOverlay;

	UPROPERTY(EditAnywhere, Category="Announcement")
	TSubclassOf<UUserWidget> AnnouncementClass;
	
	UPROPERTY()
	TObjectPtr<UAnnouncement> Announcement;

	UPROPERTY(EditAnywhere, Category="Gameplay|Sniper Scope")
	TSubclassOf<UUserWidget> SniperScopeClass;
	
	UPROPERTY()
	TObjectPtr<USniperScope> SniperScope;
	
	UPROPERTY(EditDefaultsOnly, Category = Pause)
	TSubclassOf<UPauseMenu> PauseMenuClass;
	
	UPROPERTY()
	TObjectPtr<UPauseMenu> PauseMenu;
	
	UPROPERTY(EditDefaultsOnly, Category = Elim)
	TSubclassOf<UElimAnnouncement> ElimAnnouncementClass;
	
	UPROPERTY(EditDefaultsOnly, Category = Elim)
	float ElimAnnouncementTime = 3.5f;
	
	UPROPERTY()
	TArray<TObjectPtr<UElimAnnouncement>> ElimMessages;
	
	void AddAnnouncement();
	void AddElimAnnouncement(const FString& Attacker, const FString& Victim);
	void AddSniperScope();
	
protected:
	virtual void BeginPlay() override;
	
private:
	UPROPERTY()
	FHUDPackage HUDPackage;
	void DrawCrosshair(UTexture2D* Texture, const FVector2D& ViewportCenter, const FVector2D& Spread, const FLinearColor& CrosshairColor);
	
	UPROPERTY(EditAnywhere)
	float CrosshairSpreadMax = 16.f;
public:
	FORCEINLINE void SetHudPackage(const FHUDPackage& HudPackage) { HUDPackage = HudPackage; }
};
