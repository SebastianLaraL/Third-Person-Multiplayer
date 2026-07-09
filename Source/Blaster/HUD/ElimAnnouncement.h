// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ElimAnnouncement.generated.h"

class UTextBlock;
class UHorizontalBox;
/**
 * 
 */
UCLASS()
class BLASTER_API UElimAnnouncement : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetElimAnnouncementText(const FString& AttackerName, const FString& VictimName) const;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> AnnouncementBox;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AnnouncementText;
};
