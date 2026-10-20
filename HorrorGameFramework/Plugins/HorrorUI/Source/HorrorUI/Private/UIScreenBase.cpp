#include "UIScreenBase.h"
#include "UIManagerSubsystem.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Layout/Children.h"
#include "Widgets/SWidget.h"

namespace
{
    // Walk only the active subtree: hidden or disabled containers also exclude their children.
    void FindNavigationTargets(const TSharedRef<SWidget>& Parent, const TSharedPtr<SWidget>& Preferred,
        TSharedPtr<SWidget>& First, TSharedPtr<SWidget>& Remembered)
    {
        FChildren* Children = Parent->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            const TSharedRef<SWidget> Child = Children->GetChildAt(Index);
            if (!Child->GetVisibility().IsVisible() || !Child->IsEnabled()) continue;
            if (Child->SupportsKeyboardFocus())
            {
                if (!First) First = Child;
                if (Child == Preferred) Remembered = Child;
            }
            FindNavigationTargets(Child, Preferred, First, Remembered);
        }
    }
}

void UUIScreenBase::RememberNavigationFocus()
{
    const APlayerController* PC = GetOwningPlayer();
    const ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
    if (!Player || !FSlateApplication::IsInitialized()) return;
    const TOptional<int32> UserIndex = FSlateApplication::Get().GetUserIndexForController(Player->GetControllerId());
    if (!UserIndex.IsSet()) return;
    TSharedPtr<SWidget> First, Focused;
    FindNavigationTargets(TakeWidget(), FSlateApplication::Get().GetUserFocusedWidget(UserIndex.GetValue()), First, Focused);
    if (Focused) LastNavigationFocus = Focused;
}

TSharedRef<SWidget> UUIScreenBase::GetNavigationFocusTarget()
{
    const TSharedRef<SWidget> Root = TakeWidget();
    // Live presentations handle their own gameplay keys at the screen root.
    if (bUseGameplayInput) return Root;
    TSharedPtr<SWidget> First, Remembered;
    FindNavigationTargets(Root, LastNavigationFocus.Pin(), First, Remembered);
    if (Remembered) return Remembered.ToSharedRef();
    return First ? First.ToSharedRef() : Root;
}

void UUIScreenBase::CloseScreen()
{
    if (Manager && Manager->GetTopScreen() == this) Manager->PopScreen();
}

FReply UUIScreenBase::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::Escape || KeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        CloseScreen();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, KeyEvent);
}
