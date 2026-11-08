#include "RoomEditorWidget.h"
#include "RoomController.h"
#include "ProductResearch.h"
#include "ProductResearchWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "RoomEditorStyle.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Rendering/DrawElementTypes.h"
namespace
{
const FLinearColor Ink(.1, .15, .14), Muted(.35, .4, .37), Paper(.94, .95, .89, .97);
TSharedRef<STextBlock> Text(const FString &S, int32 Size = 12)
{
    return SNew(STextBlock)
        .Text(FText::FromString(S))
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", Size))
        .ColorAndOpacity(Ink)
        .AutoWrapText(true);
}
TSharedRef<SButton> Button(const FString &Label, TFunction<void()> Action)
{
    return SNew(SButton)
        .ButtonStyle(&YohakuUI::EditorButtonStyle())
        .IsFocusable(false)
        .ContentPadding(FMargin(8, 6))
        .OnClicked_Lambda([Action]() {
            Action();
            return FReply::Handled();
        })[Text(Label)];
}
TSharedRef<SWidget> Field(const FString &Label, TFunction<double()> Get, TFunction<void(double)> Set, double Min,
                          double Max, double Step = 100)
{
    return SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Label, 11)] +
           SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(
               115)[SNew(SSpinBox<double>)
                        .MinValue(Min)
                        .MaxValue(Max)
                        .Delta(Step)
                        .MaxFractionalDigits(1)
                        .Value_Lambda([Get]() { return Get(); })
                        .OnValueCommitted_Lambda([Set](double V, ETextCommit::Type) { Set(V); })]];
}
TSharedRef<SWidget> Section(const FString &S)
{
    return SNew(SBox).Padding(FMargin(0, 14, 0, 7))[Text(S, 14)];
}
TSharedRef<SWidget> Menu(TFunction<FString()> Caption, TFunction<TSharedRef<SWidget>()> Content)
{
    return SNew(SComboButton)
        .ButtonStyle(&YohakuUI::EditorButtonStyle())
        .ButtonContent()[SNew(STextBlock)
                             .ColorAndOpacity(Ink)
                             .Text_Lambda([Caption]() { return FText::FromString(Caption()); })
                             .AutoWrapText(true)]
        .OnGetMenuContent_Lambda([Content]() { return Content(); });
}
TSharedRef<SWidget> MenuBox(TSharedRef<SVerticalBox> List)
{
    return SNew(SBox).WidthOverride(310).MaxDesiredHeight(300)[SNew(SScrollBox) + SScrollBox::Slot()[List]];
}
} // namespace
void SRoomEditor::Construct(const FArguments &Args)
{
    Controller = Args._Controller;
    const auto C = Controller;
    auto Left = SNew(SVerticalBox), Right = SNew(SVerticalBox);
    Left->AddSlot().AutoHeight()[Text(TEXT("ROOM / 部屋づくり"), 14)];
    Left->AddSlot().AutoHeight().Padding(0, 7)[Menu([C]() { return C.IsValid() ? C->CurrentRoom().Name : TEXT(""); },
                                                    [C]() {
                                                        auto List = SNew(SVerticalBox);
                                                        if (C.IsValid())
                                                            for (const auto &R : C->Document.Get().Rooms)
                                                            {
                                                                auto Id = R.Id;
                                                                List->AddSlot().AutoHeight().Padding(
                                                                    2)[Button(R.Name, [C, Id]() {
                                                                    if (C.IsValid())
                                                                        C->ChooseRoom(Id);
                                                                    FSlateApplication::Get().DismissAllMenus();
                                                                })];
                                                            }
                                                        return MenuBox(List);
                                                    })];
    Left->AddSlot().AutoHeight()[Button(TEXT("長方形の部屋を追加"), [C]() {
        if (C.IsValid())
            C->SetTool(ERoomTool::AddRoom);
    })];
    Left->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("頂点を描いて部屋を追加"), [C]() {
        if (C.IsValid())
            C->StartPolygon(false);
    })];
    Left->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("選択した部屋の形を描き直す"), [C]() {
        if (C.IsValid())
            C->StartPolygon(true);
    })];
    Left->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("間取りを確定"), [C]() {
        if (C.IsValid())
            C->FinishPolygon();
    })];
    Left->AddSlot()
        .AutoHeight()[Text(TEXT("直交する頂点を順にクリック。最後の辺を含め交差のない形で確定します。"), 10)];
    for (int32 I = 0; I < 3; ++I)
    {
        Left->AddSlot().AutoHeight().Padding(0, 3)[Field(
            I == 0   ? TEXT("幅 mm")
            : I == 1 ? TEXT("奥行 mm")
                     : TEXT("天井高 mm"),
            [C, I]() {
                if (!C.IsValid())
                    return 0.;
                const auto &R = C->CurrentRoom();
                return I == 0 ? R.Width() : I == 1 ? R.Depth() : R.Height;
            },
            [C, I](double V) {
                if (C.IsValid())
                {
                    const auto R = C->CurrentRoom();
                    if (I < 2 && R.Polygon.Num() != 4)
                    {
                        C->Notice = TEXT("多角形の幅・奥行は頂点から変更してください");
                        return;
                    }
                    C->SetRoom(I == 0 ? V : R.Width(), I == 1 ? V : R.Depth(), I == 2 ? V : R.Height);
                }
            },
            I == 2 ? 1000 : 500, I == 2 ? 6000 : 30000)];
    }
    Left->AddSlot().AutoHeight()[Text(TEXT("部屋寸法は入力値・実測照合前"), 10)];
    Left->AddSlot().AutoHeight().Padding(0, 4)[Button(TEXT("選択した部屋を削除"), [C]() {
        if (C.IsValid())
            C->RemoveRoom();
    })];
    Left->AddSlot().AutoHeight()[Section(TEXT("SURFACE / 床・壁"))];
    Left->AddSlot().AutoHeight()[Menu(
        [C]() {
            if (!C.IsValid() || !C->SelectedWall.IsValid())
                return FString(TEXT("床を選択"));
            const auto &R = C->CurrentRoom();
            for (int32 I = 0; I < R.Walls.Num(); ++I)
                if (R.Walls[I].Id == C->SelectedWall)
                    return FString::Printf(TEXT("壁 %d を選択"), I + 1);
            return FString(TEXT("壁を選択"));
        },
        [C]() {
            auto List = SNew(SVerticalBox);
            List->AddSlot().AutoHeight()[Button(TEXT("床"), [C]() {
                if (C.IsValid())
                {
                    C->SelectedWall.Invalidate();
                    C->Surface = Yohaku::ESurface::Floor;
                }
                FSlateApplication::Get().DismissAllMenus();
            })];
            if (C.IsValid())
            {
                const auto &R = C->CurrentRoom();
                for (int32 I = 0; I < R.Walls.Num(); ++I)
                {
                    const auto Id = R.Walls[I].Id;
                    List->AddSlot().AutoHeight().Padding(
                        0, 2)[Button(FString::Printf(TEXT("壁 %d / %.0f mm"), I + 1,
                                                     (R.Polygon[(I + 1) % R.Polygon.Num()] - R.Polygon[I]).Size()),
                                     [C, Id]() {
                                         if (C.IsValid())
                                         {
                                             C->SelectedWall = Id;
                                             C->SelectedOpening.Invalidate();
                                         }
                                         FSlateApplication::Get().DismissAllMenus();
                                     })];
                }
            }
            return MenuBox(List);
        })];
    const TCHAR *Floors[] = {TEXT("オーク"), TEXT("ウォルナット"), TEXT("白木"), TEXT("石")};
    const TCHAR *Walls[] = {TEXT("アイボリー"), TEXT("セージ"), TEXT("クレイ"), TEXT("白"), TEXT("スレート")};
    for (int32 I = 0; I < 5; ++I)
        Left->AddSlot().AutoHeight().Padding(0, 2)[SNew(SBox).Visibility_Lambda([C, I]() {
            return C.IsValid() && !C->SelectedWall.IsValid() && I == 4 ? EVisibility::Collapsed : EVisibility::Visible;
        })[SNew(SButton)
               .ButtonStyle(&YohakuUI::EditorButtonStyle())
               .IsFocusable(false)
               .ContentPadding(FMargin(8, 5))
               .OnClicked_Lambda([C, I]() {
                   if (C.IsValid())
                       C->SetFinish(I);
                   return FReply::Handled();
               })[SNew(STextBlock).ColorAndOpacity(Ink).Text_Lambda([C, I, Floors, Walls]() {
                   return FText::FromString(C.IsValid() && C->SelectedWall.IsValid() ? Walls[I] : Floors[I % 4]);
               })]]];
    Left->AddSlot().AutoHeight()[Section(TEXT("OPENINGS / 窓・ドア"))];
    Left->AddSlot().AutoHeight()[Menu(
        [C]() {
            return C.IsValid() && C->SelectedOpening.IsValid() ? FString(TEXT("選択した開口を編集"))
                                                               : FString(TEXT("新しい開口 / 保存済みから選択"));
        },
        [C]() {
            auto List = SNew(SVerticalBox);
            List->AddSlot().AutoHeight()[Button(TEXT("新しい開口"), [C]() {
                if (C.IsValid())
                    C->SelectedOpening.Invalidate();
                FSlateApplication::Get().DismissAllMenus();
            })];
            if (C.IsValid())
            {
                int32 Index = 0;
                for (const auto &O : C->Document.Get().Openings)
                {
                    const auto Id = O.Id;
                    List->AddSlot().AutoHeight().Padding(
                        0, 2)[Button(FString::Printf(TEXT("%s %d / %.0f mm"),
                                                     O.Kind == Yohaku::EOpeningKind::Door ? TEXT("ドア") : TEXT("窓"),
                                                     ++Index, O.Width),
                                     [C, Id]() {
                                         if (C.IsValid())
                                             C->SelectOpening(Id);
                                         FSlateApplication::Get().DismissAllMenus();
                                     })];
                }
            }
            return MenuBox(List);
        })];
    const TCHAR *OpeningLabels[] = {TEXT("壁の始点から mm"), TEXT("開口幅 mm"), TEXT("開口高さ mm"),
                                    TEXT("窓の下端高 mm")};
    for (int32 I = 0; I < 4; ++I)
        Left->AddSlot().AutoHeight().Padding(0, 3)[Field(
            OpeningLabels[I],
            [C, I]() {
                if (!C.IsValid())
                    return 0.;
                return I == 0   ? C->OpeningOffset
                       : I == 1 ? C->OpeningWidth
                       : I == 2 ? C->OpeningHeight
                                : C->OpeningSill;
            },
            [C, I](double V) {
                if (!C.IsValid())
                    return;
                if (I == 0)
                    C->OpeningOffset = V;
                else if (I == 1)
                    C->OpeningWidth = V;
                else if (I == 2)
                    C->OpeningHeight = V;
                else
                    C->OpeningSill = V;
            },
            I == 0 || I == 3 ? 0 : 300, I == 0 ? 30000 : 5000)];
    Left->AddSlot().AutoHeight()[SNew(SCheckBox)
                                     .IsChecked_Lambda([C]() {
                                         return C.IsValid() && C->bHingeAtEnd ? ECheckBoxState::Checked
                                                                              : ECheckBoxState::Unchecked;
                                     })
                                     .OnCheckStateChanged_Lambda([C](ECheckBoxState S) {
                                         if (C.IsValid())
                                             C->bHingeAtEnd = S == ECheckBoxState::Checked;
                                     })[Text(TEXT("ドアの蝶番を反対側にする"), 11)]];
    Left->AddSlot().AutoHeight()[SNew(SCheckBox)
                                     .IsChecked_Lambda([C]() {
                                         return C.IsValid() && C->bSwingInward ? ECheckBoxState::Checked
                                                                               : ECheckBoxState::Unchecked;
                                     })
                                     .OnCheckStateChanged_Lambda([C](ECheckBoxState S) {
                                         if (C.IsValid())
                                             C->bSwingInward = S == ECheckBoxState::Checked;
                                     })[Text(TEXT("部屋の内側へ開く"), 11)]];
    Left->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("ドアとして追加・更新"), [C]() {
        if (C.IsValid())
            C->AddOpening(Yohaku::EOpeningKind::Door);
    })];
    Left->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("窓として追加・更新"), [C]() {
        if (C.IsValid())
        {
            if (!C->SelectedOpening.IsValid() && C->OpeningHeight == 2000)
                C->OpeningHeight = 1200;
            C->AddOpening(Yohaku::EOpeningKind::Window);
        }
    })];
    Left->AddSlot().AutoHeight()[Button(TEXT("選択した開口を削除"), [C]() {
        if (C.IsValid())
            C->RemoveOpening();
    })];
    Left->AddSlot().AutoHeight()[Section(TEXT("FURNITURE / 家具"))];
    Left->AddSlot().AutoHeight()[Text(TEXT("PSF SOFA 2P / 写真参照の試作"), 11)];
    Left->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("ブラウンを配置"), [C]() {
        if (C.IsValid())
            C->StartPlacement(Yohaku::EVariant::Brown);
    })];
    Left->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("ブルーグレーを配置"), [C]() {
        if (C.IsValid())
            C->StartPlacement(Yohaku::EVariant::BlueGray);
    })];
    Left->AddSlot().AutoHeight()[Menu([]() { return FString(TEXT("保存済みの生成試作を配置")); },
                                      [C]() {
                                          auto List = SNew(SVerticalBox);
                                          if (C.IsValid())
                                              for (const auto &A : C->ModelLibrary)
                                              {
                                                  List->AddSlot().AutoHeight().Padding(
                                                      2)[Button(A.Name + TEXT(" / 無地・未検証"), [C, A]() {
                                                      if (C.IsValid())
                                                          C->PlaceStudyAsset(A);
                                                      FSlateApplication::Get().DismissAllMenus();
                                                  })];
                                              }
                                          if (!C.IsValid() || C->ModelLibrary.IsEmpty())
                                              List->AddSlot()
                                                  .AutoHeight()[Text(TEXT("商品URLから写真を選んで生成できます"), 11)];
                                          return MenuBox(List);
                                      })];
    Right->AddSlot().AutoHeight()[Text(TEXT("INSPECTOR / 選択した家具"), 14)];
    Right->AddSlot().AutoHeight().Padding(
        0, 8)[SNew(STextBlock).ColorAndOpacity(Ink).AutoWrapText(true).Text_Lambda([C]() {
        if (!C.IsValid() || !C->SelectedItem())
            return FText::FromString(TEXT("家具をクリックして選択"));
        const auto *A = C->Document.Get().Asset(C->SelectedItem()->AssetId);
        return FText::FromString(
            A ? FString::Printf(TEXT("%s\n%.0f × %.0f × %.0f mm\n%s"), *A->Name, A->Size.X, A->Size.Y, A->Size.Z,
                                A->bGeneratedStudy
                                    ? TEXT("掲載値に合わせた無地の試作\n背面・正面方向は推定 / 材質・寸法は未照合")
                                    : TEXT("写真参照の試作・実物校正前"))
              : TEXT("家具版なし"));
    })];
    for (int32 I = 0; I < 3; ++I)
        Right->AddSlot().AutoHeight().Padding(0, 3)[Field(
            I == 0   ? TEXT("X mm")
            : I == 1 ? TEXT("Z mm")
                     : TEXT("角度 °"),
            [C, I]() {
                const auto *P = C.IsValid() ? C->SelectedItem() : nullptr;
                return P ? (I == 0 ? P->X : I == 1 ? P->Z : FMath::RadiansToDegrees(P->Yaw)) : 0.;
            },
            [C, I](double V) {
                if (C.IsValid())
                    if (const auto *P = C->SelectedItem())
                    {
                        auto Copy = *P;
                        C->EditPose(I == 0 ? V : Copy.X, I == 1 ? V : Copy.Z,
                                    I == 2 ? V : FMath::RadiansToDegrees(Copy.Yaw));
                    }
            },
            -100000, 100000, I == 2 ? 45 : 100)];
    Right->AddSlot().AutoHeight().Padding(0, 5)[Button(TEXT("45° 回転 [R]"), [C]() {
        if (C.IsValid())
            C->Rotate();
    })];
    Right->AddSlot().AutoHeight()[Button(TEXT("複製"), [C]() {
        if (C.IsValid())
            C->Duplicate();
    })];
    Right->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("削除 [Delete]"), [C]() {
        if (C.IsValid())
            C->DeleteSelected();
    })];
    Right->AddSlot().AutoHeight()[Section(TEXT("REPLACE / 家具交換"))];
    Right->AddSlot().AutoHeight()[SNew(SCheckBox)
                                      .IsChecked_Lambda([C]() {
                                          return C.IsValid() && C->bKeepWallDistance ? ECheckBoxState::Checked
                                                                                     : ECheckBoxState::Unchecked;
                                      })
                                      .OnCheckStateChanged_Lambda([C](ECheckBoxState S) {
                                          if (C.IsValid())
                                              C->bKeepWallDistance = S == ECheckBoxState::Checked;
                                      })[Text(TEXT("左側で選択した壁からの距離を保つ"), 11)]];
    Right->AddSlot().AutoHeight()[Text(TEXT("オフでは中心位置を保持。向き・床接地・家具IDは維持します。"), 10)];
    Right->AddSlot().AutoHeight().Padding(
        0, 5)[Menu([]() { return FString(TEXT("交換先の家具・色を選ぶ")); },
                   [C]() {
                       auto List = SNew(SVerticalBox);
                       if (C.IsValid())
                           for (const auto &A : C->Document.Get().Assets)
                           {
                               const FString Id = A.Id;
                               List->AddSlot().AutoHeight().Padding(
                                   2)[Button(A.Name + (A.bGeneratedStudy              ? TEXT(" / 無地試作")
                                                       : A.Id.EndsWith(TEXT("brown")) ? TEXT(" / ブラウン")
                                                                                      : TEXT(" / ブルーグレー")),
                                             [C, Id]() {
                                                 if (C.IsValid())
                                                     C->ReplaceAsset(Id);
                                                 FSlateApplication::Get().DismissAllMenus();
                                             })];
                           }
                       return MenuBox(List);
                   })];
    Right->AddSlot().AutoHeight()[Section(TEXT("PASSAGE / 通路の確認"))];
    Right->AddSlot().AutoHeight()[Field(
        TEXT("必要幅 mm"), [C]() { return C.IsValid() ? C->PassageWidth : 800.; },
        [C](double V) {
            if (C.IsValid())
            {
                C->PassageWidth = V;
                C->PassageResult = Yohaku::FPassageResult();
            }
        },
        300, 2000)];
    Right->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("床で始点を指定"), [C]() {
        if (C.IsValid())
            C->SetTool(ERoomTool::PassageStart);
    })];
    Right->AddSlot().AutoHeight().Padding(0, 3)[Button(TEXT("床で終点を指定"), [C]() {
        if (C.IsValid())
            C->SetTool(ERoomTool::PassageEnd);
    })];
    Right->AddSlot().AutoHeight()[Button(TEXT("通路を確認"), [C]() {
        if (C.IsValid())
            C->CheckPassage();
    })];
    Right->AddSlot().AutoHeight()[Text(TEXT("100mm格子の保守的な概算です。通行や安全性を保証しません。"), 10)];
    Right->AddSlot().AutoHeight()[Section(TEXT("VIEW / 視点"))];
    for (int32 I = 0; I < 3; ++I)
        Right->AddSlot().AutoHeight().Padding(0, 3)[Button(I == 0   ? TEXT("俯瞰")
                                                           : I == 1 ? TEXT("真上")
                                                                    : TEXT("室内"),
                                                           [C, I]() {
                                                               if (C.IsValid())
                                                                   C->View(I);
                                                           })];
    Right->AddSlot().AutoHeight().Padding(0, 6)[SNew(SCheckBox)
                                                    .IsChecked_Lambda([C]() {
                                                        return C.IsValid() && C->bSnap ? ECheckBoxState::Checked
                                                                                       : ECheckBoxState::Unchecked;
                                                    })
                                                    .OnCheckStateChanged_Lambda([C](ECheckBoxState S) {
                                                        if (C.IsValid())
                                                            C->bSnap = S == ECheckBoxState::Checked;
                                                    })[Text(TEXT("100mm スナップ"), 11)]];
    Right->AddSlot().AutoHeight()[Text(TEXT("左ドラッグ：家具移動\n右ドラッグ：視点回転\nShift＋右：視点移動\nホイール"
                                            "：拡大・縮小\nEsc：未確定の操作を取消"),
                                       10)];
    auto Header = SNew(SHorizontalBox);
    Header->AddSlot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("余白  /  YOHAKU ROOM STUDIO"), 22)];
    const TCHAR *HeaderNames[] = {TEXT("商品URL"),  TEXT("選択"), TEXT("元に戻す"),
                                  TEXT("やり直す"), TEXT("保存"), TEXT("読込")};
    for (int32 I = 0; I < 6; ++I)
        Header->AddSlot().AutoWidth().Padding(
            3)[SNew(SBox).WidthOverride(I == 0 ? 96 : 80)[Button(HeaderNames[I], [C, I]() {
            if (!C.IsValid())
                return;
            switch (I)
            {
            case 0:
                C->bShowResearch = !C->bShowResearch;
                break;
            case 1:
                C->Cancel();
                break;
            case 2:
                C->Undo();
                break;
            case 3:
                C->Redo();
                break;
            case 4:
                C->Save();
                break;
            case 5:
                C->Load();
                break;
            }
        })]];
    ChildSlot[SNew(SOverlay) +
              SOverlay::Slot().VAlign(VAlign_Top)[SNew(SBorder)
                                                      .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                                      .BorderBackgroundColor(Paper)
                                                      .Padding(FMargin(22, 12))[Header]] +
              SOverlay::Slot()
                  .HAlign(HAlign_Left)
                  .VAlign(VAlign_Fill)
                  .Padding(FMargin(18, 86, 0, 78))[SNew(SBox).WidthOverride(
                      275)[SNew(SBorder)
                               .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                               .BorderBackgroundColor(Paper)
                               .Padding(14)[SNew(SScrollBox) + SScrollBox::Slot()[Left]]]] +
              SOverlay::Slot()
                  .HAlign(HAlign_Right)
                  .VAlign(VAlign_Fill)
                  .Padding(FMargin(0, 86, 18, 78))[SNew(SBox).WidthOverride(
                      265)[SNew(SBorder)
                               .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                               .BorderBackgroundColor(Paper)
                               .Padding(14)[SNew(SScrollBox) + SScrollBox::Slot()[Right]]]] +
              SOverlay::Slot()
                  .HAlign(HAlign_Center)
                  .VAlign(VAlign_Bottom)
                  .Padding(0, 0, 0, 16)[SNew(SBox).WidthOverride(
                      680)[SNew(SBorder)
                               .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                               .BorderBackgroundColor(Paper)
                               .Padding(12)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(Ink).Text_Lambda([C]() {
                                   return FText::FromString(C.IsValid() ? C->Status() : TEXT(""));
                               })]]] +
              SOverlay::Slot()
                  .HAlign(HAlign_Center)
                  .VAlign(VAlign_Center)[SNew(SBox).WidthOverride(700).HeightOverride(680).Visibility_Lambda([C]() {
                      return C.IsValid() && C->bShowResearch ? EVisibility::Visible : EVisibility::Collapsed;
                  })[SNew(SProductResearch)
                         .Research(C->Research)
                         .OnClose_Lambda([C]() {
                             if (C.IsValid())
                                 C->bShowResearch = false;
                         })
                         .OnPlaceStudy_Lambda([C]() {
                             if (C.IsValid())
                                 C->PlaceGenerated();
                         })]]];
}

