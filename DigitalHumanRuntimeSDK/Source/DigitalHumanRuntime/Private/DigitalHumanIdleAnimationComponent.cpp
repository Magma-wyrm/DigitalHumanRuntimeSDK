// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanIdleAnimationComponent.h"
#include "Engine/World.h"

UDigitalHumanIdleAnimationComponent::UDigitalHumanIdleAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDigitalHumanIdleAnimationComponent::BeginPlay()
{
	Super::BeginPlay();

	NextBlinkInSeconds = FMath::FRandRange(BlinkIntervalMinSeconds, BlinkIntervalMaxSeconds);
	NextSaccadeInSeconds = FMath::FRandRange(SaccadeIntervalMinSeconds, SaccadeIntervalMaxSeconds);

	// Random per-instance seeds so multiple characters in the same scene don't move in lockstep.
	NoiseSeedYaw = FMath::FRandRange(0.0f, 1000.0f);
	NoiseSeedPitch = FMath::FRandRange(0.0f, 1000.0f);
	NoiseSeedRoll = FMath::FRandRange(0.0f, 1000.0f);
}

void UDigitalHumanIdleAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	const double TimeSeconds = World ? World->GetTimeSeconds() : 0.0;

	if (bEnableBlinking)
	{
		TickBlink(DeltaTime);
	}

	if (bEnableBreathing)
	{
		TickBreathing(TimeSeconds);
	}

	if (bEnableEyeSaccades)
	{
		TickEyeSaccades(DeltaTime);
	}

	if (bEnableHeadMicroMovement)
	{
		TickHeadMicroMovement(TimeSeconds);
	}
}

void UDigitalHumanIdleAnimationComponent::TriggerBlink()
{
	if (BlinkPhase == EBlinkPhase::Idle)
	{
		BlinkPhase = EBlinkPhase::Closing;
		BlinkPhaseElapsed = 0.0f;
	}
}

void UDigitalHumanIdleAnimationComponent::TickBlink(float DeltaTime)
{
	switch (BlinkPhase)
	{
	case EBlinkPhase::Idle:
	{
		NextBlinkInSeconds -= DeltaTime;
		if (NextBlinkInSeconds <= 0.0f)
		{
			BlinkPhase = EBlinkPhase::Closing;
			BlinkPhaseElapsed = 0.0f;
		}
		break;
	}
	case EBlinkPhase::Closing:
	{
		BlinkPhaseElapsed += DeltaTime;
		const float Alpha = BlinkCloseDuration > 0.0f ? FMath::Clamp(BlinkPhaseElapsed / BlinkCloseDuration, 0.0f, 1.0f) : 1.0f;
		BlinkAmount = Alpha;
		if (Alpha >= 1.0f)
		{
			BlinkPhase = EBlinkPhase::Holding;
			BlinkPhaseElapsed = 0.0f;
		}
		break;
	}
	case EBlinkPhase::Holding:
	{
		BlinkAmount = 1.0f;
		BlinkPhaseElapsed += DeltaTime;
		if (BlinkPhaseElapsed >= BlinkHoldDuration)
		{
			BlinkPhase = EBlinkPhase::Opening;
			BlinkPhaseElapsed = 0.0f;
		}
		break;
	}
	case EBlinkPhase::Opening:
	{
		BlinkPhaseElapsed += DeltaTime;
		const float Alpha = BlinkOpenDuration > 0.0f ? FMath::Clamp(BlinkPhaseElapsed / BlinkOpenDuration, 0.0f, 1.0f) : 1.0f;
		BlinkAmount = 1.0f - Alpha;
		if (Alpha >= 1.0f)
		{
			BlinkPhase = EBlinkPhase::Idle;
			BlinkPhaseElapsed = 0.0f;
			BlinkAmount = 0.0f;
			NextBlinkInSeconds = FMath::FRandRange(BlinkIntervalMinSeconds, BlinkIntervalMaxSeconds);
		}
		break;
	}
	}
}

void UDigitalHumanIdleAnimationComponent::TickBreathing(double TimeSeconds)
{
	if (BreathingCycleSeconds <= 0.0f)
	{
		BreathingAmount = 0.0f;
		return;
	}

	const double Phase = FMath::Fmod(TimeSeconds, static_cast<double>(BreathingCycleSeconds)) / static_cast<double>(BreathingCycleSeconds);
	BreathingAmount = static_cast<float>((FMath::Sin(Phase * 2.0 * PI) * 0.5) + 0.5);
}

void UDigitalHumanIdleAnimationComponent::TickEyeSaccades(float DeltaTime)
{
	if (SaccadeMoveElapsed < SaccadeMoveDuration)
	{
		SaccadeMoveElapsed += DeltaTime;
		const float Alpha = SaccadeMoveDuration > 0.0f ? FMath::Clamp(SaccadeMoveElapsed / SaccadeMoveDuration, 0.0f, 1.0f) : 1.0f;
		EyeSaccadeOffset = FMath::Lerp(SaccadeStartOffset, SaccadeTargetOffset, Alpha);
	}
	else
	{
		NextSaccadeInSeconds -= DeltaTime;
		if (NextSaccadeInSeconds <= 0.0f)
		{
			SaccadeStartOffset = EyeSaccadeOffset;

			// Occasionally return to center rather than always darting to a new extreme -
			// reads more natural than constant motion.
			if (FMath::FRand() < 0.3f)
			{
				SaccadeTargetOffset = FVector2D::ZeroVector;
			}
			else
			{
				SaccadeTargetOffset = FVector2D(
					FMath::FRandRange(-SaccadeMaxOffsetDegrees, SaccadeMaxOffsetDegrees),
					FMath::FRandRange(-SaccadeMaxOffsetDegrees, SaccadeMaxOffsetDegrees));
			}

			SaccadeMoveElapsed = 0.0f;
			NextSaccadeInSeconds = FMath::FRandRange(SaccadeIntervalMinSeconds, SaccadeIntervalMaxSeconds);
		}
	}
}

void UDigitalHumanIdleAnimationComponent::TickHeadMicroMovement(double TimeSeconds)
{
	const double T = TimeSeconds * HeadMicroMovementSpeed;

	const float Yaw = FMath::PerlinNoise1D(static_cast<float>(T) + NoiseSeedYaw) * HeadMicroMovementAmplitudeDegrees;
	const float Pitch = FMath::PerlinNoise1D(static_cast<float>(T) + NoiseSeedPitch) * HeadMicroMovementAmplitudeDegrees;
	const float Roll = FMath::PerlinNoise1D(static_cast<float>(T) + NoiseSeedRoll) * HeadMicroMovementAmplitudeDegrees * 0.3f;

	HeadMicroMovementOffset = FRotator(Pitch, Yaw, Roll);
}
