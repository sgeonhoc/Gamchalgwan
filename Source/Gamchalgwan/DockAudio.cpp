#include "DockAudio.h"
#include "Math/RandomStream.h"

namespace
{
	constexpr float R = (float)FDockAudio::Rate;

	struct FBiquad
	{
		float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
		// Type: 0 low pass, 1 high pass, 2 band pass (RBJ cookbook).
		FBiquad(int32 Type, float F, float Q)
		{
			const float W = 2.f * PI * F / R;
			const float C = FMath::Cos(W), S = FMath::Sin(W), Al = S / (2.f * Q);
			if (Type == 0) { b0 = (1 - C) * .5f; b1 = 1 - C; b2 = (1 - C) * .5f; }
			else if (Type == 1) { b0 = (1 + C) * .5f; b1 = -(1 + C); b2 = (1 + C) * .5f; }
			else { b0 = Al; b1 = 0; b2 = -Al; }
			const float A0 = 1 + Al;
			a1 = -2 * C / A0; a2 = (1 - Al) / A0; b0 /= A0; b1 /= A0; b2 /= A0;
		}
		float Run(float X)
		{
			const float Y = b0 * X + z1;
			z1 = b1 * X - a1 * Y + z2;
			z2 = b2 * X - a2 * Y;
			return Y;
		}
	};

	TArray<float> Make(float Sec) { TArray<float> A; A.SetNumZeroed(FMath::CeilToInt(Sec * R)); return A; }
	float Fade(float T, float Len, float In, float Out)
	{
		float G = 1.f;
		if (T < In) G = T / In;
		if (T > Len - Out) G = FMath::Min(G, FMath::Max(0.f, (Len - T) / Out));
		return G;
	}
}

