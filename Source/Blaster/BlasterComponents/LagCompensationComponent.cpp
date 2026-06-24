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
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// Save Frame packages history.
	FFramePackage ThisFramePackage;
	
	if (FrameHistory.Num() <= 1)
	{
		SaveFramePackage(ThisFramePackage);
		FrameHistory.AddFront(ThisFramePackage);
	}
	else
	{
		float HistoryLength = FrameHistory.First().Time - FrameHistory.Last().Time;
		while (HistoryLength > MaxRecordTime)
		{
			FrameHistory.Pop();
			HistoryLength = FrameHistory.First().Time - FrameHistory.Last().Time;
		}
		SaveFramePackage(ThisFramePackage);
		FrameHistory.AddFront(ThisFramePackage);

		//ShowFramePackage(ThisFramePackage, FColor::Red); Debug only.
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

FFramePackage ULagCompensationComponent::InterpBetweenFrames(const FFramePackage& OlderFrame,
	const FFramePackage& YoungerFrame, const float HitTime) const
{
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	const float InterpFraction = FMath::Clamp((HitTime - OlderFrame.Time) / Distance, 0.f, 1.f);

	FFramePackage InterpFramePackage;
	InterpFramePackage.Time = HitTime;
	
	for (const auto& YoungerPair : YoungerFrame.HitCapsuleInfo)
	{
		const FName& CapsuleInfoName = YoungerPair.Key;

		const FCapsuleInformation& OlderCapsule = OlderFrame.HitCapsuleInfo[CapsuleInfoName];
		const FCapsuleInformation& YoungerCapsule = YoungerFrame.HitCapsuleInfo[CapsuleInfoName];

		FCapsuleInformation InterpCapsuleInfo;

		// TODO: use a delta time in the next values.
		InterpCapsuleInfo.Location = FMath::VInterpTo(OlderCapsule.Location, YoungerCapsule.Location, 1.f, InterpFraction);
		InterpCapsuleInfo.Rotation = FMath::RInterpTo(OlderCapsule.Rotation, YoungerCapsule.Rotation, 1.f, InterpFraction);
		
		InterpCapsuleInfo.Radius = FMath::FInterpTo(OlderCapsule.Radius, YoungerCapsule.Radius, 1.f, InterpFraction); 
		InterpCapsuleInfo.CapsuleHalfHeight = FMath::FInterpTo(OlderCapsule.CapsuleHalfHeight, YoungerCapsule.CapsuleHalfHeight, 1.f, InterpFraction);

		InterpFramePackage.HitCapsuleInfo.Add(CapsuleInfoName, InterpCapsuleInfo);
	}

	return InterpFramePackage;
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

void ULagCompensationComponent::ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation, const float HitTime)
{
	if (!HitCharacter || !HitCharacter->GetLagCompensationComponent())
	{
		return;
	}
	
	const TRingBuffer<FFramePackage>& History = HitCharacter->GetLagCompensationComponent()->FrameHistory;
	if (History.Num() == 0)
	{
		return;
	}
	
	const float NewestHistoryTime = History.First().Time;
	const float OldestHistoryTime = History.Last().Time;
	
	// Too old, too much lag to make Server side rewind.
	if (OldestHistoryTime > HitTime)
	{
		return;
	}

	FFramePackage FrameToCheck;

	// Border case exact match with the oldest.
	if (OldestHistoryTime == HitTime) [[unlikely]]
	{
		FrameToCheck = History.Last();
	}
	// Border case: newer than latest frame.
	else if (NewestHistoryTime <= HitTime)
	{
		FrameToCheck = History.First();
	}
	// We need to browse the list and interpolate.
	else
	{
		int32 OlderIndex = 0; // Start at newest (First).
		while (History[OlderIndex].Time > HitTime)
		{
			++OlderIndex;
		}
		
		const int32 YoungerIndex = OlderIndex - 1; // The previous in the browsing is the newest.
		
		// Unlikely border case: exact match in this point.
		if (History[OlderIndex].Time == HitTime) [[unlikely]]
		{
			FrameToCheck = History[OlderIndex];
		}
		else
		{
			// TODO: interpolation.
		}
	}
}
