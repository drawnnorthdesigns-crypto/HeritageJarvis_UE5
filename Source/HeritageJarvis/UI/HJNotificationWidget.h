#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HJNotificationWidget.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UTextBlock;
class UBorder;
class USpacer;

UENUM(BlueprintType)
enum class EHJNotifType : uint8
{
    Info    UMETA(DisplayName = "Info"),
    Success UMETA(DisplayName = "Success"),
    Warning UMETA(DisplayName = "Warning"),
    Error   UMETA(DisplayName = "Error")
};

USTRUCT(BlueprintType)
struct FHJNotification
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Message;
    UPROPERTY(BlueprintReadOnly) EHJNotifType Type = EHJNotifType::Info;
    UPROPERTY(BlueprintReadOnly) float Duration = 3.0f;
    UPROPERTY(BlueprintReadOnly) float TimeRemaining = 3.0f;
};

/**
 * UHJNotificationWidget — Screen-corner toast overlay.
 * Add one instance to the viewport in HJMainGameMode and TartariaGameMode.
 * Call the static Toast() helper from anywhere.
 *
 * Builds its UMG layout programmatically in NativeOnInitialized().
 * Blueprint children can still override OnNotificationAdded / OnNotificationExpired.
 */
UCLASS(Abstract)
class HERITAGEJARVIS_API UHJNotificationWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------
    // Static singleton access — call from anywhere
    // -------------------------------------------------------

    static UHJNotificationWidget* Instance;

    static void Toast(const FString& Message,
                      EHJNotifType Type     = EHJNotifType::Info,
                      float        Duration = 3.0f);

    // -------------------------------------------------------
    // Direct call
    // -------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "HJ|Notifications")
    void ShowNotification(const FString& Message,
                          EHJNotifType  Type     = EHJNotifType::Info,
                          float         Duration = 3.0f);

    // Live list of toasts currently on screen
    UPROPERTY(BlueprintReadOnly, Category = "HJ|Notifications")
    TArray<FHJNotification> ActiveNotifications;

    // -------------------------------------------------------
    // Blueprint native events — default C++ impl, overridable in BP
    // -------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Notifications")
    void OnNotificationAdded(const FHJNotification& Notification);

    UFUNCTION(BlueprintNativeEvent, Category = "HJ|Notifications")
    void OnNotificationExpired(int32 Index);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildProgrammaticLayout();

    UPROPERTY()
    UVerticalBox* ToastStack = nullptr;
};
