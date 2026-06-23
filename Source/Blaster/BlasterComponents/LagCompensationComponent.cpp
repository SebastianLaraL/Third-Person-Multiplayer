// Sebastian Lara. All rights reserved.


#include "LagCompensationComponent.h"

#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"

// Sets default values for this component's properties
ULagCompensationComponent::ULagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
}

void ULagCompensationComponent::InitializeComponent()
{
	Super::InitializeComponent();
	
	BlasterCharacter = Cast<ABlasterCharacter>(GetOwner());
}


void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (BlasterCharacter)
	{
		BlasterPlayerController = Cast<ABlasterPlayerController>(BlasterCharacter->GetController());
	}
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	//
}

