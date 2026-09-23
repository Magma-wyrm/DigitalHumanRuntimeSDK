// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigitalHumanIdleAnimationComponent.generated.h"

/**
 * Procedural "alive" motion that runs continuously and independently of the
 * audio-driven mouth curves from UDigitalHumanFacialDriverComponent - blinking,
 * breathing, eye saccades, and small head noise. None of this touches
 * MouthOpenAmount/MouthWideAmount; it's meant to be layered underneath them
 * in the same AnimBP so the character doesn't look frozen/dead-eyed between
 * sentences or while idle.
 *
 * Nothing here depends on audio at all - this component can run with zero
 * backend connection and the character will still blink/breathe/look around.
 */
UCLASS(ClassGroup = (DigitalHuman), meta = (BlueprintSpawnableComponent))
class DIGITALHUMANRUNTIME_API UDigitalHumanIdleAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigitalHumanIdleAnimationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---------------- Blinking ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Blink")
	bool bEnableBlinking = true;

	/** Random range between the start of one blink and the start of the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Blink")
	float BlinkIntervalMinSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Blink")
	float BlinkIntervalMaxSeconds = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Blink")
	float BlinkCloseDuration = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Blink")
	float BlinkHoldDuration = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Blink")
	float BlinkOpenDuration = 0.12f;

	/** 0 = eyes fully open, 1 = fully closed. Wire into your blink curve(s). */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Idle|Blink")
	float BlinkAmount = 0.0f;

	/** Force a blink to start right now, ignoring the random timer. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Idle|Blink")
	void TriggerBlink();

	// ---------------- Breathing ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Breathing")
	bool bEnableBreathing = true;

	/** Full inhale-exhale cycle length in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Breathing")
	float BreathingCycleSeconds = 4.0f;

	/** 0-1, smooth sine cycle. Wire into a chest/shoulder curve or bone offset if your rig has one. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Idle|Breathing")
	float BreathingAmount = 0.0f;

	// ---------------- Eye saccades ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Eyes")
	bool bEnableEyeSaccades = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Eyes")
	float SaccadeIntervalMinSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Eyes")
	float SaccadeIntervalMaxSeconds = 4.0f;

	/** How far the eyes can dart from center, in degrees, on each axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Eyes")
	float SaccadeMaxOffsetDegrees = 3.0f;

	/** How long the snap to a new saccade target takes - keep this short, real saccades are near-instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Eyes")
	float SaccadeMoveDuration = 0.05f;

	/** Small (Yaw, Pitch) offset in degrees, meant to be added to your Look-At target's aim direction, not used as an absolute rotation. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Idle|Eyes")
	FVector2D EyeSaccadeOffset = FVector2D::ZeroVector;

	// ---------------- Head micro movement ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Head")
	bool bEnableHeadMicroMovement = true;

	/** Max degrees of noise-driven offset on pitch/yaw; roll gets 30% of this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Head")
	float HeadMicroMovementAmplitudeDegrees = 1.5f;

	/** Higher = faster/jitterier noise, lower = slower/more languid drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Idle|Head")
	float HeadMicroMovementSpeed = 0.3f;

	/** Small additive rotator - wire into a Transform (Modify) Bone on the head bone, Local space, Additive mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Idle|Head")
	FRotator HeadMicroMovementOffset = FRotator::ZeroRotator;

private:
	void TickBlink(float DeltaTime);
	void TickBreathing(double TimeSeconds);
	void TickEyeSaccades(float DeltaTime);
	void TickHeadMicroMovement(double TimeSeconds);

	enum class EBlinkPhase : uint8 { Idle, Closing, Holding, Opening };
	EBlinkPhase BlinkPhase = EBlinkPhase::Idle;
	float BlinkPhaseElapsed = 0.0f;
	float NextBlinkInSeconds = 0.0f;

	FVector2D SaccadeStartOffset = FVector2D::ZeroVector;
	FVector2D SaccadeTargetOffset = FVector2D::ZeroVector;
	float SaccadeMoveElapsed = 0.0f;
	float NextSaccadeInSeconds = 0.0f;

	float NoiseSeedYaw = 0.0f;
	float NoiseSeedPitch = 0.0f;
	float NoiseSeedRoll = 0.0f;
};
