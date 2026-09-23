// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanGestureComponent.h"
#include "DigitalHumanSubsystem.h"
#include "DigitalHumanRuntimeModule.h"

#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UDigitalHumanGestureComponent::UDigitalHumanGestureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDigitalHumanGestureComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!BodyMeshComponent)
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("GestureComponent: BodyMeshComponent is not set - PlayGesture() will fail until this is assigned in the Details panel."));
	}

	if (bAutoSubscribeToBackend)
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UDigitalHumanSubsystem* Subsystem = GameInstance->GetSubsystem<UDigitalHumanSubsystem>())
			{
				Subsystem->OnJsonReceived.AddDynamic(this, &UDigitalHumanGestureComponent::HandleJsonReceived);
				bSubscribedToSubsystem = true;
			}
		}
	}
}

void UDigitalHumanGestureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bSubscribedToSubsystem)
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UDigitalHumanSubsystem* Subsystem = GameInstance->GetSubsystem<UDigitalHumanSubsystem>())
			{
				Subsystem->OnJsonReceived.RemoveDynamic(this, &UDigitalHumanGestureComponent::HandleJsonReceived);
			}
		}
		bSubscribedToSubsystem = false;
	}

	Super::EndPlay(EndPlayReason);
}

void UDigitalHumanGestureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (NodPhase)
	{
	case ENodPhase::Idle:
		NodPitchOffset = 0.0f;
		break;

	case ENodPhase::Down:
	{
		NodPhaseElapsed += DeltaTime;
		const float Alpha = NodDownDuration > 0.0f ? FMath::Clamp(NodPhaseElapsed / NodDownDuration, 0.0f, 1.0f) : 1.0f;
		NodPitchOffset = FMath::InterpEaseOut(0.0f, NodAmplitudeDegrees, Alpha, 2.0f);
		if (Alpha >= 1.0f)
		{
			NodPhase = ENodPhase::Up;
			NodPhaseElapsed = 0.0f;
		}
		break;
	}

	case ENodPhase::Up:
	{
		NodPhaseElapsed += DeltaTime;
		const float Alpha = NodUpDuration > 0.0f ? FMath::Clamp(NodPhaseElapsed / NodUpDuration, 0.0f, 1.0f) : 1.0f;
		NodPitchOffset = FMath::InterpEaseIn(NodAmplitudeDegrees, 0.0f, Alpha, 2.0f);
		if (Alpha >= 1.0f)
		{
			NodPhase = ENodPhase::Idle;
			NodPhaseElapsed = 0.0f;
			NodPitchOffset = 0.0f;
		}
		break;
	}
	}
}

bool UDigitalHumanGestureComponent::PlayGesture(FName GestureName)
{
	if (!BodyMeshComponent)
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("GestureComponent: PlayGesture(\"%s\") failed - BodyMeshComponent is not set."), *GestureName.ToString());
		return false;
	}

	const TObjectPtr<UAnimMontage>* FoundMontage = GestureLibrary.Find(GestureName);
	if (!FoundMontage || !(*FoundMontage))
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("GestureComponent: PlayGesture(\"%s\") failed - no montage assigned for this name in GestureLibrary."), *GestureName.ToString());
		return false;
	}

	UAnimInstance* AnimInstance = BodyMeshComponent->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("GestureComponent: PlayGesture(\"%s\") failed - BodyMeshComponent has no AnimInstance (check its Anim Class is set)."), *GestureName.ToString());
		return false;
	}

	AnimInstance->Montage_Play(*FoundMontage);
	return true;
}

void UDigitalHumanGestureComponent::TriggerNod()
{
	NodPhase = ENodPhase::Down;
	NodPhaseElapsed = 0.0f;
}

void UDigitalHumanGestureComponent::HandleJsonReceived(const FString& JsonText)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	FString MessageType;
	if (!JsonObject->TryGetStringField(TEXT("type"), MessageType) || MessageType != TEXT("gesture"))
	{
		return;
	}

	FString GestureString;
	if (!JsonObject->TryGetStringField(TEXT("gesture"), GestureString))
	{
		return;
	}

	if (GestureString == TEXT("nod"))
	{
		TriggerNod();
	}
	else
	{
		PlayGesture(FName(*GestureString));
	}
}
