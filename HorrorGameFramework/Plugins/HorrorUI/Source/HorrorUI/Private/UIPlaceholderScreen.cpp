#include "UIPlaceholderScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

UUIPlaceholderScreen::UUIPlaceholderScreen(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(true);
}

TSharedRef<SWidget> UUIPlaceholderScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Background->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.92f));
    Background->SetPadding(FMargin(64.f));
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Background->SetContent(Layout);
    WidgetTree->RootWidget = Background;

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(ScreenTitle);
    Layout->AddChildToVerticalBox(Title);
    UTextBlock* Description = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Description->SetText(NSLOCTEXT("HorrorUI", "Placeholder", "Tela de demonstracao. Conteudo em desenvolvimento."));
    Layout->AddChildToVerticalBox(Description);
    UButton* Back = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    UTextBlock* BackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    BackText->SetText(NSLOCTEXT("HorrorUI", "Back", "Voltar"));
    Back->SetContent(BackText);
    Back->OnClicked.AddDynamic(this, &UUIPlaceholderScreen::HandleClose);
    Layout->AddChildToVerticalBox(Back);
    return Super::RebuildWidget();
}

void UUIPlaceholderScreen::HandleClose()
{
    CloseScreen();
}
