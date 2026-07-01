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
	
	OwnerCharacter = Cast<ABlasterCharacter>(GetOwner());
}


void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (OwnerCharacter)
	{
		BlasterPlayerController = Cast<ABlasterPlayerController>(OwnerCharacter->GetController());
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
	const FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ConfirmHit(FrameToCheck,  HitCharacter, TraceStart, HitLocation);
}

void ULagCompensationComponent::ServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, const float HitTime,
	AWeapon* DamageCauser)
{
	const FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart, HitLocation, HitTime);
	
	if (OwnerCharacter && HitCharacter && DamageCauser && Confirm.bHitConfirmed)
	{
		UGameplayStatics::ApplyDamage(HitCharacter, DamageCauser->GetDamage(), OwnerCharacter->Controller, DamageCauser, UDamageType::StaticClass());
	}
}

void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(const TArray<ABlasterCharacter*>& HitCharacters,
	const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, const float HitTime)
{
	FShotgunServerSideRewindResult Confirm = ShotgunServerSideRewind(HitCharacters, TraceStart, HitLocations, HitTime);
	
	for (const auto& HitCharacter : HitCharacters)
	{
		if (!HitCharacter || !HitCharacter->GetEquippedWeapon() || !OwnerCharacter) continue;
		float TotalDamage = 0.f;
		if (Confirm.HeadShots.Contains(HitCharacter))
		{
			const float HeadshotDamage = Confirm.HeadShots[HitCharacter] * OwnerCharacter->GetEquippedWeapon()->GetDamage();
			TotalDamage += HeadshotDamage;
		}
		if (Confirm.BodyShots.Contains(HitCharacter))
		{
			const float BodyShotDamage = Confirm.BodyShots[HitCharacter] * OwnerCharacter->GetEquippedWeapon()->GetDamage();
			TotalDamage += BodyShotDamage;
		}
		
		UGameplayStatics::ApplyDamage(HitCharacter, TotalDamage, OwnerCharacter->Controller, HitCharacter->GetEquippedWeapon(), UDamageType::StaticClass());
	}
}

void ULagCompensationComponent::SaveFramePackage()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;
	
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
	OwnerCharacter = OwnerCharacter ? OwnerCharacter.Get() : Cast<ABlasterCharacter>(GetOwner());
	if (!OwnerCharacter) return;
	
	Package.Time = GetWorld()->GetTimeSeconds();
	Package.Character = OwnerCharacter;
	for (const auto& Capsule : OwnerCharacter->HitCollisionCapsules)
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
		
		InterpCapsuleInfo.Location = FMath::Lerp(OlderCapsule.Location, YoungerCapsule.Location, InterpFraction); // FMath::VInterpTo(OlderCapsule.Location, YoungerCapsule.Location, 1.f, InterpFraction);
		InterpCapsuleInfo.Rotation = FMath::Lerp(OlderCapsule.Rotation, YoungerCapsule.Rotation, InterpFraction);// FMath::RInterpTo(OlderCapsule.Rotation, YoungerCapsule.Rotation, 1.f, InterpFraction);
		
		InterpCapsuleInfo.Radius =  FMath::Lerp(OlderCapsule.Radius, YoungerCapsule.Radius, InterpFraction);// FMath::FInterpTo(OlderCapsule.Radius, YoungerCapsule.Radius, 1.f, InterpFraction); 
		InterpCapsuleInfo.CapsuleHalfHeight = FMath::Lerp(OlderCapsule.CapsuleHalfHeight, YoungerCapsule.CapsuleHalfHeight, InterpFraction);// FMath::FInterpTo(OlderCapsule.CapsuleHalfHeight, YoungerCapsule.CapsuleHalfHeight, 1.f, InterpFraction);

		InterpFramePackage.HitCapsuleInfo.Add(CapsuleInfoName, InterpCapsuleInfo);
	}

	return InterpFramePackage;
}

FFramePackage ULagCompensationComponent::GetFrameToCheck(ABlasterCharacter* HitCharacter, const float HitTime) const
{
	FFramePackage EmptyResult{};
	if (!HitCharacter || !HitCharacter->GetLagCompensationComponent())
	{
		return EmptyResult;
	}
    
	bool bShouldInterpolate = true;
	const TRingBuffer<FFramePackage>& History = HitCharacter->GetLagCompensationComponent()->FrameHistory;
	if (History.Num() == 0)
	{
		return EmptyResult;
	}
    
	const float NewestHistoryTime = History.First().Time;
	const float OldestHistoryTime = History.Last().Time;

	// Too old, too laggy to make Server side rewind.
	if (OldestHistoryTime > HitTime)
	{
		return EmptyResult;
	}

	FFramePackage FrameToCheck{};

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
			bShouldInterpolate = false;
		}

		if (bShouldInterpolate)
		{
			// Interpolate between older and younger.
			FrameToCheck = InterpBetweenFrames(History[OlderIndex], History[YoungerIndex], HitTime);
		}
	}

	return FrameToCheck;
}

