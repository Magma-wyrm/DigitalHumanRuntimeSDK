// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "IWebSocket.h"
#include "Containers/Ticker.h"

DECLARE_MULTICAST_DELEGATE(FDigitalHumanWSConnected);
DECLARE_MULTICAST_DELEGATE_OneParam(FDigitalHumanWSConnectionError, const FString& /*ErrorMessage*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDigitalHumanWSClosed, int32 /*StatusCode*/, const FString& /*Reason*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDigitalHumanWSTextMessage, const FString& /*Message*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDigitalHumanWSBinaryMessage, const TArray<uint8>& /*Bytes*/);

/**
 * Internal, non-UObject WebSocket connection manager. Wraps the engine's
 * IWebSocket with reconnect (exponential backoff) and application-level
 * heartbeat behaviour. Not exposed to Blueprint directly - UDigitalHumanSubsystem
 * owns one instance of this and re-broadcasts its events as Blueprint-assignable
 * dynamic delegates.
 *
 * Threading note: the engine's WebSockets module ticks its pending connection
 * and message state on the Game Thread (via FTSTicker internally), so every
 * callback bound below already arrives on the Game Thread. This class never
 * blocks the Game Thread - connecting, sending, and receiving are all handled
 * asynchronously by the underlying platform socket implementation. "Off Game
 * Thread" in the wider spec refers to not stalling it with blocking I/O, which
 * this satisfies; it does not mean these specific callbacks run on a worker
 * thread, since the engine's own WebSockets module does not expose one.
 *
 * Lifetime requirement: must be held via TSharedPtr (constructed with MakeShared),
 * since it uses TSharedFromThis internally for safe delegate binding.
 */
class DIGITALHUMANRUNTIME_API FDigitalHumanWebSocketManager : public TSharedFromThis<FDigitalHumanWebSocketManager>
{
public:
	FDigitalHumanWebSocketManager();
	~FDigitalHumanWebSocketManager();

	/** Begins connecting to the given ws:// or wss:// URL. Safe to call again to change target URL. */
	void Connect(const FString& InServerURL);

	/** Closes the connection and disables auto-reconnect until Connect() is called again. */
	void Disconnect();

	/** True if the underlying socket reports itself connected right now. */
	bool IsConnected() const;

	/** Sends a UTF-8 text frame (e.g. a JSON command string). No-ops with a warning log if not connected. */
	void SendText(const FString& Message);

	/** Sends a raw binary frame. No-ops with a warning log if not connected. */
	void SendBinary(const TArray<uint8>& Bytes);

	/** Enables/disables automatic reconnect with exponential backoff after an unexpected close or error. Enabled by default. */
	void SetAutoReconnect(bool bEnabled) { bAutoReconnectEnabled = bEnabled; }

	/** Enables/disables periodic application-level heartbeat text frames while connected. */
	void SetHeartbeatEnabled(bool bEnabled, float IntervalSeconds = 15.0f);

	FDigitalHumanWSConnected OnConnected;
	FDigitalHumanWSConnectionError OnConnectionError;
	FDigitalHumanWSClosed OnClosed;
	FDigitalHumanWSTextMessage OnTextMessage;
	FDigitalHumanWSBinaryMessage OnBinaryMessage;

private:
	void BindSocketDelegates();
	void StartReconnectTimer();
	void ClearReconnectTimer();
	bool TickReconnect(float DeltaTime);
	bool TickHeartbeat(float DeltaTime);

	TSharedPtr<IWebSocket> Socket;
	FString ServerURL;

	bool bAutoReconnectEnabled;
	bool bManualDisconnect;
	float CurrentReconnectDelay;
	static constexpr float MinReconnectDelay = 1.0f;
	static constexpr float MaxReconnectDelay = 30.0f;

	bool bHeartbeatEnabled;
	float HeartbeatInterval;
	float HeartbeatAccumulator;

	FTSTicker::FDelegateHandle ReconnectTickerHandle;
	FTSTicker::FDelegateHandle HeartbeatTickerHandle;
};
