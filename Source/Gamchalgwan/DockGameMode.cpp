#include "DockGameMode.h"
#include "DockThings.h"
#include "DockTuning.h"
#include "DockHUD.h"
#include "DockPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"

namespace
{
	const FVector MeStart(-1450, 150, 0);
	const FVector ClerkSpot(0, -380, 0);
	const FVector ClerkAt(0, -600, 0);
	const FVector GuardAt(1750, -500, 0);
	const FVector FrontAt(-1950, 0, 0);
	const FVector BackAt(1950, -500, 0);
	const FVector UnionAt(-2300, -60, 0);
	const FVector RecordAt(40, -520, 97);
	const FVector ShipAt(300, -2050, 0);

	float Smooth(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
}

ADockGameMode::ADockGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	HUDClass = ADockHUD::StaticClass();
	PlayerControllerClass = ADockPlayerController::StaticClass();
}

const UDockTuning* ADockGameMode::Tun() const { return GetDefault<UDockTuning>(); }

APlayerController* ADockGameMode::PC() const { return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr; }

void ADockGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Intentionally empty: RestartPlayer would try to spawn a pawn / spectator and steal the view target.
}

void ADockGameMode::BeginPlay()
{
	Super::BeginPlay();

	const TCHAR* Cmd = FCommandLine::Get();
	bAutoplay = FParse::Param(Cmd, TEXT("autoplay"));
	FParse::Value(Cmd, TEXT("capdir="), CapDir);
	bCapture = bAutoplay && !CapDir.IsEmpty();
	FParse::Value(Cmd, TEXT("capmax="), CapMaxFrames);

	// The build is meant to open as a 1280x720 window on any PC. Command line sizes win.
	int32 ResX = 0;
	if (!bAutoplay && !FParse::Value(Cmd, TEXT("ResX="), ResX) && GEngine)
	{
		if (UGameUserSettings* S = GEngine->GetGameUserSettings())
		{
			if (S->GetFullscreenMode() != EWindowMode::Windowed)
			{
				S->SetFullscreenMode(EWindowMode::Windowed);
				S->SetScreenResolution(FIntPoint(1280, 720));
				S->ApplyResolutionSettings(false);
				S->SaveSettings();
			}
		}
	}

	BuildLook();
	Audio.Build();

	if (bCapture)
	{
		IFileManager::Get().MakeDirectory(*CapDir, true);
		ShotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &ADockGameMode::OnShot);
	}
	else
	{
		Wave = NewObject<USoundWaveProcedural>(this);
		Wave->SetSampleRate(FDockAudio::Rate);
		Wave->NumChannels = 1;
		Wave->Duration = INDEFINITELY_LOOPING_DURATION;
		Wave->bLooping = false;
		Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
		AudioComp = UGameplayStatics::CreateSound2D(this, Wave, 1.f, 1.f, 0.f, nullptr, false, false);
		if (AudioComp)
		{
			FeedAudio();
			AudioComp->Play();
		}
	}

	if (bAutoplay) BuildScript();
	ResetGame();
}

void ADockGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ShotHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ShotHandle);
	if (AudioComp) AudioComp->Stop();
	Super::EndPlay(Reason);
}

void ADockGameMode::BuildLook()
{
	const UDockTuning* T = Tun();
	UWorld* W = GetWorld();

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Yard = W->SpawnActor<ADockStage>(ADockStage::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	Yard->Build();
	for (UPointLightComponent* L : Yard->QuayLamps) if (L) L->SetIntensity(T->LampCandela);
	for (UPointLightComponent* L : Yard->GateLamps) if (L) L->SetIntensity(T->LampCandela);

	LookHost = W->SpawnActor<ADockThing>(ADockThing::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);

	Moon = NewObject<UDirectionalLightComponent>(LookHost);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetupAttachment(LookHost->Root);
	Moon->SetRelativeRotation(FRotator(-50.f, 125.f, 0.f));
	Moon->SetIntensity(T->MoonLux);
	Moon->SetLightColor(FLinearColor(0.55f, 0.66f, 1.f));
	Moon->SetAtmosphereSunLight(true);
	Moon->RegisterComponent();
	LookHost->AddInstanceComponent(Moon);

	USkyAtmosphereComponent* Atmo = NewObject<USkyAtmosphereComponent>(LookHost);
	Atmo->SetupAttachment(LookHost->Root);
	Atmo->RegisterComponent();
	LookHost->AddInstanceComponent(Atmo);

	Sky = NewObject<USkyLightComponent>(LookHost);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetupAttachment(LookHost->Root);
	Sky->SetRealTimeCapture(true);
	Sky->SetIntensity(T->SkyIntensity);
	Sky->SetLightColor(FLinearColor(0.6f, 0.7f, 1.f));
	Sky->RegisterComponent();
	LookHost->AddInstanceComponent(Sky);

	Fog = NewObject<UExponentialHeightFogComponent>(LookHost);
	Fog->SetupAttachment(LookHost->Root);
	Fog->SetFogDensity(0.03f);
	Fog->SetFogHeightFalloff(0.25f);
	Fog->SetStartDistance(1800.f);
	Fog->SetFogInscatteringColor(FLinearColor(0.02f, 0.03f, 0.06f));
	Fog->RegisterComponent();
	LookHost->AddInstanceComponent(Fog);

	CamBase = FVector(0, 2300, 2500);
	Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CamBase, FRotator(-48.f, -90.f, 0.f), P);
	UCameraComponent* CC = Cam->GetCameraComponent();
	CC->SetFieldOfView(78.f);
	CC->SetConstraintAspectRatio(false);
	FPostProcessSettings& PP = CC->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;      PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureBias = true;        PP.AutoExposureBias = T->ExposureBias;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true; PP.AutoExposureApplyPhysicalCameraExposure = 0;
	PP.bOverride_MotionBlurAmount = true;        PP.MotionBlurAmount = 0.f;
	CC->PostProcessBlendWeight = 1.f;

	if (APlayerController* C = PC()) C->SetViewTarget(Cam);
}

