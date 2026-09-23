// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DigitalHumanSubsystem.generated.h"

class FDigitalHumanWebSocketManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDigitalHumanOnConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDigitalHumanOnConnectionError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDigitalHumanOnDisconnected, int32, StatusCode, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDigitalHumanOnJsonReceived, const FString&, JsonText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDigitalHumanOnBinaryReceived, const TArray<uint8>&, Bytes);

/**
 * Owns the single backend WebSocket connection for the lifetime of the game
 * instance. This is intentionally the ONLY thing in the plugin that talks to
 * the network - UDigitalHumanComponent (added in a later stage, attached to
 * the MetaHuman actor) subscribes to this subsystem's events rather than
 * opening its own connection, so a single backend connection is shared
 * correctly even if multiple digital human components exist in a level.
 *
 * Accessible from Blueprint via "Get Game Instance Subsystem" -> class
 * DigitalHumanSubsystem.
 */
UCLASS()
class DIGITALHUMANRUNTIME_API UDigitalHumanSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** Connects to the backend WebSocket server, e.g. "ws://127.0.0.1:8765". */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Network")
	void Connect(const FString& ServerURL);

	/** Closes the connection and disables automatic reconnect until Connect() is called again. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Network")
	void Disconnect();

	/** True if currently connected to the backend. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Digital Human|Network")
	bool IsConnected() const;

	/** Sends a JSON command string to the backend as a text frame. Caller is responsible for valid JSON formatting. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Network")
	void SendJSON(const FString& JsonText);

	/** Sends raw binary data to the backend as a binary frame. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Network")
	void SendBinary(const TArray<uint8>& Bytes);

	/** Enables/disables automatic reconnect with exponential backoff after an unexpected disconnect. Enabled by default. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Network")
	void SetAutoReconnect(bool bEnabled);

	/** Enables periodic application-level heartbeat text frames ({"type":"heartbeat"}) while connected. */
	UFUNCTION(BlueprintCallable, Category = "Digital Human|Network")
	void SetHeartbeatEnabled(bool bEnabled, float IntervalSeconds = 15.0f);

	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Network")
	FDigitalHumanOnConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Network")
	FDigitalHumanOnConnectionError OnConnectionError;

	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Network")
	FDigitalHumanOnDisconnected OnDisconnected;

	/** Fires for every text frame received - the raw string, unparsed. JSON parsing happens in a later stage / in Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Network")
	FDigitalHumanOnJsonReceived OnJsonReceived;

	/** Fires for every binary frame received - e.g. a chunk of raw PCM audio from the backend's streaming TTS. */
	UPROPERTY(BlueprintAssignable, Category = "Digital Human|Network")
	FDigitalHumanOnBinaryReceived OnBinaryReceived;

private:
	TSharedPtr<FDigitalHumanWebSocketManager> WebSocketManager;
};
