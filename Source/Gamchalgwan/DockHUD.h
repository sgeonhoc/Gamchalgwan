// All on-screen UI for the dock graybox, drawn with Canvas (no UMG, no assets).
// Every clickable thing is registered in ADockGameMode::Buttons each frame, so mouse and autoplay share one path.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Fonts/SlateFontInfo.h"
#include "DockGameMode.h"
#include "DockHUD.generated.h"

struct FCompositeFont;

UCLASS()
class GAMCHALGWAN_API ADockHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

private:
	void MakeFonts();
	FSlateFontInfo Font(float Pt, bool bBold = false) const;
	FVector2D Measure(const FString& Str, const FSlateFontInfo& Fi) const;

	// Drawing helpers. Positions and sizes are in 1280x720 design units.
	void Text(const FString& Str, const FVector2D& Pos, const FSlateFontInfo& Fi, const FLinearColor& Col, bool bShadow = true);
	void Center(const FString& Str, float Y, const FSlateFontInfo& Fi, const FLinearColor& Col);
	void Box(const FVector2D& Pos, const FVector2D& Dim, const FLinearColor& Col);
	void Edge(const FVector2D& Pos, const FVector2D& Dim, const FLinearColor& Col, float Thick);
	void Tri(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FLinearColor& Col);
	// Draws a button and registers it. Returns true while the pointer is over it.
	bool Button(ADockGameMode* GM, EDockAct Act, const FString& Caption, const FVector2D& Pos, const FVector2D& Dim,
		bool bEnabled, const FString& Note, bool bPaper);

	void DrawLabels(ADockGameMode* GM);
	void DrawTop(ADockGameMode* GM);
	void DrawPanel(ADockGameMode* GM);
	void DrawLines(ADockGameMode* GM);
	void DrawRecord(ADockGameMode* GM);
	void DrawEnding(ADockGameMode* GM);
	void DrawIntro(ADockGameMode* GM);
	void DrawDebug(ADockGameMode* GM);
	void DrawCursor(ADockGameMode* GM);

	TSharedPtr<FCompositeFont> Composite;
	// Canvas draws nothing for an FSlateFontInfo that only holds a bare composite font,
	// but it does draw runtime UFonts (the engine's own Roboto works). So the composite is wrapped in one.
	UPROPERTY(Transient)
	TObjectPtr<class UFont> UiFont;
	FString Version;
	float S = 1.f;          // pixels per design unit
	float VW = 1280.f;      // design width of the current viewport
	float PanelTop = 664.f; // top of the selection panel this frame (narration sits above it)
};
