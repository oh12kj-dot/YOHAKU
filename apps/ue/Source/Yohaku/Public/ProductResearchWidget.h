#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
class FProductResearch;
using FProductResearchPtr = TSharedPtr<FProductResearch, ESPMode::ThreadSafe>;
class SProductResearch : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SProductResearch) {}
    SLATE_ARGUMENT(FProductResearchPtr, Research)
    SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_EVENT(FSimpleDelegate, OnPlaceStudy)
    SLATE_END_ARGS()
    void Construct(const FArguments &Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnMouseButtonDown(const FGeometry &, const FPointerEvent &) override;
    virtual FReply OnMouseButtonUp(const FGeometry &, const FPointerEvent &) override { return FReply::Handled(); }
    virtual FReply OnMouseWheel(const FGeometry &, const FPointerEvent &) override { return FReply::Handled(); }
    virtual FReply OnKeyDown(const FGeometry &, const FKeyEvent &) override;

  private:
    FProductResearchPtr Research;
    FSimpleDelegate CloseAction;
};
