#include "TartariaSolarSystemManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

TWeakObjectPtr<ATartariaSolarSystemManager> ATartariaSolarSystemManager::Instance;

ATartariaSolarSystemManager::ATartariaSolarSystemManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;  // 10Hz orbital updates (not per-frame)
}

void ATartariaSolarSystemManager::BeginPlay()
{
	Super::BeginPlay();
	Instance = this;
	InitializeBodies();
	UE_LOG(LogTemp, Log, TEXT("SolarSystemManager: Initialized %d celestial bodies"), Bodies.Num());
}

void ATartariaSolarSystemManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateOrbitalPositions(DeltaTime);
}

// -------------------------------------------------------
// Body initialization — mirrors Python solar_system.py
// -------------------------------------------------------

void ATartariaSolarSystemManager::InitializeBodies()
{
	Bodies.Empty();

	auto AddBody = [&](const FString& Key, const FString& Type,
		double Mass, double Radius, double OrbitAU, double PeriodDays,
		float Gravity, float EscVel, bool bAtmo)
	{
		FTartariaCelestialBody B;
		B.BodyKey          = Key;
		B.BodyType         = Type;
		B.MassKg           = Mass;
		B.RadiusKm         = Radius;
		B.OrbitalRadiusAU  = OrbitAU;
		B.OrbitalPeriodDays = PeriodDays;
		B.SurfaceGravity   = Gravity;
		B.EscapeVelocity   = EscVel;
		B.bHasAtmosphere   = bAtmo;
		B.CurrentAngleRad  = FMath::FRandRange(0.0, 2.0 * PI);  // Random start angle
		Bodies.Add(B);
	};

	//                   Key              Type             Mass(kg)   Radius(km)  Orbit(AU) Period(d) g(m/s2) Vesc(m/s) Atmo
	AddBody(TEXT("sol"),      TEXT("Star"),         1.989e30, 695700.0,   0.0,       0.0,      274.0f,  617500.f, false);
	AddBody(TEXT("mercury"),  TEXT("Planet"),       3.301e23, 2440.0,     0.387,     88.0,     3.70f,   4250.f,   false);
	AddBody(TEXT("venus"),    TEXT("Planet"),       4.867e24, 6052.0,     0.723,     225.0,    8.87f,   10360.f,  true);
	AddBody(TEXT("earth"),    TEXT("Planet"),       5.972e24, 6371.0,     1.0,       365.25,   9.81f,   11186.f,  true);
	AddBody(TEXT("mars"),     TEXT("Planet"),       6.417e23, 3390.0,     1.524,     687.0,    3.72f,   5027.f,   true);
	AddBody(TEXT("jupiter"),  TEXT("Gas Giant"),    1.898e27, 69911.0,    5.203,     4333.0,   24.79f,  59500.f,  true);
	AddBody(TEXT("saturn"),   TEXT("Gas Giant"),    5.683e26, 58232.0,    9.537,     10759.0,  10.44f,  35500.f,  true);
	AddBody(TEXT("titan"),    TEXT("Moon"),         1.345e23, 2575.0,     9.537,     15.95,    1.35f,   2640.f,   true);
	AddBody(TEXT("europa"),   TEXT("Moon"),         4.8e22,   1561.0,     5.203,     3.55,     1.315f,  2025.f,   false);
	AddBody(TEXT("asteroid_belt"), TEXT("Mining Region"), 3.0e21, 0.0,   2.7,       1680.0,   0.0f,    0.f,      false);
}

// -------------------------------------------------------
// Orbital position updates
// -------------------------------------------------------

