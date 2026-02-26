#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HAL/PlatformProcess.h"
#include "HJFlaskProcess.generated.h"

/**
 * UHJFlaskProcess — Manages the Flask backend as a child process.
 * Created by HJGameInstance::Init(), destroyed on Shutdown().
 *
 * Lifecycle:
 *   LaunchFlask()  → FPlatformProcess::CreateProc()
 *   ShutdownFlask() → POST /shutdown, then TerminateProc() after 3s
 *   IsProcessRunning() → FPlatformProcess::IsProcRunning()
 */
UCLASS()
class HERITAGEJARVIS_API UHJFlaskProcess : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Launch the Flask backend.
	 * @param PythonExe  Path to python executable (empty = auto-discover)
	 * @param ProjectRoot  Path to the Heritage Jarvis Python project root
	 * @return true if process was started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "HJ|Flask")
	bool LaunchFlask(const FString& PythonExe, const FString& ProjectRoot);

	/** Gracefully shut down Flask: POST /shutdown, then force-kill after 3s. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Flask")
	void ShutdownFlask();

	/** Check if the Flask child process is still running. */
	UFUNCTION(BlueprintPure, Category = "HJ|Flask")
	bool IsProcessRunning() const;

	/** The resolved python executable path used for launch. */
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Flask")
	FString ResolvedPythonPath;

	/** The project root used for launch. */
	UPROPERTY(BlueprintReadOnly, Category = "HJ|Flask")
	FString ResolvedProjectRoot;

private:

	/** Discover python executable: check .venv/Scripts/python.exe, fallback to PATH. */
	FString DiscoverPython(const FString& ProjectRoot) const;

	FProcHandle ProcessHandle;
	uint32 ProcessId = 0;
};
