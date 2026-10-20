#include "Documents/DocumentScreens.h"
#include "Documents/DocumentGameplayComponent.h"
#include "DocumentCollectionComponent.h"
#include "DocumentDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"

DEFINE_LOG_CATEGORY_STATIC(LogDocumentUI, Log, All);

namespace DocumentUI
{
    UVerticalBox* Panel(UWidgetTree* Tree)
    {
        UBorder* Border = Tree->ConstructWidget<UBorder>();
        Border->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.96f));
        Border->SetPadding(FMargin(32.f));
        UVerticalBox* Layout = Tree->ConstructWidget<UVerticalBox>();
        Border->SetContent(Layout);
        Tree->RootWidget = Border;
        return Layout;
    }
    UTextBlock* Text(UWidgetTree* Tree, const FText& Value)
    {
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
        Label->SetText(Value);
        Label->SetAutoWrapText(true);
        return Label;
    }
    UButton* Button(UWidgetTree* Tree, const FText& Label)
    {
        UButton* Result = Tree->ConstructWidget<UButton>();
        Result->SetContent(Text(Tree, Label));
        return Result;
    }
}

TSharedRef<SWidget> UDocumentCollectionScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UVerticalBox* Layout = DocumentUI::Panel(WidgetTree);
    Layout->AddChildToVerticalBox(DocumentUI::Text(WidgetTree, NSLOCTEXT("Documents", "Collection", "Documentos")));
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Entries = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Entries);
    Layout->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UButton* Close = DocumentUI::Button(WidgetTree, NSLOCTEXT("Documents", "Back", "Voltar"));
    Layout->AddChildToVerticalBox(Close);
    Close->OnClicked.AddDynamic(this, &UDocumentCollectionScreen::Back);
    return Super::RebuildWidget();
}
void UDocumentCollectionScreen::SetSource(UDocumentGameplayComponent* InSource)
{
    Unbind();
    Source = InSource;
    if (InSource && InSource->GetCollection())
        InSource->GetCollection()->OnCollectionChanged.AddUniqueDynamic(this, &UDocumentCollectionScreen::Refresh);
    Refresh();
}
void UDocumentCollectionScreen::Refresh()
{
    if (!Entries) return;
    Entries->ClearChildren();
    UDocumentCollectionComponent* Collection = Source.IsValid() ? Source->GetCollection() : nullptr;
    const TArray<FCollectedDocument> Documents = Collection ? Collection->GetDocuments() : TArray<FCollectedDocument>();
    if (Documents.IsEmpty())
        Entries->AddChildToVerticalBox(DocumentUI::Text(WidgetTree, NSLOCTEXT("Documents", "Empty", "Nenhum documento coletado.")));
    for (const FCollectedDocument& Entry : Documents)
    {
        UDocumentDefinition* Document = Entry.Definition.Get();
        if (!Document) continue;
        UDocumentEntryButton* Button = WidgetTree->ConstructWidget<UDocumentEntryButton>();
        Button->Document = Document;
        const FText State = Entry.bRead ? NSLOCTEXT("Documents", "Read", "Lido") : NSLOCTEXT("Documents", "Unread", "Novo");
        Button->SetContent(DocumentUI::Text(WidgetTree, FText::Format(NSLOCTEXT("Documents", "Entry", "{0} — {1}"), State, Document->Title)));
        Button->OnClicked.AddDynamic(Button, &UDocumentEntryButton::Select);
        Button->OnDocumentSelected.AddDynamic(this, &UDocumentCollectionScreen::Read);
        Entries->AddChildToVerticalBox(Button);
    }
}
void UDocumentCollectionScreen::Read(UDocumentDefinition* Document) { if (Source.IsValid()) Source->OpenReader(Document); }
void UDocumentCollectionScreen::Back() { CloseScreen(); }
void UDocumentCollectionScreen::Unbind()
{
    if (Source.IsValid() && Source->GetCollection())
        Source->GetCollection()->OnCollectionChanged.RemoveDynamic(this, &UDocumentCollectionScreen::Refresh);
    Source.Reset();
}
void UDocumentCollectionScreen::NativeDestruct() { Unbind(); Super::NativeDestruct(); }

