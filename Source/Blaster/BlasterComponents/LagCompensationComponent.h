// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/RingBuffer.h"
#include "LagCompensationComponent.generated.h"

USTRUCT(BlueprintType)
struct FCapsuleInformation
{
	GENERATED_BODY()
	
	UPROPERTY()
	float CapsuleHalfHeight;
	
	UPROPERTY()
	float Radius;
	
	UPROPERTY()
	FVector Location;
	
	UPROPERTY()
	FRotator Rotation;
	
	// UPROPERTY()
	// FName BoneName?
};

USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()
	
	UPROPERTY()
	float Time;
	
	UPROPERTY()
	TMap<FName, FCapsuleInformation> HitCapsuleInfo;
};

USTRUCT(BlueprintType)
struct FServerSideRewindResult
{
	GENERATED_BODY()
	
	UPROPERTY()
	bool bHitConfirmed;
	
	UPROPERTY()
	bool bHeadShot;
};

class ABlasterCharacter;
class ABlasterPlayerController;
class AWeapon;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULagCompensationComponent();
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void ShowFramePackage(const FFramePackage& Package, const FColor& Color = FColor::Orange) const;
	FServerSideRewindResult ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, const float HitTime);
	
	UFUNCTION(Server, Reliable)
	void ServerScoreRequest(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, const float HitTime, AWeapon* DamageCauser);
protected:
	void SaveFramePackage();
	void SaveFramePackage(FFramePackage& Package);
	FFramePackage InterpBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, const float HitTime) const;
	FServerSideRewindResult ConfirmHit(const FFramePackage& Package, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation);
	void CacheCapsulePositions(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage) const;
	void MoveCapsules(ABlasterCharacter* HitCharacter, const FFramePackage& Package);
	void ResetHitCapsules(ABlasterCharacter* HitCharacter, const FFramePackage& Package);
	void EnableCharacterMeshCollision(const ABlasterCharacter* HitCharacter, const ECollisionEnabled::Type CollisionEnabled) const;
private:
	UPROPERTY()
	TObjectPtr<ABlasterCharacter> BlasterCharacter;
	
	UPROPERTY()
	TObjectPtr<ABlasterPlayerController> BlasterPlayerController;
	
	TRingBuffer<FFramePackage> FrameHistory; // Front is the newest package and back is the oldest.
	
	UPROPERTY(EditAnywhere, meta = (ForceUnits = "Seconds", AllowPrivateAccess = true))
	float MaxRecordTime = 4.f;
	
	// Used for checking headshot line traces.
	UPROPERTY(EditAnywhere)
	FName HeadBoneName = FName("Head");
};
