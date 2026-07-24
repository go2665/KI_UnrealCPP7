// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ActionCharacter.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Player/ResourceComponent.h"
#include "Player/StatusComponent.h"
#include "Player/WeaponManagerComponent.h"
#include "Player/InventoryComponent.h"
#include "Weapon/WeaponActor.h"
#include "Weapon/UsedWeapon.h"
#include "Weapon/ConsumableWeapon.h"
#include "Item/Pickupable.h"
#include "Item/Pickup.h"
#include "Item/PickupWeapon.h"
#include "Item/PickupItem.h"
#include "Framework/PickupFactorySubsystem.h"
#include "NPC/Interactable.h"

// Sets default values
AActionCharacter::AActionCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 350.0f;
	SpringArm->SocketOffset = FVector(0, 0, 250);
	SpringArm->bUsePawnControlRotation = true;	// 스프링암의 회전을 컨트롤러에 맞춤

	PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PlayerCamera"));
	PlayerCamera->SetupAttachment(SpringArm);
	PlayerCamera->SetRelativeRotation(FRotator(-20.0f, 0.0f, 0.0f));

	DropLocation = CreateDefaultSubobject<USceneComponent>(TEXT("DropLocation"));
	DropLocation->SetupAttachment(RootComponent);
	DropLocation->SetRelativeLocation(FVector(80.0f, 30.0f, 0.0f));

	Resource = CreateDefaultSubobject<UResourceComponent>(TEXT("PlayerResource"));
	Status = CreateDefaultSubobject<UStatusComponent>(TEXT("PlayerStatus"));
	WeaponManager = CreateDefaultSubobject<UWeaponManagerComponent>(TEXT("WeaponManager"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));

	bUseControllerRotationYaw = false;	// 컨트롤러의 Yaw 회전 사용 안함
	GetCharacterMovement()->bOrientRotationToMovement = true;	// 이동 방향으로 캐릭터 회전
	GetCharacterMovement()->RotationRate = FRotator(0, 360, 0);	
}

// Called when the game starts or when spawned
void AActionCharacter::BeginPlay()
{
	if (Resource)
	{
		Resource->OnStaminaEmpty.AddDynamic(this, &AActionCharacter::SetWalkMode);
		if (Status)
		{
			Resource->SetMaxHealth(Status->GetMaxHealth());
			Resource->SetMaxStamina(Status->GetMaxStamina());
		}
	}

	Super::BeginPlay();	// 컴포넌트들의 BeginPlay가 실행된다.
	
	if (GetMesh())
	{
		AnimInstance = GetMesh()->GetAnimInstance();	// ABP 객체 가져오기		
	}	

	// 게임 진행 중에 자주 변경되는 값은 시작 시점에서 리셋을 해주는 것이 좋다.
	bIsSprint = false;	

	// 캐릭터에 다른 액터가 오버랩되었을 때 실행하기 위한 바인딩
	OnActorBeginOverlap.AddDynamic(this, &AActionCharacter::OnBeginOverlap);

	InteractionTargets.Empty();
}

// Called every frame
void AActionCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SpendRunStamina(DeltaTime);
	UpdateInteractionTargetOrder();
}

// Called to bind functionality to input
void AActionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (enhanced)	// 입력 컴포넌트가 향상된 입력 컴포넌트일 때
	{
		enhanced->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AActionCharacter::OnMoveInput);
		enhanced->BindActionValueLambda(IA_Sprint, ETriggerEvent::Started, 
			[this](const FInputActionValue& _) {
				SetSprintMode();
			});
		enhanced->BindActionValueLambda(IA_Sprint, ETriggerEvent::Completed,
			[this](const FInputActionValue& _) {
				SetWalkMode();
			});
		enhanced->BindAction(IA_Roll, ETriggerEvent::Triggered, this, &AActionCharacter::OnRollInput);
		enhanced->BindAction(IA_Attack, ETriggerEvent::Triggered, this, &AActionCharacter::OnAttackInput);
		enhanced->BindAction(IA_Interaction, ETriggerEvent::Triggered, this, &AActionCharacter::OnInteractionInput);
	}
}

