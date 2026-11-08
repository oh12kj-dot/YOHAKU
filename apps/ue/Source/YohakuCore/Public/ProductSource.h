#pragma once
#include "CoreMinimal.h"
class FJsonObject;

namespace Yohaku
{
constexpr int32 ProductBodyLimit = 2 * 1024 * 1024;
struct YOHAKUCORE_API FProductVariant
{
    FString Id, Name, ImageUrl;
};
struct YOHAKUCORE_API FProductSource
{
    FString ProductId, Name, Brand, SourceUrl, RetrievedAt, ContentHash, AlternateConfiguration;
    double WidthMm = 0, DepthMm = 0, HeightMm = 0;
    TArray<FProductVariant> Variants;
};
YOHAKUCORE_API bool ValidateProductUrl(const FString &Input, FString &Canonical, FString &Error);
YOHAKUCORE_API bool ParseProduct(const TArray<uint8> &Bytes, const FString &Url, FProductSource &Out, FString &Error);
YOHAKUCORE_API FString EncodeProduct(const FProductSource &Product);
YOHAKUCORE_API FString Sha256(const TArray<uint8>& Bytes);
YOHAKUCORE_API TSharedRef<FJsonObject> ProductToJson(const FProductSource &Product);
YOHAKUCORE_API bool DecodeProduct(const TSharedPtr<FJsonObject> &Json, FProductSource &Out);
} // namespace Yohaku