template<class T> T* ADockGameMode::SpawnThing(const FVector& Loc, float Yaw)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<T>(T::StaticClass(), Loc, FRotator(0.f, Yaw, 0.f), P);
}

void ADockGameMode::SpawnRound()
{
	const UDockTuning* T = Tun();
	Ship = SpawnThing<ADockShip>(ShipAt);     Ship->Build();
	Clerk = SpawnThing<ADockClerk>(ClerkAt);  Clerk->Build();
	if (Clerk->BoothLamp) Clerk->BoothLamp->SetIntensity(T->LampCandela);
	Guard = SpawnThing<ADockGuard>(GuardAt, 180.f); Guard->Build();
	Front = SpawnThing<ADockGate>(FrontAt);   Front->bBack = false; Front->Build();
	Back = SpawnThing<ADockGate>(BackAt);     Back->bBack = true;   Back->Build();
	Union = SpawnThing<ADockUnion>(UnionAt);  Union->Build();
	Union->SetActorHiddenInGame(true);
	Union->SetActorEnableCollision(false);
	Record = SpawnThing<ADockRecord>(RecordAt, 15.f); Record->Build();
	Me = SpawnThing<ADockPlayer>(MeStart, -20.f); Me->Build();
	Things.Reset();
	Things.Add(Clerk.Get()); Things.Add(Guard.Get()); Things.Add(Back.Get()); Things.Add(Front.Get());
	Things.Add(Union.Get()); Things.Add(Ship.Get()); Things.Add(Me.Get());
}

void ADockGameMode::ClearRound()
{
	TArray<AActor*> Old;
	Old.Add(Me.Get()); Old.Add(Clerk.Get()); Old.Add(Guard.Get()); Old.Add(Front.Get());
	Old.Add(Back.Get()); Old.Add(Union.Get()); Old.Add(Record.Get()); Old.Add(Ship.Get());
	for (AActor* A : Old)
		if (IsValid(A)) A->Destroy();
	Things.Reset();
	Me = nullptr; Clerk = nullptr; Guard = nullptr; Front = nullptr; Back = nullptr;
	Union = nullptr; Record = nullptr; Ship = nullptr; Hover = nullptr; Selected = nullptr;
}

void ADockGameMode::ResetGame()
{
	ClearRound();
	Events.Reset(); Moves.Reset(); Anims.Reset();
	Lines.Reset(); Log.Reset(); Buttons.Reset(); UiRects.Reset();
	Phase = EDockPhase::Intro; Ending = EDockEnd::None; PhaseTime = 0.f;
	Minutes = 0; Stage = 0; AskCount = 0;
	bNear = bRequested = bHasRecord = bContacted = false;
	bUnionHere = bBackOpen = bGuardAtFront = bCaught = false;
	bRecordOpen = bLampBlink = false;
	Flash = 0.f; Shake = 0.f; ClerkStatus.Reset(); EndTitle.Reset(); EndLine.Reset();
	++Round;
	SpawnRound();
	AddLog(TEXT("부두 정문으로 들어옴"));
}

// ---------------------------------------------------------------- tick

void ADockGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DeltaSeconds > 0.f) Fps = FMath::Lerp(Fps, 1.f / DeltaSeconds, 0.05f);

	if (APlayerController* C = PC())
		if (Cam && C->GetViewTarget() != Cam) C->SetViewTarget(Cam);

	if (bCapture)
	{
		if (bCapDone) return;
		if (bShotPending)
		{
			if (FPlatformTime::Seconds() - ShotAsked > 5.0)
			{
				FScreenshotRequest::RequestScreenshot(FString(), false, false);
				ShotAsked = FPlatformTime::Seconds();
			}
			return;
		}
		const float Dt = 1.f / 30.f;
		RunScript(Dt);
		if (bCapDone) return;
		Step(Dt);
		const int32 N = FDockAudio::Rate / 30;
		const int32 Old = CapAudio.AddUninitialized(N);
		Audio.Mix(CapAudio.GetData() + Old, N);
		bShotPending = true;
		ShotAsked = FPlatformTime::Seconds();
		FScreenshotRequest::RequestScreenshot(FString(), false, false);
		return;
	}

	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	if (bAutoplay) RunScript(Dt);
	Step(Dt);
	FeedAudio();
}

