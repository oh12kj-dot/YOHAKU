#include "ProductResearchWidget.h"
#include "ProductResearch.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "RoomEditorStyle.h"
#include "Framework/Application/SlateApplication.h"

void SProductResearch::Construct(const FArguments &Args)
{
    Research = Args._Research;
    CloseAction = Args._OnClose;
    const auto R = Args._Research;
    const FSimpleDelegate Close = Args._OnClose;
    const FSimpleDelegate PlaceStudy = Args._OnPlaceStudy;
    auto Text = [](const FString &Value, int32 Size) {
        return SNew(STextBlock)
            .Text(FText::FromString(Value))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", Size))
            .ColorAndOpacity(FLinearColor(.10f, .15f, .14f))
            .AutoWrapText(true);
    };
    ChildSlot[SNew(SBorder)
                  .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                  .BorderBackgroundColor(FLinearColor(.93f, .94f, .88f, 1))
                  .Padding(20)
                      [SNew(SVerticalBox) +
                       SVerticalBox::Slot().AutoHeight()[Text(TEXT("商品URLから掲載情報を調べる"), 20)] +
                       SVerticalBox::Slot().AutoHeight().Padding(
                           0, 8)[Text(TEXT("FLYMEeの商品ページに対応。寸法と色候補を出典付きで取得します。"), 12)] +
                       SVerticalBox::Slot().AutoHeight().Padding(
                           0, 5)[SNew(SEditableTextBox)
                                     .Text_Lambda([R]() { return FText::FromString(R->Input); })
                                     .HintText(FText::FromString(TEXT("https://flymee.jp/product/番号/")))
                                     .OnTextChanged_Lambda([R](const FText &T) { R->SetInput(T.ToString()); })] +
                       SVerticalBox::Slot().AutoHeight().Padding(
                           0, 8)[SNew(SHorizontalBox) +
                                 SHorizontalBox::Slot().AutoWidth().Padding(
                                     0, 0, 8, 0)[SNew(SButton)
                                                     .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                                     .ContentPadding(FMargin(12, 8))
                                                     .IsEnabled_Lambda([R]() { return !R->IsBusy(); })
                                                     .OnClicked_Lambda([R]() {
                                                         R->Start();
                                                         return FReply::Handled();
                                                     })[Text(TEXT("掲載情報を取得"), 13)]] +
                                 SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                                     [SNew(SButton)
                                          .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                          .ContentPadding(FMargin(12, 8))
                                          .IsEnabled_Lambda([R]() { return R->IsBusy(); })
                                          .OnClicked_Lambda([R]() {
                                              R->Cancel();
                                              return FReply::Handled();
                                          })[Text(TEXT("取得を中止"), 13)]] +
                                 SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                                     [SNew(SButton)
                                          .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                          .ContentPadding(FMargin(12, 8))
                                          .IsEnabled_Lambda([R]() { return R->CanSaveCandidate(); })
                                          .OnClicked_Lambda([R]() {
                                              R->SaveCandidate();
                                              return FReply::Handled();
                                          })[Text(TEXT("候補として保存"), 13)]] +
                                 SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                                                                        .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                                                        .ContentPadding(FMargin(12, 8))
                                                                        .OnClicked_Lambda([R, Close]() {
                                                                            if (R->IsBusy())
                                                                                R->Cancel();
                                                                            Close.ExecuteIfBound();
                                                                            return FReply::Handled();
                                                                        })[Text(TEXT("閉じる"), 13)]]] +
                       SVerticalBox::Slot().AutoHeight().Padding(
                           0, 5)[SNew(STextBlock)
                                     .AutoWrapText(true)
                                     .ColorAndOpacity(FLinearColor(.10f, .15f, .14f))
                                     .Text_Lambda([R]() { return FText::FromString(R->Status); })] +
                       SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                           [SNew(SHorizontalBox) +
                            SHorizontalBox::Slot().AutoWidth().Padding(
                                0, 0, 8, 0)[SNew(SComboButton)
                                                .IsEnabled_Lambda([R]() { return !R->IsBusy(); })
                                                .ButtonContent()[Text(TEXT("保存済み候補を開く"), 13)]
                                                .OnGetMenuContent_Lambda([R, Text]() -> TSharedRef<SWidget> {
                                                    auto List = SNew(SVerticalBox);
                                                    if (R->SavedCandidates().IsEmpty())
                                                        List->AddSlot()
                                                            .AutoHeight()[Text(TEXT("保存済み候補はありません"), 12)];
                                                    for (const auto &Item : R->SavedCandidates())
                                                    {
                                                        const FGuid Id = Item.Id;
                                                        const FString Label = Item.Source.Name + TEXT("\n") +
                                                                              Item.SavedAt + TEXT("  /  ") +
                                                                              Item.Source.ContentHash.Left(12);
                                                        List->AddSlot().AutoHeight().Padding(
                                                            3)[SNew(SButton)
                                                                   .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                                                   .OnClicked_Lambda([R, Id]() {
                                                                       R->OpenCandidate(Id);
                                                                       FSlateApplication::Get().DismissAllMenus();
                                                                       return FReply::Handled();
                                                                   })[Text(Label, 12)]];
                                                    }
                                                    return SNew(SBox).WidthOverride(480).HeightOverride(
                                                        240)[SNew(SScrollBox) + SScrollBox::Slot()[List]];
                                                })] +
                            SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                                                                   .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                                                   .IsEnabled_Lambda([R]() { return !R->IsBusy(); })
                                                                   .OnClicked_Lambda([R]() {
                                                                       R->ReloadCandidates();
                                                                       return FReply::Handled();
                                                                   })[Text(TEXT("候補を再読込"), 12)]]] +
                       SVerticalBox::Slot()
                           .AutoHeight()[SNew(STextBlock)
                                             .AutoWrapText(true)
                                             .ColorAndOpacity(FLinearColor(.10f, .15f, .14f))
                                             .Text_Lambda([R]() { return FText::FromString(R->CandidateStatus); })] +
                       SVerticalBox::Slot().AutoHeight().Padding(
                           0, 8)[SNew(SHorizontalBox) +
                                 SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                                     [SNew(SComboButton)
                                          .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                          .IsEnabled_Lambda([R]() { return !R->IsBusy() && R->Result.IsSet(); })
                                          .ButtonContent()[SNew(STextBlock)
                                                               .ColorAndOpacity(FLinearColor(.10f, .15f, .14f))
                                                               .Text_Lambda([R]() {
                                                                   return FText::FromString(
                                                                       R->GenerationVariantId.IsEmpty()
                                                                           ? TEXT("生成する写真の仕様を選ぶ")
                                                                           : TEXT("選択した仕様ID：") +
                                                                                 R->GenerationVariantId);
                                                               })]
                                          .OnGetMenuContent_Lambda([R, Text]() -> TSharedRef<SWidget> {
                                              auto List = SNew(SVerticalBox);
                                              if (R->Result.IsSet())
                                                  for (const auto &V : R->Result->Variants)
                                                  {
                                                      const FString Id = V.Id;
                                                      List->AddSlot().AutoHeight().Padding(
                                                          2)[SNew(SButton)
                                                                 .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                                                 .IsEnabled(!V.ImageUrl.IsEmpty())
                                                                 .OnClicked_Lambda([R, Id]() {
                                                                     R->GenerationVariantId = Id;
                                                                     FSlateApplication::Get().DismissAllMenus();
                                                                     return FReply::Handled();
                                                                 })[Text(V.Name + (V.ImageUrl.IsEmpty()
                                                                                       ? TEXT(" / 写真未特定")
                                                                                       : TEXT(" / 対応写真あり")),
                                                                         12)]];
                                                  }
                                              return SNew(SBox).WidthOverride(300).MaxDesiredHeight(
                                                  220)[SNew(SScrollBox) + SScrollBox::Slot()[List]];
                                          })] +
                                 SHorizontalBox::Slot().AutoWidth()[SNew(SButton)
                                                                        .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                                                        .ContentPadding(FMargin(8, 6))
                                                                        .IsEnabled_Lambda([R]() {
                                                                            return R->CanSaveCandidate() &&
                                                                                   !R->GenerationVariantId.IsEmpty() &&
                                                                                   R->LocalGeneration.Available();
                                                                        })
                                                                        .OnClicked_Lambda([R]() {
                                                                            R->Generate(R->GenerationVariantId);
                                                                            return FReply::Handled();
                                                                        })[Text(TEXT("写真から3D試作"), 12)]]] +
                       SVerticalBox::Slot().AutoHeight()
                           [SNew(STextBlock)
                                .AutoWrapText(true)
                                .ColorAndOpacity(FLinearColor(.10f, .15f, .14f))
                                .Text_Lambda([R]() {
                                    return FText::FromString(
                                        R->LocalGeneration.Status.IsEmpty()
                                            ? (R->LocalGeneration.Available()
                                                   ? TEXT("ローカル生成の準備済み。形状のみの試作です。材質・公式寸法の"
                                                          "照合は別工程です。")
                                                   : TEXT("ローカル生成の準備が必要です。有料APIは使いません。"))
                                            : R->LocalGeneration.Status);
                                })] +
                       SVerticalBox::Slot().AutoHeight().Padding(
                           0,
                           5)[SNew(SButton)
                                  .ButtonStyle(&YohakuUI::EditorButtonStyle())
                                  .IsEnabled_Lambda([R]() { return !R->IsBusy() && R->LocalGeneration.Result.IsSet(); })
                                  .OnClicked_Lambda([PlaceStudy]() {
                                      PlaceStudy.ExecuteIfBound();
                                      return FReply::Handled();
                                  })[Text(TEXT("生成した無地の試作を部屋で使う（未採用）"), 12)]] +
                       SVerticalBox::Slot().FillHeight(1).Padding(0, 10)
                           [SNew(SScrollBox) +
                            SScrollBox::Slot()[SNew(STextBlock)
                                                   .AutoWrapText(true)
                                                   .ColorAndOpacity(FLinearColor(.10f, .15f, .14f))
                                                   .Text_Lambda([R]() { return FText::FromString(R->Summary()); })]]]];
}
FReply SProductResearch::OnMouseButtonDown(const FGeometry &, const FPointerEvent &)
{
    return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
}
FReply SProductResearch::OnKeyDown(const FGeometry &, const FKeyEvent &Event)
{
    if (Event.GetKey() == EKeys::Escape)
    {
        if (Research->IsBusy())
            Research->Cancel();
        CloseAction.ExecuteIfBound();
    }
    return FReply::Handled();
}
