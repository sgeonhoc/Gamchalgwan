// Mouse + keys for the dock graybox. No pawn: the controller only forwards the pointer and three keys to the game mode.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DockPlayerController.generated.h"

UCLASS()
class GAMCHALGWAN_API ADockPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ADockPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
};
