#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJDashboardWidget.generated.h"

class UWebBrowser;
class UOverlay;
class UBorder;
class UHorizontalBox;
class UTextBlock;
class UButton;
class USpacer;

/**
 * UHJDashboardWidget — Embeds the Flask web dashboard via CEF UWebBrowser.
 *
 * Critical CEF fix: UWebBrowser is a DIRECT child of UOverlay (Layer 1).
 * Previous approach nested it inside VerticalBox/ScrollBox which caused
 * SharedPointer assertion crashes in Slate rendering.
 *
 * Layout:
 *   UOverlay (root)
 *     |- UWebBrowser (Layer 1 — fills panel, 40px bottom padding)
 *     |- UBorder + HBox (Layer 2 — 40px status bar at bottom)
 *          |- FlaskStatusDot + StatusLabel
 *          |- Spacer (fill)
 *          |- "Enter Tartaria" button
 *
 * Fallback: if UWebBrowser fails to load within 10s, show "Open in Browser" button.
 */
UCLASS()
class HERITAGEJARVIS_API UHJDashboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Navigate the embedded browser to a specific URL. */
	UFUNCTION(BlueprintCallable, Category = "HJ|Dashboard")
	void NavigateTo(const FString& Url);

	UFUNCTION()
	void OnEnterTartariaClicked();

	UFUNCTION()
	void OnOpenInBrowserClicked();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	void CheckFlaskHealth();
	void OnFallbackTimeout();

	UPROPERTY()
	UWebBrowser* WebBrowserWidget = nullptr;

	UPROPERTY()
	UTextBlock* StatusLabel = nullptr;

	UPROPERTY()
	UBorder* StatusDot = nullptr;

	UPROPERTY()
	UButton* FallbackButton = nullptr;

	FTimerHandle HealthTimerHandle;
	FTimerHandle FallbackTimerHandle;
	bool bFlaskOnline = false;
	bool bBrowserLoaded = false;
};