void AActionCharacter::AddItem_Implementation(UItemDataAsset* ItemData, int32 Count)
{
	if (ItemData && Count > 0)
	{
		const UEnum* EnumPtr = StaticEnum<EItemCode>();
		UE_LOG(LogTemp, Log, TEXT("아이템 추가 : %s"), *EnumPtr->GetDisplayNameTextByValue(static_cast<int8>(ItemData->ItemCode)).ToString());
		int32 remaining = Inventory->AddItem(ItemData, Count);
		if (remaining > 0)
		{
			// 추가 실패한 양이 있다.
			APickup* pickup = GetWorld()->GetSubsystem<UPickupFactorySubsystem>()->SpawnPickup(
				ItemData->ItemCode,
				DropLocation->GetComponentLocation(),
				GetActorRotation()
			);
			FVector velocity = (GetActorForwardVector() + GetActorUpVector()) * 300.0f;
			pickup->AddImpulse(velocity);
			
			APickupItem* pickupItem = Cast<APickupItem>(pickup);
			if (pickupItem)
			{
				pickupItem->SetItemCount(remaining);
			}
		}
	}	
}

void AActionCharacter::AddWeapon_Implementation(EWeaponCode Code, int32 UseCount)
{		
	const UEnum* EnumPtr = StaticEnum<EItemCode>();
	UE_LOG(LogTemp, Log, TEXT("무기획득 : %s"), *EnumPtr->GetDisplayNameTextByValue(static_cast<int8>(Code)).ToString());
	EquipWeapon(Code);
	CurrentWeapon->OnWeaponPickuped(UseCount);
}

void AActionCharacter::AddMoney_Implementation(int32 Income)
{
	UE_LOG(LogTemp, Log, TEXT("돈 (%d) 골드를 획득했습니다."), Income);
	Inventory->AddMoney(Income);
}

void AActionCharacter::RemoveMoney_Implementation(int32 Expense)
{
	UE_LOG(LogTemp, Log, TEXT("돈 (%d) 골드를 사용했습니다."), Expense);
	Inventory->AddMoney(-Expense);
}

bool AActionCharacter::HasEnoughMoney_Implementation(int32 Amount)
{	
	return Amount <= Inventory->GetMoney();
}

void AActionCharacter::HealHealth_Implementation(float InHeal)
{
	if (Resource)
	{
		Resource->AddHealth(InHeal);
	}
}

void AActionCharacter::DamageHealth_Implementation(float InDamage)
{
	if (Resource)
	{
		Resource->AddHealth(-InDamage);
	}
}

void AActionCharacter::RecoveryStamina_Implementation(float InRecovery)
{
	if (Resource)
	{
		Resource->AddStamina(InRecovery);
	}
}

void AActionCharacter::AddInteractionTarget_Implementation(AActor* InTarget)
{
	if (InTarget->Implements<UInteractable>())
	{
		InteractionTargets.Add(InTarget);

		UpdateInteractionTargetOrder();		
	}	
}

void AActionCharacter::ClearInteractionTarget_Implementation(AActor* InTarget)
{
	InteractionTargets.RemoveSingle(InTarget);	
}

void AActionCharacter::TryInteraction_Implementation()
{
	if (!InteractionTargets.IsEmpty())
	{
		IInteractable::Execute_OnInteraction(InteractionTargets[0].Get());
	}
}