void ADockGameMode::FeedAudio()
{
	if (!Wave) return;
	int16 Buf[1024];
	int32 Loops = 0;
	while (Wave->GetAvailableAudioByteCount() < 9600 && Loops++ < 16)
	{
		Audio.Mix(Buf, 1024);
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Buf), sizeof(Buf));
	}
}

void ADockGameMode::Step(float Dt)
{
	const UDockTuning* T = Tun();
	SimTime += Dt;
	PhaseTime += Dt;
	if (Phase == EDockPhase::Intro && PhaseTime > 1.3f) { Phase = EDockPhase::Play; PhaseTime = 0.f; }

	// Timed events. Run the due ones from a copy; a callback may queue more.
	{
		TArray<FEvent> Due;
		for (int32 i = Events.Num() - 1; i >= 0; --i)
			if (Events[i].At <= SimTime) { Due.Insert(MoveTemp(Events[i]), 0); Events.RemoveAt(i); }
		Due.Sort([](const FEvent& A, const FEvent& B) { return A.At < B.At; });
		for (FEvent& E : Due) E.Fn();
	}

	// Walks.
	{
		TArray<TFunction<void()>> Finished;
		for (int32 i = Moves.Num() - 1; i >= 0; --i)
		{
			FMove& M = Moves[i];
			AActor* A = M.A.Get();
			if (!A || M.Path.Num() == 0) { Moves.RemoveAt(i); continue; }
			FVector Pos = A->GetActorLocation();
			float Left = M.Speed * Dt;
			while (Left > 0.f && M.Path.Num() > 0)
			{
				const FVector To(M.Path[0].X, M.Path[0].Y, Pos.Z);
				const FVector D = To - Pos;
				const float Len = D.Size2D();
				if (Len > 1.f) A->SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f));
				if (Len <= Left) { Pos = To; Left -= Len; M.Path.RemoveAt(0); }
				else { Pos += D / Len * Left; Left = 0.f; }
			}
			A->SetActorLocation(Pos);
			if (M.bSteps)
			{
				M.StepAcc += Dt;
				if (M.StepAcc > 0.42f) { M.StepAcc = 0.f; Audio.Play(EDockSnd::Step, A == Me ? 0.5f : 0.35f); }
			}
			if (M.Path.Num() == 0)
			{
				if (M.Done) Finished.Add(MoveTemp(M.Done));
				Moves.RemoveAt(i);
			}
		}
		for (TFunction<void()>& F : Finished) F();
	}

	// Short animations.
	for (int32 i = Anims.Num() - 1; i >= 0; --i)
		if (Anims[i](Dt)) Anims.RemoveAt(i);

	// The ship breathes on the water.
	if (Ship && Ship->Hull)
	{
		Ship->Hull->SetRelativeLocation(FVector(0, 0, FMath::Sin(SimTime * 0.6f) * 8.f));
		Ship->Hull->SetRelativeRotation(FRotator(0.f, 0.f, FMath::Sin(SimTime * 0.45f) * 0.7f));
	}

	// Booth lamp flickers while the clerk is on the phone.
	if (Clerk && Clerk->BoothLamp)
	{
		const float On = bLampBlink ? (FMath::Fmod(SimTime, 0.5f) < 0.25f ? 1.8f : 0.35f) : 1.f;
		Clerk->BoothLamp->SetIntensity(T->LampCandela * On);
	}

	Flash = FMath::Max(0.f, Flash - Dt * 1.6f);
	Shake = FMath::Max(0.f, Shake - Dt * 28.f);
	if (Cam) Cam->SetActorLocation(CamBase + FVector(FMath::Sin(SimTime * 53.f) * Shake, 0.f, FMath::Cos(SimTime * 47.f) * Shake));

	for (ADockThing* Th : Things)
		if (Th) Th->SetHighlight(Th == Selected ? 1.f : (Th == Hover ? 0.55f : 0.f));

	for (int32 i = Lines.Num() - 1; i >= 0; --i)
		if (SimTime - Lines[i].Time > 9.f) Lines.RemoveAt(i);
}

// ---------------------------------------------------------------- helpers

void ADockGameMode::After(float Delay, TFunction<void()> Fn, bool bBlock)
{
	Events.Add(FEvent{ SimTime + Delay, MoveTemp(Fn), bBlock });
}

void ADockGameMode::MoveTo(AActor* A, TArray<FVector> Path, float Speed, bool bSteps, TFunction<void()> Done)
{
	CancelMoves(A);
	Moves.Add(FMove{ A, MoveTemp(Path), Speed, bSteps, 0.3f, MoveTemp(Done) });
}

bool ADockGameMode::IsMoving(const AActor* A) const
{
	for (const FMove& M : Moves) if (M.A.Get() == A) return true;
	return false;
}

void ADockGameMode::CancelMoves(const AActor* A)
{
	Moves.RemoveAll([A](const FMove& M) { return M.A.Get() == A; });
}

bool ADockGameMode::IsBusy() const
{
	if (Phase != EDockPhase::Play || bCaught) return true;
	if (IsMoving(Me) || Anims.Num() > 0) return true;
	for (const FEvent& E : Events) if (E.bBlock) return true;
	return false;
}

