// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanSubsystem.h"
#include "DigitalHumanWebSocketManager.h"
#include "DigitalHumanRuntimeModule.h"

void UDigitalHumanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	WebSocketManager = MakeShared<FDigitalHumanWebSocketManager>();

	WebSocketManager->OnConnected.AddLambda([this]()
	{
		OnConnected.Broadcast();
	});

	WebSocketManager->OnConnectionError.AddLambda([this](const FString& Error)
	{
		OnConnectionError.Broadcast(Error);
	});

	WebSocketManager->OnClosed.AddLambda([this](int32 StatusCode, const FString& Reason)
	{
		OnDisconnected.Broadcast(StatusCode, Reason);
	});

	WebSocketManager->OnTextMessage.AddLambda([this](const FString& Message)
	{
		OnJsonReceived.Broadcast(Message);
	});

	WebSocketManager->OnBinaryMessage.AddLambda([this](const TArray<uint8>& Bytes)
	{
		OnBinaryReceived.Broadcast(Bytes);
	});

	UE_LOG(LogDigitalHuman, Log, TEXT("UDigitalHumanSubsystem initialized (Stage 2: networking core)."));
}

void UDigitalHumanSubsystem::Deinitialize()
{
	if (WebSocketManager.IsValid())
	{
		WebSocketManager->Disconnect();
		WebSocketManager.Reset();
	}

	UE_LOG(LogDigitalHuman, Log, TEXT("UDigitalHumanSubsystem deinitialized."));

	Super::Deinitialize();
}

void UDigitalHumanSubsystem::Connect(const FString& ServerURL)
{
	if (WebSocketManager.IsValid())
	{
		WebSocketManager->Connect(ServerURL);
	}
}

void UDigitalHumanSubsystem::Disconnect()
{
	if (WebSocketManager.IsValid())
	{
		WebSocketManager->Disconnect();
	}
}

bool UDigitalHumanSubsystem::IsConnected() const
{
	return WebSocketManager.IsValid() && WebSocketManager->IsConnected();
}

void UDigitalHumanSubsystem::SendJSON(const FString& JsonText)
{
	if (WebSocketManager.IsValid())
	{
		WebSocketManager->SendText(JsonText);
	}
}

void UDigitalHumanSubsystem::SendBinary(const TArray<uint8>& Bytes)
{
	if (WebSocketManager.IsValid())
	{
		WebSocketManager->SendBinary(Bytes);
	}
}

void UDigitalHumanSubsystem::SetAutoReconnect(bool bEnabled)
{
	if (WebSocketManager.IsValid())
	{
		WebSocketManager->SetAutoReconnect(bEnabled);
	}
}

void UDigitalHumanSubsystem::SetHeartbeatEnabled(bool bEnabled, float IntervalSeconds)
{
	if (WebSocketManager.IsValid())
	{
		WebSocketManager->SetHeartbeatEnabled(bEnabled, IntervalSeconds);
	}
}