void AActionCharacter::EquipWeapon(EWeaponCode WeaponCode)
{
	if (CurrentWeapon.IsValid())
	{
		// 장비하고 있던 무기가 기본 무기가 아니면
		if (CurrentWeapon->GetWeaponID() != EWeaponCode::BasicWeapon	// 장비하고 있던 무기가 Consumable이고
			&& CurrentWeapon->GetWeaponID() != WeaponCode				// 새로 장비할 무기와 다른 종류고
			&& CurrentWeapon->CanAttack())								// 장비하고 있던 무기에 회수가 남아있는 상황이면
		{
			DropCurrentWeapon(CurrentWeapon->GetWeaponID());
		}

		// 장비하고 있던 무기는 해제
		CurrentWeapon->WeaponActivate(false);
	}

	// WeaponCode에 해당하는 무기 장비
	CurrentWeapon = WeaponManager->GetEquippedWeapon(WeaponCode);
	CurrentWeapon->WeaponActivate(true);
}

void AActionCharacter::OnAttackEnable(bool bEnable)
{
	if (CurrentWeapon.IsValid())
	{
		CurrentWeapon->AttackEnable(bEnable);
	}
}

void AActionCharacter::OnWeaponTrailEnable(bool bEnable)
{
	if (CurrentWeapon.IsValid())
	{
		CurrentWeapon->TrailEnable(bEnable);
	}
}

void AActionCharacter::OnAreaAttack()
{
	//UE_LOG(LogTemp, Log, TEXT("OnAreaAttack"));
	if (CurrentWeapon.IsValid())
	{
		//UE_LOG(LogTemp, Log, TEXT("OnAreaAttack : CurrentWeapon.IsValid()") );
		CurrentWeapon->DamageToArea();
	}	
}

void AActionCharacter::TestDropUsedWeapon()
{
	if (CurrentWeapon.IsValid())
	{
		DropWeapon(CurrentWeapon->GetWeaponID());
	}
}

void AActionCharacter::TestDropCurrentWeapon()
{
	UE_LOG(LogTemp, Log, TEXT("TestDropCurrentWeapon"));
	DropCurrentWeapon(CurrentWeapon->GetWeaponID());
}

void AActionCharacter::OnMoveInput(const FInputActionValue& InValue)
{
	FVector2D inputDirection = InValue.Get<FVector2D>();
	//UE_LOG(LogTemp, Log, TEXT("Dir : (%.1f, %.1f)"), inputDirection.X, inputDirection.Y);
	//UE_LOG(LogTemp, Log, TEXT("Dir : (%s)"), *inputDirection.ToString());
	FVector moveDirection(inputDirection.Y, inputDirection.X, 0.0f);

	//FQuat controlYawRotation = FQuat(FRotator(0, GetControlRotation().Yaw, 0));	// 컨트롤러의 Yaw회전을 따로 뽑아와서
	//moveDirection = controlYawRotation.RotateVector(moveDirection);	// 이동 방향에 적용

	// Yaw 각도를 라디안으로 변환
	float YawRad = FMath::DegreesToRadians(GetControlRotation().Yaw);

	// Z축(UpVector)을 기준으로 회전하는 쿼터니언을 직접 생성
	FQuat controlYawRotation(FVector::UpVector, YawRad);

	// 이동 방향에 적용
	moveDirection = controlYawRotation.RotateVector(moveDirection);
	
	AddMovementInput(moveDirection);
	
}

void AActionCharacter::OnRollInput(const FInputActionValue& InValue)
{
	//if (GetController()->IsMoveInputIgnored()) return;
	if (AnimInstance.IsValid() && !GetController()->IsMoveInputIgnored())
	{
		if (!AnimInstance->IsAnyMontagePlaying() 
			&& Resource->HasEnoughStamina(RollStaminaCost))	// 몽타주 재생중이 아니고 충분한 스태미너가 있을 때만 작동
		{
			//if (!GetLastMovementInputVector().IsNearlyZero())	// 입력을 하는 중에만 즉시 회전
			//{
			//	SetActorRotation(GetLastMovementInputVector().Rotation());	// 마지막 입력 방향으로 즉시 회전 시키기
			//}
			Resource->AddStamina(-RollStaminaCost);	// 스태미너 감소
			PlayAnimMontage(RollMontage);
		}
	}
}

