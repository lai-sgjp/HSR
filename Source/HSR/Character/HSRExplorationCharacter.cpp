#include "HSRExplorationCharacter.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "../Player/HSRPlayerController.h"
#include "../Interaction/HSRInteractionComponent.h"

// 构造函数：配置第三人称控制器与相机，创建交互检测组件。
AHSRExplorationCharacter::AHSRExplorationCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 不把控制器的旋转直接应用到角色朝向（由移动方向驱动朝向）。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 角色朝向跟随移动方向，转向速度 500°/秒。
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// 弹簧臂：第三人称相机，跟随控制器旋转。
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;

	// 相机挂到弹簧臂末端，本身不再跟随控制器旋转。
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 交互检测组件（登记/查询附近可交互物体）。
	InteractionComponent = CreateDefaultSubobject<UHSRInteractionComponent>(TEXT("InteractionComponent"));
}

// 绑定增强输入：移动/视角/跳跃/交互。
void AHSRExplorationCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationCharacter::SetupPlayerInputComponent - Character=%s Controller=%s"),
		*GetName(), GetController() ? *GetController()->GetName() : TEXT("None"));

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationCharacter::SetupPlayerInputComponent - InputComponent is not UEnhancedInputComponent"));
		return;
	}

	// 移动：按住期间持续触发。
	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHSRExplorationCharacter::Move);
	}

	// 视角：按住期间持续触发。
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHSRExplorationCharacter::Look);
	}

	// 跳跃：按下开始、松开停止。
	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AHSRExplorationCharacter::HSJump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHSRExplorationCharacter::HSStopJumping);
	}

	// 交互：按下瞬间触发。
	if (InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AHSRExplorationCharacter::Interact);
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationCharacter::SetupPlayerInputComponent - Bound Move=%s Look=%s Jump=%s Interact=%s"),
		MoveAction ? *MoveAction->GetPathName() : TEXT("None"),
		LookAction ? *LookAction->GetPathName() : TEXT("None"),
		JumpAction ? *JumpAction->GetPathName() : TEXT("None"),
		InteractAction ? *InteractAction->GetPathName() : TEXT("None"));
}

// 移动：按控制器朝向计算前/右方向并叠加输入（仅探索控制模式下生效）。
void AHSRExplorationCharacter::Move(const FInputActionValue& Value)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	// 非探索控制模式（例如战斗/结算 UI 独占）时禁止移动。
	if (const AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(PC))
	{
		const EHSRPlayerControlMode Mode = HSRPC->GetControlMode();
		if (Mode != EHSRPlayerControlMode::Exploration)
		{
			return;
		}
	}

	FVector2D MovementVector = Value.Get<FVector2D>();

	// 以控制器偏航角为基准，分解出世界空间的前/右单位向量。
	const FRotator YawRotation(0.0f, PC->GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// W/S（Y 轴）→ 前后；A/D（X 轴）→ 左右。
	AddMovementInput(Forward, MovementVector.Y);
	AddMovementInput(Right, MovementVector.X);
}

// 视角：把输入叠加到控制器的偏航/俯仰（仅探索控制模式下生效）。
void AHSRExplorationCharacter::Look(const FInputActionValue& Value)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	EHSRPlayerControlMode Mode = EHSRPlayerControlMode::Exploration;
	if (const AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(PC))
	{
		Mode = HSRPC->GetControlMode();
		if (Mode != EHSRPlayerControlMode::Exploration)
		{
			UE_LOG(LogTemp, Log, TEXT("HSRExplorationCharacter::Look - BLOCKED Mode=%d (non-exploration)"), static_cast<int32>(Mode));
			return;
		}
	}

	FVector2D LookAxis = Value.Get<FVector2D>();

	// X → 水平旋转；Y → 俯仰。
	PC->AddYawInput(LookAxis.X);
	PC->AddPitchInput(LookAxis.Y);
}

// 跳跃开始。
void AHSRExplorationCharacter::HSJump()
{
	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationCharacter::HSJump - Started"));
	Jump();
}

// 跳跃结束。
void AHSRExplorationCharacter::HSStopJumping()
{
	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationCharacter::HSStopJumping - Completed"));
	StopJumping();
}

// 交互：委托给交互组件尝试与当前候选目标交互。
void AHSRExplorationCharacter::Interact()
{
	if (!InteractionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationCharacter::Interact - InteractionComponent is null"));
		return;
	}

	FHSRInteractionResult Result = InteractionComponent->TryInteract();
	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationCharacter::Interact - Character=%s result=success=%d reason=%s"),
		*GetName(), Result.bSuccess, *UEnum::GetValueAsString(Result.FailureReason));
}