void ADockGameMode::Say(const FString& S)
{
	Lines.Add(FLine{ S, SimTime });
	while (Lines.Num() > 3) Lines.RemoveAt(0);
}

void ADockGameMode::AddLog(const FString& S)
{
	Log.Add(FLog{ Minutes, S });
}

void ADockGameMode::SpendTime(const FString& LogText)
{
	Minutes += Tun()->MinutesPerAction;
	AddLog(LogText);
}

FString ADockGameMode::Clock(int32 Min) const
{
	const int32 M = (Tun()->StartMinutes + Min) % (24 * 60);
	return FString::Printf(TEXT("%02d:%02d"), M / 60, M % 60);
}

FVector2D ADockGameMode::ScreenOf(const FVector& World, bool* bOk) const
{
	FVector2D Out(-1000, -1000);
	bool b = false;
	if (APlayerController* C = PC()) b = C->ProjectWorldLocationToScreen(World, Out, false);
	if (bOk) *bOk = b;
	return Out;
}

bool ADockGameMode::InUi(const FVector2D& P) const
{
	for (const FDockButton& B : Buttons) if (B.Rect.IsInside(P)) return true;
	for (const FBox2D& R : UiRects) if (R.IsInside(P)) return true;
	return false;
}

ADockThing* ADockGameMode::PickAt(const FVector2D& P) const
{
	APlayerController* C = PC();
	if (!C) return nullptr;
	FHitResult Hit;
	if (C->GetHitResultAtScreenPosition(P, ECC_Visibility, false, Hit))
		if (ADockThing* T = Cast<ADockThing>(Hit.GetActor()))
			if (T->bTarget && !T->IsHidden() && Things.Contains(T)) return T;

	// Forgiving fallback: the nearest name tag or body within a finger's width.
	float ViewY = 720.f;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		ViewY = GEngine->GameViewport->Viewport->GetSizeXY().Y;
	const float R = 70.f * ViewY / 720.f;
	ADockThing* Best = nullptr;
	double BestD = double(R) * R;
	for (ADockThing* T : Things)
	{
		if (!T || T->IsHidden()) continue;
		for (const FVector& W : { T->GetActorLocation() + T->LabelOffset, T->GetActorLocation() + T->LabelOffset * 0.45f })
		{
			bool bOk = false;
			const FVector2D S = ScreenOf(W, &bOk);
			if (!bOk) continue;
			const double D = FVector2D::DistSquared(S, P);
			if (D < BestD) { BestD = D; Best = T; }
		}
	}
	return Best;
}

// ---------------------------------------------------------------- input

void ADockGameMode::PointerMove(const FVector2D& P)
{
	Pointer = P;
	if (Phase != EDockPhase::Play || bRecordOpen || InUi(P)) { Hover = nullptr; return; }
	Hover = PickAt(P);
}

void ADockGameMode::PointerClick(const FVector2D& P)
{
	Pointer = P;
	for (int32 i = Buttons.Num() - 1; i >= 0; --i) // drawn last = on top
	{
		const FDockButton& B = Buttons[i];
		if (!B.Rect.IsInside(P)) continue;
		if (B.bEnabled) DoAct(B.Act);
		return;
	}
	if (Phase != EDockPhase::Play || bRecordOpen || InUi(P)) return;
	ADockThing* T = PickAt(P);
	if (T != Selected) Audio.Play(EDockSnd::Click, T ? 0.45f : 0.25f);
	Selected = T;
}

void ADockGameMode::Key(const FKey& K)
{
	if (K == EKeys::R) { DoAct(EDockAct::Restart); return; }
	if (K == EKeys::F1) { bDebug = !bDebug; return; }
	if (K == EKeys::Escape)
	{
		if (bRecordOpen) DoAct(EDockAct::CloseRecord);
		else DoAct(EDockAct::Quit);
	}
}

// ---------------------------------------------------------------- rules

void ADockGameMode::PanelActs(TArray<EDockAct>& Out) const
{
	Out.Reset();
	if (!Selected || Phase != EDockPhase::Play) return;
	if (Selected == Clerk)
	{
		if (!bNear) Out.Add(EDockAct::Approach);
		else
		{
			if (!bRequested) Out.Add(EDockAct::Request);
			Out.Add(EDockAct::AskMore);
		}
	}
	else if (Selected == Front) Out.Add(EDockAct::ExitFront);
	else if (Selected == Back) { if (bBackOpen) Out.Add(EDockAct::ExitBack); }
	else if (Selected == Me) { if (bHasRecord) Out.Add(EDockAct::ViewRecord); }
}

bool ADockGameMode::ActCostsTime(EDockAct A) const
{
	switch (A)
	{
	case EDockAct::Approach: case EDockAct::Request: case EDockAct::AskMore: case EDockAct::Contact:
	case EDockAct::ExitFront: case EDockAct::ExitBack: return true;
	default: return false;
	}
}

