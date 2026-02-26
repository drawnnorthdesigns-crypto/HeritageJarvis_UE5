#include "HJApiClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Async/Async.h"

// -------------------------------------------------------
// Projects
// -------------------------------------------------------

void UHJApiClient::GetProjects(FOnApiResponse OnComplete)
{
	Get(TEXT("/api/projects"), OnComplete);
}

void UHJApiClient::CreateProject(const FString& Name, const FString& InputPrimaMateria,
                                  bool bCheckpointMode, FOnApiResponse OnComplete)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("name"), Name);
	Body->SetStringField(TEXT("input_prima_materia"), InputPrimaMateria);
	Body->SetStringField(TEXT("checkpoint_mode"), bCheckpointMode ? TEXT("checkpoint") : TEXT("classic"));

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	Post(TEXT("/api/projects"), BodyStr, OnComplete);
}

void UHJApiClient::RunPipeline(const FString& ProjectId, FOnApiResponse OnComplete)
{
	FString Endpoint = FString::Printf(TEXT("/api/project/%s/run_pipeline"), *ProjectId);
	Post(Endpoint, TEXT("{}"), OnComplete);
}

void UHJApiClient::StopPipeline(const FString& ProjectId, FOnApiResponse OnComplete)
{
	FString Endpoint = FString::Printf(TEXT("/api/project/%s/stop_pipeline"), *ProjectId);
	Post(Endpoint, TEXT("{}"), OnComplete);
}

void UHJApiClient::GetPipelineStatus(const FString& ProjectId, FOnApiResponse OnComplete)
{
	FString Endpoint = FString::Printf(TEXT("/api/project/%s/pipeline_status"), *ProjectId);
	Get(Endpoint, OnComplete);
}

void UHJApiClient::DeleteProject(const FString& ProjectId, FOnApiResponse OnComplete)
{
	FString Endpoint = FString::Printf(TEXT("/api/project/%s"), *ProjectId);
	SendRequest(Endpoint, TEXT("DELETE"), TEXT(""), OnComplete);
}

// -------------------------------------------------------
// Chat
// -------------------------------------------------------

void UHJApiClient::SendChatMessage(const FString& Message, const FString& ProjectId,
                                    FOnApiResponse OnComplete)
{
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("prompt"), Message);
	if (!ProjectId.IsEmpty())
		Body->SetStringField(TEXT("project_id"), ProjectId);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	Post(TEXT("/chat"), BodyStr, OnComplete);
}

// -------------------------------------------------------
// Streaming Chat (Fix 1.1)
// -------------------------------------------------------

