#include "HJFlaskProcess.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

bool UHJFlaskProcess::LaunchFlask(const FString& PythonExe, const FString& ProjectRoot)
{
	// Resolve project root
	ResolvedProjectRoot = ProjectRoot;
	if (ResolvedProjectRoot.IsEmpty())
	{
		// Default: derive from UE5 project directory, go up one level
		ResolvedProjectRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("HeritageJarvis_Project - Copy")));
	}

	// Resolve python path
	ResolvedPythonPath = PythonExe;
	if (ResolvedPythonPath.IsEmpty())
	{
		ResolvedPythonPath = DiscoverPython(ResolvedProjectRoot);
	}

	if (ResolvedPythonPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("HJFlaskProcess: Could not find Python executable"));
		return false;
	}

	// Build command: python -m heritage_jarvis web --no-browser
	FString Args = FString::Printf(TEXT("-m heritage_jarvis web --no-browser"));

	UE_LOG(LogTemp, Log, TEXT("HJFlaskProcess: Launching Flask — %s %s (cwd: %s)"),
		*ResolvedPythonPath, *Args, *ResolvedProjectRoot);

	ProcessHandle = FPlatformProcess::CreateProc(
		*ResolvedPythonPath,
		*Args,
		/* bLaunchDetached = */ false,
		/* bLaunchHidden = */ true,
		/* bLaunchReallyHidden = */ true,
		&ProcessId,
		/* PriorityModifier = */ 0,
		*ResolvedProjectRoot,  // Working directory
		nullptr                // Pipe (unused)
	);

	if (!ProcessHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("HJFlaskProcess: Failed to create process"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("HJFlaskProcess: Flask started (PID %u)"), ProcessId);
	return true;
}

void UHJFlaskProcess::ShutdownFlask()
{
	if (!ProcessHandle.IsValid() || !FPlatformProcess::IsProcRunning(ProcessHandle))
	{
		UE_LOG(LogTemp, Log, TEXT("HJFlaskProcess: No running process to shut down"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("HJFlaskProcess: Sending /shutdown to Flask..."));

	// POST /shutdown for graceful exit
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://127.0.0.1:5000/shutdown"));
	Request->SetVerb(TEXT("POST"));
	Request->SetTimeout(3.0f);
	Request->ProcessRequest();

	// Wait up to 3 seconds for graceful shutdown
	double StartTime = FPlatformTime::Seconds();
	while (FPlatformProcess::IsProcRunning(ProcessHandle))
	{
		if (FPlatformTime::Seconds() - StartTime > 3.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("HJFlaskProcess: Grace period expired — force-killing PID %u"), ProcessId);
			FPlatformProcess::TerminateProc(ProcessHandle, /* bKillTree = */ true);
			break;
		}
		FPlatformProcess::Sleep(0.1f);
	}

	FPlatformProcess::CloseProc(ProcessHandle);
	ProcessHandle.Reset();
	ProcessId = 0;
	UE_LOG(LogTemp, Log, TEXT("HJFlaskProcess: Flask process terminated"));
}

bool UHJFlaskProcess::IsProcessRunning() const
{
	FProcHandle Handle = ProcessHandle;
	return Handle.IsValid() && FPlatformProcess::IsProcRunning(Handle);
}

FString UHJFlaskProcess::DiscoverPython(const FString& ProjectRoot) const
{
	// 1. Check .venv/Scripts/python.exe relative to project root (Windows)
	FString VenvPython = FPaths::Combine(ProjectRoot, TEXT(".venv"), TEXT("Scripts"), TEXT("python.exe"));
	if (IFileManager::Get().FileExists(*VenvPython))
	{
		UE_LOG(LogTemp, Log, TEXT("HJFlaskProcess: Found venv Python at %s"), *VenvPython);
		return VenvPython;
	}

	// 2. Check .venv/bin/python (Linux/Mac)
	FString VenvPythonUnix = FPaths::Combine(ProjectRoot, TEXT(".venv"), TEXT("bin"), TEXT("python"));
	if (IFileManager::Get().FileExists(*VenvPythonUnix))
	{
		UE_LOG(LogTemp, Log, TEXT("HJFlaskProcess: Found venv Python at %s"), *VenvPythonUnix);
		return VenvPythonUnix;
	}

	// 3. Fallback: python on PATH
	UE_LOG(LogTemp, Log, TEXT("HJFlaskProcess: No venv found, using 'python' from PATH"));
	return TEXT("python");
}
