#include "DockPlayerController.h"
#include "DockGameMode.h"

#include "Engine/World.h"
#include "InputCoreTypes.h"

ADockPlayerController::ADockPlayerController()
{
	bShowMouseCursor = true;
	bAutoManageActiveCameraTarget = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ADockPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void ADockPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	ADockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADockGameMode>() : nullptr;
	if (!GM || GM->bAutoplay) return;

	float X = 0.f, Y = 0.f;
	if (GetMousePosition(X, Y))
	{
		const FVector2D P(X, Y);
		GM->PointerMove(P);
		if (WasInputKeyJustPressed(EKeys::LeftMouseButton)) GM->PointerClick(P);
	}

	if (WasInputKeyJustPressed(EKeys::R)) GM->Key(EKeys::R);
	if (WasInputKeyJustPressed(EKeys::Escape)) GM->Key(EKeys::Escape);
	if (WasInputKeyJustPressed(EKeys::F1)) GM->Key(EKeys::F1);
}
