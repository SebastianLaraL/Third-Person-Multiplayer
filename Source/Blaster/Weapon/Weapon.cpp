// Sebastian Lara. All rights reserved.

#include "Weapon.h"

#include "Casing.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimationAsset.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundCue.h"

AWeapon::AWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	AActor::SetReplicateMovement(true); // To prevent area sphere issues on clients.

	// Mesh. Block everything but pawns. No collision.
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Weapon Mesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionResponseToAllChannels(ECR_Block);
	WeaponMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_PURPLE);
	WeaponMesh->MarkRenderStateDirty();
	EnableCustomDepth(true);

	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Area Sphere"));
	AreaSphere->SetupAttachment(RootComponent);
	AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Pickup widget.
	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("Pickup Widget Component"));
	PickupWidget->SetupAttachment(RootComponent);
	PickupWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PickupWidget->SetDrawAtDesiredSize(true);
}


void AWeapon::EnableCustomDepth(const bool bEnable)
{
	if (WeaponMesh)
	{
		WeaponMesh->SetRenderCustomDepth(bEnable);
	}
}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	// Hide the pickup widget at start.
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(false);
	}

	// If on server.
	if (HasAuthority())
	{
		// Enable collision.
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		AreaSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		// Bind BeginOverlap.
		AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnSphereBeginOverlap);
		// Bind EndOverlap.
		AreaSphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::OnSphereEndOverlap);
	}
}


void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Set Weapon State to replicate.
	DOREPLIFETIME(AWeapon, WeaponState);
	// Current ammo.
	DOREPLIFETIME(AWeapon, Ammo)
}

void AWeapon::OnRep_Ammo()
{
	BlasterOwnerCharacter = BlasterOwnerCharacter ? BlasterOwnerCharacter.Get() : Cast<ABlasterCharacter>(GetOwner());
	// Stop reloading when shotgun ammo is full.
	if (BlasterOwnerCharacter && BlasterOwnerCharacter->GetCombatComponent() && IsFull())
	{
		BlasterOwnerCharacter->GetCombatComponent()->JumpToShotgunEnd();
	}
	SetHUDAmmo();
}

void AWeapon::SpendRound()
{
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	SetHUDAmmo();
}

void AWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	// Every time this weapon is picked up by a player we update the hud.
	// Update the hud when this weapon is picked up by a player, and it is not picked up as a secondary weapon. There was an issue in which clients picked up a secondary weapon, causing the HUD ammo to display the ammo of the secondary weapon instead of equipped weapon's ammo.
	if (Owner && WeaponState != EWeaponState::EWS_EquippedSecondary)
	{
		SetHUDAmmo();
	}
	// or if it is dropped we clean references.
	else
	{
		BlasterOwnerCharacter = nullptr;
		BlasterOwnerController = nullptr;
	}
}

void AWeapon::SetHUDAmmo()
{
	BlasterOwnerCharacter = BlasterOwnerCharacter ? BlasterOwnerCharacter.Get() : Cast<ABlasterCharacter>(GetOwner());
	if (!BlasterOwnerCharacter)
	{
		return;
	}
	
	BlasterOwnerController = BlasterOwnerController ? BlasterOwnerController.Get() : Cast<ABlasterPlayerController>(BlasterOwnerCharacter->Controller);
	if (BlasterOwnerController)
	{
		BlasterOwnerController->SetHUDWeaponAmmo(Ammo);
	}
}

