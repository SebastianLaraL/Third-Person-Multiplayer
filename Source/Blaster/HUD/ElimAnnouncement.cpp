// Sebastian Lara. All rights reserved.


#include "ElimAnnouncement.h"

#include "Components/TextBlock.h"

void UElimAnnouncement::SetElimAnnouncementText(const FString& AttackerName, const FString& VictimName) const
{
	if (!AnnouncementText) return;
	
	const FString ElimAnnouncementText = FString::Printf(TEXT("%s elimmed %s!"), *AttackerName, *VictimName);
	AnnouncementText->SetText(FText::FromString(ElimAnnouncementText));
}
