// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigitalHumanFacialDriverComponent.generated.h"

class UDigitalHumanAudioPlayerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDigitalHumanOnFaceCurvesUpdated, float, MouthOpenAmount, float, MouthWideAmount);

/** Simple RBJ-cookbook constant-skirt-gain band-pass biquad. Stateful across
 *  calls (Direct Form II Transposed) - each instance tracks one frequency
 *  band's energy continuously across chunks, not just within one chunk. */
USTRUCT()
struct FDigitalHumanBandpassFilter
{
	GENERATED_BODY()

	void Configure(float CenterFreqHz, float Q, float SampleRateHz);
	float ProcessSample(float InSample);

	float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
	float z1 = 0.0f, z2 = 0.0f;
};

/**
 * Extracts realtime facial animation values from raw PCM audio - no phoneme
 * data, no ML model, no external dependency. Two signals were the original
 * (Stage 4) set:
 *
 *   - RMS volume level -> MouthOpenAmount (jaw-open amount, 0-1)
 *   - Zero-crossing rate -> MouthWideAmount (mouth-width/narrow amount, 0-1)
 *
 * Three more (added later, toggle-able via bEnableMultiBandAnalysis) come
 * from splitting each chunk into low/mid/high frequency bands via simple
 * band-pass filters (no FFT, no new dependencies) and looking at how energy
 * is distributed across them:
 *
 *   - Low-band energy dominance   -> MouthFunnelAmount (rounded-lip shape,
 *     e.g. "oo"/"oh")
 *   - High-band noise-like energy -> FricativeAmount (teeth/lip tension,
 *     e.g. "s"/"f"/"th" - these get blurred together, can't be told apart)
 *   - A sharp loud onset right after near-silence -> PlosiveAmount, a short
 *     attack/decay pulse approximating a stop consonant (b/p/t/d/k/g -
 *     again, can't tell which one)
 *
 * None of this is phoneme detection - it's frequency-content heuristics, the
 * same "poor man's phonemes" idea as the original two signals, just split
 * across more bands. It will not survive a lip-reader's scrutiny, but reads
 * as noticeably more alive than jaw-open alone.
 *
 * Wire this component's outputs (or the OnFaceCurvesUpdated event for the
 * original two) into your Face Animation Blueprint's Modify Curve setup, in
 * place of any test/placeholder source.
 *
 * By default this looks for a UDigitalHumanAudioPlayerComponent on the same
 * Actor and subscribes to its OnPCMChunkProcessed event automatically - no
 * manual wiring needed if both components are on the same Actor.
 */
UCLASS(ClassGroup = (DigitalHuman), meta = (BlueprintSpawnableComponent))
class DIGITALHUMANRUNTIME_API UDigitalHumanFacialDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigitalHumanFacialDriverComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** If true, automatically finds a UDigitalHumanAudioPlayerComponent on the same Actor and subscribes to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face")
	bool bAutoSubscribeToAudioPlayer = true;

	/** How quickly MouthOpenAmount/MouthWideAmount chase their target values. Higher = snappier, lower = smoother/laggier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face")
	float EnvelopeSmoothingSpeed = 12.0f;

	/** Gain applied to the raw RMS level before clamping to 0-1. Raw speech RMS is typically low - tune this against your actual TTS voice's loudness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face")
	float MouthOpenGain = 4.0f;

	/** Gain applied to the raw zero-crossing rate before clamping to 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face")
	float MouthWideGain = 6.0f;

	/** If no audio chunk has arrived for longer than this (seconds), the mouth closes back toward 0 rather than staying stuck open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face")
	float SilenceTimeoutSeconds = 0.3f;

	/** Current smoothed jaw-open value, 0-1. Read this every frame from your AnimBP, or bind to OnFaceCurvesUpdated instead. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Face")
	float MouthOpenAmount = 0.0f;

	/** Current smoothed mouth-width value, 0-1. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Face")
	float MouthWideAmount = 0.0f;

	/** Fires whenever the smoothed values change meaningfully - an alternative to polling the properties above every tick. */
	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Face")
	FDigitalHumanOnFaceCurvesUpdated OnFaceCurvesUpdated;

	// ---------------- Multi-band extension (rounded/fricative/plosive) ----------------

	/** Master switch - set false to fall back to the original Stage 4 two-curve behavior with zero risk of the new analysis interfering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face|MultiBand")
	bool bEnableMultiBandAnalysis = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face|MultiBand")
	float MouthFunnelGain = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face|MultiBand")
	float FricativeGain = 4.0f;

	/** RMS jump (this chunk minus a slow-moving average) needed to count as a plosive onset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face|MultiBand")
	float PlosiveOnsetThreshold = 0.08f;

	/** The slow-moving average must be below this (i.e. genuinely near-silent) right before the onset for it to count as a plosive rather than just getting louder mid-word. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face|MultiBand")
	float PlosiveSilenceThreshold = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face|MultiBand")
	float PlosiveAttackDuration = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Face|MultiBand")
	float PlosiveDecayDuration = 0.12f;

	/** Rounded-lip amount (e.g. "oo"/"oh"), 0-1. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Face|MultiBand")
	float MouthFunnelAmount = 0.0f;

	/** Teeth/lip tension amount (e.g. "s"/"f"/"th" - blurred together), 0-1. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Face|MultiBand")
	float FricativeAmount = 0.0f;

	/** Short attack/decay pulse on a sudden onset after near-silence (approximates b/p/t/d/k/g - can't tell which), 0-1. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Face|MultiBand")
	float PlosiveAmount = 0.0f;

	/**
	 * Manually feed a PCM chunk for analysis. Called automatically via the
	 * audio player's OnPCMChunkProcessed when bAutoSubscribeToAudioPlayer is
	 * true - call this directly instead if you disabled auto-subscribe (e.g.
	 * driving the face from a source other than UDigitalHumanAudioPlayerComponent).
	 */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Face")
	void AnalyzePCMChunk(const TArray<uint8>& PCMBytes, int32 SampleRate, int32 NumChannels);

private:
	UFUNCTION()
	void HandlePCMChunkProcessed(const TArray<uint8>& PCMBytes, int32 SampleRate, int32 NumChannels);

	UPROPERTY()
	TObjectPtr<UDigitalHumanAudioPlayerComponent> CachedAudioPlayer;

	float TargetMouthOpenAmount = 0.0f;
	float TargetMouthWideAmount = 0.0f;
	double LastChunkTimeSeconds = -1000.0;

	float LastBroadcastMouthOpenAmount = -1.0f;
	float LastBroadcastMouthWideAmount = -1.0f;

	// Multi-band state - persistent filter instances (they carry memory across
	// chunks) plus onset-detection and pulse state.
	FDigitalHumanBandpassFilter LowBandFilter;
	FDigitalHumanBandpassFilter MidBandFilter;
	FDigitalHumanBandpassFilter HighBandFilter;
	int32 ConfiguredSampleRateForFilters = 0;

	float TargetMouthFunnelAmount = 0.0f;
	float TargetFricativeAmount = 0.0f;

	float SlowMovingAverageRMS = 0.0f;

	enum class EPlosivePhase : uint8 { Idle, Attack, Decay };
	EPlosivePhase PlosivePhase = EPlosivePhase::Idle;
	float PlosivePhaseElapsed = 0.0f;
};
