#pragma once

#include "CoreMinimal.h"
#include "HJWebSocketClient.generated.h"

class IWebSocket;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWSMessage, const FString&, Channel, const FString&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWSConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWSDisconnected, const FString&, Reason);

/**
 * UHJWebSocketClient -- Real-time event bridge to Flask WebSocket hub.
 * Connects to ws://127.0.0.1:5000/ws and receives multiplexed channel messages.
 * Channels: game, pipeline, health, proxy, chat, mesh
 * Falls back gracefully if connection fails -- HTTP polling continues separately.
 */
UCLASS()
class HERITAGEJARVIS_API UHJWebSocketClient : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize and attempt connection. */
	void Connect(const FString& URL = TEXT("ws://127.0.0.1:5000/ws"));

	/** Disconnect cleanly. */
	void Disconnect();

	/** Is the WebSocket currently connected? */
	UFUNCTION(BlueprintCallable, Category = "HJ|WebSocket")
	bool IsConnected() const;

	/** Send a message on a specific channel. */
	UFUNCTION(BlueprintCallable, Category = "HJ|WebSocket")
	void Send(const FString& Channel, const FString& Data);

	/** Subscribe to a channel. */
	UFUNCTION(BlueprintCallable, Category = "HJ|WebSocket")
	void Subscribe(const FString& Channel);

	// -------------------------------------------------------
	// Events
	// -------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "HJ|WebSocket")
	FOnWSMessage OnMessage;

	UPROPERTY(BlueprintAssignable, Category = "HJ|WebSocket")
	FOnWSConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "HJ|WebSocket")
	FOnWSDisconnected OnDisconnected;

	/** Call periodically to attempt reconnection if disconnected. */
	void Tick(float DeltaTime);

private:
	TSharedPtr<IWebSocket> Socket;
	bool bConnected = false;
	bool bConnecting = false;

	// Reconnection
	float ReconnectTimer = 0.f;
	float ReconnectInterval = 5.f;
	int32 ReconnectAttempts = 0;
	int32 MaxReconnectAttempts = 10;

	FString ServerURL;

	void OnSocketConnected();
	void OnSocketConnectionError(const FString& Error);
	void OnSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnSocketMessage(const FString& Message);

	/** Parse multiplexed message format: {"channel": "...", "data": {...}} */
	void ParseMessage(const FString& RawMessage);
};