int32 SRoomEditor::OnPaint(const FPaintArgs &Args, const FGeometry &Geometry, const FSlateRect &Cull,
                           FSlateWindowElementList &Elements, int32 Layer, const FWidgetStyle &Style,
                           bool Enabled) const
{
    const auto C = Controller;
    if (C.IsValid() && GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint Pixels = GEngine->GameViewport->Viewport->GetSizeXY();
        auto Project = [&](FVector2D P, FVector2f &Out) {
            FVector2D Screen;
            if (!C->ProjectWorldLocationToScreen(FVector(P.X / 10, P.Y / 10, 1), Screen, true) || Pixels.X < 1 ||
                Pixels.Y < 1)
                return false;
            Out = FVector2f(Screen.X * Geometry.GetLocalSize().X / Pixels.X,
                            Screen.Y * Geometry.GetLocalSize().Y / Pixels.Y);
            return true;
        };
        auto Draw = [&](const TArray<FVector2D> &Points, FLinearColor Color, bool Close = false) {
            TArray<FVector2f> Screen;
            for (auto P : Points)
            {
                FVector2f Q;
                if (!Project(P, Q))
                    return;
                Screen.Add(Q);
            }
            if (Close && Screen.Num() > 1)
                Screen.Add(Screen[0]);
            if (Screen.Num() > 1)
                FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Screen,
                                             ESlateDrawEffect::None, Color, true, 2);
        };
        if (const auto *I = C->DisplayPose())
            if (const auto *A = C->Document.Get().Asset(I->AssetId))
                Draw(Yohaku::Footprint(*I, *A),
                     A->bGeneratedStudy ? FLinearColor(1.f, .7f, .2f) : FLinearColor(.4f, .85f, .55f), true);
        FVector2D A, B;
        if (C->PendingRectangle(A, B))
            Draw({A, {B.X, A.Y}, B, {A.X, B.Y}}, FLinearColor(.4f, .85f, .55f), true);
        if (!C->PolygonPoints.IsEmpty())
        {
            auto Points = C->PolygonPoints;
            if (C->FloorPoint(B))
            {
                A = Points.Last();
                if (FMath::Abs(B.X - A.X) > FMath::Abs(B.Y - A.Y))
                    B.Y = A.Y;
                else
                    B.X = A.X;
                B.X = FMath::GridSnap(B.X, 100.);
                B.Y = FMath::GridSnap(B.Y, 100.);
                Points.Add(B);
            }
            Draw(Points, FLinearColor(.4f, .85f, .55f));
            for (auto P : C->PolygonPoints)
            {
                FVector2f Q;
                if (Project(P, Q))
                    FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(),
                                                 TArray<FVector2f>{Q + FVector2f(-3, -3), Q + FVector2f(3, -3),
                                                                   Q + FVector2f(3, 3), Q + FVector2f(-3, 3),
                                                                   Q + FVector2f(-3, -3)},
                                                 ESlateDrawEffect::None, FLinearColor::White, true, 2);
            }
        }
        Draw(C->PassageResult.Path, FLinearColor(.2f, .8f, 1.f));
        for (const auto &O : C->Document.Get().Openings)
            if (O.Kind == Yohaku::EOpeningKind::Door)
            {
                const auto Walls = Yohaku::Walls(C->Document.Get());
                const auto *W = Walls.FindByPredicate([&](const auto &V) { return V.Id == O.WallId; });
                if (!W)
                    continue;
                auto T = (W->B - W->A).GetSafeNormal(), N = FVector2D(-T.Y, T.X) * (O.SwingInward ? 1. : -1.),
                     H = W->A + T * (O.Offset + (O.HingeAtEnd ? O.Width : 0));
                T *= O.HingeAtEnd ? -1. : 1.;
                TArray<FVector2D> Arc{H};
                for (int32 I = 0; I <= 12; ++I)
                {
                    double Angle = UE_DOUBLE_HALF_PI * I / 12;
                    Arc.Add(H + O.Width * (FMath::Cos(Angle) * T + FMath::Sin(Angle) * N));
                }
                Arc.Add(H);
                Draw(Arc, FLinearColor(.8f, .6f, .25f));
            }
    }
    return SCompoundWidget::OnPaint(Args, Geometry, Cull, Elements, Layer + 1, Style, Enabled);
}