void FDockAudio::Build()
{
	FRandomStream Rng(5112026);
	auto Noise = [&Rng]() { return Rng.FRandRange(-1.f, 1.f); };

	{ // Click: UI tick
		auto& A = Bank[(int32)EDockSnd::Click] = Make(0.05f);
		FBiquad Hp(1, 2000, 0.7f);
		for (int32 i = 0; i < A.Num(); ++i) { const float t = i / R; A[i] = FMath::Sin(2 * PI * 1800 * t) * FMath::Exp(-t * 120) * .35f + Hp.Run(Noise()) * FMath::Exp(-t * 400) * .25f; }
	}
	{ // Step: soft footfall
		auto& A = Bank[(int32)EDockSnd::Step] = Make(0.12f);
		FBiquad Lp(0, 300, 0.7f);
		for (int32 i = 0; i < A.Num(); ++i) { const float t = i / R; A[i] = Lp.Run(Noise()) * FMath::Exp(-t * 35) * .9f + FMath::Sin(2 * PI * 70 * t) * FMath::Exp(-t * 40) * .3f; }
	}
	{ // Pen: scratching strokes
		auto& A = Bank[(int32)EDockSnd::Pen] = Make(0.9f);
		FBiquad Bp(2, 3500, 2.f);
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R; const float S = .5f + .5f * FMath::Sin(2 * PI * 7 * t);
			A[i] = Bp.Run(Noise()) * S * S * Fade(t, 0.9f, 0.03f, 0.15f) * 1.4f;
		}
	}
	{ // Paper: three rustles
		auto& A = Bank[(int32)EDockSnd::Paper] = Make(0.5f);
		FBiquad Hp(1, 1500, 0.7f);
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R; float E = 0;
			for (float s : { 0.f, 0.12f, 0.25f }) if (t >= s) E += FMath::Exp(-(t - s) * 30);
			A[i] = Hp.Run(Noise()) * E * .35f;
		}
	}
	{ // Phone: two rings, 440 + 480 Hz chopped at 20 Hz
		auto& A = Bank[(int32)EDockSnd::Phone] = Make(1.3f);
		FBiquad Bp(2, 1000, 0.7f);
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R;
			const bool On = (t < 0.45f) || (t > 0.65f && t < 1.1f);
			const float Tr = (FMath::Sin(2 * PI * 20 * t) > 0) ? 1.f : .15f;
			const float X = On ? (FMath::Sin(2 * PI * 440 * t) + FMath::Sin(2 * PI * 480 * t)) * .3f * Tr : 0.f;
			A[i] = Bp.Run(X) * 1.6f;
		}
	}
	{ // Radio: gated crackle and a squelch beep
		auto& A = Bank[(int32)EDockSnd::Radio] = Make(0.95f);
		FBiquad Bp(2, 1800, 1.5f);
		bool Gate = true; int32 Next = 0;
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R;
			if (i >= Next) { Gate = Rng.FRand() > 0.35f; Next = i + (int32)(R * Rng.FRandRange(0.015f, 0.05f)); }
			float X = Bp.Run(Noise()) * (Gate ? .9f : .15f) * Fade(t, 0.75f, 0.01f, 0.05f);
			if (t > 0.78f && t < 0.88f) X += FMath::Sin(2 * PI * 1250 * t) * .25f;
			A[i] = X;
		}
	}
	{ // Car door: thud
		auto& A = Bank[(int32)EDockSnd::CarDoor] = Make(0.45f);
		FBiquad Lp(0, 200, 0.7f); FBiquad Hp(1, 3000, 0.7f);
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R;
			A[i] = Lp.Run(Noise()) * FMath::Exp(-t * 18) * 1.3f + FMath::Sin(2 * PI * 55 * t) * FMath::Exp(-t * 12) * .6f + Hp.Run(Noise()) * FMath::Exp(-t * 300) * .4f;
		}
	}
	{ // Horn: two short blasts
		auto& A = Bank[(int32)EDockSnd::Horn] = Make(0.9f);
		FBiquad Lp(0, 2500, 0.7f);
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R;
			const float G = (t < 0.22f) ? Fade(t, 0.22f, 0.01f, 0.03f) : (t > 0.32f ? Fade(t - 0.32f, 0.58f, 0.01f, 0.08f) : 0.f);
			A[i] = Lp.Run((FMath::Tanh(3 * FMath::Sin(2 * PI * 350 * t)) + FMath::Tanh(3 * FMath::Sin(2 * PI * 440 * t))) * .2f) * G;
		}
	}
	{ // Creak: slow stick-slip saw
		auto& A = Bank[(int32)EDockSnd::Creak] = Make(1.4f);
		FBiquad Bp(2, 700, 3.f);
		float Ph = 0;
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R;
			const float F = 90 + 60 * FMath::Sin(2 * PI * 0.8f * t) + 15 * FMath::Sin(2 * PI * 5.3f * t);
			Ph += F / R; Ph -= FMath::FloorToFloat(Ph);
			A[i] = Bp.Run(2 * Ph - 1) * Fade(t, 1.4f, 0.1f, 0.3f) * 2.2f;
		}
	}
	{ // Clank: latch, inharmonic partials
		auto& A = Bank[(int32)EDockSnd::Clank] = Make(0.8f);
		const float Fr[] = { 523, 1307, 2093, 3271, 4400 }, Am[] = { .4f, .3f, .25f, .15f, .1f }, Dc[] = { 6, 9, 12, 16, 22 };
		FBiquad Hp(1, 2000, 0.7f);
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R; float X = Hp.Run(Noise()) * FMath::Exp(-t * 200) * .5f;
			for (int32 k = 0; k < 5; ++k) X += FMath::Sin(2 * PI * Fr[k] * t) * Am[k] * FMath::Exp(-t * Dc[k]);
			A[i] = X * .7f;
		}
	}
	{ // Whistle: two blasts with trill
		auto& A = Bank[(int32)EDockSnd::Whistle] = Make(1.05f);
		FBiquad Bp(2, 2900, 4.f);
		float Ph = 0;
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R;
			const float G = (t < 0.35f) ? Fade(t, 0.35f, 0.02f, 0.05f) : (t > 0.45f ? Fade(t - 0.45f, 0.6f, 0.02f, 0.1f) : 0.f);
			Ph += (2900 + 140 * FMath::Sin(2 * PI * 28 * t)) / R; Ph -= FMath::FloorToFloat(Ph);
			A[i] = (FMath::Sin(2 * PI * Ph) * .3f + Bp.Run(Noise()) * .15f) * G;
		}
	}
	{ // Escape: soft rising arpeggio
		auto& A = Bank[(int32)EDockSnd::Escape] = Make(1.8f);
		const float N[] = { 523.25f, 659.25f, 783.99f, 1046.5f };
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R; float X = 0;
			for (int32 k = 0; k < 4; ++k)
			{
				const float s = k * 0.18f; if (t < s) continue; const float u = t - s;
				X += (FMath::Sin(2 * PI * N[k] * u) + .3f * FMath::Sin(4 * PI * N[k] * u)) * FMath::Exp(-u * 2.5f) * FMath::Min(1.f, u * 200);
			}
			A[i] = X * .18f;
		}
	}
	{ // Boom: low drop
		auto& A = Bank[(int32)EDockSnd::Boom] = Make(1.6f);
		FBiquad Lp(0, 150, 0.7f);
		float Ph = 0;
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const float t = i / R;
			Ph += (38 + 42 * FMath::Exp(-t * 3)) / R;
			A[i] = FMath::Sin(2 * PI * Ph) * FMath::Exp(-t * 2.2f) * .9f * FMath::Min(1.f, t * 300) + Lp.Run(Noise()) * FMath::Exp(-t * 4) * 1.2f;
		}
	}
	{ // Ambience: water, distant hum, buoy bell. 13 s rendered, folded to a 12 s seamless loop.
		TArray<float> Src = Make(13.f);
		FBiquad Lp(0, 400, 0.7f); FBiquad Bp(2, 1400, 0.8f);
		for (int32 i = 0; i < Src.Num(); ++i)
		{
			const float t = i / R;
			const float Swell = .5f + .5f * FMath::Sin(2 * PI * 0.18f * t + 2.f * FMath::Sin(2 * PI * 0.07f * t));
			const float Lap = FMath::Max(0.f, FMath::Sin(2 * PI * 0.45f * t + 1.3f * FMath::Sin(2 * PI * 0.11f * t)));
			float X = Lp.Run(Noise()) * (.12f + .25f * Swell) + Bp.Run(Noise()) * .05f * Lap * Lap;
			X += FMath::Sin(2 * PI * 55 * t) * .018f + FMath::Sin(2 * PI * 110 * t) * .008f;
			for (float b : { 3.2f, 9.1f })
			{
				if (t < b) continue; const float u = t - b;
				X += (FMath::Sin(2 * PI * 620 * u) + .5f * FMath::Sin(2 * PI * 1530 * u)) * FMath::Exp(-u * 1.8f) * .05f;
			}
			Src[i] = X;
		}
		const int32 L = (int32)(12.f * R), F = (int32)R;
		Amb.SetNumZeroed(L);
		for (int32 i = 0; i < L; ++i)
		{
			if (i < F) { const float a = (float)i / F; Amb[i] = Src[i] * a + Src[L + i] * (1 - a); }
			else Amb[i] = Src[i];
		}
	}
}

void FDockAudio::Play(EDockSnd S, float Gain)
{
	FVoice V; V.Snd = (int32)S; V.Gain = Gain;
	Voices.Add(V);
}

void FDockAudio::Mix(int16* Out, int32 N)
{
	for (int32 i = 0; i < N; ++i)
	{
		float S = 0.f;
		if (Amb.Num() > 0) { S = Amb[AmbPos] * AmbGain; AmbPos = (AmbPos + 1) % Amb.Num(); }
		for (int32 v = Voices.Num() - 1; v >= 0; --v)
		{
			FVoice& V = Voices[v];
			const TArray<float>& B = Bank[V.Snd];
			if (V.Pos >= B.Num()) { Voices.RemoveAtSwap(v); continue; }
			S += B[V.Pos++] * V.Gain;
		}
		Out[i] = (int16)FMath::Clamp(FMath::Tanh(S * 1.1f) * 0.92f * 32767.f, -32768.f, 32767.f);
	}
}