FString ADockGameMode::ActLabel(EDockAct A) const
{
	switch (A)
	{
	case EDockAct::Approach:    return TEXT("다가가기");
	case EDockAct::Request:     return TEXT("기록 요청");
	case EDockAct::AskMore:     return TEXT("더 묻기");
	case EDockAct::Contact:     return TEXT("조합에 연락");
	case EDockAct::ViewRecord:  return TEXT("기록 보기");
	case EDockAct::CloseRecord: return TEXT("닫기");
	case EDockAct::ExitFront:   return TEXT("정문으로 나가기");
	case EDockAct::ExitBack:    return TEXT("후문으로 나가기");
	case EDockAct::Restart:     return TEXT("다시 (R)");
	case EDockAct::Quit:        return TEXT("끝내기 (Esc)");
	default: return FString();
	}
}

FString ADockGameMode::ActTip(EDockAct A) const
{
	const FString Min = FString::Printf(TEXT(" · +%d분"), Tun()->MinutesPerAction);
	switch (A)
	{
	case EDockAct::Approach:   return TEXT("관리인 앞까지 걸어간다") + Min;
	case EDockAct::Request:    return TEXT("3번 창고 반출 기록을 보여 달라고 한다") + Min;
	case EDockAct::AskMore:    return TEXT("배와 서명에 대해 더 묻는다") + Min;
	case EDockAct::Contact:    return TEXT("기록을 근거로 조합에 전화한다") + Min;
	case EDockAct::ViewRecord: return TEXT("받은 반출 기록을 펼친다 · 시간 안 듦");
	case EDockAct::ExitFront:  return TEXT("들어온 길로 부두를 나간다") + Min;
	case EDockAct::ExitBack:   return TEXT("열린 후문으로 빠져나간다") + Min;
	default: return FString();
	}
}

FString ADockGameMode::DescFor(const ADockThing* T) const
{
	if (!T) return FString();
	if (T == Clerk) return bRequested ? TEXT("기록을 건넨 뒤로 자꾸 장부와 전화기를 본다.") : TEXT("부두 사무소 관리인. 오늘 밤 반출 기록을 쥐고 있다.");
	if (T == Guard) return bGuardAtFront ? TEXT("정문 앞에서 조합 사람들을 막고 있다.") : TEXT("후문을 지키는 경비. 허리에 무전기.");
	if (T == Front) return TEXT("들어온 길. 여기로 나가면 오늘 밤은 끝이다.");
	if (T == Back) return bBackOpen ? TEXT("빗장이 풀려 있다.") : TEXT("잠겨 있다. 경비가 지키는 쪽문.");
	if (T == Union) return TEXT("조합 사람들. 정문 밖에서 경비와 실랑이 중.");
	if (T == Ship) return TEXT("23:40 출항 예정 화물선. 3번 창고 상자가 실려 있다.");
	if (T == Me) return bHasRecord ? TEXT("나. 감찰관. 반출 기록을 손에 넣었다.") : TEXT("나. 감찰관. 3번 창고 반출 기록을 찾으러 왔다.");
	return FString();
}

void ADockGameMode::RaiseStage(int32 By)
{
	const int32 Old = Stage;
	Stage += By;
	const int32 End = Tun()->ReportEnd;
	if (Stage >= End && Old < End)
	{
		StartCaught(TEXT("경비의 무전기가 지직거린다 — 관리인의 보고가 닿았다."), 0.8f);
	}
	else if (Stage >= 2 && Old < 2)
	{
		After(0.6f, [this]()
		{
			Audio.Play(EDockSnd::Phone);
			bLampBlink = true;
			ClerkStatus = TEXT("전화 중");
			Say(TEXT("관리인이 수화기를 든다. 누군가에게 보고하는 눈치다."));
		}, false);
	}
}

void ADockGameMode::StartCaught(const FString& Line, float Delay)
{
	if (bCaught) return;
	bCaught = true;
	Selected = nullptr;
	bRecordOpen = false;
	After(Delay, [this, Line]()
	{
		Audio.Play(EDockSnd::Radio);
		Say(Line);
		After(0.9f, [this]()
		{
			if (!Guard || !Me) return;
			const FVector G = Guard->GetActorLocation();
			const FVector M = Me->GetActorLocation();
			FVector Dir = (G - M).GetSafeNormal2D();
			if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);
			const FVector Stop = M + Dir * 130.f;
			MoveTo(Guard, { Stop }, Tun()->GuardSpeed, true, [this]()
			{
				Audio.Play(EDockSnd::Whistle);
				if (Guard && Me)
				{
					const FVector D = Me->GetActorLocation() - Guard->GetActorLocation();
					Guard->SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f));
				}
				After(0.35f, [this]()
				{
					Audio.Play(EDockSnd::Boom);
					Flash = 1.f;
					Shake = 25.f;
					Say(TEXT("\"거기 서!\""));
					After(Tun()->CaughtDelay, [this]() { EndGame(EDockEnd::C); });
				});
			});
		});
	});
}

