#include "HJNotificationWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Styling/CoreStyle.h"

UHJNotificationWidget* UHJNotificationWidget::Instance = nullptr;

// -------------------------------------------------------
// Programmatic layout
// -------------------------------------------------------

void UHJNotificationWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!WidgetTree) return;

    // If a Blueprint child already has a layout, don't overwrite it
    UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
    if (ExistingRoot && ExistingRoot->GetChildrenCount() > 0) return;

    BuildProgrammaticLayout();
}

void UHJNotificationWidget::BuildProgrammaticLayout()
{
    // Root: CanvasPanel for absolute positioning
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), FName("Root"));
    WidgetTree->RootWidget = Root;

    // Toast stack: VerticalBox anchored to top-right
    ToastStack = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), FName("ToastStack"));

    UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ToastStack);
    ToastSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
    ToastSlot->SetAlignment(FVector2D(1.0f, 0.0f));
    ToastSlot->SetAutoSize(true);
    ToastSlot->SetOffsets(FMargin(0, 50, 0, 0)); // 50px from top
}

// -------------------------------------------------------
// Lifecycle (unchanged)
// -------------------------------------------------------

void UHJNotificationWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Instance = this;
}

void UHJNotificationWidget::NativeDestruct()
{
    if (Instance == this) Instance = nullptr;
    Super::NativeDestruct();
}

void UHJNotificationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    for (int32 i = ActiveNotifications.Num() - 1; i >= 0; --i)
    {
        ActiveNotifications[i].TimeRemaining -= InDeltaTime;
        if (ActiveNotifications[i].TimeRemaining <= 0.0f)
        {
            OnNotificationExpired(i);
            ActiveNotifications.RemoveAt(i);
        }
    }
}

void UHJNotificationWidget::ShowNotification(const FString& Message,
                                              EHJNotifType  Type,
                                              float         Duration)
{
    FHJNotification N;
    N.Message       = Message;
    N.Type          = Type;
    N.Duration      = Duration;
    N.TimeRemaining = Duration;

    // Cap at 5 simultaneous toasts — drop oldest
    if (ActiveNotifications.Num() >= 5)
        ActiveNotifications.RemoveAt(0);

    ActiveNotifications.Add(N);
    OnNotificationAdded(N);
}

// static
void UHJNotificationWidget::Toast(const FString& Message, EHJNotifType Type, float Duration)
{
    if (Instance)
        Instance->ShowNotification(Message, Type, Duration);
}

// -------------------------------------------------------
// BlueprintNativeEvent default implementations
// -------------------------------------------------------

void UHJNotificationWidget::OnNotificationAdded_Implementation(const FHJNotification& Notification)
{
    if (!ToastStack) return;

    // Pick background color based on notification type
    FLinearColor BgColor;
    switch (Notification.Type)
    {
    case EHJNotifType::Success: BgColor = FLinearColor(0.1f, 0.3f, 0.1f, 0.95f); break;
    case EHJNotifType::Warning: BgColor = FLinearColor(0.35f, 0.3f, 0.05f, 0.95f); break;
    case EHJNotifType::Error:   BgColor = FLinearColor(0.35f, 0.1f, 0.1f, 0.95f); break;
    default:                    BgColor = FLinearColor(0.15f, 0.15f, 0.25f, 0.95f); break;
    }

    // Create toast border (dynamic — use NewObject, not WidgetTree->ConstructWidget)
    UBorder* ToastBorder = NewObject<UBorder>(this);
    ToastBorder->SetBrushColor(BgColor);
    ToastBorder->SetPadding(FMargin(10.f));

    // Create message text
    UTextBlock* MsgText = NewObject<UTextBlock>(this);
    MsgText->SetText(FText::FromString(Notification.Message));
    MsgText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 12);
    MsgText->SetFont(Font);
    MsgText->SetAutoWrapText(true);

    ToastBorder->AddChild(MsgText);

    // Add spacer below for gap between toasts
    ToastStack->AddChild(ToastBorder);

    USpacer* Gap = NewObject<USpacer>(this);
    Gap->SetSize(FVector2D(0.f, 4.f));
    ToastStack->AddChild(Gap);
}

void UHJNotificationWidget::OnNotificationExpired_Implementation(int32 Index)
{
    if (!ToastStack) return;

    // Each toast is a Border + Spacer pair, so child index = Index * 2
    int32 ChildIndex = Index * 2;
    if (ChildIndex + 1 < ToastStack->GetChildrenCount())
    {
        // Remove spacer first (higher index), then the border
        ToastStack->RemoveChildAt(ChildIndex + 1);
        ToastStack->RemoveChildAt(ChildIndex);
    }
    else if (ChildIndex < ToastStack->GetChildrenCount())
    {
        ToastStack->RemoveChildAt(ChildIndex);
    }
}
