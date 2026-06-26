// Sebastian Lara. All rights reserved.


#include "LagCompensationComponent.h"

#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Blaster/Utils/DebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/Weapon/Weapon.h"

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
	
	SaveFramePackage();
}


FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation, const float HitTime)
{
	constexpr FServerSideRewindResult EmptyResult{};
	if (!HitCharacter || !HitCharacter->GetLagCompensationComponent())
	{
		UE_LOG(LogTemp, Error, TEXT("ServerSideRewind: HitCharacter or LagCompensationComponent invalid"));
		return EmptyResult;
	}
    
	bool bShouldInterpolate = true;
	const TRingBuffer<FFramePackage>& History = HitCharacter->GetLagCompensationComponent()->FrameHistory;
	if (History.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerSideRewind: History is empty"));
		return EmptyResult;
	}
    
	const float NewestHistoryTime = History.First().Time;
	const float OldestHistoryTime = History.Last().Time;
    
	UE_LOG(LogTemp, Warning, TEXT("HitTime: %f | NewestHistoryTime: %f | OldestHistoryTime: %f | History.Num(): %d"), HitTime, NewestHistoryTime, OldestHistoryTime, History.Num());
    
	if (OldestHistoryTime > HitTime)
	{
		UE_LOG(LogTemp, Error, TEXT("ServerSideRewind: Too old, too laggy"));
		return EmptyResult;
	}

	FFramePackage FrameToCheck;

	// Border case exact match with the oldest.
	if (OldestHistoryTime == HitTime) [[unlikely]]
	{
		FrameToCheck = History.Last();
		// bShouldInterpolate = false;
	}
	// Border case: newer than latest frame.
	else if (NewestHistoryTime <= HitTime)
	{
		FrameToCheck = History.First();
		// bShouldInterpolate = false;
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
			bShouldInterpolate = false;
		}
		//else
		//{
			if (bShouldInterpolate)
			{
				// Interpolate between older and younger.
				FrameToCheck = InterpBetweenFrames(History[OlderIndex], History[YoungerIndex], HitTime);
			}
		//}
	}
	UE_LOG(LogTemp, Warning, TEXT("FrameToCheck.Time: %f"), FrameToCheck.Time);

	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

void ULagCompensationComponent::ServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, const float HitTime,
	AWeapon* DamageCauser)
{
	UE_LOG(LogTemp, Warning, TEXT("ServerScoreRequest called. HitTime: %f"), HitTime);

	const FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart, HitLocation, HitTime);
	
	UE_LOG(LogTemp, Warning, TEXT("Confirm.bHitConfirmed: %d, Confirm.bHeadShot: %d"), Confirm.bHitConfirmed, Confirm.bHeadShot);

	
	if (BlasterCharacter && HitCharacter && DamageCauser && Confirm.bHitConfirmed)
	{
		UGameplayStatics::ApplyDamage(HitCharacter, DamageCauser->GetDamage(), BlasterCharacter->Controller, DamageCauser, UDamageType::StaticClass());
		DRAW_DEBUG_CAPSULE(GetWorld(), HitLocation, 0.5f, 0.5f, FQuat::Identity, FColor::Red, true, -1, 0, 1);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, TEXT("Applied damage from lag compensation"));
	}
		
}

