// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigitalHumanGestureComponent.generated.h"

class USkeletalMeshComponent;
class UAnimMontage;

/**
 * Two separate things live here, deliberately kept apart:
 *
 *  1. A named Anim Montage library (GestureLibrary) for real body gestures -
 *     a wave, an open-palm "here's the floor plan" sweep, a nod, whatever
 *     you need. This SDK does NOT ship any gesture animations - there's no
 *     way to synthesize a convincing hand/arm gesture from nothing, the same
 *     honest limitation as everything else in this project that needed real
 *     assets (a trained viseme model, MetaHuman's own rig, etc). You supply
 *     the Anim Montages (Mixamo, purchased mocap, your own capture, whatever
 *     fits your budget/pipeline) and assign them here by name; this
 *     component just plays whichever one is requested, by name, at runtime -
 *     from Blueprint or from the backend via {"type":"gesture","gesture":"wave"}.
 *
 *  2. One procedural gesture that needs zero external assets: a head nod.
 *     Outputs a small additive Pitch offset (NodPitchOffset) the same way
 *     UDigitalHumanIdleAnimationComponent outputs HeadMicroMovementOffset -
 *     wire it into the same head bone chain in ABP_BodyIdleMotion (Break
 *     Rotator both offsets, add the Pitch components, Make Rotator, feed
 *     that combined value into Transform (Modify) Bone) so idle drift and a
 *     nod can happen at the same time without fighting each other.
 */
UCLASS(ClassGroup = (DigitalHuman), meta = (BlueprintSpawnableComponent))
class DIGITALHUMANRUNTIME_API UDigitalHumanGestureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDigitalHumanGestureComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---------------- Montage-based gestures (bring your own assets) ----------------

	/** Must be set to the Actor's Body skeletal mesh component (NOT Face) - there are two skeletal mesh components on a MetaHuman and this can't safely guess which is which. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Gesture")
	TObjectPtr<USkeletalMeshComponent> BodyMeshComponent;

	/** Name -> Montage. Populate with whatever gesture animations you have; empty by default (no montages ship with this plugin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Gesture")
	TMap<FName, TObjectPtr<UAnimMontage>> GestureLibrary;

	/** If true, listens for {"type":"gesture","gesture":"<name>"} from the backend and calls PlayGesture with that name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Gesture")
	bool bAutoSubscribeToBackend = true;

	/** Plays the named montage from GestureLibrary on BodyMeshComponent. Returns false (and logs a warning, does not assert) if the name isn't in the library or BodyMeshComponent isn't set - a missing gesture should never crash the app. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Gesture")
	bool PlayGesture(FName GestureName);

	// ---------------- Procedural nod (no assets required) ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Gesture|Nod")
	float NodAmplitudeDegrees = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Gesture|Nod")
	float NodDownDuration = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digital Human|Gesture|Nod")
	float NodUpDuration = 0.22f;

	/** Additive Pitch-only offset in degrees - combine with HeadMicroMovementOffset before feeding the head bone, don't replace it. */
	UPROPERTY(BlueprintReadOnly, Category = "Digital Human|Gesture|Nod")
	float NodPitchOffset = 0.0f;

	/** Starts a single nod. Safe to call while a previous nod is finishing - it retriggers from wherever it currently is. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Gesture|Nod")
	void TriggerNod();

private:
	UFUNCTION()
	void HandleJsonReceived(const FString& JsonText);

	bool bSubscribedToSubsystem = false;

	enum class ENodPhase : uint8 { Idle, Down, Up };
	ENodPhase NodPhase = ENodPhase::Idle;
	float NodPhaseElapsed = 0.0f;
};
