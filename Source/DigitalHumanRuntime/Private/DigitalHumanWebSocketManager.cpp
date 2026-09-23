// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanWebSocketManager.h"
#include "WebSocketsModule.h"
#include "DigitalHumanRuntimeModule.h"

FDigitalHumanWebSocketManager::FDigitalHumanWebSocketManager()
	: bAutoReconnectEnabled(true)
	, bManualDisconnect(false)
	, CurrentReconnectDelay(MinReconnectDelay)
	, bHeartbeatEnabled(false)
	, HeartbeatInterval(15.0f)
	, HeartbeatAccumulator(0.0f)
{
}

FDigitalHumanWebSocketManager::~FDigitalHumanWebSocketManager()
{
	ClearReconnectTimer();

	if (HeartbeatTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
		HeartbeatTickerHandle.Reset();
	}

	if (Socket.IsValid())
	{
		Socket->Close();
		Socket.Reset();
	}
}

void FDigitalHumanWebSocketManager::Connect(const FString& InServerURL)
{
	ServerURL = InServerURL;
	bManualDisconnect = false;

	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::Get().LoadModule("WebSockets");
	}

	if (Socket.IsValid())
	{
		Socket->Close();
		Socket.Reset();
	}

	Socket = FWebSocketsModule::Get().CreateWebSocket(ServerURL, TEXT("ws"));
	BindSocketDelegates();
	Socket->Connect();
}

void FDigitalHumanWebSocketManager::Disconnect()
{
	bManualDisconnect = true;
	ClearReconnectTimer();

	if (Socket.IsValid() && Socket->IsConnected())
	{
		Socket->Close();
	}
}

bool FDigitalHumanWebSocketManager::IsConnected() const
{
	return Socket.IsValid() && Socket->IsConnected();
}

void FDigitalHumanWebSocketManager::SendText(const FString& Message)
{
	if (IsConnected())
	{
		Socket->Send(Message);
	}
	else
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("FDigitalHumanWebSocketManager::SendText called while not connected - message dropped."));
	}
}

void FDigitalHumanWebSocketManager::SendBinary(const TArray<uint8>& Bytes)
{
	if (IsConnected())
	{
		Socket->Send(Bytes.GetData(), Bytes.Num(), /*bIsBinary=*/true);
	}
	else
	{
		UE_LOG(LogDigitalHuman, Warning, TEXT("FDigitalHumanWebSocketManager::SendBinary called while not connected - message dropped."));
	}
}

void FDigitalHumanWebSocketManager::SetHeartbeatEnabled(bool bEnabled, float IntervalSeconds)
{
	bHeartbeatEnabled = bEnabled;
	HeartbeatInterval = FMath::Max(1.0f, IntervalSeconds);
	HeartbeatAccumulator = 0.0f;

	if (HeartbeatTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
		HeartbeatTickerHandle.Reset();
	}

	if (bHeartbeatEnabled)
	{
		HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FDigitalHumanWebSocketManager::TickHeartbeat), 1.0f);
	}
}

void FDigitalHumanWebSocketManager::BindSocketDelegates()
{
	TWeakPtr<FDigitalHumanWebSocketManager> SelfWeakPtr = AsShared();

	Socket->OnConnected().AddLambda([SelfWeakPtr]()
	{
		if (TSharedPtr<FDigitalHumanWebSocketManager> Pinned = SelfWeakPtr.Pin())
		{
			Pinned->CurrentReconnectDelay = MinReconnectDelay;
			Pinned->ClearReconnectTimer();
			UE_LOG(LogDigitalHuman, Log, TEXT("WebSocket connected: %s"), *Pinned->ServerURL);
			Pinned->OnConnected.Broadcast();
		}
	});

	Socket->OnConnectionError().AddLambda([SelfWeakPtr](const FString& Error)
	{
		if (TSharedPtr<FDigitalHumanWebSocketManager> Pinned = SelfWeakPtr.Pin())
		{
			UE_LOG(LogDigitalHuman, Warning, TEXT("WebSocket connection error: %s"), *Error);
			Pinned->OnConnectionError.Broadcast(Error);

			if (Pinned->bAutoReconnectEnabled && !Pinned->bManualDisconnect)
			{
				Pinned->StartReconnectTimer();
			}
		}
	});

	Socket->OnClosed().AddLambda([SelfWeakPtr](int32 StatusCode, const FString& Reason, bool bWasClean)
	{
		if (TSharedPtr<FDigitalHumanWebSocketManager> Pinned = SelfWeakPtr.Pin())
		{
			UE_LOG(LogDigitalHuman, Log, TEXT("WebSocket closed: code=%d reason=%s clean=%s"),
				StatusCode, *Reason, bWasClean ? TEXT("true") : TEXT("false"));
			Pinned->OnClosed.Broadcast(StatusCode, Reason);

			if (Pinned->bAutoReconnectEnabled && !Pinned->bManualDisconnect)
			{
				Pinned->StartReconnectTimer();
			}
		}
	});

	Socket->OnMessage().AddLambda([SelfWeakPtr](const FString& Message)
	{
		if (TSharedPtr<FDigitalHumanWebSocketManager> Pinned = SelfWeakPtr.Pin())
		{
			Pinned->OnTextMessage.Broadcast(Message);
		}
	});

	Socket->OnRawMessage().AddLambda([SelfWeakPtr](const void* Data, SIZE_T Size, SIZE_T BytesRemaining)
	{
		if (TSharedPtr<FDigitalHumanWebSocketManager> Pinned = SelfWeakPtr.Pin())
		{
			TArray<uint8> Bytes;
			Bytes.Append(static_cast<const uint8*>(Data), Size);
			Pinned->OnBinaryMessage.Broadcast(Bytes);
		}
	});
}

void FDigitalHumanWebSocketManager::StartReconnectTimer()
{
	ClearReconnectTimer();

	UE_LOG(LogDigitalHuman, Log, TEXT("Reconnecting to %s in %.1f seconds..."), *ServerURL, CurrentReconnectDelay);

	ReconnectTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FDigitalHumanWebSocketManager::TickReconnect),
		CurrentReconnectDelay);
}

void FDigitalHumanWebSocketManager::ClearReconnectTimer()
{
	if (ReconnectTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReconnectTickerHandle);
		ReconnectTickerHandle.Reset();
	}
}

bool FDigitalHumanWebSocketManager::TickReconnect(float DeltaTime)
{
	if (!bManualDisconnect)
	{
		UE_LOG(LogDigitalHuman, Log, TEXT("Attempting reconnect to %s"), *ServerURL);
		CurrentReconnectDelay = FMath::Min(CurrentReconnectDelay * 2.0f, MaxReconnectDelay);
		Connect(ServerURL);
	}
	return false; // one-shot ticker: do not repeat automatically, a fresh timer is scheduled on the next failure
}

bool FDigitalHumanWebSocketManager::TickHeartbeat(float DeltaTime)
{
	if (bHeartbeatEnabled && IsConnected())
	{
		HeartbeatAccumulator += DeltaTime;
		if (HeartbeatAccumulator >= HeartbeatInterval)
		{
			HeartbeatAccumulator = 0.0f;
			SendText(TEXT("{\"type\":\"heartbeat\"}"));
		}
	}
	return true; // keep repeating every second while heartbeat is enabled
}
