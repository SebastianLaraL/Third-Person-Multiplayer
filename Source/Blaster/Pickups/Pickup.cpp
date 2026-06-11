// Sebastian Lara. All rights reserved.


#include "Pickup.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

DEFINE_LOG_CATEGORY(LogPickup)

APickup::APickup()
{
	PrimaryActorTick.bCanEverTick = false;
	
	NewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Pickup root"));
	SetRootComponent(NewRoot);
	RootComponent->SetMobility(EComponentMobility::Type::Movable);
	
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere Comp"));
	SphereComponent->SetupAttachment(RootComponent);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
	SphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn,ECR_Overlap);
	SphereComponent->SetSphereRadius(150.f);
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pickup Mesh"));
	Mesh->SetupAttachment(SphereComponent);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetRenderCustomDepth(true);
	Mesh->SetCustomDepthStencilValue(250);
	
	bReplicates = true;
	
	PickupEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Pickup Effect Comp"));
	PickupEffectComponent->SetupAttachment(RootComponent);
}

void APickup::Destroyed()
{
	Super::Destroyed();
	
	if (!SoftPickupSound.IsNull())
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoftPickupSound.LoadSynchronous(), GetActorLocation());
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupEffect, GetActorLocation(), GetActorRotation());
}

void APickup::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority())
	{
		// Slight delay. If players overlap at the exact time of creation (APickupSpawnPoint), binding OnDestroyed will not be achieved and no more pickups will be spawned.
		GetWorldTimerManager().SetTimer(BindOverlapTimer, this, &ThisClass::BindOverlapTimerFinished, BindOverlapTime);
	}
}

void APickup::OnBeginOverlapCallback(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogPickup, Display, TEXT("Pickup Begin overlap event for item: %s"), *GetNameSafe(this))
	Destroy();
}

void APickup::BindOverlapTimerFinished()
{
	SphereComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnBeginOverlapCallback);
}
