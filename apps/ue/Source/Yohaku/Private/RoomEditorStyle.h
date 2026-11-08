#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateColorBrush.h"
namespace YohakuUI
{
inline const FButtonStyle &EditorButtonStyle()
{
    static FButtonStyle Style = FButtonStyle()
                                    .SetNormal(FSlateColorBrush(FLinearColor(.74f, .80f, .67f)))
                                    .SetHovered(FSlateColorBrush(FLinearColor(.82f, .87f, .76f)))
                                    .SetPressed(FSlateColorBrush(FLinearColor(.61f, .72f, .53f)))
                                    .SetDisabled(FSlateColorBrush(FLinearColor(.84f, .85f, .80f)))
                                    .SetNormalPadding(FMargin(0))
                                    .SetPressedPadding(FMargin(0));
    return Style;
}
} // namespace YohakuUI
