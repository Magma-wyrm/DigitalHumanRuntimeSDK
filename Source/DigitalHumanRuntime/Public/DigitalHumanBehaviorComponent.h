// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigitalHumanBehaviorComponent.generated.h"

class UDigitalHumanAudioPlayerComponent;
class UDigitalHumanIdleAnimationComponent;

UENUM(BlueprintType)
enum class EDigitalHumanConversationState : uint8
{
	Idle,
	Listening,
	Thinking,
	Speaking
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDigitalHumanOnStateChanged, EDigitalHumanConversationState, OldState, EDigitalHumanConversationState, NewState);

/**
 * Conversation state machine (Idle / Listening / Thinking / Speaking) that
 * other systems can react to. This is the first stage that ties the
 * previously-independent pieces together:
 *
 *   - Speaking is auto-detected from UDigitalHumanAudioPlayerComponent::IsPlaying()
 *     on the same Actor - you don't need to call SetState(Speaking) yourself,
 *     though you still can for latency-sensitive cases (e.g. flip to Speaking
 *     the instant your backend sends "audio_start", before the first chunk
 *     has actually queued).
 *   - Idle / Listening / Thinking are NOT auto-detected - nothing in this SDK
 *     knows when your mic is capturing or your backend is "thinking" unless
 *     you tell it. Drive these either by calling SetState() directly from
 *     your own app logic, or by having your backend send
 *     {"type":"state","state":"listening"|"thinking"|"idle"} as a JSON text
 *     frame - this component listens for that automatically.
 *   - While Speaking, if a UDigitalHumanIdleAnimationComponent exists on the
 *     same Actor, eye saccades are temporarily suppressed (and restored to
 *     whatever they were before, not forced back on) - real speakers dart
 *     their eyes around less while actively talking than while idle.
 *
 * Whenever actual audio is playing, that always wins over a manually/JSON-set
 * state - you cannot manually set Idle/Listening/Thinking while audio is
 * genuinely still streaming, since that would visibly contradict the moving
 * mouth. Once playback stops, state falls back to Idle automatically; call
 * SetState(Listening) right after if you expect the customer to respond next.
 */
UCLASS(ClassGroup = (DigitalHuman), meta = (BlueprintSpawnableComponent))
class DIGITALHUMANRUNTIME_API UDigitalHumanBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigitalHumanBehaviorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** If true, finds a UDigitalHumanAudioPlayerComponent on the same Actor and treats IsPlaying()==true as authoritative Speaking state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Behavior")
	bool bAutoDetectSpeakingFromAudio = true;

	/** If true, listens to the DigitalHumanSubsystem for {"type":"state","state":"..."} JSON messages from the backend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Behavior")
	bool bAutoSubscribeToBackend = true;

	/** If true, finds a UDigitalHumanIdleAnimationComponent on the same Actor and suppresses eye saccades while Speaking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Behavior")
	bool bSuppressSaccadesWhileSpeaking = true;

	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Behavior")
	EDigitalHumanConversationState CurrentState = EDigitalHumanConversationState::Idle;

	/** Optional - an actor for the character to "look at" (wire this into whatever Look-At target system your MetaHuman's eyes already use; this component does not drive eye rotation directly). */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Behavior")
	TObjectPtr<AActor> CurrentLookAtTarget;

	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Behavior")
	FDigitalHumanOnStateChanged OnStateChanged;

	/**
	 * Request a state change. Ignored (logged, not asserted) if audio is
	 * genuinely playing and the requested state isn't Speaking - see class
	 * comment. Re-entering the same state is a no-op (no event fires).
	 */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Behavior")
	void SetState(EDigitalHumanConversationState NewState);

	UFUNCTION(BlueprintCallable, Category = "Digital Human|Behavior")
	void SetLookAtTarget(AActor* NewTarget);

	/** True while Thinking - a convenience flag for a brief gaze-aversion look-away, a common "recalling information" cue. Wire it however your look-at system supports; this component only reports the flag. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Digital Human|Behavior")
	bool ShouldAvertGaze() const { return CurrentState == EDigitalHumanConversationState::Thinking; }

private:
	void ApplyState(EDigitalHumanConversationState NewState);

	UFUNCTION()
	void HandleJsonReceived(const FString& JsonText);

	UPROPERTY()
	TObjectPtr<UDigitalHumanAudioPlayerComponent> CachedAudioPlayer;

	UPROPERTY()
	TObjectPtr<UDigitalHumanIdleAnimationComponent> CachedIdleAnimation;

	bool bSubscribedToSubsystem = false;
	bool bSaccadesEnabledBeforeSpeaking = true;
};