void UHJApiClient::SendStreamingChatMessage(const FString& Message, const FString& ProjectId,
                                             FOnStreamToken OnToken, FOnStreamComplete OnStreamDone,
                                             FOnApiResponse OnFallback)
{
	FHttpModule& Http = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();

	// Build JSON body
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField(TEXT("prompt"), Message);
	if (!ProjectId.IsEmpty())
		Body->SetStringField(TEXT("project_id"), ProjectId);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	Request->SetURL(BaseUrl + TEXT("/chat/stream"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("text/event-stream"));
	Request->SetTimeout(120.0f);  // Streaming can take longer
	Request->SetContentAsString(BodyStr);

	// Track how many bytes we've already processed from the stream
	TSharedPtr<int32> LastProcessedByte = MakeShared<int32>(0);

	// SSE progress callback — fires as chunks arrive
	Request->OnRequestProgress64().BindLambda(
		[OnToken, LastProcessedByte](FHttpRequestPtr Req, uint64 BytesSent, uint64 BytesReceived)
		{
			if (!Req.IsValid() || !Req->GetResponse().IsValid()) return;

			TArray<uint8> Content = Req->GetResponse()->GetContent();
			if (Content.Num() <= *LastProcessedByte) return;

			// Extract only new bytes (safe: explicit length, no null-terminator required)
			int32 NewStart = *LastProcessedByte;
			int32 NewLen = Content.Num() - NewStart;
			FUTF8ToTCHAR Converter(reinterpret_cast<const char*>(Content.GetData() + NewStart), NewLen);
			FString NewData(Converter.Length(), Converter.Get());

			*LastProcessedByte = Content.Num();

			// Parse SSE events: split on double newline
			// We process complete events (ending with \n\n)
			TArray<FString> Lines;
			NewData.ParseIntoArray(Lines, TEXT("\n"), false);

			for (const FString& Line : Lines)
			{
				if (!Line.StartsWith(TEXT("data: "))) continue;

				FString JsonStr = Line.Mid(6); // Skip "data: "
				TSharedPtr<FJsonObject> Obj;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
				if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) continue;

				// Check for token
				FString Token;
				if (Obj->TryGetStringField(TEXT("token"), Token))
				{
					// Fire on game thread
					AsyncTask(ENamedThreads::GameThread, [OnToken, Token]()
					{
						OnToken.ExecuteIfBound(Token);
					});
				}
			}
		});

	// Completion callback — fires when stream ends
	Request->OnProcessRequestComplete().BindLambda(
		[OnStreamDone, OnFallback, OnToken, LastProcessedByte](
			FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				// Stream failed — trigger fallback to non-streaming
				AsyncTask(ENamedThreads::GameThread, [OnFallback]()
				{
					OnFallback.ExecuteIfBound(false, TEXT("{\"error\":\"Streaming connection failed\"}"));
				});
				return;
			}

			int32 Code = Resp->GetResponseCode();
			if (Code < 200 || Code >= 300)
			{
				// Server rejected streaming — trigger fallback
				FString RespBody = Resp->GetContentAsString();
				AsyncTask(ENamedThreads::GameThread, [OnFallback, RespBody]()
				{
					OnFallback.ExecuteIfBound(false, RespBody);
				});
				return;
			}

			// Process any remaining data not caught by progress callback
			FString FullContent = Resp->GetContentAsString();
			FString State;
			FString Flags;

			// Find the "done" event in the full content
			TArray<FString> Lines;
			FullContent.ParseIntoArray(Lines, TEXT("\n"), false);

			for (const FString& Line : Lines)
			{
				if (!Line.StartsWith(TEXT("data: "))) continue;

				FString JsonStr = Line.Mid(6);
				TSharedPtr<FJsonObject> Obj;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
				if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) continue;

				bool bDone = false;
				if (Obj->TryGetBoolField(TEXT("done"), bDone) && bDone)
				{
					Obj->TryGetStringField(TEXT("state"), State);
					// Flags may be an array — serialize as string
					const TArray<TSharedPtr<FJsonValue>>* FlagsArr;
					if (Obj->TryGetArrayField(TEXT("flags"), FlagsArr))
					{
						TArray<FString> FlagStrs;
						for (auto& V : *FlagsArr)
						{
							FlagStrs.Add(V->AsString());
						}
						Flags = FString::Join(FlagStrs, TEXT(","));
					}
				}
			}

			AsyncTask(ENamedThreads::GameThread, [OnStreamDone, State, Flags]()
			{
				OnStreamDone.ExecuteIfBound(State, Flags);
			});
		});

	Request->ProcessRequest();
}

// -------------------------------------------------------
// Game World (Phase 3F)
// -------------------------------------------------------

void UHJApiClient::GetWorldState(FOnApiResponse OnComplete)
{
	Get(TEXT("/api/game/world_state"), OnComplete);
}

void UHJApiClient::PostGameTick(float DeltaTime, FOnApiResponse OnComplete)
{
	FString Body = FString::Printf(TEXT("{\"delta_time\":%.4f}"), DeltaTime);
	Post(TEXT("/api/game/tick"), Body, OnComplete);
}

// -------------------------------------------------------
// Library
// -------------------------------------------------------

void UHJApiClient::GetLibrary(FOnApiResponse OnComplete)
{
	Get(TEXT("/api/library/all_books"), OnComplete);
}

void UHJApiClient::SearchLibrary(const FString& Query, FOnApiResponse OnComplete)
{
	// Use vault endpoint with query as a filter
	FString Endpoint = FString::Printf(TEXT("/api/library/vault"));
	Get(Endpoint, OnComplete);
}

// -------------------------------------------------------
// Health
// -------------------------------------------------------

void UHJApiClient::CheckHealth(FOnApiResponse OnComplete)
{
	Get(TEXT("/health"), OnComplete);
}

// -------------------------------------------------------
// Generic
// -------------------------------------------------------

void UHJApiClient::Get(const FString& Endpoint, FOnApiResponse OnComplete)
{
	SendRequest(Endpoint, TEXT("GET"), TEXT(""), OnComplete);
}

void UHJApiClient::Post(const FString& Endpoint, const FString& JsonBody,
                         FOnApiResponse OnComplete)
{
	SendRequest(Endpoint, TEXT("POST"), JsonBody, OnComplete);
}

// -------------------------------------------------------
// Private — request with timeout + retry
// -------------------------------------------------------

