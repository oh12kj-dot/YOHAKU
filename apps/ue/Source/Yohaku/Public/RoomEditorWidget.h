#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
class ARoomController;
class SRoomEditor : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SRoomEditor) {}
    SLATE_ARGUMENT(TWeakObjectPtr<ARoomController>, Controller) SLATE_END_ARGS() void Construct(const FArguments &Args);
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;

  private:
    TWeakObjectPtr<ARoomController> Controller;
};