void AWeapon::ShowPickupWidget(bool bShowWidget)
{
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void AWeapon::Fire(const FVector& HitTarget)
{
	// Sounds and effects included in animations.
	if (FireAnimation && WeaponMesh)
	{
		WeaponMesh->PlayAnimation(FireAnimation, false);
	}
	// If weapon does not have a firing animation, play sound and particles.
	else if (!bImplementsFiringAnimations)
	{
		if (FiringParticle)
		{
			USkeletalMeshSocket const* MuzzleFlashSocket = WeaponMesh->GetSocketByName(MuzzleSocketName);
			const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(WeaponMesh);
			UGameplayStatics::SpawnEmitterAtLocation(this, FiringParticle, SocketTransform.GetLocation(), // In case of crash here, the socket was not found.
			                                         SocketTransform.Rotator());
		}
		if (FiringCue)
		{
			UGameplayStatics::SpawnSoundAttached(FiringCue,WeaponMesh);
		}
	}
	if (CasingClass)
	{
		USkeletalMeshSocket const* AmmoEjectSocket = WeaponMesh->GetSocketByName(FName("AmmoEject"));
		const FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh); // In case of crash here, the socket was not found.
		if (UWorld* World = GetWorld())
		{
			World->SpawnActor<ACasing>
			(CasingClass, SocketTransform.GetLocation(),
				SocketTransform.GetRotation().Rotator());
		}
	}
	if (HasAuthority())
	{
		SpendRound();
	}
}

void AWeapon::Drop()
{
	SetWeaponState(EWeaponState::EWS_Dropped);
	
	const FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld,true);
	WeaponMesh->DetachFromComponent(DetachRules);
	SetOwner(nullptr);
	// Clean references since this weapon is no longer being use by any player.
	BlasterOwnerCharacter = nullptr;
	BlasterOwnerController = nullptr;
}

void AWeapon::AddAmmo(const int32 AmmoToAdd)
{
	Ammo = FMath::Clamp(Ammo + AmmoToAdd,0, MagCapacity);
	SetHUDAmmo();
}

void AWeapon::SetWeaponState(EWeaponState NewState)
{
	WeaponState = NewState;
	OnWeaponStateSet();
}

void AWeapon::OnWeaponStateSet()
{
	switch (WeaponState)
	{
	case EWeaponState::EWS_Equipped:					OnEquipped();	break;
	case EWeaponState::EWS_EquippedSecondary:	 OnEquippedSecondary(); break;
	case EWeaponState::EWS_Dropped:						OnDropped();	break;
	default:															break;
	}
}

void AWeapon::OnEquipped()
{
	// Hide pickup widget and disable collision.
	ShowPickupWidget(false);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Disable physics, gravity and collision (mesh).
	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetEnableGravity(false);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (WeaponType == EWeaponType::EWT_SubmachineGun) // Enable strap physics.
	{
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
		WeaponMesh->SetEnableGravity(true);
		WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
	EnableCustomDepth(false);
}

void AWeapon::OnEquippedSecondary()
{
	ShowPickupWidget(false);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	if (!WeaponMesh) return;
	
	// Disable physics, gravity and collision (mesh).
	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetEnableGravity(false);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (WeaponType == EWeaponType::EWT_SubmachineGun) // Enable strap physics.
	{
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
		WeaponMesh->SetEnableGravity(true);
		WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
	EnableCustomDepth(true);
	WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_TAN); // Secondary weapon color will be tan.
	WeaponMesh->MarkRenderStateDirty();
}

void AWeapon::OnDropped()
{
	if (HasAuthority())
	{
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	// Enable physics, gravity and collision (mesh).
	WeaponMesh->SetSimulatePhysics(true);
	WeaponMesh->SetEnableGravity(true);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_PURPLE);
	WeaponMesh->MarkRenderStateDirty();
	EnableCustomDepth(true);
}

void AWeapon::OnRep_WeaponState()
{
	OnWeaponStateSet();
}

void AWeapon::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool FromSweep,
                                   const FHitResult& SweepResult)
{
	ABlasterCharacter* const BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter && PickupWidget)
	{
		BlasterCharacter->SetOverlappingWeapon(this);
	}
}

void AWeapon::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ABlasterCharacter* const BlasterCharacter = Cast<ABlasterCharacter>(OtherActor))
	{
		BlasterCharacter->SetOverlappingWeapon(nullptr);
	}
}
