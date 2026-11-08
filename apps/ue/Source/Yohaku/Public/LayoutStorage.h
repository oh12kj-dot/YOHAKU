#pragma once
#include "CoreMinimal.h"
namespace Yohaku
{
class FLayoutStorage
{
  public:
    static FString Path();
    static bool Save(const FString &Json, FString &Error);
    static bool Load(FString &Json, FString &Error);
};
} // namespace Yohaku
