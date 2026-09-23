// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigitalHumanEmotionComponent.generated.h"

UENUM(BlueprintType)
enum class EDigitalHumanEmotion : uint8
{
	Neutral,
	Happy,
	Friendly,
	Concerned,
	Excited
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDigitalHumanOnEmotionChanged, EDigitalHumanEmotion, OldEmotion, EDigitalHumanEmotion, NewEmotion);

/**
 * Smoothly blends toward a small set of named expression-intensity values
 * based on the current emotion, instead of hard-switching curves. Outputs
 * three generic 0-1 dimensions - SmileAmount, BrowRaiseAmount, EyeSquintAmount -
 * rather than specific curve names, because exact expression curve names
 * are rig-specific and need verifying per-project (same reason jaw/blink
 * curve names had to be confirmed in the Skeleton Editor rather than assumed).
 * Wire these three outputs into whichever curves you confirm exist on this
 * character, the same way MouthOpenAmount was wired to CTRL_expressions_jawOpen -
 * via Modify Curve's right-click "Add Curve Pin", NOT the Curve Map property.
 *
 * This is intentionally independent of the audio-driven mouth curves and the
 * Behavior state machine - nothing here reads conversation state automatically.
 * If you want emotion to react to state (e.g. a subtle Concerned look while
 * Thinking), that's a deliberate design decision to wire yourself via
 * DigitalHumanBehaviorComponent::OnStateChanged -> SetEmotion(), not a default.
 */
UCLASS(ClassGroup = (DigitalHuman), meta = (BlueprintSpawnableComponent))
class DIGITALHUMANRUNTIME_API UDigitalHumanEmotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigitalHumanEmotionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Emotion")
	EDigitalHumanEmotion CurrentEmotion = EDigitalHumanEmotion::Neutral;

	/** How quickly expression values chase their target when the emotion changes - higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Emotion")
	float TransitionSpeed = 3.0f;

	/** If true, listens to the DigitalHumanSubsystem for {"type":"emotion","emotion":"..."} JSON messages from the backend (e.g. an LLM-derived sentiment tag alongside its response). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Emotion")
	bool bAutoSubscribeToBackend = true;

	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Emotion")
	float SmileAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Emotion")
	float BrowRaiseAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Emotion")
	float EyeSquintAmount = 0.0f;

	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Emotion")
	FDigitalHumanOnEmotionChanged OnEmotionChanged;

	UFUNCTION(BlueprintCallable, Category = "Digital Human|Emotion")
	void SetEmotion(EDigitalHumanEmotion NewEmotion);

private:
	UFUNCTION()
	void HandleJsonReceived(const FString& JsonText);

	void GetTargetsForEmotion(EDigitalHumanEmotion Emotion, float& OutSmile, float& OutBrowRaise, float& OutSquint) const;

	bool bSubscribedToSubsystem = false;

	float TargetSmileAmount = 0.0f;
	float TargetBrowRaiseAmount = 0.0f;
	float TargetEyeSquintAmount = 0.0f;
};
