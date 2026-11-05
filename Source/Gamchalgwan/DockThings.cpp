#include "DockThings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	UStaticMesh* ShapeMesh(EDockShape S)
	{
		const TCHAR* Path = S == EDockShape::Cyl ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
			: S == EDockShape::Sphere ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Cube.Cube");
		return LoadObject<UStaticMesh>(nullptr, Path);
	}
	UMaterialInterface* ShapeMat()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	FLinearColor C(float R, float G, float B) { return FLinearColor(R, G, B, 1.f); }
}

ADockThing::ADockThing()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
}

UStaticMeshComponent* ADockThing::AddPart(const FVector& Loc, const FVector& Size, const FLinearColor& Color,
	USceneComponent* Parent, const FRotator& Rot, EDockShape Shape)
{
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
	M->SetMobility(EComponentMobility::Movable);
	M->SetupAttachment(Parent ? Parent : Root.Get());
	M->SetStaticMesh(ShapeMesh(Shape));
	M->SetRelativeLocation(Loc);
	M->SetRelativeRotation(Rot);
	M->SetRelativeScale3D(Size / 100.f);
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(ShapeMat(), this);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	M->SetMaterial(0, Mid);
	M->RegisterComponent();
	AddInstanceComponent(M);
	Parts.Add(M);
	Mids.Add(Mid);
	BaseColors.Add(Color);
	return M;
}

UPointLightComponent* ADockThing::AddLamp(const FVector& Loc, const FLinearColor& Color, float Candela, float Radius, USceneComponent* Parent)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(Parent ? Parent : Root.Get());
	L->SetRelativeLocation(Loc);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Candela);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(false);
	L->RegisterComponent();
	AddInstanceComponent(L);
	return L;
}

USceneComponent* ADockThing::AddPivot(const FVector& Loc, USceneComponent* Parent)
{
	USceneComponent* P = NewObject<USceneComponent>(this);
	P->SetMobility(EComponentMobility::Movable);
	P->SetupAttachment(Parent ? Parent : Root.Get());
	P->SetRelativeLocation(Loc);
	P->RegisterComponent();
	AddInstanceComponent(P);
	return P;
}

void ADockThing::SetHighlight(float Amount)
{
	if (FMath::IsNearlyEqual(Amount, Highlight)) return;
	Highlight = Amount;
	for (int32 i = 0; i < Mids.Num(); ++i)
	{
		const FLinearColor B = BaseColors[i];
		Mids[i]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(B, FLinearColor::White, Amount * 0.3f));
	}
}

