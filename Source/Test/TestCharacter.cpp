// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/UnrealMathVectorCommon.h"
#include "Kismet/KismetMathLibrary.h"


//////////////////////////////////////////////////////////////////////////
// ATestCharacter

ATestCharacter::ATestCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	//Setting The wall running trigger capsule
	TriggerCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Trigger Capsule"));
	TriggerCapsule->InitCapsuleSize(55.0f, 96.0f);
	TriggerCapsule->SetCollisionProfileName(TEXT("Trigger"));
	TriggerCapsule->SetupAttachment(RootComponent);

	TriggerCapsule->OnComponentBeginOverlap.AddDynamic(this, &ATestCharacter::OnOverlapBegin);
	TriggerCapsule->OnComponentEndOverlap.AddDynamic(this, &ATestCharacter::OnOverlapEnd);
	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	//Create a First Person Camera
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	


	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}



//////////////////////////////////////////////////////////////////////////
// Input

void ATestCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &ATestCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ATestCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ATestCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ATestCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ATestCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &ATestCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ATestCharacter::OnResetVR);

	//Changing between ThirdPerson and FirstPerson
	PlayerInputComponent->BindAction("ChangeCamera",IE_Pressed, this, &ATestCharacter::ChangeCamera);
}


void ATestCharacter::OnResetVR()
{
	// If Test is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in Test.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ATestCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void ATestCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void ATestCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ATestCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ATestCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
		
		WallRun();
	}
}

void ATestCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f)  )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		//gets left or right key
		DirectionOfMovement = Value;
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	
		CheckDirection();
		WallRun();
		//Initial Wall running 
		//SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, 300.0f),true);
	}
}

//handles when character overlaps with collision boxes
void ATestCharacter::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	//need this check to see if it overlaps with itself
	if (OtherActor && (OtherActor != this) && OtherComp) {
		OverlapingObjectName = OtherActor->GetActorLabel();
		//checks objects that is overlapping
		CheckForInteractable();

		//OverlapingObjectName = OtherActor->GetName();
	
	}
}

//handles the end of overlap with collision boxes
void ATestCharacter::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{		
	
	if (OtherActor && (OtherActor != this) && OtherComp ) {

		bIsWallRunning = false;
		
		if ((OverlapingObjectName.Contains("RunnableWall"))) {
			WallJumpEnd();
		}
		
		bIsOnLedge = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		//SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z));
		GetCharacterMovement()->GravityScale = 1;
		//GetCharacterMovement()->ConstrainLocationToPlane(FVector(1.0f,0.0f,0.0f));	
	}

}

//changes between first and third person camera 
void ATestCharacter::ChangeCamera() {
	//switches between FollowCamera and FirstPersonCamera
	if (FollowCamera->IsActive()) {
		FollowCamera->Deactivate();
		FirstPersonCamera->Activate();
	}
	else
	{
		FollowCamera->Activate();
		FirstPersonCamera->Deactivate();
	}
}

//locks the Z axis by saving the first Z-value when hitting the box collisiion
void ATestCharacter::WallRun() 
{
	
	if (bIsWallRunning)
	{
		//gets the Z axis location when overlapping with the wall	
	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, WallRunZAxis), true);		
	}
}

//adds another jump to the character so that it can jump off walls
void ATestCharacter::WallJumpBegin()
{
	GetCharacterMovement()->AddImpulse(FVector(0.4f,0.4f,0.0f), true);
	//adds more velocity to the jump 
	GetCharacterMovement()->JumpZVelocity = 900.0f;
	//adds another jump to the character 
	JumpMaxCount += 1;

}

//removes the extra jump and sets the velocity back to default
void ATestCharacter::WallJumpEnd()
{
	JumpMaxCount -= 1;
	GetCharacterMovement()->JumpZVelocity = 600.0f;
}

//checks what type of objects is interacting with
void ATestCharacter::CheckForInteractable()
{
	
	if (OverlapingObjectName.Contains("GrabbableLedge"))
	{
		GrabLedge();
		//used for debugging
		UE_LOG(LogTemp, Warning, TEXT("%s"), *OverlapingObjectName);
	}
	if ((OverlapingObjectName.Contains("RunnableWall")))
	{
		//saves the Z axis (height) when overlapping with a wall
		WallRunZAxis = GetActorLocation().Z;
		//enable wall running if colliding with a runnable wall
		bIsWallRunning = true;
		WallJumpBegin();
		//used for debugging 
		UE_LOG(LogTemp, Warning, TEXT("%s"), *OverlapingObjectName);
	}
}

//logic that handles ledge grabbing
void ATestCharacter::GrabLedge()
{
	bIsOnLedge = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->GravityScale = 0;

		
}

//checks for left and right to play the proper wall running animations
void ATestCharacter::CheckDirection()
{
	if (DirectionOfMovement == 1.0f) 
	{
	bIsOnRight = true;
	bIsOnLeft = false;
	}
	if (DirectionOfMovement == -1.0f)
	{
	bIsOnLeft = true;
	bIsOnRight = false;
	}
}