void AActionCharacter::OnAttackInput(const FInputActionValue& InValue)
{
	if (GetController()->IsMoveInputIgnored()) return;

	// 애님 인스턴스가 있고, 스태미너도 충분하고, 현재 무기가 공격을 할 수 있어야 한다.
	if (AnimInstance.IsValid() && Resource->HasEnoughStamina(AttackStaminaCost)
		&& (CurrentWeapon.IsValid() && CurrentWeapon->CanAttack())) 
	{
		if (!AnimInstance->IsAnyMontagePlaying() )	// 몽타주가 재생 중이 아닐 때
		{
			// 첫번째 공격			
			PlayAnimMontage(AttackMontage);	// 몽타주 재생

			FOnMontageEnded onMontageEnded;
			onMontageEnded.BindUObject(this, &AActionCharacter::OnAttackMontageEnded);
			AnimInstance->Montage_SetEndDelegate(onMontageEnded);	// 몽타주가 끝났을 때 델리게이트 발송(몽타주 플레이 이후에 등록해야 함)

			Resource->AddStamina(-AttackStaminaCost);	// 스태미너 감소
			if (CurrentWeapon.IsValid())
			{
				CurrentWeapon->OnAttack();	// 무기 공격시 처리(회수 차감)
			}
		}
		else if (AnimInstance->GetCurrentActiveMontage() == AttackMontage)	// 몽타주가 재생 중인데, AttackMontage가 재생중이면
		{			
			// 콤보 공격
			SectionJumpForCombo();			
		}		
	}
}

void AActionCharacter::OnInteractionInput(const FInputActionValue& InValue)
{
	IInteractor::Execute_TryInteraction(this);
}

void AActionCharacter::SetSprintMode()
{
	//UE_LOG(LogTemp, Warning, TEXT("달리기 모드"));
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	bIsSprint = true;
}

void AActionCharacter::SetWalkMode()
{
	//UE_LOG(LogTemp, Warning, TEXT("걷기 모드"));
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	bIsSprint = false;
}

void AActionCharacter::OnBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	//UE_LOG(LogTemp, Log, TEXT("Char overlap : other is %s"), *OtherActor->GetName());

	// Cast를 이용한 인터페이스 사용
	//IPickupable* test = Cast<IPickupable>(OtherActor);
	//if (test)
	//{
	//	IPickupable::Execute_OnPickup(OtherActor);	// 만약에 블루프린트 구현이 있을 경우. 블루프린트의 구현이 실행된다.
	//	//test->OnPickup_Implementation();	// 블루프린트 구현은 무시
	//}

	// Implements를 이용한 인터페이스 사용
	if (OtherActor->Implements<UPickupable>())	// OtherActor가 IPickable인터페이스를 구현했는지 확인
	{
		IPickupable::Execute_OnPickup(OtherActor, this);	// 구현이 되어 있으면 실행
	}
}

void AActionCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// UE_LOG(LogTemp, Log, TEXT("공격 몽타주가 끝남"));
	if (CurrentWeapon.IsValid() && !CurrentWeapon->CanAttack())	// CurrentWeapon이 공격할 수 없으면(=사용회수가 안남았다)
	{
		//DropUsedWeapon();		
		DropWeapon(CurrentWeapon->GetWeaponID());	// 현재 사용 중인 무기 버리기
		EquipWeapon(EWeaponCode::BasicWeapon);
	}
}

void AActionCharacter::SectionJumpForCombo()
{
	if (SectionJumpNotify.IsValid() && bComboReady)	// SectionJumpNotify가 있고 콤보가 가능한 상태이면
	{
		UAnimMontage* current = AnimInstance->GetCurrentActiveMontage();
		AnimInstance->Montage_SetNextSection(					// 다음 섹션으로 점프하기
			AnimInstance->Montage_GetCurrentSection(current),		// 현재 섹션
			SectionJumpNotify->GetNextSectionName(),				// 다음 섹션의 이름
			current);												// 실행될 몽타주
			
		bComboReady = false;	// 중복실행 방지
		Resource->AddStamina(-AttackStaminaCost);	// 스태미너 감소
		if (CurrentWeapon.IsValid())
		{
			CurrentWeapon->OnAttack();
		}
	}
}

