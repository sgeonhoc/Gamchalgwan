// Small software synth + mixer. Every sound is generated at startup, so the build ships no audio files.
#pragma once

#include "CoreMinimal.h"

enum class EDockSnd : uint8
{
	Click, Step, Pen, Paper, Phone, Radio, CarDoor, Horn, Creak, Clank, Whistle, Escape, Boom, Count
};

class FDockAudio
{
public:
	static constexpr int32 Rate = 48000;

	void Build();
	void Play(EDockSnd S, float Gain = 1.f);
	void StopAll() { Voices.Reset(); }
	// Mixes N mono samples (ambience + voices) into Out as int16.
	void Mix(int16* Out, int32 N);

	float AmbGain = 0.55f;

private:
	struct FVoice { int32 Snd = 0; int32 Pos = 0; float Gain = 1.f; };
	TArray<float> Bank[(int32)EDockSnd::Count];
	TArray<float> Amb;
	int32 AmbPos = 0;
	TArray<FVoice> Voices;
};
