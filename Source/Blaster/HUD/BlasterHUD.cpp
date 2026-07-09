// Sebastian Lara. All rights reserved.


#include "BlasterHUD.h"

#include "Announcement.h"
#include "GameFramework/PlayerController.h"
#include "CharacterOverlay.h"
#include "ElimAnnouncement.h"
#include "SniperScope.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"

void ABlasterHUD::DrawHUD()
{
	Super::DrawHUD();
	if (GEngine)
	{
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

		const float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;

		if (HUDPackage.CrosshairCenter)
		{
			DrawCrosshair(HUDPackage.CrosshairCenter, ViewportCenter, FVector2D::ZeroVector, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairTop)
		{
			DrawCrosshair(HUDPackage.CrosshairTop, ViewportCenter, FVector2D(0.f, -SpreadScaled), HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairBottom)
		{
			DrawCrosshair(HUDPackage.CrosshairBottom, ViewportCenter, FVector2D(0.f, SpreadScaled), HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairLeft)
		{
			DrawCrosshair(HUDPackage.CrosshairLeft, ViewportCenter, FVector2D(-SpreadScaled, 0.f), HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairRight)
		{
			DrawCrosshair(HUDPackage.CrosshairRight, ViewportCenter, FVector2D(SpreadScaled, 0.f), HUDPackage.CrosshairsColor);
		}
	}
}

// Create and add to viewport.
void ABlasterHUD::AddAnnouncement()
{
	const auto PlayerController = GetOwningPlayerController();
	if (PlayerController && AnnouncementClass)
	{
		Announcement = CreateWidget<UAnnouncement>(PlayerController, AnnouncementClass);
		Announcement->AddToViewport();
	}
}

void ABlasterHUD::AddElimAnnouncement(const FString& Attacker, const FString& Victim)
{
	const auto OwningPC = GetOwningPlayerController();
	if (OwningPC && ElimAnnouncementClass)
	{
		if (UElimAnnouncement* ElimAnnouncementWidget = CreateWidget<UElimAnnouncement>(OwningPC,ElimAnnouncementClass))
		{
			ElimAnnouncementWidget->SetElimAnnouncementText(Attacker, Victim);
			ElimAnnouncementWidget->AddToViewport();
			
			// Stack messages (old on the top) using the horizontal box height.
			for (const UElimAnnouncement* Msg : ElimMessages)
			{
				if (!Msg || !Msg->AnnouncementBox) continue;
				
				if (UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Msg->AnnouncementBox))
				{
					const FVector2D Position = CanvasSlot->GetPosition();
					const FVector2D NewPosition(CanvasSlot->GetPosition().X, Position.Y - CanvasSlot->GetSize().Y);
					CanvasSlot->SetPosition(NewPosition);
				}
			}
			
			ElimMessages.Add(ElimAnnouncementWidget);
			
			
			FTimerHandle ElimMsgTimer;
			const FTimerDelegate ElimMsgDelegate = FTimerDelegate::CreateWeakLambda(this,
				[this, ElimAnnouncementWidget]()
				{
					if (ElimAnnouncementWidget)
					{
						ElimMessages.Remove(ElimAnnouncementWidget);
						ElimAnnouncementWidget->RemoveFromParent();
					}
				}
			);
			GetWorldTimerManager().SetTimer(ElimMsgTimer, ElimMsgDelegate, ElimAnnouncementTime, false);
		}
	}
}

// Create and add to viewport.
void ABlasterHUD::AddSniperScope()
{
	const auto PlayerController = GetOwningPlayerController();
	if (PlayerController && SniperScopeClass)
	{
		SniperScope = CreateWidget<USniperScope>(PlayerController, SniperScopeClass);
		SniperScope->AddToViewport();
		SniperScope->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ABlasterHUD::BeginPlay()
{
	Super::BeginPlay();

	const auto PlayerController = GetOwningPlayerController();
	if (!PlayerController) return;
	
	// Note we are not adding it to viewport.
	if (CharacterOverlayClass)
	{
		CharacterOverlay = CreateWidget<UCharacterOverlay>(PlayerController, CharacterOverlayClass);
	}
}

void ABlasterHUD::AddCharacterOverlay()
{
	if (CharacterOverlay)
	{
		CharacterOverlay->AddToViewport();
	}
}

void ABlasterHUD::DrawCrosshair(UTexture2D* Texture, const FVector2D& ViewportCenter, const FVector2D& Spread,
                                const FLinearColor& CrosshairColor)
{
	if (!IsValid(Texture)) return;
	const auto TextureWidth = Texture->GetSizeX();
	const auto TextureHeight = Texture->GetSizeY();
	const FVector2D TextureDrawPoint(
		ViewportCenter.X - (TextureWidth / 2.f) + Spread.X,
		ViewportCenter.Y - (TextureHeight / 2.f) + Spread.Y
	);
	DrawTexture(Texture, TextureDrawPoint.X, TextureDrawPoint.Y,
		TextureWidth, TextureHeight,
		0.f, 0.f, 1.f, 1.f,
	            CrosshairColor);
}