void ADockStage::Build()
{
	// Ground outside the yard, the yard slab and the water.
	AddPart(FVector(0, 750, -10), FVector(12000, 4100, 20), C(.035f, .036f, .04f));
	AddPart(FVector(0, 0, 1), FVector(3900, 2600, 6), C(.11f, .11f, .105f));
	AddPart(FVector(0, -4300, -80), FVector(12000, 6000, 20), C(.008f, .02f, .04f));
	// Quay edge with a yellow curb and bollards.
	AddPart(FVector(0, -1275, 15), FVector(12000, 50, 30), C(.32f, .25f, .04f));
	for (int32 x = -1600; x <= 1600; x += 800)
		AddPart(FVector(x, -1225, 30), FVector(40, 40, 60), C(.1f, .1f, .1f), nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
	// Far breakwater.
	AddPart(FVector(0, -3300, -20), FVector(8000, 300, 150), C(.09f, .09f, .09f));
	// Yard walls. West has the front gate opening (Y -330..330), east the back gate opening (Y -830..-170).
	const FLinearColor W = C(.14f, .13f, .12f);
	AddPart(FVector(-1950, (-1250 - 330) * .5f, 150), FVector(40, 920, 300), W);
	AddPart(FVector(-1950, (330 + 1300) * .5f, 150), FVector(40, 970, 300), W);
	AddPart(FVector(1950, (-1250 - 830) * .5f, 150), FVector(40, 420, 300), W);
	AddPart(FVector(1950, (-170 + 1300) * .5f, 150), FVector(40, 1470, 300), W);
	// Low fence along the road side.
	AddPart(FVector(0, 1300, 60), FVector(3900, 20, 120), C(.2f, .2f, .2f));
	// Containers 610 x 244 x 259.
	const FVector Ct(610, 244, 259);
	AddPart(FVector(-800, -1000, 129.5f), Ct, C(.25f, .05f, .03f));
	AddPart(FVector(800, -1050, 129.5f), Ct, C(.03f, .08f, .2f));
	AddPart(FVector(-1200, 900, 129.5f), Ct, C(.05f, .15f, .06f));
	AddPart(FVector(-1200, 900, 388.5f), Ct, C(.03f, .08f, .2f));
	AddPart(FVector(1200, 900, 129.5f), Ct, C(.35f, .12f, .02f));
	// A stack of crates near the warehouse corner.
	AddPart(FVector(-300, 650, 50), FVector(100, 100, 100), C(.3f, .2f, .1f));
	AddPart(FVector(-180, 640, 50), FVector(100, 100, 100), C(.28f, .19f, .1f));
	AddPart(FVector(-240, 650, 150), FVector(100, 100, 100), C(.3f, .21f, .11f));
	// Warehouse 3, low, on the road side of the yard; its shutter faces the yard.
	AddPart(FVector(300, 1160, 150), FVector(1200, 260, 300), C(.16f, .15f, .14f));
	AddPart(FVector(300, 1026, 120), FVector(500, 10, 240), C(.06f, .06f, .065f));
	AddPart(FVector(300, 1020, 255), FVector(120, 6, 40), C(.5f, .45f, .2f));
	// Quay lamps: poles and heads.
	for (int32 x : { -1200, 0, 1200 })
	{
		AddPart(FVector(x, -1150, 300), FVector(20, 20, 600), C(.12f, .12f, .12f), nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
		AddPart(FVector(x, -1110, 600), FVector(60, 100, 20), C(.9f, .85f, .7f));
		QuayLamps.Add(AddLamp(FVector(x, -1100, 570), C(.75f, .82f, 1.f), 200.f, 1800.f));
	}
	// Front gate lamps on the outer road.
	for (int32 y : { -420, 420 })
	{
		AddPart(FVector(-2050, y, 250), FVector(16, 16, 500), C(.12f, .12f, .12f), nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
		AddPart(FVector(-2050, y, 500), FVector(50, 50, 20), C(1.f, .85f, .6f));
		GateLamps.Add(AddLamp(FVector(-2050, y, 470), C(1.f, .78f, .5f), 200.f, 1400.f));
	}
}

void ADockPlayer::Build()
{
	AddPart(FVector(0, 0, 7), FVector(110, 110, 14), C(.1f, .3f, .95f), nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
	AddPart(FVector(0, 0, 75), FVector(60, 60, 120), C(.15f, .35f, .9f), nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
	AddPart(FVector(0, 0, 155), FVector(42, 42, 42), C(.2f, .4f, .9f), nullptr, FRotator::ZeroRotator, EDockShape::Sphere);
	// Facing marker so the walk direction reads.
	AddPart(FVector(55, 0, 7), FVector(30, 20, 16), C(.6f, .8f, 1.f));
}

void ADockClerk::Build()
{
	// Booth behind the clerk (actor sits at the clerk; booth 220 cm towards the water).
	AddPart(FVector(0, -220, 120), FVector(260, 200, 240), C(.2f, .2f, .18f));
	AddPart(FVector(0, -220, 250), FVector(300, 240, 20), C(.1f, .1f, .1f));
	AddPart(FVector(0, -119, 150), FVector(180, 4, 80), C(.6f, .55f, .35f));
	// Desk between the clerk and the visitor.
	AddPart(FVector(0, 80, 47.5f), FVector(170, 70, 95), C(.22f, .16f, .1f));
	// Clerk: body, head, ledger.
	AddPart(FVector(0, 0, 75), FVector(70, 70, 150), C(.28f, .24f, .16f), nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
	AddPart(FVector(0, 0, 175), FVector(45, 45, 45), C(.55f, .42f, .32f), nullptr, FRotator::ZeroRotator, EDockShape::Sphere);
	AddPart(FVector(-45, 80, 97), FVector(40, 28, 4), C(.1f, .15f, .3f));
	BoothLamp = AddLamp(FVector(0, -100, 320), C(1.f, .75f, .45f), 200.f, 1200.f);
}

void ADockGuard::Build()
{
	AddPart(FVector(0, 0, 80), FVector(72, 72, 160), C(.03f, .04f, .1f), nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
	AddPart(FVector(0, 0, 185), FVector(45, 45, 45), C(.5f, .38f, .3f), nullptr, FRotator::ZeroRotator, EDockShape::Sphere);
	AddPart(FVector(0, 0, 210), FVector(52, 52, 12), C(.02f, .02f, .04f), nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
	AddPart(FVector(10, 38, 120), FVector(10, 12, 26), C(.02f, .02f, .02f));
	AddPart(FVector(10, 38, 138), FVector(4, 4, 14), C(.6f, .05f, .03f));
}

void ADockGate::Build()
{
	Title = bBack ? TEXT("후문") : TEXT("정문");
	const FLinearColor P = C(.3f, .3f, .3f);
	AddPart(FVector(0, -360, 175), FVector(60, 60, 350), P);
	AddPart(FVector(0, 360, 175), FVector(60, 60, 350), P);
	if (bBack)
	{
		// Door hinged at the south pillar, swings outward (+X) when opened.
		DoorPivot = AddPivot(FVector(0, -300, 0));
		AddPart(FVector(0, 300, 140), FVector(16, 600, 280), C(.17f, .2f, .17f), DoorPivot);
		AddPart(FVector(-14, 560, 140), FVector(10, 30, 50), C(.25f, .22f, .1f), DoorPivot);
		AddPart(FVector(0, -360, 365), FVector(40, 40, 20), C(.4f, .9f, .5f));
		Lamp = AddLamp(FVector(-40, -360, 340), C(.25f, 1.f, .35f), 0.f, 1200.f);
	}
	else
	{
		AddPart(FVector(0, 0, 340), FVector(60, 780, 20), C(.2f, .2f, .2f));
	}
}

void ADockUnion::Build()
{
	const FLinearColor Vest = C(.7f, .45f, .04f);
	for (const FVector& L : { FVector(0, -90, 0), FVector(50, 40, 0), FVector(-40, 140, 0) })
	{
		AddPart(L + FVector(0, 0, 78), FVector(66, 66, 156), Vest, nullptr, FRotator::ZeroRotator, EDockShape::Cyl);
		AddPart(L + FVector(0, 0, 180), FVector(42, 42, 42), C(.5f, .4f, .32f), nullptr, FRotator::ZeroRotator, EDockShape::Sphere);
	}
	// Van parked on the road.
	AddPart(FVector(-330, 330, 110), FVector(480, 210, 200), C(.45f, .45f, .43f));
	AddPart(FVector(-60, 330, 90), FVector(80, 200, 140), C(.08f, .1f, .12f));
	for (const FVector& Wl : { FVector(-480, 220, 35), FVector(-480, 440, 35), FVector(-170, 220, 35), FVector(-170, 440, 35) })
		AddPart(Wl, FVector(70, 30, 70), C(.02f, .02f, .02f), nullptr, FRotator(0, 0, 90), EDockShape::Cyl);
}

void ADockRecord::Build()
{
	AddPart(FVector(0, 0, 0), FVector(32, 23, 2), C(.85f, .83f, .75f));
}

void ADockShip::Build()
{
	Hull = AddPivot(FVector::ZeroVector);
	AddPart(FVector(0, 0, 0), FVector(2000, 520, 300), C(.05f, .05f, .06f), Hull);
	AddPart(FVector(0, 0, 160), FVector(2000, 520, 20), C(.25f, .06f, .04f), Hull);
	AddPart(FVector(-700, 0, 330), FVector(400, 420, 320), C(.4f, .4f, .38f), Hull);
	AddPart(FVector(-700, 0, 520), FVector(200, 200, 60), C(.15f, .15f, .15f), Hull);
	for (int32 i = 0; i < 6; ++i)
		AddPart(FVector(-200 + (i % 3) * 260, (i < 3) ? -110 : 110, 230), FVector(200, 180, 120), C(.3f, .2f, .1f), Hull);
}
