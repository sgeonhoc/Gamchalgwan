#include "DockHUD.h"
#include "DockGameMode.h"
#include "DockThings.h"
#include "DockTuning.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineFontServices.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "GlobalRenderResources.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace DockHudStyle
{
	const FLinearColor White(0.96f, 0.97f, 0.98f, 1.f);
	const FLinearColor Accent(0.95f, 0.75f, 0.3f, 1.f);
	const FLinearColor Gold(0.96f, 0.84f, 0.55f, 1.f);
	const FLinearColor Grey(0.7f, 0.72f, 0.77f, 1.f);
	const FLinearColor Ink(0.13f, 0.12f, 0.11f, 1.f);
	const FLinearColor InkSoft(0.38f, 0.36f, 0.33f, 1.f);
	const FLinearColor InkRed(0.62f, 0.12f, 0.1f, 1.f);
	const FLinearColor PaperCol(0.86f, 0.84f, 0.78f, 1.f);
	const float H = 720.f;

	FLinearColor WithA(const FLinearColor& C, float Alpha) { return FLinearColor(C.R, C.G, C.B, C.A * Alpha); }
	int32 B(bool b) { return b ? 1 : 0; }
}
using namespace DockHudStyle;

// ---------------------------------------------------------------- fonts

void ADockHUD::MakeFonts()
{
	if (Composite.IsValid()) return;
	// Latin from Roboto, Hangul from Droid Sans Fallback. Both ship loose in Content/Fonts (see DefaultGame.ini).
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Fonts"));
	TSharedRef<FCompositeFont> CF = MakeShared<FCompositeFont>();
	CF->DefaultTypeface.AppendFont(TEXT("Regular"), Dir / TEXT("Roboto-Regular.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	CF->DefaultTypeface.AppendFont(TEXT("Bold"), Dir / TEXT("Roboto-Bold.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	CF->FallbackTypeface.Typeface.AppendFont(TEXT("Regular"), Dir / TEXT("DroidSansFallback.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	CF->FallbackTypeface.Typeface.AppendFont(TEXT("Bold"), Dir / TEXT("DroidSansFallback.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	Composite = CF;

	UiFont = NewObject<UFont>(this, TEXT("DockUiFont"), RF_Transient);
	UiFont->FontCacheType = EFontCacheType::Runtime;
	UiFont->CompositeFont = *CF;
	UiFont->LegacyFontSize = 16;
	UiFont->LegacyFontName = TEXT("Regular");

	if (!GConfig || !GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni) || Version.IsEmpty())
		Version = TEXT("0.1.0");
}

FSlateFontInfo ADockHUD::Font(float Pt, bool bBold) const
{
	const float Px = FMath::RoundToFloat(Pt * S * 2.f) * 0.5f;
	return FSlateFontInfo(UiFont.Get(), Px, bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
}

FVector2D ADockHUD::Measure(const FString& Str, const FSlateFontInfo& Fi) const
{
	const TSharedPtr<FSlateFontMeasure> M = FEngineFontServices::Get().GetFontMeasure();
	if (!M.IsValid() || S <= 0.f) return FVector2D(Str.Len() * 9.0, 18.0);
	return FVector2D(M->Measure(Str, Fi, 1.f)) / S;
}

// ---------------------------------------------------------------- primitives

void ADockHUD::Text(const FString& Str, const FVector2D& Pos, const FSlateFontInfo& Fi, const FLinearColor& Col, bool bShadow)
{
	if (Str.IsEmpty() || Col.A <= 0.f) return;
	FCanvasTextItem T(Pos * S, FText::FromString(Str), Fi, Col);
	if (bShadow) T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f * Col.A), FVector2D(1.0, 1.0) * FMath::Max(1.f, S));
	Canvas->DrawItem(T);
}

void ADockHUD::Center(const FString& Str, float Y, const FSlateFontInfo& Fi, const FLinearColor& Col)
{
	const FVector2D TS = Measure(Str, Fi);
	Text(Str, FVector2D(VW * 0.5 - TS.X * 0.5, Y), Fi, Col, true);
}

void ADockHUD::Box(const FVector2D& Pos, const FVector2D& Dim, const FLinearColor& Col)
{
	if (Col.A <= 0.f) return;
	FCanvasTileItem T(Pos * S, Dim * S, Col);
	T.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(T);
}

void ADockHUD::Edge(const FVector2D& Pos, const FVector2D& Dim, const FLinearColor& Col, float Thick)
{
	FCanvasBoxItem Bx(Pos * S, Dim * S);
	Bx.LineThickness = Thick * S;
	Bx.SetColor(Col);
	Bx.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Bx);
}

void ADockHUD::Tri(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FLinearColor& Col)
{
	FCanvasTriangleItem T(P0 * S, P1 * S, P2 * S, GWhiteTexture);
	for (FCanvasUVTri& U : T.TriangleList) { U.V0_Color = Col; U.V1_Color = Col; U.V2_Color = Col; }
	T.SetColor(Col);
	T.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(T);
}

bool ADockHUD::Button(ADockGameMode* GM, EDockAct Act, const FString& Caption, const FVector2D& Pos, const FVector2D& Dim,
	bool bEnabled, const FString& Note, bool bPaper)
{
	FDockButton Btn;
	Btn.Rect = FBox2D(Pos * S, (Pos + Dim) * S);
	Btn.Act = Act;
	Btn.bEnabled = bEnabled;
	GM->Buttons.Add(Btn);
	const bool bHov = bEnabled && Btn.Rect.IsInside(GM->Pointer);

	FLinearColor Bg, Line, Fg;
	if (bPaper)
	{
		Bg = bHov ? FLinearColor(0.32f, 0.28f, 0.22f, 1.f) : FLinearColor(0.18f, 0.16f, 0.14f, 1.f);
		Line = bHov ? Accent : FLinearColor(0.1f, 0.09f, 0.08f, 1.f);
		Fg = PaperCol;
	}
	else
	{
		Bg = bHov ? FLinearColor(0.24f, 0.26f, 0.32f, 0.97f) : FLinearColor(0.11f, 0.12f, 0.15f, 0.95f);
		Line = bHov ? Accent : FLinearColor(0.36f, 0.38f, 0.44f, 0.9f);
		Fg = White;
	}
	if (!bEnabled)
	{
		Bg = WithA(Bg, 0.6f);
		Line = FLinearColor(0.3f, 0.3f, 0.32f, 0.6f);
		Fg = FLinearColor(0.5f, 0.5f, 0.52f, 1.f);
	}
	Box(Pos, Dim, Bg);
	Edge(Pos, Dim, Line, bHov ? 1.5f : 1.f);

	const FSlateFontInfo FL = Font(12.5f, true);
	const FVector2D LS = Measure(Caption, FL);
	if (Note.IsEmpty())
	{
		Text(Caption, Pos + (Dim - LS) * 0.5, FL, Fg, false);
	}
	else
	{
		Text(Caption, FVector2D(Pos.X + 14.0, Pos.Y + (Dim.Y - LS.Y) * 0.5), FL, Fg, false);
		const FSlateFontInfo FN = Font(10.5f);
		const FVector2D NS = Measure(Note, FN);
		Text(Note, FVector2D(Pos.X + Dim.X - 12.0 - NS.X, Pos.Y + (Dim.Y - NS.Y) * 0.5), FN, WithA(Fg, 0.6f), false);
	}
	return bHov;
}

// ---------------------------------------------------------------- frame

void ADockHUD::DrawHUD()
{
	Super::DrawHUD();
	ADockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADockGameMode>() : nullptr;
	if (!GM || !Canvas || Canvas->ClipY <= 0.f) return;

	S = Canvas->ClipY / H;
	VW = Canvas->ClipX / S;
	MakeFonts();
	PanelTop = H - 56.f;
	GM->Buttons.Reset();
	GM->UiRects.Reset();

	const bool bEnding = GM->Phase == EDockPhase::Ending;

	// Back to front. A click goes to the last registered button under the pointer (see PointerClick).
	if (!bEnding)
	{
		if (!GM->bRecordOpen)
		{
			DrawLabels(GM);
			if (GM->Phase == EDockPhase::Play) DrawPanel(GM);
		}
		DrawTop(GM);
		DrawLines(GM);
		Text(TEXT("클릭 — 대상 고르기 · R 다시 · Esc 끝내기 · F1 개발 정보"), FVector2D(16.0, H - 28.0), Font(10.5f), WithA(Grey, 0.9f));
	}
	if (GM->bRecordOpen) DrawRecord(GM);
	if (bEnding) DrawEnding(GM);

	if (GM->Flash > 0.f) Box(FVector2D::ZeroVector, FVector2D(VW, H), FLinearColor(0.75f, 0.05f, 0.03f, 0.42f * GM->Flash));
	DrawIntro(GM);
	if (GM->bDebug) DrawDebug(GM);
	if (GM->bAutoplay) DrawCursor(GM);

	// Flush newly requested glyphs to the font atlas this frame. Not the cause of the 0.1.0 invisible-text bug
	// (that was the bare composite font, see UiFont), but it was in the capture that proved the fix, so it stays.
	if (FEngineFontServices::IsInitialized()) FEngineFontServices::Get().UpdateCache();
}

// ---------------------------------------------------------------- parts

void ADockHUD::DrawLabels(ADockGameMode* GM)
{
	for (ADockThing* T : GM->Things)
	{
		if (!T || T->IsHidden() || T->Title.IsEmpty()) continue;
		bool bOk = false;
		const FVector2D Px = GM->ScreenOf(T->GetActorLocation() + T->LabelOffset, &bOk);
		if (!bOk) continue;
		const FVector2D At = Px / S;
		const bool bSel = T == GM->Selected.Get();
		const bool bHov = !bSel && T == GM->Hover.Get();

		const FSlateFontInfo FL = Font(12.f, bSel);
		const FVector2D TS = Measure(T->Title, FL);
		const FVector2D Dim(TS.X + 18.0, TS.Y + 6.0);
		FVector2D P(At.X - Dim.X * 0.5, At.Y - Dim.Y - 9.0);
		P.X = FMath::Clamp<double>(P.X, 6.0, VW - Dim.X - 6.0);
		P.Y = FMath::Clamp<double>(P.Y, 6.0, H - Dim.Y - 6.0);

		const FLinearColor Bg = bSel ? Accent : (bHov ? FLinearColor(0.26f, 0.28f, 0.34f, 0.95f) : FLinearColor(0.03f, 0.04f, 0.06f, 0.74f));
		const FLinearColor Fg = bSel ? Ink : (bHov ? White : FLinearColor(0.86f, 0.88f, 0.92f, 1.f));
		Box(P, Dim, Bg);
		if (bHov) Edge(P, Dim, WithA(Accent, 0.8f), 1.f);
		const double TipX = FMath::Clamp<double>(At.X, P.X + 8.0, P.X + Dim.X - 8.0);
		Tri(FVector2D(TipX - 6.0, P.Y + Dim.Y), FVector2D(TipX + 6.0, P.Y + Dim.Y), FVector2D(TipX, P.Y + Dim.Y + 7.0), Bg);
		Text(T->Title, P + FVector2D(9.0, 3.0), FL, Fg, false);

		if (T == GM->Clerk.Get() && !GM->ClerkStatus.IsEmpty())
		{
			const bool bPhone = GM->ClerkStatus == TEXT("전화 중");
			const FSlateFontInfo FS = Font(11.f, true);
			const FVector2D SS = Measure(GM->ClerkStatus, FS);
			const FVector2D SDim(SS.X + 14.0, SS.Y + 4.0);
			const FVector2D SP(P.X + Dim.X * 0.5 - SDim.X * 0.5, P.Y - SDim.Y - 4.0);
			const float Pulse = bPhone ? 0.7f + 0.3f * FMath::Sin(GM->SimTime * 8.f) : 1.f;
			Box(SP, SDim, bPhone ? FLinearColor(0.78f, 0.18f, 0.14f, 0.95f * Pulse) : FLinearColor(0.55f, 0.42f, 0.12f, 0.92f));
			Text(GM->ClerkStatus, SP + FVector2D(7.0, 2.0), FS, White, false);
		}
	}
}

void ADockHUD::DrawTop(ADockGameMode* GM)
{
	const FVector2D P(16.0, 14.0), Dim(480.0, 90.0);
	Box(P, Dim, FLinearColor(0.02f, 0.025f, 0.04f, 0.76f));
	Box(P, FVector2D(3.0, Dim.Y), Accent);
	GM->UiRects.Add(FBox2D(P * S, (P + Dim) * S));

	Text(TEXT("11월 5일 밤 · 부두 3번 창고"), P + FVector2D(16.0, 8.0), Font(11.f), Grey);
	const FSlateFontInfo FC = Font(21.f, true);
	const FString Clk = GM->Clock(GM->Minutes);
	Text(Clk, P + FVector2D(15.0, 25.0), FC, White);
	if (GM->Minutes > 0)
	{
		const FVector2D CS = Measure(Clk, FC);
		Text(FString::Printf(TEXT("들어온 지 %d분"), GM->Minutes), P + FVector2D(15.0 + CS.X + 10.0, 36.0), Font(11.f), FLinearColor(0.62f, 0.64f, 0.7f, 1.f));
	}
	const FString Goal = GM->bHasRecord ? TEXT("기록을 들고 부두를 빠져나간다") : TEXT("3번 창고 반출 기록을 손에 넣고 부두를 빠져나간다");
	Text(Goal, P + FVector2D(16.0, 64.0), Font(12.f), Gold);

	const FSlateFontInfo FV = Font(10.f);
	const FString Ver = FString::Printf(TEXT("v%s 그레이박스"), *Version);
	const FVector2D VS = Measure(Ver, FV);
	Text(Ver, FVector2D(VW - 16.0 - VS.X, 12.0), FV, WithA(Grey, 0.85f));

	if (GM->bHasRecord && GM->Phase == EDockPhase::Play && !GM->bRecordOpen)
	{
		const FVector2D BP(VW - 16.0 - 160.0, 34.0), BD(160.0, 40.0);
		if (Button(GM, EDockAct::ViewRecord, GM->ActLabel(EDockAct::ViewRecord), BP, BD, !GM->bCaught, FString(), false))
		{
			const FSlateFontInfo FT = Font(11.f);
			const FString Tip = GM->ActTip(EDockAct::ViewRecord);
			const FVector2D TS = Measure(Tip, FT);
			Text(Tip, FVector2D(VW - 16.0 - TS.X, 80.0), FT, Gold);
		}
		else
		{
			Edge(BP - FVector2D(2.0, 2.0), BD + FVector2D(4.0, 4.0), WithA(Accent, 0.35f + 0.25f * FMath::Sin(GM->SimTime * 3.f)), 1.f);
		}
	}
}

void ADockHUD::DrawPanel(ADockGameMode* GM)
{
	ADockThing* T = GM->Selected.Get();
	if (!T) return;
	TArray<EDockAct> Acts;
	GM->PanelActs(Acts);

	const double PW = 640.0;
	const double PH = Acts.Num() > 0 ? 136.0 : 84.0;
	const FVector2D P((VW - PW) * 0.5, H - 56.0 - PH);
	Box(P, FVector2D(PW, PH), FLinearColor(0.025f, 0.03f, 0.045f, 0.9f));
	Box(P, FVector2D(PW, 2.0), Accent);
	GM->UiRects.Add(FBox2D(P * S, (P + FVector2D(PW, PH)) * S));
	PanelTop = P.Y;

	Text(T->Title, P + FVector2D(20.0, 14.0), Font(15.f, true), White);
	Text(GM->DescFor(T), P + FVector2D(20.0, 44.0), Font(12.f), FLinearColor(0.8f, 0.82f, 0.87f, 1.f));

	const bool bBusy = GM->IsBusy();
	FString Tip;
	double X = P.X + 20.0;
	for (EDockAct Act : Acts)
	{
		const FString Note = GM->ActCostsTime(Act) ? FString::Printf(TEXT("+%d분"), GM->Tun()->MinutesPerAction) : FString();
		if (Button(GM, Act, GM->ActLabel(Act), FVector2D(X, P.Y + PH - 56.0), FVector2D(180.0, 40.0), !bBusy, Note, false))
			Tip = GM->ActTip(Act);
		X += 192.0;
	}
	if (!Tip.IsEmpty())
	{
		const FSlateFontInfo FT = Font(11.f);
		const FVector2D TS = Measure(Tip, FT);
		Text(Tip, FVector2D(VW * 0.5 - TS.X * 0.5, H - 56.0 + 9.0), FT, Gold);
	}
}

void ADockHUD::DrawLines(ADockGameMode* GM)
{
	double Bottom = PanelTop - 12.0;
	const FSlateFontInfo FL = Font(13.f);
	for (int32 i = GM->Lines.Num() - 1; i >= 0; --i)
	{
		const ADockGameMode::FLine& L = GM->Lines[i];
		const float Age = GM->SimTime - L.Time;
		const float Al = FMath::Clamp(Age * 5.f, 0.f, 1.f) * FMath::Clamp((9.f - Age) / 1.5f, 0.f, 1.f);
		if (Al <= 0.f) continue;
		const bool bNew = i == GM->Lines.Num() - 1;
		const FVector2D TS = Measure(L.Text, FL);
		const FVector2D Dim(TS.X + 28.0, TS.Y + 10.0);
		const FVector2D P(VW * 0.5 - Dim.X * 0.5, Bottom - Dim.Y);
		Box(P, Dim, FLinearColor(0.f, 0.f, 0.f, 0.64f * Al));
		Text(L.Text, P + FVector2D(14.0, 5.0), FL, bNew ? WithA(White, Al) : FLinearColor(0.76f, 0.78f, 0.82f, Al), false);
		Bottom = P.Y - 6.0;
	}
}

void ADockHUD::DrawRecord(ADockGameMode* GM)
{
	Box(FVector2D::ZeroVector, FVector2D(VW, H), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	GM->UiRects.Add(FBox2D(FVector2D::ZeroVector, FVector2D(VW, H) * S));

	const FVector2D Dim(520.0, 340.0);
	const FVector2D P((VW - Dim.X) * 0.5, (H - Dim.Y) * 0.5 - 10.0);
	Box(P + FVector2D(6.0, 7.0), Dim, FLinearColor(0.f, 0.f, 0.f, 0.4f));
	Box(P, Dim, PaperCol);
	Text(TEXT("반출 기록 — 11월 5일"), P + FVector2D(28.0, 22.0), Font(16.f, true), Ink, false);
	Text(TEXT("부두 사무소 반출 대장 사본"), P + FVector2D(28.0, 52.0), Font(10.5f), InkSoft, false);
	Box(P + FVector2D(28.0, 76.0), FVector2D(Dim.X - 56.0, 1.0), WithA(InkSoft, 0.7f));

	struct FRow { const TCHAR* K; const TCHAR* V; bool bOdd; };
	const FRow Rows[] = {
		{ TEXT("창고"),      TEXT("3번 창고"),        false },
		{ TEXT("반출 시각"), TEXT("23:40"),           false },
		{ TEXT("품목"),      TEXT("기계 부품 18상자"), false },
		{ TEXT("서명"),      TEXT(""),                true  },
		{ TEXT("조합 확인"), TEXT("없음"),            true  },
	};
	const FSlateFontInfo FK = Font(12.f), FVal = Font(13.f, true);
	double Y = P.Y + 92.0;
	for (const FRow& R : Rows)
	{
		Text(R.K, FVector2D(P.X + 28.0, Y + 1.0), FK, InkSoft, false);
		if (R.V[0] == 0)
		{
			Box(FVector2D(P.X + 150.0, Y + 20.0), FVector2D(170.0, 1.0), Ink);
			Text(TEXT("비어 있음"), FVector2D(P.X + 330.0, Y + 2.0), Font(11.f), InkRed, false);
		}
		else
		{
			Text(R.V, FVector2D(P.X + 150.0, Y), FVal, R.bOdd ? InkRed : Ink, false);
		}
		Y += 30.0;
	}

	const double BY = P.Y + Dim.Y - 22.0 - 40.0;
	FString Tip;
	if (!GM->bContacted)
	{
		const FString Note = FString::Printf(TEXT("+%d분"), GM->Tun()->MinutesPerAction);
		if (Button(GM, EDockAct::Contact, GM->ActLabel(EDockAct::Contact), FVector2D(P.X + 28.0, BY), FVector2D(200.0, 40.0),
			!GM->IsBusy() && !GM->bCaught, Note, true))
			Tip = GM->ActTip(EDockAct::Contact);
	}
	else
	{
		Text(TEXT("조합에는 이미 연락했다."), FVector2D(P.X + 28.0, BY + 11.0), Font(11.f), InkSoft, false);
	}
	Button(GM, EDockAct::CloseRecord, GM->ActLabel(EDockAct::CloseRecord), FVector2D(P.X + Dim.X - 28.0 - 120.0, BY), FVector2D(120.0, 40.0), true, FString(), true);
	if (!Tip.IsEmpty()) Text(Tip, FVector2D(P.X + 28.0, P.Y + Dim.Y + 14.0), Font(11.f), Gold);
}

void ADockHUD::DrawEnding(ADockGameMode* GM)
{
	const float Al = FMath::Clamp(GM->PhaseTime / 0.8f, 0.f, 1.f);
	Box(FVector2D::ZeroVector, FVector2D(VW, H), FLinearColor(0.01f, 0.012f, 0.02f, 0.9f * Al));
	GM->UiRects.Add(FBox2D(FVector2D::ZeroVector, FVector2D(VW, H) * S));

	FLinearColor TC = Grey;
	switch (GM->Ending)
	{
	case EDockEnd::A: TC = FLinearColor(0.96f, 0.8f, 0.42f, 1.f); break;
	case EDockEnd::B: TC = FLinearColor(0.55f, 0.86f, 0.7f, 1.f); break;
	case EDockEnd::C: TC = FLinearColor(0.95f, 0.42f, 0.36f, 1.f); break;
	default: break;
	}
	Center(GM->EndTitle, 150.f, Font(24.f, true), WithA(TC, Al));
	Center(GM->EndLine, 200.f, Font(13.f), WithA(White, Al));
	Center(FString::Printf(TEXT("%s → %s · %d분"), *GM->Clock(0), *GM->Clock(GM->Minutes), GM->Minutes), 232.f, Font(11.5f), WithA(Grey, Al));

	const FSlateFontInfo FL = Font(11.5f);
	const int32 First = FMath::Max(0, GM->Log.Num() - 11);
	const double X0 = VW * 0.5 - 210.0;
	double Y = 282.0;
	Box(FVector2D(X0 - 18.0, Y - 14.0), FVector2D(456.0, (GM->Log.Num() - First) * 24.0 + 24.0), FLinearColor(1.f, 1.f, 1.f, 0.05f * Al));
	for (int32 i = First; i < GM->Log.Num(); ++i)
	{
		Text(GM->Clock(GM->Log[i].Min), FVector2D(X0, Y), FL, WithA(FLinearColor(0.6f, 0.62f, 0.68f, 1.f), Al), false);
		Text(GM->Log[i].Text, FVector2D(X0 + 64.0, Y), FL, WithA(FLinearColor(0.88f, 0.9f, 0.94f, 1.f), Al), false);
		Y += 24.0;
	}

	const bool bReady = GM->PhaseTime > 0.5f;
	const double BW = 170.0, Gap = 16.0;
	const double BX = VW * 0.5 - BW - Gap * 0.5;
	Button(GM, EDockAct::Restart, GM->ActLabel(EDockAct::Restart), FVector2D(BX, 600.0), FVector2D(BW, 44.0), bReady, FString(), false);
	Button(GM, EDockAct::Quit, GM->ActLabel(EDockAct::Quit), FVector2D(BX + BW + Gap, 600.0), FVector2D(BW, 44.0), bReady, FString(), false);
}

void ADockHUD::DrawIntro(ADockGameMode* GM)
{
	// Black for the 1.3 s intro, then the title lingers over the yard for the first seconds of play.
	float Black = 0.f, TitleA = 0.f;
	if (GM->Phase == EDockPhase::Intro)
	{
		Black = FMath::Clamp(1.f - (GM->PhaseTime - 0.5f) / 0.8f, 0.f, 1.f);
		TitleA = FMath::Clamp(GM->PhaseTime / 0.3f, 0.f, 1.f);
	}
	else if (GM->Phase == EDockPhase::Play && GM->Minutes == 0 && GM->PhaseTime < 2.4f && !GM->Selected)
	{
		TitleA = FMath::Clamp((2.4f - GM->PhaseTime) / 0.8f, 0.f, 1.f);
	}
	if (Black > 0.f) Box(FVector2D::ZeroVector, FVector2D(VW, H), FLinearColor(0.f, 0.f, 0.f, Black));
	if (TitleA <= 0.f) return;
	const float Y = 300.f;
	Box(FVector2D(0.0, Y - 20.0), FVector2D(VW, 90.0), FLinearColor(0.f, 0.f, 0.f, 0.5f * TitleA));
	Center(TEXT("11월 5일 밤 · 부두 3번 창고"), Y, Font(18.f, true), WithA(White, TitleA));
	Center(TEXT("3번 창고 반출 기록을 손에 넣고 부두를 빠져나간다"), Y + 36.f, Font(12.5f), WithA(Gold, TitleA));
}

void ADockHUD::DrawDebug(ADockGameMode* GM)
{
	const UDockTuning* T = GM->Tun();
	TArray<FString> L;
	L.Add(FString::Printf(TEXT("보고 단계 %d / %d  (요청 시 %d · 행동마다 +%d)"), GM->Stage, T->ReportEnd, T->ReportOnRequest, T->ReportPerTimeAction));
	L.Add(FString::Printf(TEXT("시각 %s  (+%d분 · 행동당 %d분)"), *GM->Clock(GM->Minutes), GM->Minutes, T->MinutesPerAction));
	L.Add(FString::Printf(TEXT("가까이 %d · 요청 %d · 기록 %d · 연락 %d · 더 묻기 %d"), B(GM->bNear), B(GM->bRequested), B(GM->bHasRecord), B(GM->bContacted), GM->AskCount));
	L.Add(FString::Printf(TEXT("조합 %d · 후문 %d · 경비 정문 %d · 잡힘 %d · 바쁨 %d"), B(GM->bUnionHere), B(GM->bBackOpen), B(GM->bGuardAtFront), B(GM->bCaught), B(GM->IsBusy())));
	L.Add(FString::Printf(TEXT("이동 %.0f · 경비 %.0f · 잡힘 지연 %.1f초"), T->MoveSpeed, T->GuardSpeed, T->CaughtDelay));
	L.Add(FString::Printf(TEXT("노출 %.2f · 달 %.2f · 등 %.0f · 하늘 %.2f"), T->ExposureBias, T->MoonLux, T->LampCandela, T->SkyIntensity));
	L.Add(FString::Printf(TEXT("fps %.0f · 판 %d%s"), GM->Fps, GM->Round, GM->bAutoplay ? TEXT(" · 자동 플레이") : TEXT("")));
	L.Add(TEXT("다이얼 = Config/DefaultGame.ini [DockTuning]"));

	const double BW = 420.0;
	const FVector2D P(VW - 16.0 - BW, 84.0);
	Box(P, FVector2D(BW, 16.0 + L.Num() * 19.0), FLinearColor(0.f, 0.03f, 0.01f, 0.82f));
	const FSlateFontInfo FD = Font(10.5f);
	double Y = P.Y + 8.0;
	for (const FString& Ln : L)
	{
		Text(Ln, FVector2D(P.X + 12.0, Y), FD, FLinearColor(0.62f, 1.f, 0.72f, 1.f), false);
		Y += 19.0;
	}
}

void ADockHUD::DrawCursor(ADockGameMode* GM)
{
	if (GM->Pointer.X < 0.0) return;
	const FVector2D P = GM->Pointer / S;
	Tri(P + FVector2D(-1.5, -2.5), P + FVector2D(-1.5, 23.0), P + FVector2D(17.0, 16.0), FLinearColor(0.f, 0.f, 0.f, 1.f));
	Tri(P, P + FVector2D(0.0, 19.0), P + FVector2D(13.5, 13.5), FLinearColor(1.f, 1.f, 1.f, 1.f));
}
