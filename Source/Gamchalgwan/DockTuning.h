// Tuning dials for the dock graybox (stand-in for DA_Tuning).
// Values live in Config/DefaultGame.ini [/Script/Gamchalgwan.DockTuning] so they can be changed without a rebuild.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DockTuning.generated.h"

UCLASS(Config = Game)
class GAMCHALGWAN_API UDockTuning : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	// Report stage at which the guard comes for the player.
	UPROPERTY(Config, EditAnywhere, Category = "Report") int32 ReportEnd = 3;
	// Report stage set by asking for the record.
	UPROPERTY(Config, EditAnywhere, Category = "Report") int32 ReportOnRequest = 1;
	// Stage added by every later time action.
	UPROPERTY(Config, EditAnywhere, Category = "Report") int32 ReportPerTimeAction = 1;
	UPROPERTY(Config, EditAnywhere, Category = "Time") int32 MinutesPerAction = 5;
	// 22:40 = 1360 minutes.
	UPROPERTY(Config, EditAnywhere, Category = "Time") int32 StartMinutes = 1360;
	UPROPERTY(Config, EditAnywhere, Category = "Move") float MoveSpeed = 900.f;
	UPROPERTY(Config, EditAnywhere, Category = "Move") float GuardSpeed = 700.f;
	UPROPERTY(Config, EditAnywhere, Category = "Report") float CaughtDelay = 2.5f;
	UPROPERTY(Config, EditAnywhere, Category = "Look") float ExposureBias = 0.f;
	UPROPERTY(Config, EditAnywhere, Category = "Look") float MoonLux = 1.f;
	UPROPERTY(Config, EditAnywhere, Category = "Look") float LampCandela = 200.f;
	UPROPERTY(Config, EditAnywhere, Category = "Look") float SkyIntensity = 1.f;
};