void ULagCompensationComponent::SaveFramePackage()
{
	if (!BlasterCharacter || !BlasterCharacter->HasAuthority()) return;
	
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

FServerSideRewindResult ULagCompensationComponent::ConfirmHit(const FFramePackage& Package,
	ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	if (!HitCharacter) return FServerSideRewindResult{};

	UE_LOG(LogTemp, Warning, TEXT("ConfirmHit: TraceStart: %s | HitLocation: %s"), *TraceStart.ToString(), *HitLocation.ToString());

	FFramePackage CurrentFrame;
	CacheCapsulePositions(HitCharacter, CurrentFrame);
	MoveCapsules(HitCharacter, Package);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	// Find head capsule by bone name.
	UCapsuleComponent* HeadCapsule = nullptr;
	for (UCapsuleComponent* Capsule : HitCharacter->HitCollisionCapsules)
	{
		if (Capsule && Capsule->GetFName() == HeadBoneName)
		{
			HeadCapsule = Capsule;
			break;
		}
	}
	if (!HeadCapsule) return FServerSideRewindResult{};	

	// Enable collision for the head first
	HeadCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadCapsule->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f; // Increase range by 25%.
	if (const UWorld* World = GetWorld())
	{
		FHitResult ConfirmHitResult;
		
		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceEnd,
			ECollisionChannel::ECC_Visibility
		);
		UE_LOG(LogTemp, Warning, TEXT("ConfirmHitResult.bBlockingHit (head): %d"), ConfirmHitResult.bBlockingHit);

		if (ConfirmHitResult.bBlockingHit) // we hit the head, return early
		{
			ResetHitCapsules(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryOnly);
			return FServerSideRewindResult{ true, true };
		}
		
		// Didn't hit head, check the rest of the boxes
		
		for (auto& HitBoxPair : HitCharacter->HitCollisionCapsules)
		{
			if (HitBoxPair)
			{
				HitBoxPair->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				HitBoxPair->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
			}
		}
		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceEnd,
			ECollisionChannel::ECC_Visibility
		);
		UE_LOG(LogTemp, Warning, TEXT("ConfirmHitResult.bBlockingHit (head): %d"), ConfirmHitResult.bBlockingHit);

		if (ConfirmHitResult.bBlockingHit)
		{
			ResetHitCapsules(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryOnly);
			return FServerSideRewindResult{ true, false };
		}
	}

	ResetHitCapsules(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics); // TODO: test with query only.
	return FServerSideRewindResult{ false, false };
}

void ULagCompensationComponent::CacheCapsulePositions(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage) const
{
	if (!HitCharacter) return;
	for (const auto& HitCapsule : HitCharacter->HitCollisionCapsules)
	{
		if (HitCapsule)
		{
			FCapsuleInformation CapsuleInfo;
			CapsuleInfo.Location = HitCapsule->GetComponentLocation();
			CapsuleInfo.Rotation = HitCapsule->GetComponentRotation();
			CapsuleInfo.Radius = HitCapsule->GetScaledCapsuleRadius();
			CapsuleInfo.CapsuleHalfHeight = HitCapsule->GetScaledCapsuleHalfHeight();
			OutFramePackage.HitCapsuleInfo.Add(HitCapsule->GetFName(), CapsuleInfo);
		}
	}
}

void ULagCompensationComponent::MoveCapsules(ABlasterCharacter* HitCharacter, const FFramePackage& Package)
{
	if (!HitCharacter) return;
	
	for (auto& HitCapsule : HitCharacter->HitCollisionCapsules)
	{
		if (HitCapsule)
		{
			HitCapsule->SetWorldLocationAndRotation(Package.HitCapsuleInfo[HitCapsule->GetFName()].Location, Package.HitCapsuleInfo[HitCapsule->GetFName()].Rotation);
			HitCapsule->SetCapsuleRadius(Package.HitCapsuleInfo[HitCapsule->GetFName()].Radius);
			HitCapsule->SetCapsuleHalfHeight(Package.HitCapsuleInfo[HitCapsule->GetFName()].CapsuleHalfHeight);
		}
	}
}

void ULagCompensationComponent::ResetHitCapsules(ABlasterCharacter* HitCharacter, const FFramePackage& Package)
{
	if (!HitCharacter) return;
	
	for (auto& HitCapsule : HitCharacter->HitCollisionCapsules)
	{
		if (HitCapsule)
		{
			HitCapsule->SetWorldLocationAndRotation(Package.HitCapsuleInfo[HitCapsule->GetFName()].Location, Package.HitCapsuleInfo[HitCapsule->GetFName()].Rotation);
			HitCapsule->SetCapsuleRadius(Package.HitCapsuleInfo[HitCapsule->GetFName()].Radius);
			HitCapsule->SetCapsuleHalfHeight(Package.HitCapsuleInfo[HitCapsule->GetFName()].CapsuleHalfHeight);
			HitCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ULagCompensationComponent::EnableCharacterMeshCollision(const ABlasterCharacter* HitCharacter,
	const ECollisionEnabled::Type CollisionEnabled) const
{
	if (HitCharacter && HitCharacter->GetMesh())
	{
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
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