void ATartariaSolarSystemManager::UpdateOrbitalPositions(float DeltaTime)
{
	SimTimeSec += static_cast<double>(DeltaTime) * static_cast<double>(TimeScale);

	for (FTartariaCelestialBody& Body : Bodies)
	{
		if (Body.OrbitalPeriodDays <= 0.0 || Body.OrbitalRadiusAU <= 0.0)
			continue;  // Sol or zero-orbit bodies don't move

		// Angular velocity: 2*PI / period_seconds
		double PeriodSec = Body.OrbitalPeriodDays * 86400.0;
		double AngularVel = (2.0 * PI) / PeriodSec;

		Body.CurrentAngleRad = FMath::Fmod(
			Body.CurrentAngleRad + AngularVel * static_cast<double>(DeltaTime) * static_cast<double>(TimeScale),
			2.0 * PI);
	}
}

// -------------------------------------------------------
// Accessors
// -------------------------------------------------------

FTartariaCelestialBody ATartariaSolarSystemManager::GetBody(const FString& BodyKey) const
{
	for (const FTartariaCelestialBody& B : Bodies)
	{
		if (B.BodyKey.Equals(BodyKey, ESearchCase::IgnoreCase))
			return B;
	}
	return FTartariaCelestialBody();
}

FVector ATartariaSolarSystemManager::GetBodyWorldPosition(const FString& BodyKey) const
{
	for (const FTartariaCelestialBody& B : Bodies)
	{
		if (!B.BodyKey.Equals(BodyKey, ESearchCase::IgnoreCase))
			continue;

		if (B.OrbitalRadiusAU <= 0.0)
			return OriginOffset;  // Sol at origin

		// Convert orbital position to UE5 coords (XY plane)
		double X = B.OrbitalRadiusAU * AU_TO_UE_CM * FMath::Cos(B.CurrentAngleRad);
		double Y = B.OrbitalRadiusAU * AU_TO_UE_CM * FMath::Sin(B.CurrentAngleRad);

		return FVector(
			static_cast<float>(X) + OriginOffset.X,
			static_cast<float>(Y) + OriginOffset.Y,
			OriginOffset.Z
		);
	}

	return FVector::ZeroVector;
}

void ATartariaSolarSystemManager::SetPlayerCurrentBody(const FString& BodyKey)
{
	PlayerCurrentBody = BodyKey;
	RebaseOriginToBody(BodyKey);

	// ── Fetch celestial content from Python backend ──────────────────────────
	// Fire-and-forget HTTP GET to /api/game/celestial/<body_id>.
	// On success: parse landing zones + resources into the matching Body entry.
	// Sol has no landing zones but we still fetch for consistency.
	FString Url = FString::Printf(TEXT("http://127.0.0.1:5000/api/game/celestial/%s"), *BodyKey);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("Accept"), TEXT("application/json"));

	// Capture BodyKey by value; 'this' captured weakly via weak ptr
	TWeakObjectPtr<ATartariaSolarSystemManager> WeakThis(this);

	Req->OnProcessRequestComplete().BindLambda(
		[WeakThis, BodyKey](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SolarSystem: celestial content fetch failed for %s (no connection)"),
					*BodyKey);
				return;
			}

			if (Response->GetResponseCode() != 200)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SolarSystem: celestial content HTTP %d for %s"),
					Response->GetResponseCode(), *BodyKey);
				return;
			}

			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader =
				TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SolarSystem: failed to parse celestial JSON for %s"), *BodyKey);
				return;
			}

			ATartariaSolarSystemManager* Manager = WeakThis.Get();
			if (Manager)
			{
				Manager->ParseCelestialContent(JsonObj, BodyKey);
			}
		});

	Req->ProcessRequest();
}

// -------------------------------------------------------
// Celestial content parsing
// -------------------------------------------------------

