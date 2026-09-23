// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanFacialDriverComponent.h"
#include "DigitalHumanAudioPlayerComponent.h"
#include "DigitalHumanRuntimeModule.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

void FDigitalHumanBandpassFilter::Configure(float CenterFreqHz, float Q, float SampleRateHz)
{
	// RBJ Audio EQ Cookbook - band-pass, constant skirt gain (peak gain = Q).
	const float w0 = 2.0f * PI * CenterFreqHz / SampleRateHz;
	const float CosW0 = FMath::Cos(w0);
	const float SinW0 = FMath::Sin(w0);
	const float Alpha = SinW0 / (2.0f * Q);

	const float a0 = 1.0f + Alpha;
	b0 = (Q * Alpha) / a0;
	b1 = 0.0f;
	b2 = (-Q * Alpha) / a0;
	a1 = (-2.0f * CosW0) / a0;
	a2 = (1.0f - Alpha) / a0;

	z1 = 0.0f;
	z2 = 0.0f;
}

float FDigitalHumanBandpassFilter::ProcessSample(float InSample)
{
	// Direct Form II Transposed.
	const float Out = b0 * InSample + z1;
	z1 = b1 * InSample - a1 * Out + z2;
	z2 = b2 * InSample - a2 * Out;
	return Out;
}

UDigitalHumanFacialDriverComponent::UDigitalHumanFacialDriverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDigitalHumanFacialDriverComponent::BeginPlay()
{
	Super::BeginPlay();

	LastChunkTimeSeconds = -1000.0; // ensures we start closed-mouth even if no audio ever arrives

	if (bAutoSubscribeToAudioPlayer)
	{
		if (AActor* Owner = GetOwner())
		{
			if (UDigitalHumanAudioPlayerComponent* Player = Owner->FindComponentByClass<UDigitalHumanAudioPlayerComponent>())
			{
				Player->OnPCMChunkProcessed.AddDynamic(this, &UDigitalHumanFacialDriverComponent::HandlePCMChunkProcessed);
				CachedAudioPlayer = Player;
				UE_LOG(LogDigitalHuman, Log, TEXT("FacialDriverComponent subscribed to AudioPlayerComponent on the same Actor."));
			}
			else
			{
				UE_LOG(LogDigitalHuman, Warning, TEXT("FacialDriverComponent could not find a DigitalHumanAudioPlayerComponent on this Actor - add one, or disable bAutoSubscribeToAudioPlayer and call AnalyzePCMChunk manually."));
			}
		}
	}
}

void UDigitalHumanFacialDriverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedAudioPlayer)
	{
		CachedAudioPlayer->OnPCMChunkProcessed.RemoveDynamic(this, &UDigitalHumanFacialDriverComponent::HandlePCMChunkProcessed);
		CachedAudioPlayer = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UDigitalHumanFacialDriverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	if (Now - LastChunkTimeSeconds > SilenceTimeoutSeconds)
	{
		TargetMouthOpenAmount = 0.0f;
		TargetMouthWideAmount = 0.0f;
	}

	MouthOpenAmount = FMath::FInterpTo(MouthOpenAmount, TargetMouthOpenAmount, DeltaTime, EnvelopeSmoothingSpeed);
	MouthWideAmount = FMath::FInterpTo(MouthWideAmount, TargetMouthWideAmount, DeltaTime, EnvelopeSmoothingSpeed);

	if (bEnableMultiBandAnalysis)
	{
		MouthFunnelAmount = FMath::FInterpTo(MouthFunnelAmount, TargetMouthFunnelAmount, DeltaTime, EnvelopeSmoothingSpeed);
		FricativeAmount = FMath::FInterpTo(FricativeAmount, TargetFricativeAmount, DeltaTime, EnvelopeSmoothingSpeed);

		// Plosive is a one-shot attack/decay pulse, not a chased target - it
		// ticks here (every frame) rather than only when a chunk arrives, so
		// the release is smooth regardless of chunk timing.
		switch (PlosivePhase)
		{
		case EPlosivePhase::Attack:
		{
			PlosivePhaseElapsed += DeltaTime;
			const float Alpha = PlosiveAttackDuration > 0.0f ? FMath::Clamp(PlosivePhaseElapsed / PlosiveAttackDuration, 0.0f, 1.0f) : 1.0f;
			PlosiveAmount = Alpha;
			if (Alpha >= 1.0f)
			{
				PlosivePhase = EPlosivePhase::Decay;
				PlosivePhaseElapsed = 0.0f;
			}
			break;
		}
		case EPlosivePhase::Decay:
		{
			PlosivePhaseElapsed += DeltaTime;
			const float Alpha = PlosiveDecayDuration > 0.0f ? FMath::Clamp(PlosivePhaseElapsed / PlosiveDecayDuration, 0.0f, 1.0f) : 1.0f;
			PlosiveAmount = 1.0f - Alpha;
			if (Alpha >= 1.0f)
			{
				PlosivePhase = EPlosivePhase::Idle;
				PlosivePhaseElapsed = 0.0f;
				PlosiveAmount = 0.0f;
			}
			break;
		}
		case EPlosivePhase::Idle:
		default:
			break;
		}
	}
	else
	{
		MouthFunnelAmount = 0.0f;
		FricativeAmount = 0.0f;
		PlosiveAmount = 0.0f;
	}

	const bool bChanged =
		!FMath::IsNearlyEqual(MouthOpenAmount, LastBroadcastMouthOpenAmount, 0.001f) ||
		!FMath::IsNearlyEqual(MouthWideAmount, LastBroadcastMouthWideAmount, 0.001f);

	if (bChanged)
	{
		LastBroadcastMouthOpenAmount = MouthOpenAmount;
		LastBroadcastMouthWideAmount = MouthWideAmount;
		OnFaceCurvesUpdated.Broadcast(MouthOpenAmount, MouthWideAmount);
	}
}