void ADockGameMode::EndGame(EDockEnd E)
{
	Ending = E;
	Phase = EDockPhase::Ending;
	PhaseTime = 0.f;
	Selected = nullptr; Hover = nullptr; bRecordOpen = false;
	bLampBlink = false;
	switch (E)
	{
	case EDockEnd::A:
		EndTitle = TEXT("A — 정문으로, 기록과 함께");
		EndLine = TEXT("관리인이 보고를 올리기 전에 부두를 빠져나왔다.");
		Audio.Play(EDockSnd::Escape);
		break;
	case EDockEnd::B:
		EndTitle = TEXT("B — 후문으로, 조합이 연 길");
		EndLine = TEXT("조합이 경비를 정문에 묶어 둔 사이 후문으로 나왔다.");
		Audio.Play(EDockSnd::Escape);
		break;
	case EDockEnd::C:
		EndTitle = TEXT("C — 붙잡힘");
		EndLine = TEXT("보고가 경비에게 닿았다. 기록은 압수됐다.");
		break;
	default:
		EndTitle = TEXT("D — 빈손");
		EndLine = TEXT("아무것도 들지 못하고 부두를 나왔다.");
		break;
	}
	AddLog(TEXT("끝 — ") + EndTitle);
}

void ADockGameMode::DoAct(EDockAct A)
{
	const UDockTuning* T = Tun();

	if (A == EDockAct::Restart) { Audio.Play(EDockSnd::Click, 0.6f); ResetGame(); return; }
	if (A == EDockAct::Quit)
	{
		UKismetSystemLibrary::QuitGame(this, PC(), EQuitPreference::Quit, false);
		return;
	}
	if (A == EDockAct::CloseRecord) { bRecordOpen = false; Audio.Play(EDockSnd::Paper, 0.4f); return; }
	if (Phase != EDockPhase::Play || bCaught) return;
	if (A == EDockAct::ViewRecord)
	{
		if (!bHasRecord) return;
		bRecordOpen = true;
		Audio.Play(EDockSnd::Paper, 0.6f);
		return;
	}
	if (IsBusy()) return;

	Audio.Play(EDockSnd::Click, 0.6f);
	const bool bRises = Stage > 0 && A != EDockAct::Request && A != EDockAct::ExitFront && A != EDockAct::ExitBack;

	switch (A)
	{
	case EDockAct::Approach:
		SpendTime(TEXT("관리인에게 다가감"));
		MoveTo(Me, { ClerkSpot }, T->MoveSpeed, true, [this]()
		{
			bNear = true;
			if (Me) Me->SetActorRotation(FRotator(0.f, -90.f, 0.f));
			Say(TEXT("관리인이 장부에서 눈을 든다."));
		});
		break;

	case EDockAct::Request:
		bRequested = true;
		SpendTime(TEXT("3번 창고 반출 기록을 요청함"));
		Stage = FMath::Max(Stage, T->ReportOnRequest);
		Audio.Play(EDockSnd::Pen);
		ClerkStatus = TEXT("장부에 적음");
		Say(TEXT("\"반출 기록이요? …잠깐만요.\" 관리인이 장부에 무언가 적는다."));
		After(1.5f, [this]()
		{
			Audio.Play(EDockSnd::Paper);
			if (!Record || !Me) return;
			const FVector From = Record->GetActorLocation();
			const FVector To = Me->GetActorLocation() + FVector(0, 0, 110);
			Anims.Add([this, From, To, t = 0.f](float Dt) mutable -> bool
			{
				t = FMath::Min(1.f, t + Dt / 0.8f);
				const float E = Smooth(t);
				if (Record)
				{
					Record->SetActorLocation(FMath::Lerp(From, To, E) + FVector(0, 0, FMath::Sin(t * UE_PI) * 120.f));
					Record->SetActorRotation(FRotator(0.f, 15.f + t * 300.f, FMath::Sin(t * UE_PI) * 25.f));
				}
				if (t >= 1.f)
				{
					bHasRecord = true;
					if (Record) Record->SetActorHiddenInGame(true);
					Say(TEXT("반출 기록을 받았다 — 오른쪽 위 「기록 보기」"));
					return true;
				}
				return false;
			});
		});
		After(3.6f, [this]() { if (ClerkStatus == TEXT("장부에 적음")) ClerkStatus.Reset(); }, false);
		break;

	case EDockAct::AskMore:
		SpendTime(TEXT("관리인에게 더 물음"));
		if (!bHasRecord) Say(TEXT("\"…기록을 보려면 요청서를 내셔야 해요.\""));
		else
		{
			static const TCHAR* Ans[] = {
				TEXT("\"그 배는 조합 쪽 물건이 아니에요.\""),
				TEXT("\"서명은… 나중에 받기로 했다던데요.\""),
				TEXT("\"더 알고 싶으면 사무실로 정식으로 오세요.\"") };
			Say(Ans[FMath::Min(AskCount, 2)]);
			++AskCount;
		}
		break;

	case EDockAct::Contact:
		if (bContacted || !bHasRecord) return;
		bContacted = true;
		bRecordOpen = false;
		SpendTime(TEXT("기록을 근거로 조합에 연락함"));
		Say(TEXT("조합에 전화했다 — \"정문 앞으로 사람을 보내겠다.\""));
		After(1.2f, [this]()
		{
			if (!Union) return;
			bUnionHere = true;
			Union->SetActorHiddenInGame(false);
			Union->SetActorEnableCollision(true);
			Union->SetActorScale3D(FVector(0.6f));
			Audio.Play(EDockSnd::CarDoor);
			Anims.Add([this, t = 0.f](float Dt) mutable -> bool
			{
				t = FMath::Min(1.f, t + Dt / 0.35f);
				const float S = 0.6f + 0.4f * t + FMath::Sin(t * UE_PI) * 0.08f;
				if (Union) Union->SetActorScale3D(FVector(S));
				return t >= 1.f;
			});
		});
		After(1.4f, [this]() { Audio.Play(EDockSnd::CarDoor, 0.7f); });
		After(1.7f, [this]() { Audio.Play(EDockSnd::Horn); Say(TEXT("정문 밖에 조합 승합차가 섰다.")); });
		After(2.1f, [this]()
		{
			if (bCaught || !Guard) return;
			bGuardAtFront = true;
			Say(TEXT("경비가 무전기를 쥐고 정문으로 달려간다."));
			MoveTo(Guard, { FVector(900, 250, 0), FVector(-1720, 60, 0) }, Tun()->GuardSpeed, true, [this]()
			{
				if (Guard) Guard->SetActorRotation(FRotator(0.f, 180.f, 0.f));
			});
		});
		After(3.6f, [this]() { Audio.Play(EDockSnd::Clank); });
		After(4.0f, [this]()
		{
			if (!Back || !Back->DoorPivot) return;
			Audio.Play(EDockSnd::Creak);
			bBackOpen = true;
			if (Back->Lamp) Back->Lamp->SetIntensity(Tun()->LampCandela * 1.5f);
			Say(TEXT("후문 빗장이 풀렸다 — 조합 쪽 누군가 열어 두었다."));
			Anims.Add([this, t = 0.f](float Dt) mutable -> bool
			{
				t = FMath::Min(1.f, t + Dt / 1.4f);
				const float E = 1.f - FMath::Square(1.f - t);
				if (Back && Back->DoorPivot) Back->DoorPivot->SetRelativeRotation(FRotator(0.f, -100.f * E, 0.f));
				return t >= 1.f;
			});
		});
		break;

	case EDockAct::ExitFront:
		SpendTime(TEXT("정문으로 나감"));
		Selected = nullptr;
		if (bGuardAtFront)
		{
			MoveTo(Me, { FrontAt + FVector(450, 0, 0) }, T->MoveSpeed, true, [this]()
			{
				StartCaught(TEXT("정문 앞을 경비가 막고 있었다."), 0.2f);
			});
		}
		else
		{
			MoveTo(Me, { FrontAt, FrontAt + FVector(-220, 0, 0) }, T->MoveSpeed, true, [this]()
			{
				EndGame(bHasRecord ? EDockEnd::A : EDockEnd::D);
			});
		}
		break;

	case EDockAct::ExitBack:
		if (!bBackOpen) return;
		SpendTime(TEXT("후문으로 나감"));
		Selected = nullptr;
		MoveTo(Me, { FVector(1700, -500, 0), FVector(2250, -500, 0) }, T->MoveSpeed, true, [this]()
		{
			EndGame(EDockEnd::B);
		});
		break;

	default:
		return;
	}

	if (bRises) RaiseStage(T->ReportPerTimeAction);
}