void AActionCharacter::SpendRunStamina(float DeltaTime)
{
	if ((bIsSprint && !GetVelocity().IsNearlyZero())							// 달리기 상태이고 움직이지 않고 있다.			
		&& (AnimInstance.IsValid() && !AnimInstance->IsAnyMontagePlaying()))	// 어떤 몽타쥬도 재생중이지 않다.(루트모션 때문에 Velocity 변경있음)
	{
		Resource->AddStamina(-SprintStaminaCost * DeltaTime);	// 스태미너 감소
		//UE_LOG(LogTemp, Log, TEXT("Velocity : %s"), *GetVelocity().ToString());
		//UE_LOG(LogTemp, Warning, TEXT("Stamina : %.1f"), Resource->GetCurrentStamina());
	}

	//GetWorld()->GetFirstPlayerController()->GetHUD();
}

void AActionCharacter::DropWeapon(EWeaponCode WeaponCode)
{
	UE_LOG(LogTemp, Log, TEXT("다쓴 무기 버리기"));
	if (TSubclassOf<AUsedWeapon> usedClass = WeaponManager->GetUsedWeaponClass(WeaponCode))
	{
		GetWorld()->SpawnActor<AActor>(
			usedClass,
			DropLocation->GetComponentLocation(),
			GetActorRotation());
	}
}

void AActionCharacter::DropCurrentWeapon(EWeaponCode WeaponCode)
{
	if (CurrentWeapon.IsValid() && CurrentWeapon->GetWeaponID() != EWeaponCode::BasicWeapon)
	{
		if (TSubclassOf<APickupWeapon> pickupClass = WeaponManager->GetPickupWeaponClass(WeaponCode))
		{
			EItemCode itemCode = WeaponManager->GetItemCode(WeaponCode);
			APickup* pickup = GetWorld()->GetSubsystem<UPickupFactorySubsystem>()->SpawnPickup(
				itemCode,
				DropLocation->GetComponentLocation(),
				GetActorRotation()
			);
			FVector velocity = (GetActorForwardVector() + GetActorUpVector()) * 300.0f;
			pickup->AddImpulse(velocity);

			// 새로 생긴 픽업에 남은 회수 넣기
			APickupWeapon* pickupWeapon = Cast<APickupWeapon>(pickup);
			AConsumableWeapon* consumableWeapon = Cast<AConsumableWeapon>(CurrentWeapon);
			if (pickupWeapon && consumableWeapon)
			{
				pickupWeapon->SetWeaponUseCount(consumableWeapon->GetRemainingUseCount());
			}			
		}
	}
}

bool AActionCharacter::IsChangeOrder(AActor* InTarget1, AActor* InTarget2)
{
	float distanceOld = FVector::DistSquared(GetActorLocation(), InTarget1->GetActorLocation());
	float distanceNew = FVector::DistSquared(GetActorLocation(), InTarget2->GetActorLocation());
	return distanceNew < distanceOld;
}

void AActionCharacter::UpdateInteractionTargetOrder()
{
	if (InteractionTargets.Num() > 1)	// 최소한으로 실행하기 위해 2개 이상일 때만 처리
	{
		for (int32 i = InteractionTargets.Num() - 1; i > 0; i--)
		{
			if (IsChangeOrder(InteractionTargets[i - 1].Get(), InteractionTargets[i].Get()))
			{
				AActor* temp = InteractionTargets[i - 1].Get();
				InteractionTargets[i - 1] = InteractionTargets[i].Get();
				InteractionTargets[i] = temp;
			}
			else
			{
				break;	// 변경되지 않았다 = 적절한 위치다 = 그만 확인해도 된다
			}
		}
	}
}
