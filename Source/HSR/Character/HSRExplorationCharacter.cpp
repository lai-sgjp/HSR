#include "HSRExplorationCharacter.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

AHSRExplorationCharacter::AHSRExplorationCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AHSRExplorationCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRExplorationCharacter::SetupPlayerInputComponent - InputComponent is not UEnhancedInputComponent"));
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHSRExplorationCharacter::Move);
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHSRExplorationCharacter::Look);
	}

	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AHSRExplorationCharacter::HSJump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHSRExplorationCharacter::HSStopJumping);
	}

	if (InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AHSRExplorationCharacter::Interact);
	}
}

void AHSRExplorationCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	const FRotator YawRotation(0.0f, PC->GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, MovementVector.Y);
	AddMovementInput(Right, MovementVector.X);
}

void AHSRExplorationCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxis = Value.Get<FVector2D>();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	PC->AddYawInput(LookAxis.X);
	PC->AddPitchInput(LookAxis.Y);
}

void AHSRExplorationCharacter::HSJump()
{
	Jump();
}

void AHSRExplorationCharacter::HSStopJumping()
{
	StopJumping();
}

void AHSRExplorationCharacter::Interact()
{
	UE_LOG(LogTemp, Log, TEXT("AHSRExplorationCharacter::Interact - Called (placeholder)"));
}
