// Sebastian Lara. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "WeaponTypes.h"
#include "GameFramework/Actor.h"
#include "Weapon.generated.h"

class UParticleSystem;
class ABlasterPlayerController;
class ABlasterCharacter;
class ACasing;
class UWidgetComponent;
class USphereComponent;
class USkeletalMeshComponent;
class UAnimationAsset;
class USoundCue;

UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	EWS_Initial UMETA(DisplayName = "Initial State"),
	EWS_Equipped UMETA(DisplayName = "Equipped"),
	EWS_Dropped UMETA(DisplayName = "Dropped"),

	EWS_MAX UMETA(DisplayName = "DefaultMax")
};

/*
 * Base Weapon class.
 * Custom depth is enabled by default.
 * If you do not see the custom depth effect, go to Project Settings, Engine - Rendering
 * Custom Depth-Stencil Pass and select Enabled with Stencil.
 * 
 * Known errors:
 * If you set up a weapon and set bImplementsFiringAnimations to false, but assign a firing animation asset, the weapon will rotate a bit when firing (only one rotation), just remove the animation asset and it should not happen again.
 */
UCLASS(Abstract, Blueprintable)
class BLASTER_API AWeapon : public AActor
{
	GENERATED_BODY()

public:
	AWeapon();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	void SetHUDAmmo();
	void ShowPickupWidget(bool bShowWidget);
	/* Play Fire animation and spawn casings. */
	virtual void Fire(const FVector& HitTarget);
	
	/** Mark as dropped and enable physics, gravity and collision. **/
	void Drop();

	void SetWeaponState(EWeaponState NewState);
	
	void AddAmmo(const int32 AmmoToAdd);
	
	/*
	* Textures for the weapon crosshairs.
	*/
	UPROPERTY(EditDefaultsOnly, Category=Crosshairs)
	TObjectPtr<UTexture2D> CrosshairsCenter;
	
	UPROPERTY(EditDefaultsOnly, Category=Crosshairs)
	TObjectPtr<UTexture2D> CrosshairsLeft;
	
	UPROPERTY(EditDefaultsOnly, Category=Crosshairs)
	TObjectPtr<UTexture2D> CrosshairsRight;
	
	UPROPERTY(EditDefaultsOnly, Category=Crosshairs)
	TObjectPtr<UTexture2D> CrosshairsTop;
	
	UPROPERTY(EditDefaultsOnly, Category=Crosshairs)
	TObjectPtr<UTexture2D> CrosshairsBottom;

	/*
	 * Zoom FOV while aiming. 
	 */
	UPROPERTY(EditAnywhere)
	float ZoomedFOV = 30.f;
	
	/*
	 Zoom in interpolation speed.
	 */
	UPROPERTY(EditAnywhere)
	float ZoomInterpSpeed = 20.f;
	
	/*
	 * Automatic Fire.
	 */
	UPROPERTY(EditAnywhere, Category = Combat, meta = (ClampMin = 0.0001))
	float FireDelay = 0.15f;
	
	UPROPERTY(EditAnywhere, Category = Combat)
	bool bAutomatic = true;
	
	UPROPERTY(EditAnywhere, Category = Reload)
	bool bAutomaticReload = true;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundCue> EquipSound;
	
	void EnableCustomDepth(const bool bEnable);
	
	// Set to true when spawning if this is the default spawn weapon.
	bool bDestroyWeapon = false;
	
protected:
	virtual void BeginPlay() override;
	virtual void OnRep_Owner() override;

	UFUNCTION()
	virtual void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool FromSweep,
	                                  const FHitResult& SweepResult);
	UFUNCTION()
	virtual void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	
	/*
	 * Here is where the projectile will be spawned in case of ProjectileWeapon or where
	 * the line trace will start in a HitScanWeapon.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	FName MuzzleSocketName = FName("MuzzleFlash");

private:
	/* Mesh */
	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/* Area Sphere */
	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	TObjectPtr<USphereComponent> AreaSphere;

	/* Weapon State */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_WeaponState, Category = "Weapon Properties")
	EWeaponState WeaponState;

	/* Pickup widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Properties", meta = (AllowPrivateAccess = true))
	TObjectPtr<UWidgetComponent> PickupWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon Properties")
	TObjectPtr<UAnimationAsset> FireAnimation;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<ACasing> CasingClass;
	
	/* Implements Firing animations. If it is set to false then fire SFX will be caused by code instead by animation and anim notifies. */
	UPROPERTY(EditDefaultsOnly, meta = (AllowPrivateAccess = true))
	bool bImplementsFiringAnimations = true;
	
	// This project uses legacy particle assets, if you need niagara you can replace this.
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UParticleSystem> FiringParticle;
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<USoundCue> FiringCue;
	
	/* Ammo. */
	
	
	// Current ammo amount.
	UPROPERTY(ReplicatedUsing = OnRep_Ammo, EditAnywhere, Category = Ammo)
	int32 Ammo = 1;
	
	UPROPERTY(EditAnywhere, Category = Ammo, meta = (ClampMin = 1))
	int32 MagCapacity;
	
	// UPROPERTY()
	TObjectPtr<ABlasterCharacter> BlasterOwnerCharacter;
	
	// UPROPERTY()
	TObjectPtr<ABlasterPlayerController> BlasterOwnerController;
	
	UPROPERTY(EditAnywhere)
	EWeaponType WeaponType;
	
	UFUNCTION()
	void OnRep_Ammo();
	
	void SpendRound();
	
	UFUNCTION()
	void OnRep_WeaponState();
	
public:
	FORCEINLINE USphereComponent* GetAreaSphere() const { return AreaSphere; }
	FORCEINLINE USkeletalMeshComponent* GetMesh() const { return WeaponMesh; }
	FORCEINLINE float GetZoomFOV() const { return ZoomedFOV; }
	FORCEINLINE float GetZoomInterpSpeed() const { return ZoomInterpSpeed; }
	// Refers to ammo amount.
	FORCEINLINE bool IsEmpty() const { return Ammo <= 0; }
	FORCEINLINE bool IsFull() const { return Ammo == MagCapacity; }
	FORCEINLINE EWeaponType GetWeaponType() const { return WeaponType; }
	FORCEINLINE int32 GetAmmo() const { return Ammo; }
	FORCEINLINE int32 GetMagCapacity() const { return MagCapacity; }
};
