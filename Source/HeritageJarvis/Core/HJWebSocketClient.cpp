#include "HJWebSocketClient.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

void UHJWebSocketClient::Connect(const FString& URL)
{
	if (bConnected || bConnecting) return;

	ServerURL = URL;
	bConnecting = true;

	// Ensure WebSockets module is loaded
	FModuleManager::Get().LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));

	Socket = FWebSocketsModule::Get().CreateWebSocket(URL, TEXT("ws"));

	Socket->OnConnected().AddLambda([this]()
	{
		OnSocketConnected();
	});

	Socket->OnConnectionError().AddLambda([this](const FString& Error)
	{
		OnSocketConnectionError(Error);
	});

	Socket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean)
	{
		OnSocketClosed(StatusCode, Reason, bWasClean);
	});

	Socket->OnMessage().AddLambda([this](const FString& Message)
	{
		OnSocketMessage(Message);
	});

	Socket->Connect();

	UE_LOG(LogTemp, Log, TEXT("HJWebSocket: Connecting to %s"), *URL);
}

void UHJWebSocketClient::Disconnect()
{
	if (Socket.IsValid() && Socket->IsConnected())
	{
		Socket->Close();
	}
	bConnected = false;
	bConnecting = false;
	Socket.Reset();
}

bool UHJWebSocketClient::IsConnected() const
{
	return bConnected && Socket.IsValid() && Socket->IsConnected();
}

void UHJWebSocketClient::Send(const FString& Channel, const FString& Data)
{
	if (!IsConnected()) return;

	TSharedPtr<FJsonObject> Msg = MakeShareable(new FJsonObject());
	Msg->SetStringField(TEXT("channel"), Channel);
	Msg->SetStringField(TEXT("data"), Data);

	FString MsgStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MsgStr);
	FJsonSerializer::Serialize(Msg.ToSharedRef(), Writer);

	Socket->Send(MsgStr);
}

void UHJWebSocketClient::Subscribe(const FString& Channel)
{
	// Send a subscribe message
	Send(TEXT("subscribe"), Channel);
}

void UHJWebSocketClient::Tick(float DeltaTime)
{
	if (bConnected || bConnecting) return;

	// Attempt reconnection with backoff
	if (ReconnectAttempts >= MaxReconnectAttempts) return;

	ReconnectTimer += DeltaTime;
	float CurrentInterval = ReconnectInterval * FMath::Min(4.f, FMath::Pow(1.5f, (float)ReconnectAttempts));

	if (ReconnectTimer >= CurrentInterval)
	{
		ReconnectTimer = 0.f;
		ReconnectAttempts++;
		UE_LOG(LogTemp, Log, TEXT("HJWebSocket: Reconnect attempt %d/%d"), ReconnectAttempts, MaxReconnectAttempts);
		Connect(ServerURL);
	}
}

void UHJWebSocketClient::OnSocketConnected()
{
	bConnected = true;
	bConnecting = false;
	ReconnectAttempts = 0;
	ReconnectTimer = 0.f;

	UE_LOG(LogTemp, Log, TEXT("HJWebSocket: Connected to %s"), *ServerURL);

	// Auto-subscribe to key channels
	Subscribe(TEXT("game"));
	Subscribe(TEXT("pipeline"));
	Subscribe(TEXT("health"));

	OnConnected.Broadcast();
}

void UHJWebSocketClient::OnSocketConnectionError(const FString& Error)
{
	bConnecting = false;
	bConnected = false;
	UE_LOG(LogTemp, Warning, TEXT("HJWebSocket: Connection error: %s"), *Error);
}

void UHJWebSocketClient::OnSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	bConnected = false;
	bConnecting = false;
	UE_LOG(LogTemp, Log, TEXT("HJWebSocket: Closed (code=%d, reason=%s, clean=%d)"),
		StatusCode, *Reason, bWasClean);
	OnDisconnected.Broadcast(Reason);
}

void UHJWebSocketClient::OnSocketMessage(const FString& Message)
{
	ParseMessage(Message);
}

void UHJWebSocketClient::ParseMessage(const FString& RawMessage)
{
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawMessage);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		// Not JSON -- broadcast as raw on "raw" channel
		OnMessage.Broadcast(TEXT("raw"), RawMessage);
		return;
	}

	FString Channel;
	if (!Json->TryGetStringField(TEXT("channel"), Channel))
	{
		Channel = TEXT("unknown");
	}

	// Extract data as string (could be object or primitive)
	FString Payload;
	const TSharedPtr<FJsonObject>* DataObj;
	if (Json->TryGetObjectField(TEXT("data"), DataObj))
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
		FJsonSerializer::Serialize((*DataObj).ToSharedRef(), Writer);
	}
	else
	{
		Json->TryGetStringField(TEXT("data"), Payload);
	}

	OnMessage.Broadcast(Channel, Payload);
}
