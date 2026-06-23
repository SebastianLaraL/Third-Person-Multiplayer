// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

class ABlasterPlayerController;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULagCompensationComponent();
	friend class ABlasterCharacter;
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	UPROPERTY()
	TObjectPtr<ABlasterCharacter> BlasterCharacter;
	
	UPROPERTY()
	TObjectPtr<ABlasterPlayerController> BlasterPlayerController;
};
