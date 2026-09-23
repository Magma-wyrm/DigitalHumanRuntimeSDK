// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanBehaviorComponent.h"
#include "DigitalHumanAudioPlayerComponent.h"
#include "DigitalHumanIdleAnimationComponent.h"
#include "DigitalHumanSubsystem.h"
#include "DigitalHumanRuntimeModule.h"

#include "GameFramework/Actor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UDigitalHumanBehaviorComponent::UDigitalHumanBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDigitalHumanBehaviorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		CachedAudioPlayer = Owner->FindComponentByClass<UDigitalHumanAudioPlayerComponent>();
		CachedIdleAnimation = Owner->FindComponentByClass<UDigitalHumanIdleAnimationComponent>();

		if (bAutoDetectSpeakingFromAudio && !CachedAudioPlayer)
		{
			UE_LOG(LogDigitalHuman, Warning, TEXT("BehaviorComponent: bAutoDetectSpeakingFromAudio is true but no DigitalHumanAudioPlayerComponent was found on this Actor - Speaking will never be auto-detected."));
		}
		if (bSuppressSaccadesWhileSpeaking && !CachedIdleAnimation)
		{
			UE_LOG(LogDigitalHuman, Warning, TEXT("BehaviorComponent: bSuppressSaccadesWhileSpeaking is true but no DigitalHumanIdleAnimationComponent was found on this Actor - nothing to suppress."));
		}
	}

	if (bAutoSubscribeToBackend)
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UDigitalHumanSubsystem* Subsystem = GameInstance->GetSubsystem<UDigitalHumanSubsystem>())
			{
				Subsystem->OnJsonReceived.AddDynamic(this, &UDigitalHumanBehaviorComponent::HandleJsonReceived);
				bSubscribedToSubsystem = true;
			}
		}
	}
}

void UDigitalHumanBehaviorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bSubscribedToSubsystem)
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UDigitalHumanSubsystem* Subsystem = GameInstance->GetSubsystem<UDigitalHumanSubsystem>())
			{
				Subsystem->OnJsonReceived.RemoveDynamic(this, &UDigitalHumanBehaviorComponent::HandleJsonReceived);
			}
		}
		bSubscribedToSubsystem = false;
	}

	Super::EndPlay(EndPlayReason);
}

void UDigitalHumanBehaviorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bAutoDetectSpeakingFromAudio && CachedAudioPlayer)
	{
		const bool bIsPlaying = CachedAudioPlayer->IsPlaying();

		if (bIsPlaying && CurrentState != EDigitalHumanConversationState::Speaking)
		{
			ApplyState(EDigitalHumanConversationState::Speaking);
		}
		else if (!bIsPlaying && CurrentState == EDigitalHumanConversationState::Speaking)
		{
			// Playback just ended - fall back to Idle. Caller decides what comes next
			// (e.g. SetState(Listening) if they expect the customer to respond).
			ApplyState(EDigitalHumanConversationState::Idle);
		}
	}
}

void UDigitalHumanBehaviorComponent::SetState(EDigitalHumanConversationState NewState)
{
	const bool bAudioGenuinelyPlaying = bAutoDetectSpeakingFromAudio && CachedAudioPlayer && CachedAudioPlayer->IsPlaying();

	if (bAudioGenuinelyPlaying && NewState != EDigitalHumanConversationState::Speaking)
	{
		UE_LOG(LogDigitalHuman, Verbose, TEXT("BehaviorComponent: ignoring SetState request while audio is genuinely playing - state stays Speaking until playback ends."));
		return;
	}

	ApplyState(NewState);
}

void UDigitalHumanBehaviorComponent::SetLookAtTarget(AActor* NewTarget)
{
	CurrentLookAtTarget = NewTarget;
}

void UDigitalHumanBehaviorComponent::ApplyState(EDigitalHumanConversationState NewState)
{
	if (NewState == CurrentState)
	{
		return;
	}

	const EDigitalHumanConversationState OldState = CurrentState;
	CurrentState = NewState;

	if (bSuppressSaccadesWhileSpeaking && CachedIdleAnimation)
	{
		if (NewState == EDigitalHumanConversationState::Speaking && OldState != EDigitalHumanConversationState::Speaking)
		{
			bSaccadesEnabledBeforeSpeaking = CachedIdleAnimation->bEnableEyeSaccades;
			CachedIdleAnimation->bEnableEyeSaccades = false;
		}
		else if (OldState == EDigitalHumanConversationState::Speaking && NewState != EDigitalHumanConversationState::Speaking)
		{
			CachedIdleAnimation->bEnableEyeSaccades = bSaccadesEnabledBeforeSpeaking;
		}
	}

	UE_LOG(LogDigitalHuman, Log, TEXT("BehaviorComponent: state changed %d -> %d"), static_cast<int32>(OldState), static_cast<int32>(NewState));
	OnStateChanged.Broadcast(OldState, NewState);
}

void UDigitalHumanBehaviorComponent::HandleJsonReceived(const FString& JsonText)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	FString MessageType;
	if (!JsonObject->TryGetStringField(TEXT("type"), MessageType) || MessageType != TEXT("state"))
	{
		return;
	}

	FString StateString;
	if (!JsonObject->TryGetStringField(TEXT("state"), StateString))
	{
		return;
	}

	if (StateString == TEXT("idle"))
	{
		SetState(EDigitalHumanConversationState::Idle);
	}
	else if (StateString == TEXT("listening"))
	{
		SetState(EDigitalHumanConversationState::Listening);
	}
	else if (StateString == TEXT("thinking"))
	{
		SetState(EDigitalHumanConversationState::Thinking);
	}
	else if (StateString == TEXT("speaking"))
	{
		SetState(EDigitalHumanConversationState::Speaking);
	}
	else
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("BehaviorComponent: received unknown state \"%s\" from backend, ignoring."), *StateString);
	}
}