FServerSideRewindResult ULagCompensationComponent::ConfirmHit(const FFramePackage& Package,
                                                              ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	if (!HitCharacter) return FServerSideRewindResult{};

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

		if (ConfirmHitResult.bBlockingHit)
		{
			ResetHitCapsules(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryOnly);
			return FServerSideRewindResult{ true, false };
		}
	}

	ResetHitCapsules(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
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

void ULagCompensationComponent::MoveCapsules(ABlasterCharacter* HitCharacter, const FFramePackage& Package) const
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

void ULagCompensationComponent::ResetHitCapsules(ABlasterCharacter* HitCharacter, const FFramePackage& Package) const
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

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunServerSideRewind(const TArray<ABlasterCharacter*>& HitActors,
	const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, const float HitTime) const
{
	if (HitActors.IsEmpty()) return {};
	
	TArray<FFramePackage> FramesToCheck;
	FramesToCheck.Reserve(HitActors.Num());
	
	for (ABlasterCharacter* HitActor : HitActors)
	{
		FramesToCheck.Add(GetFrameToCheck(HitActor, HitTime));
	}
	return ShotgunConfirmHit(FramesToCheck, TraceStart, HitLocations);
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunConfirmHit(const TArray<FFramePackage>& FramePackages,
                                                                            const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations) const
{
	for (const auto& Frame : FramePackages)
	{
		if (!Frame.Character) return FShotgunServerSideRewindResult();
	}
	
	FShotgunServerSideRewindResult ShotgunResult;
	TArray<FFramePackage> CurrentFrames;
	CurrentFrames.Reserve(FramePackages.Num());
	
	for (const FFramePackage& Frame : FramePackages)
	{
		FFramePackage CurrentFrame;
		CurrentFrame.Character = Frame.Character;
		CacheCapsulePositions(Frame.Character, CurrentFrame);
		MoveCapsules(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::NoCollision);
		CurrentFrames.Add(CurrentFrame);
	}

	// Headshots section.
	
	// This should be fast if Head is the first item in the array.
	auto FindHeadCapsuleByBoneName = [](const TArray<UCapsuleComponent*>& CollisionCapsules, const FName& HeadBoneName) -> UCapsuleComponent*
	{
		for (UCapsuleComponent* Capsule : CollisionCapsules)
		{
			if (Capsule && Capsule->GetFName() == HeadBoneName)
			{
				return Capsule;
			}
		}
		return nullptr;
	};
	
	for (const auto& Frame : FramePackages)
	{
		// Enable collision for the head first (find head capsule by bone name).
		UCapsuleComponent* HeadCapsule = FindHeadCapsuleByBoneName(Frame.Character->HitCollisionCapsules, HeadBoneName);
		if (!HeadCapsule) continue;
		
		HeadCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		HeadCapsule->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECR_Block);
	}
	
	const auto& World = GetWorld();
	// Check for headshots.

	auto CheckShots = [TraceStart, &HitLocations](const UWorld* World, TMap<AActor*, uint32>& ShotMap)
	{
		for (const FVector_NetQuantize& HitLocation : HitLocations)
		{
			FHitResult ConfirmHitResult;
			const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
			if (!World)
				return;
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Visibility);
			
			if (auto HitActor = ConfirmHitResult.GetActor())
			{
				if (ShotMap.Contains(HitActor))
				{
					ShotMap[HitActor]++;
				}
				else
				{
					ShotMap.Emplace(HitActor, 1);
				}
			}
		}
	};
	
	CheckShots(World, ShotgunResult.HeadShots);
	
	/*
	for (auto& HitLocation : HitLocations)
	{
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());
			if (BlasterCharacter)
			{
				if (ShotgunResult.HeadShots.Contains(BlasterCharacter))
				{
					ShotgunResult.HeadShots[BlasterCharacter]++;
				}
				else
				{
					ShotgunResult.HeadShots.Emplace(BlasterCharacter, 1);
				}
			}
		}
	}
	*/

	// enable collision for all capsule, then disable for head capsule.
	for (const auto& Frame : FramePackages)
	{
		for (auto& HitCollisionCapsule : Frame.Character->HitCollisionCapsules)
		{
			if (HitCollisionCapsule)
			{
				HitCollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				HitCollisionCapsule->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
			}
		}
		
		if (UCapsuleComponent* HeadCapsule = FindHeadCapsuleByBoneName(Frame.Character->HitCollisionCapsules, HeadBoneName) ; HeadCapsule)
		{
			HeadCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// check for body shots
	
	CheckShots(World, ShotgunResult.BodyShots);
	/*
	for (auto& HitLocation : HitLocations)
	{
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());
			if (BlasterCharacter)
			{
				if (ShotgunResult.BodyShots.Contains(BlasterCharacter))
				{
					ShotgunResult.BodyShots[BlasterCharacter]++;
				}
				else
				{
					ShotgunResult.BodyShots.Emplace(BlasterCharacter, 1);
				}
			}
		}
	}
	*/

	for (const auto& Frame : CurrentFrames)
	{
		ResetHitCapsules(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::QueryAndPhysics);
	}

	return ShotgunResult;
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