void ATartariaSolarSystemManager::ParseCelestialContent(
	const TSharedPtr<FJsonObject>& ContentJson, const FString& BodyId)
{
	if (!ContentJson.IsValid())
		return;

	// Find the matching body entry
	FTartariaCelestialBody* BodyPtr = nullptr;
	for (FTartariaCelestialBody& B : Bodies)
	{
		if (B.BodyKey.Equals(BodyId, ESearchCase::IgnoreCase))
		{
			BodyPtr = &B;
			break;
		}
	}

	if (!BodyPtr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SolarSystem: ParseCelestialContent — body '%s' not in Bodies array"), *BodyId);
		return;
	}

	// Clear stale data before repopulating
	BodyPtr->LandingZones.Empty();

	// Parse "landing_zones" array
	const TArray<TSharedPtr<FJsonValue>>* ZonesArr = nullptr;
	if (!ContentJson->TryGetArrayField(TEXT("landing_zones"), ZonesArr) || !ZonesArr)
	{
		// Body may legitimately have no landing zones (e.g. Sol)
		UE_LOG(LogTemp, Log,
			TEXT("SolarSystem: '%s' has no landing_zones in content"), *BodyId);
		return;
	}

	for (const TSharedPtr<FJsonValue>& ZoneVal : *ZonesArr)
	{
		const TSharedPtr<FJsonObject>* ZoneObjPtr = nullptr;
		if (!ZoneVal->TryGetObject(ZoneObjPtr) || !ZoneObjPtr)
			continue;

		const TSharedPtr<FJsonObject>& ZoneObj = *ZoneObjPtr;
		FTartariaLandingZone Zone;

		ZoneObj->TryGetStringField(TEXT("zone_id"),    Zone.ZoneId);
		ZoneObj->TryGetStringField(TEXT("name"),       Zone.ZoneName);
		ZoneObj->TryGetStringField(TEXT("biome_type"), Zone.BiomeType);
		ZoneObj->TryGetStringField(TEXT("description"),Zone.Description);
		ZoneObj->TryGetBoolField  (TEXT("discovered"), Zone.bDiscovered);

		double HazardLevel = 0.0;
		ZoneObj->TryGetNumberField(TEXT("hazard_level"), HazardLevel);
		Zone.HazardLevel = static_cast<float>(HazardLevel);

		// Parse resources within each zone
		const TArray<TSharedPtr<FJsonValue>>* ResArr = nullptr;
		if (ZoneObj->TryGetArrayField(TEXT("resources"), ResArr) && ResArr)
		{
			for (const TSharedPtr<FJsonValue>& ResVal : *ResArr)
			{
				const TSharedPtr<FJsonObject>* ResObjPtr = nullptr;
				if (!ResVal->TryGetObject(ResObjPtr) || !ResObjPtr)
					continue;

				const TSharedPtr<FJsonObject>& ResObj = *ResObjPtr;
				FCelestialResource Res;

				ResObj->TryGetStringField(TEXT("type"), Res.ResourceType);

				double Abundance = 0.0, Difficulty = 0.0, Depleted = 0.0;
				ResObj->TryGetNumberField(TEXT("abundance"),              Abundance);
				ResObj->TryGetNumberField(TEXT("extraction_difficulty"),  Difficulty);
				ResObj->TryGetNumberField(TEXT("depleted"),               Depleted);

				Res.Abundance            = static_cast<float>(Abundance);
				Res.ExtractionDifficulty = static_cast<float>(Difficulty);
				Res.DepletionPct         = static_cast<float>(Depleted);

				Zone.Resources.Add(Res);
			}
		}

		BodyPtr->LandingZones.Add(Zone);
	}

	UE_LOG(LogTemp, Log,
		TEXT("SolarSystem: Populated %d landing zone(s) for '%s'"),
		BodyPtr->LandingZones.Num(), *BodyId);

	// Log available zone names for immediate debugging
	for (const FTartariaLandingZone& Z : BodyPtr->LandingZones)
	{
		UE_LOG(LogTemp, Log,
			TEXT("  Zone: %s | Biome: %s | Hazard: %.2f | Resources: %d"),
			*Z.ZoneName, *Z.BiomeType, Z.HazardLevel, Z.Resources.Num());
	}
}

// -------------------------------------------------------
// Transit calculations (Hohmann transfer)
// -------------------------------------------------------

