// The whole graybox loop: one dock, one clerk, report stage 0..3, four endings.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/GameModeBase.h"
#include "DockAudio.h"
#include "DockGameMode.generated.h"

class ADockThing; class ADockStage; class ADockPlayer; class ADockClerk; class ADockGuard;
class ADockGate; class ADockUnion; class ADockRecord; class ADockShip;
class ACameraActor; class UDirectionalLightComponent; class USkyLightComponent;
class UExponentialHeightFogComponent; class USoundWaveProcedural; class UAudioComponent;
class UDockTuning;

enum class EDockPhase : uint8 { Intro, Play, Ending };
enum class EDockAct : uint8 { None, Approach, Request, AskMore, Contact, ViewRecord, CloseRecord, ExitFront, ExitBack, Restart, Quit };
enum class EDockEnd : uint8 { None, A, B, C, D };

struct FDockButton
{
	FBox2D Rect = FBox2D(ForceInit);
	EDockAct Act = EDockAct::None;
	bool bEnabled = true;
};

UCLASS()
class GAMCHALGWAN_API ADockGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADockGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	// No pawn and no spectator: the camera is fixed and the player is a dock actor.
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	// Input (real mouse via the player controller, or the autoplay script).
	void PointerMove(const FVector2D& P);
	void PointerClick(const FVector2D& P);
	void Key(const FKey& K);

	// Read by the HUD.
	const UDockTuning* Tun() const;
	FString Clock(int32 Min) const;
	void PanelActs(TArray<EDockAct>& Out) const;
	FString ActLabel(EDockAct A) const;
	FString ActTip(EDockAct A) const;
	bool ActCostsTime(EDockAct A) const;
	FString DescFor(const ADockThing* T) const;
	bool IsBusy() const;
	FVector2D ScreenOf(const FVector& World, bool* bOk = nullptr) const;

	EDockPhase Phase = EDockPhase::Intro;
	EDockEnd Ending = EDockEnd::None;
	float PhaseTime = 0.f;
	float SimTime = 0.f;
	int32 Minutes = 0;
	int32 Stage = 0;
	int32 AskCount = 0;
	bool bNear = false, bRequested = false, bHasRecord = false, bContacted = false;
	bool bUnionHere = false, bBackOpen = false, bGuardAtFront = false, bCaught = false;
	bool bRecordOpen = false, bDebug = false, bLampBlink = false;
	bool bAutoplay = false, bCapture = false;
	float Flash = 0.f;
	FVector2D Pointer = FVector2D(-100, -100);
	FString ClerkStatus;
	FString EndTitle, EndLine;
	struct FLine { FString Text; float Time; };
	TArray<FLine> Lines;
	struct FLog { int32 Min; FString Text; };
	TArray<FLog> Log;
	TArray<FDockButton> Buttons; // rebuilt by the HUD every frame
	TArray<FBox2D> UiRects;      // panels drawn by the HUD; clicks there never pick the world
	float Fps = 0.f;
	int32 Round = 0;

	UPROPERTY() TObjectPtr<ADockThing> Hover;
	UPROPERTY() TObjectPtr<ADockThing> Selected;
	UPROPERTY() TArray<TObjectPtr<ADockThing>> Things;
	UPROPERTY() TObjectPtr<ADockPlayer> Me;
	UPROPERTY() TObjectPtr<ADockClerk> Clerk;
	UPROPERTY() TObjectPtr<ADockGuard> Guard;
	UPROPERTY() TObjectPtr<ADockGate> Front;
	UPROPERTY() TObjectPtr<ADockGate> Back;
	UPROPERTY() TObjectPtr<ADockUnion> Union;
	UPROPERTY() TObjectPtr<ADockRecord> Record;
	UPROPERTY() TObjectPtr<ADockShip> Ship;
	UPROPERTY() TObjectPtr<ADockStage> Yard;

private:
	void BuildLook();
	void SpawnRound();
	void ClearRound();
	void ResetGame();
	void Step(float Dt);
	void DoAct(EDockAct A);
	void Say(const FString& S);
	void AddLog(const FString& S);
	void SpendTime(const FString& LogText);
	void RaiseStage(int32 By);
	void StartCaught(const FString& Line, float Delay);
	void EndGame(EDockEnd E);
	void After(float Delay, TFunction<void()> Fn, bool bBlock = true);
	void MoveTo(AActor* A, TArray<FVector> Path, float Speed, bool bSteps, TFunction<void()> Done);
	bool IsMoving(const AActor* A) const;
	void CancelMoves(const AActor* A);
	ADockThing* PickAt(const FVector2D& P) const;
	bool InUi(const FVector2D& P) const;
	APlayerController* PC() const;
	template<class T> T* SpawnThing(const FVector& Loc, float Yaw = 0.f);

	// Audio
	void FeedAudio();
	void OnShot(int32 W, int32 H, const TArray<FColor>& Colors);
	void FinishCapture();

	// Autoplay script
	enum class EStepKind : uint8 { Wait, Idle, ToThing, ToButton, Click, KeyR };
	enum class EWho : uint8 { None, Clerk, Front, Back, Me };
	struct FStep { EStepKind K; float T; EWho Who; EDockAct Act; };
	void BuildScript();
	void RunScript(float Dt);
	void NextStep();
	TArray<FStep> Script;
	int32 ScriptIdx = 0;
	float StepT = 0.f;
	float IdleT = 0.f;
	float FailT = 0.f;
	bool bStepAimed = false;
	FVector2D StepFrom = FVector2D::ZeroVector;

	struct FEvent { float At; TFunction<void()> Fn; bool bBlock; };
	TArray<FEvent> Events;
	struct FMove { TWeakObjectPtr<AActor> A; TArray<FVector> Path; float Speed; bool bSteps; float StepAcc; TFunction<void()> Done; };
	TArray<FMove> Moves;
	TArray<TFunction<bool(float)>> Anims; // return true when finished

	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	UPROPERTY() TObjectPtr<ADockThing> LookHost;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY() TObjectPtr<USkyLightComponent> Sky;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<USoundWaveProcedural> Wave;
	UPROPERTY() TObjectPtr<UAudioComponent> AudioComp;
	FVector CamBase = FVector::ZeroVector;
	float Shake = 0.f;
	FDockAudio Audio;
	TArray<int16> CapAudio;
	FString CapDir;
	int32 CapFrame = 0;
	int32 CapMaxFrames = 6000;
	bool bShotPending = false;
	double ShotAsked = 0.0;
	bool bCapDone = false;
	FDelegateHandle ShotHandle;
};
