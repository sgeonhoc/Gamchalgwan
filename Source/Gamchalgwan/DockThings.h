// Graybox stand-ins. Every one is built from /Engine/BasicShapes at runtime, so the project ships no .uasset.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DockThings.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

enum class EDockShape : uint8 { Box, Cyl, Sphere };

UCLASS()
class GAMCHALGWAN_API ADockThing : public AActor
{
	GENERATED_BODY()
public:
	ADockThing();

	// Builds the look. Called once by the game mode right after spawning.
	virtual void Build() {}

	UStaticMeshComponent* AddPart(const FVector& Loc, const FVector& Size, const FLinearColor& Color,
		USceneComponent* Parent = nullptr, const FRotator& Rot = FRotator::ZeroRotator, EDockShape Shape = EDockShape::Box);
	UPointLightComponent* AddLamp(const FVector& Loc, const FLinearColor& Color, float Candela, float Radius, USceneComponent* Parent = nullptr);
	USceneComponent* AddPivot(const FVector& Loc, USceneComponent* Parent = nullptr);

	// 0 = normal colour, 1 = white. Used for hover and selection.
	void SetHighlight(float Amount);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	TArray<FLinearColor> BaseColors;

	FString Title;
	bool bTarget = false;
	FVector LabelOffset = FVector(0, 0, 260);
	float Highlight = 0.f;
};

UCLASS() class GAMCHALGWAN_API ADockStage : public ADockThing
{
	GENERATED_BODY()
public:
	virtual void Build() override;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> QuayLamps;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> GateLamps;
};

UCLASS() class GAMCHALGWAN_API ADockPlayer : public ADockThing
{
	GENERATED_BODY()
public:
	ADockPlayer() { Title = TEXT("나"); bTarget = true; LabelOffset = FVector(0, 0, 120); }
	virtual void Build() override;
};

UCLASS() class GAMCHALGWAN_API ADockClerk : public ADockThing
{
	GENERATED_BODY()
public:
	ADockClerk() { Title = TEXT("관리인"); bTarget = true; LabelOffset = FVector(0, 0, 250); }
	virtual void Build() override;
	UPROPERTY() TObjectPtr<UPointLightComponent> BoothLamp;
};

UCLASS() class GAMCHALGWAN_API ADockGuard : public ADockThing
{
	GENERATED_BODY()
public:
	ADockGuard() { Title = TEXT("경비"); bTarget = true; LabelOffset = FVector(0, 0, 260); }
	virtual void Build() override;
};

UCLASS() class GAMCHALGWAN_API ADockGate : public ADockThing
{
	GENERATED_BODY()
public:
	ADockGate() { bTarget = true; LabelOffset = FVector(0, 0, 420); }
	virtual void Build() override;
	bool bBack = false;
	UPROPERTY() TObjectPtr<USceneComponent> DoorPivot;
	UPROPERTY() TObjectPtr<UPointLightComponent> Lamp;
};

UCLASS() class GAMCHALGWAN_API ADockUnion : public ADockThing
{
	GENERATED_BODY()
public:
	ADockUnion() { Title = TEXT("조합 사람들"); bTarget = true; LabelOffset = FVector(0, 0, 260); }
	virtual void Build() override;
};

UCLASS() class GAMCHALGWAN_API ADockRecord : public ADockThing
{
	GENERATED_BODY()
public:
	virtual void Build() override;
};

UCLASS() class GAMCHALGWAN_API ADockShip : public ADockThing
{
	GENERATED_BODY()
public:
	ADockShip() { Title = TEXT("화물선"); bTarget = true; LabelOffset = FVector(0, 0, 700); }
	virtual void Build() override;
	UPROPERTY() TObjectPtr<USceneComponent> Hull;
};
