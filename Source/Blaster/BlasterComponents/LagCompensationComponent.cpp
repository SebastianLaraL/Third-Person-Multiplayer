// Sebastian Lara. All rights reserved.


#include "LagCompensationComponent.h"

#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Blaster/Utils/DebugHelpers.h"

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
	
	FFramePackage Package;
	
	SaveFramePackage(Package);
	ShowFramePackage(Package);
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	//
	FFramePackage ThisFramePackage;
	
	if (FrameHistory.Num() <= 1)
	{
		SaveFramePackage(ThisFramePackage);
		FrameHistory.AddFront(ThisFramePackage);
	}
	else
	{
		float HistoryLength = FrameHistory.First().Time - FrameHistory.First().Time;
		while (HistoryLength > MaxRecordTime)
		{
			FrameHistory.Pop();
			HistoryLength = FrameHistory.First().Time - FrameHistory.First().Time;
		}
		SaveFramePackage(ThisFramePackage);
		FrameHistory.AddFront(ThisFramePackage);

		ShowFramePackage(ThisFramePackage, FColor::Red);
	}
}

void ULagCompensationComponent::SaveFramePackage(FFramePackage& Package)
{
	BlasterCharacter = BlasterCharacter ? BlasterCharacter.Get() : Cast<ABlasterCharacter>(GetOwner());
	if (!BlasterCharacter) return;
	
	Package.Time = GetWorld()->GetTimeSeconds();
	for (const auto& Capsule : BlasterCharacter->HitCollisionCapsules)
	{
		FCapsuleInformation CapsuleInformation;
		CapsuleInformation.Location = Capsule->GetComponentLocation();
		CapsuleInformation.Rotation = Capsule->GetComponentRotation();
		CapsuleInformation.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		CapsuleInformation.Radius = Capsule->GetScaledCapsuleRadius();
		Package.HitCapsuleInfo.Add(Capsule->GetFName(), CapsuleInformation); // Course uses a TMap approach (because it creates colliders one by one, then adds them to the map, but I do not consider that necessary with my approach.
	}
}

void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package, const FColor& Color) const
{
	for (const auto& CapsuleInfo : Package.HitCapsuleInfo)
	{
		DRAW_DEBUG_CAPSULE(
			GetWorld(),
			CapsuleInfo.Value.Location,
			CapsuleInfo.Value.CapsuleHalfHeight,
			CapsuleInfo.Value.Radius,
			FQuat(CapsuleInfo.Value.Rotation),
			Color, false, 1, 0, 0 
		);
	}
}