// ---------------------------------------------------------------- capture

void ADockGameMode::OnShot(int32 W, int32 H, const TArray<FColor>& Colors)
{
	if (!bShotPending || bCapDone) return;
	const FString Path = FPaths::Combine(CapDir, FString::Printf(TEXT("f_%05d.jpg"), CapFrame));
	FImageView View(Colors.GetData(), W, H, EGammaSpace::sRGB);
	FImageUtils::SaveImageAutoFormat(*Path, View, 92);
	++CapFrame;
	bShotPending = false;
	if (CapFrame >= CapMaxFrames) FinishCapture();
}

void ADockGameMode::FinishCapture()
{
	if (bCapDone) return;
	bCapDone = true;
	if (bCapture)
	{
		const int32 DataBytes = CapAudio.Num() * 2;
		TArray<uint8> Wav;
		Wav.Reserve(44 + DataBytes);
		auto Put32 = [&Wav](uint32 V) { for (int32 i = 0; i < 4; ++i) Wav.Add(uint8((V >> (8 * i)) & 0xFF)); };
		auto Put16 = [&Wav](uint16 V) { Wav.Add(uint8(V & 0xFF)); Wav.Add(uint8(V >> 8)); };
		auto Tag = [&Wav](const char* S) { for (int32 i = 0; i < 4; ++i) Wav.Add(uint8(S[i])); };
		Tag("RIFF"); Put32(36 + DataBytes); Tag("WAVE");
		Tag("fmt "); Put32(16); Put16(1); Put16(1); Put32(FDockAudio::Rate); Put32(FDockAudio::Rate * 2); Put16(2); Put16(16);
		Tag("data"); Put32(DataBytes);
		const int32 Old = Wav.AddUninitialized(DataBytes);
		FMemory::Memcpy(Wav.GetData() + Old, CapAudio.GetData(), DataBytes);
		FFileHelper::SaveArrayToFile(Wav, *FPaths::Combine(CapDir, TEXT("audio.wav")));
		const FString Info = FString::Printf(TEXT("frames=%d\nfps=30\nround=%d\n"), CapFrame, Round);
		FFileHelper::SaveStringToFile(Info, *FPaths::Combine(CapDir, TEXT("done.txt")));
	}
	FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------- autoplay

void ADockGameMode::BuildScript()
{
	auto W = [this](float T) { Script.Add(FStep{ EStepKind::Wait, T, EWho::None, EDockAct::None }); };
	auto I = [this]() { Script.Add(FStep{ EStepKind::Idle, 0.4f, EWho::None, EDockAct::None }); };
	auto To = [this](EWho Who) { Script.Add(FStep{ EStepKind::ToThing, 0.7f, Who, EDockAct::None }); Script.Add(FStep{ EStepKind::Click, 0.35f, EWho::None, EDockAct::None }); };
	auto Btn = [this](EDockAct A) { Script.Add(FStep{ EStepKind::ToButton, 0.6f, EWho::None, A }); Script.Add(FStep{ EStepKind::Click, 0.35f, EWho::None, EDockAct::None }); };

	// Round 1: ask one question too many and get caught.
	W(2.0f);
	To(EWho::Clerk); Btn(EDockAct::Approach); I();
	Btn(EDockAct::Request); I();
	Btn(EDockAct::ViewRecord); W(2.2f); Btn(EDockAct::CloseRecord);
	Btn(EDockAct::AskMore); I(); W(1.4f);
	Btn(EDockAct::ViewRecord); W(1.0f); Btn(EDockAct::Contact);
	I(); W(4.0f);
	Btn(EDockAct::Restart); W(1.8f);

	// Round 2: record, union call, out through the back gate.
	To(EWho::Clerk); Btn(EDockAct::Approach); I();
	Btn(EDockAct::Request); I();
	Btn(EDockAct::ViewRecord); W(1.6f); Btn(EDockAct::Contact);
	I(); W(0.6f);
	To(EWho::Back); Btn(EDockAct::ExitBack);
	I(); W(4.5f);
}

void ADockGameMode::NextStep()
{
	++ScriptIdx;
	StepT = 0.f; IdleT = 0.f; FailT = 0.f; bStepAimed = false;
	StepFrom = Pointer;
}

void ADockGameMode::RunScript(float Dt)
{
	if (ScriptIdx >= Script.Num())
	{
		if (bCapture) FinishCapture();
		return;
	}
	if (ScriptIdx == 0 && StepT == 0.f && Pointer.X < 0.f)
	{
		float VX = 1280.f, VY = 720.f;
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			VX = GEngine->GameViewport->Viewport->GetSizeXY().X;
			VY = GEngine->GameViewport->Viewport->GetSizeXY().Y;
		}
		Pointer = FVector2D(VX * 0.55f, VY * 0.62f);
		StepFrom = Pointer;
	}

	const FStep S = Script[ScriptIdx];
	StepT += Dt;

	auto Glide = [this, &S](const FVector2D& Target)
	{
		if (!bStepAimed) { bStepAimed = true; StepFrom = Pointer; StepT = 0.f; }
		const float A = Smooth(StepT / S.T);
		PointerMove(FMath::Lerp(StepFrom, Target, A));
		if (StepT >= S.T + 0.12f) NextStep();
	};

	switch (S.K)
	{
	case EStepKind::Wait:
		PointerMove(Pointer);
		if (StepT >= S.T) NextStep();
		break;

	case EStepKind::Idle:
	{
		PointerMove(Pointer);
		const bool bQuiet = Phase == EDockPhase::Ending
			|| (Phase == EDockPhase::Play && !IsBusy() && !IsMoving(Guard) && Anims.Num() == 0);
		IdleT = bQuiet ? IdleT + Dt : 0.f;
		if (IdleT >= S.T || StepT > 25.f) NextStep();
		break;
	}

	case EStepKind::ToThing:
	{
		ADockThing* T = Me.Get();
		if (S.Who == EWho::Clerk) T = Clerk.Get();
		else if (S.Who == EWho::Front) T = Front.Get();
		else if (S.Who == EWho::Back) T = Back.Get();
		if (!T) { NextStep(); break; }
		Glide(ScreenOf(T->GetActorLocation() + T->LabelOffset * 0.6f));
		break;
	}

	case EStepKind::ToButton:
	{
		const FDockButton* Found = Buttons.FindByPredicate([&S](const FDockButton& B) { return B.Act == S.Act && B.bEnabled; });
		if (!Found)
		{
			PointerMove(Pointer);
			FailT += Dt;
			StepT = 0.f;
			if (FailT > 4.f) { ++ScriptIdx; NextStep(); } // give up: skip this step and its click
			break;
		}
		Glide(Found->Rect.GetCenter());
		break;
	}

	case EStepKind::Click:
		if (StepT <= Dt + UE_KINDA_SMALL_NUMBER) PointerClick(Pointer);
		PointerMove(Pointer);
		if (StepT >= S.T) NextStep();
		break;

	case EStepKind::KeyR:
		Key(EKeys::R);
		NextStep();
		break;
	}
}
