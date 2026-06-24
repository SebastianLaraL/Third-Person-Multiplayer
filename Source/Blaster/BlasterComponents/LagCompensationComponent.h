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

class ABlasterCharacter;
class ABlasterPlayerController;

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
protected:
	void SaveFramePackage(FFramePackage& Package);
private:
	UPROPERTY()
	TObjectPtr<ABlasterCharacter> BlasterCharacter;
	
	UPROPERTY()
	TObjectPtr<ABlasterPlayerController> BlasterPlayerController;
	
	TRingBuffer<FFramePackage> FrameHistory; // Front is the newest package and back is the oldest.
	
	UPROPERTY(EditAnywhere, meta = (ForceUnits = "Seconds", AllowPrivateAccess = true))
	float MaxRecordTime = 4.f;
};