float ATartariaSolarSystemManager::CalculateHohmannDeltaV(
	const FString& FromBody, const FString& ToBody) const
{
	FTartariaCelestialBody From = GetBody(FromBody);
	FTartariaCelestialBody To   = GetBody(ToBody);

	if (From.OrbitalRadiusAU <= 0.0 || To.OrbitalRadiusAU <= 0.0)
		return 0.f;

	// Hohmann transfer delta-V calculation
	// r1, r2 in meters
	double r1 = From.OrbitalRadiusAU * AU_METERS;
	double r2 = To.OrbitalRadiusAU * AU_METERS;
	double mu = G_CONST * SUN_MASS;

	// Semi-major axis of transfer orbit
	double a_transfer = (r1 + r2) / 2.0;

	// Velocity at departure orbit
	double v1_circular = FMath::Sqrt(mu / r1);
	double v1_transfer = FMath::Sqrt(mu * (2.0 / r1 - 1.0 / a_transfer));
	double dv1 = FMath::Abs(v1_transfer - v1_circular);

	// Velocity at arrival orbit
	double v2_circular = FMath::Sqrt(mu / r2);
	double v2_transfer = FMath::Sqrt(mu * (2.0 / r2 - 1.0 / a_transfer));
	double dv2 = FMath::Abs(v2_circular - v2_transfer);

	return static_cast<float>(dv1 + dv2);
}

float ATartariaSolarSystemManager::CalculateTransitTime(
	const FString& FromBody, const FString& ToBody) const
{
	FTartariaCelestialBody From = GetBody(FromBody);
	FTartariaCelestialBody To   = GetBody(ToBody);

	if (From.OrbitalRadiusAU <= 0.0 || To.OrbitalRadiusAU <= 0.0)
		return 0.f;

	double r1 = From.OrbitalRadiusAU * AU_METERS;
	double r2 = To.OrbitalRadiusAU * AU_METERS;
	double mu = G_CONST * SUN_MASS;

	// Hohmann transfer time = half the transfer orbit period
	double a_transfer = (r1 + r2) / 2.0;
	double T_transfer = PI * FMath::Sqrt(
		(a_transfer * a_transfer * a_transfer) / mu);

	// Compress to real-time seconds using TimeScale
	// T_transfer is in sim seconds; divide by TimeScale to get real seconds
	if (TimeScale > 0.f)
		return static_cast<float>(T_transfer / static_cast<double>(TimeScale));

	return static_cast<float>(T_transfer);
}

// -------------------------------------------------------
// Origin rebasing
// -------------------------------------------------------

void ATartariaSolarSystemManager::RebaseOriginToBody(const FString& BodyKey)
{
	for (const FTartariaCelestialBody& B : Bodies)
	{
		if (!B.BodyKey.Equals(BodyKey, ESearchCase::IgnoreCase))
			continue;

		if (B.OrbitalRadiusAU <= 0.0)
		{
			OriginOffset = FVector::ZeroVector;
			return;
		}

		// Negate the body's position so it becomes the new origin
		double X = B.OrbitalRadiusAU * AU_TO_UE_CM * FMath::Cos(B.CurrentAngleRad);
		double Y = B.OrbitalRadiusAU * AU_TO_UE_CM * FMath::Sin(B.CurrentAngleRad);
		OriginOffset = FVector(static_cast<float>(-X), static_cast<float>(-Y), 0.f);

		UE_LOG(LogTemp, Log, TEXT("SolarSystemManager: Origin rebased to %s (offset: %s)"),
			*BodyKey, *OriginOffset.ToString());
		return;
	}
}

// -------------------------------------------------------
// Singleton
// -------------------------------------------------------

ATartariaSolarSystemManager* ATartariaSolarSystemManager::Get(const UObject* WorldContextObject)
{
	if (Instance.IsValid())
		return Instance.Get();

	if (!WorldContextObject) return nullptr;
	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;

	for (TActorIterator<ATartariaSolarSystemManager> It(World); It; ++It)
	{
		Instance = *It;
		return Instance.Get();
	}

	return nullptr;
}