TSharedRef<SWidget> UDocumentReaderScreen::RebuildWidget()
{
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UVerticalBox* Layout = DocumentUI::Panel(WidgetTree);
    Title = DocumentUI::Text(WidgetTree, FText::GetEmpty());
    Layout->AddChildToVerticalBox(Title);
    PageScroll = WidgetTree->ConstructWidget<UScrollBox>();
    UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();
    PageScroll->AddChild(Page);
    Layout->AddChildToVerticalBox(PageScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ImageContainer = WidgetTree->ConstructWidget<USizeBox>();
    ImageContainer->SetHeightOverride(224.f);
    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    PageImage = WidgetTree->ConstructWidget<UImage>();
    Scale->SetContent(PageImage);
    ImageContainer->SetContent(Scale);
    Page->AddChildToVerticalBox(ImageContainer);
    ImageStatus = DocumentUI::Text(WidgetTree, FText::GetEmpty());
    Page->AddChildToVerticalBox(ImageStatus);
    Body = DocumentUI::Text(WidgetTree, FText::GetEmpty());
    Page->AddChildToVerticalBox(Body);
    PageNumber = DocumentUI::Text(WidgetTree, FText::GetEmpty());
    Layout->AddChildToVerticalBox(PageNumber);
    UHorizontalBox* PageNavigation = WidgetTree->ConstructWidget<UHorizontalBox>();
    Layout->AddChildToVerticalBox(PageNavigation);
    PreviousButton = DocumentUI::Button(WidgetTree, NSLOCTEXT("Documents", "Previous", "Página anterior"));
    NextButton = DocumentUI::Button(WidgetTree, NSLOCTEXT("Documents", "Next", "Próxima página"));
    PageNavigation->AddChildToHorizontalBox(PreviousButton);
    PageNavigation->AddChildToHorizontalBox(NextButton);
    PreviousButton->OnClicked.AddDynamic(this, &UDocumentReaderScreen::Previous);
    NextButton->OnClicked.AddDynamic(this, &UDocumentReaderScreen::Next);
    UButton* Close = DocumentUI::Button(WidgetTree, NSLOCTEXT("Documents", "Back", "Voltar"));
    Layout->AddChildToVerticalBox(Close);
    Close->OnClicked.AddDynamic(this, &UDocumentReaderScreen::Back);
    return Super::RebuildWidget();
}

void UDocumentReaderScreen::SetDocument(UDocumentDefinition* InDocument)
{
    Document = InDocument;
    PageIndex = 0;
    RefreshPage();
}
void UDocumentReaderScreen::CancelImageLoading()
{
    ++ImageGeneration;
    if (ImageLoadHandle) ImageLoadHandle->CancelHandle();
    ImageLoadHandle.Reset();
    LoadedImage = nullptr;
    if (PageImage) PageImage->SetBrushFromTexture(nullptr);
}
void UDocumentReaderScreen::RefreshPage()
{
    CancelImageLoading();
    if (!Document || !Document->Pages.IsValidIndex(PageIndex) || !Body) return;
    Title->SetText(Document->Title);
    const FDocumentPage& Page = Document->Pages[PageIndex];
    Body->SetText(Page.Text);
    PageScroll->ScrollToStart();
    PageNumber->SetText(FText::Format(NSLOCTEXT("Documents", "PageNumber", "Página {0} de {1}"), PageIndex + 1, Document->Pages.Num()));
    PreviousButton->SetIsEnabled(PageIndex > 0);
    NextButton->SetIsEnabled(PageIndex + 1 < Document->Pages.Num());
    ImageContainer->SetVisibility(Page.Image.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    ImageStatus->SetText(FText::GetEmpty());
    if (Page.Image.IsNull()) return;
    const uint64 Generation = ImageGeneration;
    if (Page.Image.Get()) FinishImageLoading(Generation);
    else
    {
        ImageStatus->SetText(NSLOCTEXT("Documents", "LoadingImage", "Carregando imagem…"));
        ImageLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Page.Image.ToSoftObjectPath(),
            FStreamableDelegate::CreateWeakLambda(this, [this, Generation]() { FinishImageLoading(Generation); }));
        if (!ImageLoadHandle) FinishImageLoading(Generation);
    }
}
void UDocumentReaderScreen::FinishImageLoading(uint64 Generation)
{
    if (Generation != ImageGeneration || !Document || !Document->Pages.IsValidIndex(PageIndex)) return;
    LoadedImage = Document->Pages[PageIndex].Image.Get();
    if (LoadedImage)
    {
        PageImage->SetBrushFromTexture(LoadedImage, true);
        ImageStatus->SetText(FText::GetEmpty());
    }
    else
    {
        ImageContainer->SetVisibility(ESlateVisibility::Collapsed);
        ImageStatus->SetText(NSLOCTEXT("Documents", "FailedImage", "A imagem desta página não pôde ser carregada."));
        UE_LOG(LogDocumentUI, Warning, TEXT("Cannot load page image %s for %s."),
            *Document->Pages[PageIndex].Image.ToString(), *GetNameSafe(Document));
    }
}
void UDocumentReaderScreen::Previous() { if (Document && PageIndex > 0) { --PageIndex; RefreshPage(); } }
void UDocumentReaderScreen::Next() { if (Document && PageIndex + 1 < Document->Pages.Num()) { ++PageIndex; RefreshPage(); } }
void UDocumentReaderScreen::Back() { CloseScreen(); }
FReply UDocumentReaderScreen::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::PageUp || Event.GetKey() == EKeys::Gamepad_LeftShoulder) { Previous(); return FReply::Handled(); }
    if (Event.GetKey() == EKeys::PageDown || Event.GetKey() == EKeys::Gamepad_RightShoulder) { Next(); return FReply::Handled(); }
    return Super::NativeOnKeyDown(Geometry, Event);
}
void UDocumentReaderScreen::NativeDestruct()
{
    CancelImageLoading();
    Document = nullptr;
    Super::NativeDestruct();
}