void UDigitalHumanFacialDriverComponent::HandlePCMChunkProcessed(const TArray<uint8>& PCMBytes, int32 SampleRate, int32 NumChannels)
{
	AnalyzePCMChunk(PCMBytes, SampleRate, NumChannels);
}

void UDigitalHumanFacialDriverComponent::AnalyzePCMChunk(const TArray<uint8>& PCMBytes, int32 SampleRate, int32 NumChannels)
{
	const int32 NumSamples = PCMBytes.Num() / sizeof(int16);
	if (NumSamples <= 1)
	{
		return;
	}

	const int16* Samples = reinterpret_cast<const int16*>(PCMBytes.GetData());

	double SumSquares = 0.0;
	int32 ZeroCrossings = 0;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const double NormalizedSample = static_cast<double>(Samples[i]) / 32768.0;
		SumSquares += NormalizedSample * NormalizedSample;

		if (i > 0 && ((Samples[i] >= 0) != (Samples[i - 1] >= 0)))
		{
			++ZeroCrossings;
		}
	}

	const double RMSLevel = FMath::Sqrt(SumSquares / NumSamples);
	const double ZeroCrossingRate = static_cast<double>(ZeroCrossings) / static_cast<double>(NumSamples);

	TargetMouthOpenAmount = FMath::Clamp(static_cast<float>(RMSLevel) * MouthOpenGain, 0.0f, 1.0f);
	TargetMouthWideAmount = FMath::Clamp(static_cast<float>(ZeroCrossingRate) * MouthWideGain, 0.0f, 1.0f);

	if (bEnableMultiBandAnalysis)
	{
		// (Re)configure the filters if this is the first chunk or the backend's
		// declared sample rate changed - Configure() resets filter state, which
		// is fine since a sample-rate change means a new stream anyway.
		if (ConfiguredSampleRateForFilters != SampleRate)
		{
			LowBandFilter.Configure(300.0f, 0.9f, static_cast<float>(SampleRate));
			MidBandFilter.Configure(1500.0f, 0.9f, static_cast<float>(SampleRate));
			HighBandFilter.Configure(4000.0f, 0.9f, static_cast<float>(SampleRate));
			ConfiguredSampleRateForFilters = SampleRate;
		}

		double LowSumSquares = 0.0, MidSumSquares = 0.0, HighSumSquares = 0.0;
		for (int32 i = 0; i < NumSamples; ++i)
		{
			const float NormalizedSample = static_cast<float>(Samples[i]) / 32768.0f;

			const float LowOut = LowBandFilter.ProcessSample(NormalizedSample);
			const float MidOut = MidBandFilter.ProcessSample(NormalizedSample);
			const float HighOut = HighBandFilter.ProcessSample(NormalizedSample);

			LowSumSquares += LowOut * LowOut;
			MidSumSquares += MidOut * MidOut;
			HighSumSquares += HighOut * HighOut;
		}

		const double LowRMS = FMath::Sqrt(LowSumSquares / NumSamples);
		const double MidRMS = FMath::Sqrt(MidSumSquares / NumSamples);
		const double HighRMS = FMath::Sqrt(HighSumSquares / NumSamples);
		const double TotalBandRMS = LowRMS + MidRMS + HighRMS + 1e-6; // epsilon avoids divide-by-zero on silence

		const double LowRatio = LowRMS / TotalBandRMS;
		const double HighRatio = HighRMS / TotalBandRMS;

		// Baselines below are rough - a perfectly flat/white signal splits
		// close to evenly across 3 bands, so ~0.33 is "no particular band
		// dominates"; only the amount ABOVE that counts as a real signal,
		// which is why these subtract a baseline before applying gain.
		TargetMouthFunnelAmount = FMath::Clamp(static_cast<float>(LowRatio - 0.4) * MouthFunnelGain, 0.0f, 1.0f) * TargetMouthOpenAmount;
		TargetFricativeAmount = FMath::Clamp(static_cast<float>(HighRatio - 0.4) * FricativeGain, 0.0f, 1.0f) * FMath::Clamp(static_cast<float>(RMSLevel) * 12.0f, 0.0f, 1.0f);

		// Plosive onset detection: compare this chunk's raw RMS against a slow
		// moving average. A big jump right after genuine near-silence reads as
		// a stop consonant; a big jump mid-word (already loud) doesn't.
		const bool bWasNearSilent = SlowMovingAverageRMS < PlosiveSilenceThreshold;
		const bool bSharpOnset = (RMSLevel - SlowMovingAverageRMS) > PlosiveOnsetThreshold;
		if (bWasNearSilent && bSharpOnset && PlosivePhase == EPlosivePhase::Idle)
		{
			PlosivePhase = EPlosivePhase::Attack;
			PlosivePhaseElapsed = 0.0f;
		}

		// Slow-moving average update - deliberately much slower than the
		// display envelope, so it tracks "background level" rather than
		// following every syllable (which would make onsets undetectable).
		SlowMovingAverageRMS = FMath::Lerp(SlowMovingAverageRMS, static_cast<float>(RMSLevel), 0.15f);
	}

	const UWorld* World = GetWorld();
	LastChunkTimeSeconds = World ? World->GetTimeSeconds() : LastChunkTimeSeconds;
}