void UHJApiClient::SendRequest(const FString& Endpoint, const FString& Verb,
                                const FString& Body, FOnApiResponse OnComplete,
                                int32 Attempt)
{
	FHttpModule& Http = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();

	Request->SetURL(BaseUrl + Endpoint);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

	// Fix 2.1: Prevent indefinite hangs on network issues
	Request->SetTimeout(RequestTimeoutSec);

	if (!Body.IsEmpty() && Body != TEXT("{}"))
		Request->SetContentAsString(Body);

	// Capture retry state for exponential backoff (Fix 2.2)
	int32 LocalMaxRetries = MaxRetries;
	float LocalBaseDelay = RetryBaseDelaySec;

	// Fix: Weak pointer prevents crash if ApiClient is GC'd before callback fires
	TWeakObjectPtr<UHJApiClient> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, Endpoint, Verb, Body, OnComplete, Attempt, LocalMaxRetries, LocalBaseDelay](
			FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			// Guard: ApiClient may have been destroyed during the async request
			UHJApiClient* Self = WeakThis.Get();

			// --- Connection failure ---
			if (!bConnected || !Resp.IsValid())
			{
				if (Self && Attempt < LocalMaxRetries)
				{
					float Delay = LocalBaseDelay * FMath::Pow(2.0f, static_cast<float>(Attempt));
					UE_LOG(LogTemp, Warning,
						TEXT("HJApiClient: Connection failed, retry %d/%d in %.1fs — %s"),
						Attempt + 1, LocalMaxRetries, Delay, *Endpoint);

					UWorld* World = Self->GetWorld();
					if (World)
					{
						FTimerHandle RetryTimer;
						FTimerDelegate RetryDelegate;
						RetryDelegate.BindLambda([WeakThis, Endpoint, Verb, Body, OnComplete, Attempt]()
						{
							if (UHJApiClient* RetrySelf = WeakThis.Get())
								RetrySelf->SendRequest(Endpoint, Verb, Body, OnComplete, Attempt + 1);
						});
						World->GetTimerManager().SetTimer(RetryTimer, RetryDelegate, Delay, false);
						return;
					}
				}
				UE_LOG(LogTemp, Warning,
					TEXT("HJApiClient: Request failed after %d attempts — %s"),
					Attempt + 1, *Endpoint);
				// Fix 2.4: Notify listeners that connection is exhausted
				if (Self && Self->OnConnectionExhausted.IsBound())
				{
					AsyncTask(ENamedThreads::GameThread, [WeakThis, Endpoint]()
					{
						if (UHJApiClient* BroadcastSelf = WeakThis.Get())
							BroadcastSelf->OnConnectionExhausted.Broadcast(Endpoint);
					});
				}
				OnComplete.ExecuteIfBound(false, TEXT("{\"error\":\"Flask server unreachable\"}"));
				return;
			}

			int32 Code = Resp->GetResponseCode();
			FString RespBody = Resp->GetContentAsString();

			// --- Success ---
			if (Code >= 200 && Code < 300)
			{
				OnComplete.ExecuteIfBound(true, RespBody);
				return;
			}

			// --- Server error (5xx) — retry with backoff ---
			if (Self && Code >= 500 && Attempt < LocalMaxRetries)
			{
				float Delay = LocalBaseDelay * FMath::Pow(2.0f, static_cast<float>(Attempt));
				UE_LOG(LogTemp, Warning,
					TEXT("HJApiClient: HTTP %d, retry %d/%d in %.1fs — %s"),
					Code, Attempt + 1, LocalMaxRetries, Delay, *Endpoint);

				UWorld* World = Self->GetWorld();
				if (World)
				{
					FTimerHandle RetryTimer;
					FTimerDelegate RetryDelegate;
					RetryDelegate.BindLambda([WeakThis, Endpoint, Verb, Body, OnComplete, Attempt]()
					{
						if (UHJApiClient* RetrySelf = WeakThis.Get())
							RetrySelf->SendRequest(Endpoint, Verb, Body, OnComplete, Attempt + 1);
					});
					World->GetTimerManager().SetTimer(RetryTimer, RetryDelegate, Delay, false);
					return;
				}
			}

			// --- Client error (4xx) or exhausted retries — fail immediately ---
			UE_LOG(LogTemp, Warning, TEXT("HJApiClient: HTTP %d — %s"), Code, *RespBody);
			OnComplete.ExecuteIfBound(false, RespBody);
		});

	Request->ProcessRequest();
}

UWorld* UHJApiClient::GetWorld() const
{
	// Walk outer chain to find a world context (owned by GameInstance)
	UObject* Outer = GetOuter();
	while (Outer)
	{
		UWorld* World = Cast<UWorld>(Outer);
		if (World) return World;
		if (Outer->GetWorld()) return Outer->GetWorld();
		Outer = Outer->GetOuter();
	}
	return nullptr;
}
