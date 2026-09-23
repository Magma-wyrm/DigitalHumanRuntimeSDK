// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigitalHumanAudioPlayerComponent.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDigitalHumanOnPCMChunkProcessed, const TArray<uint8>&, PCMBytes, int32, SampleRate, int32, NumChannels);

/**
 * Streams backend-provided PCM audio to a speaker in realtime, with no temp
 * files and no wait-for-complete-file delay.
 *
 * Protocol this listens for on the DigitalHumanSubsystem's events:
 *   - A JSON text message declaring the format about to follow, e.g.
 *     {"type":"audio_start","sampleRate":24000,"channels":1}
 *     (also accepts "type":"audio" with the same fields, matching the
 *     original spec's example payload).
 *   - Any number of binary frames after that, each treated as raw PCM16
 *     bytes in the most recently declared format.
 *   - {"type":"audio_end"} - informational for now (logged); conversation-
 *     state handling of this arrives in a later stage.
 *   - {"type":"interrupt"} - stops playback and flushes queued audio
 *     immediately, matching the spec's Interrupt command.
 *
 * Resampling note: this does not hand-roll DSP resampling. USoundWaveProcedural
 * is configured with the sample rate/channel count the backend declares, and
 * the engine's own audio mixer resamples from that to the output device's
 * rate automatically - that is the "automatic resampling" requirement,
 * satisfied by telling the engine the correct source format rather than by
 * writing a resampling algorithm ourselves.
 *
 * Add this component to any Actor (e.g. directly on your MetaHuman Blueprint)
 * to test it standalone, before the unifying UDigitalHumanComponent exists.
 */
UCLASS(ClassGroup = (DigitalHuman), meta = (BlueprintSpawnableComponent))
class DIGITALHUMANRUNTIME_API UDigitalHumanAudioPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigitalHumanAudioPlayerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Master volume applied to the internal Audio Component. Default 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Audio")
	float AudioVolume = 1.0f;

	/**
	 * If true (default), automatically subscribes to the DigitalHumanSubsystem's
	 * OnJsonReceived/OnBinaryReceived events on BeginPlay, so backend audio just
	 * works with no extra wiring. Turn off if you want to call FeedPCM manually
	 * instead (e.g. feeding audio from a non-backend/local source).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Audio")
	bool bAutoSubscribeToBackend = true;

	/**
	 * Directly queues a chunk of raw PCM16 audio for playback, creating or
	 * reconfiguring the internal sound wave if the format changed since the
	 * last call. Safe to call every time a new chunk arrives.
	 */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Audio")
	void FeedPCM(const TArray<uint8>& PCMBytes, int32 SampleRate, int32 NumChannels);

	/** Stops playback immediately and clears any queued-but-not-yet-played audio. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Audio")
	void StopAndFlush();

	/** True if there is currently audio playing. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Digital Human|Audio")
	bool IsPlaying() const;

	/**
	 * Fires every time a PCM chunk is successfully queued for playback, with
	 * the exact bytes/format used. Downstream systems (e.g. the facial driver)
	 * subscribe to this instead of independently listening to the network/JSON
	 * layer, so audio format parsing only happens in one place.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Audio")
	FDigitalHumanOnPCMChunkProcessed OnPCMChunkProcessed;

private:
	UFUNCTION()
	void HandleJsonReceived(const FString& JsonText);

	UFUNCTION()
	void HandleBinaryReceived(const TArray<uint8>& Bytes);

	void EnsureSoundWave(int32 SampleRate, int32 NumChannels);
	void SubscribeToBackend();
	void UnsubscribeFromBackend();

	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent;

	UPROPERTY()
	TObjectPtr<USoundWaveProcedural> ProceduralSoundWave;

	int32 CurrentSampleRate = 0;
	int32 CurrentNumChannels = 0;

	/** Format declared by the most recent audio_start/audio JSON message, applied to binary chunks that follow it. */
	int32 PendingSampleRate = 24000;
	int32 PendingNumChannels = 1;
};
