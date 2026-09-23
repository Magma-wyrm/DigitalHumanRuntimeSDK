// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanEmotionComponent.h"
#include "DigitalHumanSubsystem.h"
#include "DigitalHumanRuntimeModule.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UDigitalHumanEmotionComponent::UDigitalHumanEmotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDigitalHumanEmotionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSubscribeToBackend)
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UDigitalHumanSubsystem* Subsystem = GameInstance->GetSubsystem<UDigitalHumanSubsystem>())
			{
				Subsystem->OnJsonReceived.AddDynamic(this, &UDigitalHumanEmotionComponent::HandleJsonReceived);
				bSubscribedToSubsystem = true;
			}
		}
	}
}

void UDigitalHumanEmotionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bSubscribedToSubsystem)
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UDigitalHumanSubsystem* Subsystem = GameInstance->GetSubsystem<UDigitalHumanSubsystem>())
			{
				Subsystem->OnJsonReceived.RemoveDynamic(this, &UDigitalHumanEmotionComponent::HandleJsonReceived);
			}
		}
		bSubscribedToSubsystem = false;
	}

	Super::EndPlay(EndPlayReason);
}

void UDigitalHumanEmotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SmileAmount = FMath::FInterpTo(SmileAmount, TargetSmileAmount, DeltaTime, TransitionSpeed);
	BrowRaiseAmount = FMath::FInterpTo(BrowRaiseAmount, TargetBrowRaiseAmount, DeltaTime, TransitionSpeed);
	EyeSquintAmount = FMath::FInterpTo(EyeSquintAmount, TargetEyeSquintAmount, DeltaTime, TransitionSpeed);
}

void UDigitalHumanEmotionComponent::SetEmotion(EDigitalHumanEmotion NewEmotion)
{
	if (NewEmotion == CurrentEmotion)
	{
		return;
	}

	const EDigitalHumanEmotion OldEmotion = CurrentEmotion;
	CurrentEmotion = NewEmotion;

	GetTargetsForEmotion(NewEmotion, TargetSmileAmount, TargetBrowRaiseAmount, TargetEyeSquintAmount);

	UE_LOG(LogDigitalHuman, Log, TEXT("EmotionComponent: emotion changed %d -> %d"), static_cast<int32>(OldEmotion), static_cast<int32>(NewEmotion));
	OnEmotionChanged.Broadcast(OldEmotion, NewEmotion);
}

void UDigitalHumanEmotionComponent::GetTargetsForEmotion(EDigitalHumanEmotion Emotion, float& OutSmile, float& OutBrowRaise, float& OutSquint) const
{
	// Hand-picked starting presets - these are intentionally conservative
	// (subtle, not cartoonish) since this drives a sales-avatar's resting
	// expression, not a burst reaction. Tune freely; nothing else depends on
	// these exact numbers.
	switch (Emotion)
	{
	case EDigitalHumanEmotion::Happy:
		OutSmile = 0.7f; OutBrowRaise = 0.1f; OutSquint = 0.2f;
		break;
	case EDigitalHumanEmotion::Friendly:
		OutSmile = 0.4f; OutBrowRaise = 0.0f; OutSquint = 0.0f;
		break;
	case EDigitalHumanEmotion::Concerned:
		OutSmile = 0.0f; OutBrowRaise = 0.3f; OutSquint = 0.1f;
		break;
	case EDigitalHumanEmotion::Excited:
		OutSmile = 0.8f; OutBrowRaise = 0.4f; OutSquint = 0.1f;
		break;
	case EDigitalHumanEmotion::Neutral:
	default:
		OutSmile = 0.0f; OutBrowRaise = 0.0f; OutSquint = 0.0f;
		break;
	}
}

void UDigitalHumanEmotionComponent::HandleJsonReceived(const FString& JsonText)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	FString MessageType;
	if (!JsonObject->TryGetStringField(TEXT("type"), MessageType) || MessageType != TEXT("emotion"))
	{
		return;
	}

	FString EmotionString;
	if (!JsonObject->TryGetStringField(TEXT("emotion"), EmotionString))
	{
		return;
	}

	if (EmotionString == TEXT("neutral"))       { SetEmotion(EDigitalHumanEmotion::Neutral); }
	else if (EmotionString == TEXT("happy"))    { SetEmotion(EDigitalHumanEmotion::Happy); }
	else if (EmotionString == TEXT("friendly")) { SetEmotion(EDigitalHumanEmotion::Friendly); }
	else if (EmotionString == TEXT("concerned")){ SetEmotion(EDigitalHumanEmotion::Concerned); }
	else if (EmotionString == TEXT("excited"))  { SetEmotion(EDigitalHumanEmotion::Excited); }
	else
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("EmotionComponent: received unknown emotion \"%s\" from backend, ignoring."), *EmotionString);
	}
}
