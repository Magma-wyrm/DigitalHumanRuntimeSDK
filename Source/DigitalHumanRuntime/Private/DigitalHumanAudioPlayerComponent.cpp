// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanAudioPlayerComponent.h"
#include "DigitalHumanSubsystem.h"
#include "DigitalHumanRuntimeModule.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "GameFramework/Actor.h"
#include "Engine/GameInstance.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UDigitalHumanAudioPlayerComponent::UDigitalHumanAudioPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDigitalHumanAudioPlayerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		AudioComponent = NewObject<UAudioComponent>(Owner, UAudioComponent::StaticClass(), NAME_None, RF_Transient);
		if (AudioComponent)
		{
			AudioComponent->bAutoActivate = false;
			AudioComponent->SetVolumeMultiplier(AudioVolume);
			AudioComponent->RegisterComponent();

			if (USceneComponent* RootComp = Owner->GetRootComponent())
			{
				AudioComponent->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
	}

	if (bAutoSubscribeToBackend)
	{
		SubscribeToBackend();
	}
}

void UDigitalHumanAudioPlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnsubscribeFromBackend();

	if (AudioComponent)
	{
		AudioComponent->Stop();
		AudioComponent->DestroyComponent();
		AudioComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UDigitalHumanAudioPlayerComponent::SubscribeToBackend()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}

	if (UDigitalHumanSubsystem* Subsystem = GI->GetSubsystem<UDigitalHumanSubsystem>())
	{
		Subsystem->OnJsonReceived.AddDynamic(this, &UDigitalHumanAudioPlayerComponent::HandleJsonReceived);
		Subsystem->OnBinaryReceived.AddDynamic(this, &UDigitalHumanAudioPlayerComponent::HandleBinaryReceived);
		UE_LOG(LogDigitalHuman, Log, TEXT("AudioPlayerComponent subscribed to backend audio events."));
	}
	else
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("AudioPlayerComponent could not find DigitalHumanSubsystem - is the plugin enabled?"));
	}
}

void UDigitalHumanAudioPlayerComponent::UnsubscribeFromBackend()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}

	if (UDigitalHumanSubsystem* Subsystem = GI->GetSubsystem<UDigitalHumanSubsystem>())
	{
		Subsystem->OnJsonReceived.RemoveDynamic(this, &UDigitalHumanAudioPlayerComponent::HandleJsonReceived);
		Subsystem->OnBinaryReceived.RemoveDynamic(this, &UDigitalHumanAudioPlayerComponent::HandleBinaryReceived);
	}
}

void UDigitalHumanAudioPlayerComponent::HandleJsonReceived(const FString& JsonText)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return; // not valid JSON, or not intended for us - ignore silently
	}

	FString Type;
	if (!JsonObject->TryGetStringField(TEXT("type"), Type))
	{
		return;
	}

	if (Type == TEXT("audio_start") || Type == TEXT("audio"))
	{
		double SampleRateD = 24000.0;
		double ChannelsD = 1.0;
		JsonObject->TryGetNumberField(TEXT("sampleRate"), SampleRateD);
		JsonObject->TryGetNumberField(TEXT("channels"), ChannelsD);

		PendingSampleRate = static_cast<int32>(SampleRateD);
		PendingNumChannels = FMath::Max(1, static_cast<int32>(ChannelsD));

		UE_LOG(LogDigitalHuman, Log, TEXT("Backend declared audio format: %d Hz, %d channel(s)"),
			PendingSampleRate, PendingNumChannels);
	}
	else if (Type == TEXT("audio_end"))
	{
		UE_LOG(LogDigitalHuman, Log, TEXT("Backend signalled end of audio stream."));
		// Conversation-state handling of this (idle/talking transitions) arrives in a later stage.
	}
	else if (Type == TEXT("interrupt"))
	{
		UE_LOG(LogDigitalHuman, Log, TEXT("Backend sent interrupt - stopping and flushing playback."));
		StopAndFlush();
	}
}

void UDigitalHumanAudioPlayerComponent::HandleBinaryReceived(const TArray<uint8>& Bytes)
{
	FeedPCM(Bytes, PendingSampleRate, PendingNumChannels);
}

void UDigitalHumanAudioPlayerComponent::FeedPCM(const TArray<uint8>& PCMBytes, int32 SampleRate, int32 NumChannels)
{
	if (PCMBytes.Num() == 0)
	{
		return;
	}

	EnsureSoundWave(SampleRate, NumChannels);

	if (ProceduralSoundWave)
	{
		ProceduralSoundWave->QueueAudio(PCMBytes.GetData(), PCMBytes.Num());

		if (AudioComponent && !AudioComponent->IsPlaying())
		{
			AudioComponent->Play();
		}

		OnPCMChunkProcessed.Broadcast(PCMBytes, SampleRate, NumChannels);
	}
}

void UDigitalHumanAudioPlayerComponent::EnsureSoundWave(int32 SampleRate, int32 NumChannels)
{
	if (ProceduralSoundWave && SampleRate == CurrentSampleRate && NumChannels == CurrentNumChannels)
	{
		return; // already configured correctly for this format
	}

	// Format changed (or this is the first chunk) - start a fresh procedural
	// sound wave rather than trying to reconfigure one that may already be
	// playing, since changing sample rate on an in-flight procedural wave is
	// not a supported operation.
	if (AudioComponent)
	{
		AudioComponent->Stop();
	}

	ProceduralSoundWave = NewObject<USoundWaveProcedural>(this);
	ProceduralSoundWave->SetSampleRate(SampleRate);
	ProceduralSoundWave->NumChannels = NumChannels;
	ProceduralSoundWave->SoundGroup = SOUNDGROUP_Voice;
	ProceduralSoundWave->bLooping = false;
	// Duration is intentionally not set here - USoundWaveProcedural reports its
	// own duration internally for streaming/indefinite-length audio, so this
	// property does not need (and should not be given) a manual value.

	CurrentSampleRate = SampleRate;
	CurrentNumChannels = NumChannels;

	if (AudioComponent)
	{
		AudioComponent->SetSound(ProceduralSoundWave);
		AudioComponent->SetVolumeMultiplier(AudioVolume);
	}

	UE_LOG(LogDigitalHuman, Log, TEXT("Audio format configured: %d Hz, %d channel(s)"), SampleRate, NumChannels);
}

void UDigitalHumanAudioPlayerComponent::StopAndFlush()
{
	if (AudioComponent)
	{
		AudioComponent->Stop();
	}

	if (ProceduralSoundWave)
	{
		ProceduralSoundWave->ResetAudio();
	}
}

bool UDigitalHumanAudioPlayerComponent::IsPlaying() const
{
	return AudioComponent && AudioComponent->IsPlaying();
}
