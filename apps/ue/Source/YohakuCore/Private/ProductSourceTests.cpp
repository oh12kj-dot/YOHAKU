#include "ProductSource.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYohakuProductContract, "Yohaku.Core.ProductSource",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FYohakuProductContract::RunTest(const FString &)
{
    using namespace Yohaku;
    const FString Url = TEXT("https://flymee.jp/product/106294/?pi_id=326211");
    FString Canonical, Error;
    TestTrue(TEXT("Supported URL"), ValidateProductUrl(Url, Canonical, Error));
    for (const FString Bad :
         {TEXT("http://flymee.jp/product/106294/"), TEXT("https://flymee.jp.evil.test/product/106294/"),
          TEXT("https://user@flymee.jp/product/106294/"), TEXT("https://flymee.jp:443/product/106294/"),
          TEXT("https://flymee.jp/product/106294/?pi_id=1&pi_id=2"), TEXT("https://flymee.jp/product/106294/#x"),
          TEXT("https://127.0.0.1/product/106294/"), TEXT("https://flymee.jp/product/106294/\n"),
          TEXT("https://flymee.jp/product/../106294/"), TEXT("https://flymee.jp/product/106294/?redirect=x")})
        TestFalse(TEXT("URL bypass rejected"), ValidateProductUrl(Bad, Canonical, Error));
    TArray<uint8> Bytes;
    TestTrue(TEXT("Shared captured fixture"),
             FFileHelper::LoadFileToArray(Bytes,
                                          *(FPaths::ProjectDir() / TEXT("../../tests/fixtures/flymee-product.html"))));
    FProductSource P;
    TestTrue(TEXT("Parse captured HTML"), ParseProduct(Bytes, Url, P, Error));
    TestEqual(TEXT("Product ID"), P.ProductId, FString(TEXT("106294")));
    TestEqual(TEXT("Width mm"), P.WidthMm, 1500.0);
    TestEqual(TEXT("Depth mm"), P.DepthMm, 870.0);
    TestEqual(TEXT("Overall not seat height"), P.HeightMm, 820.0);
    TestEqual(TEXT("Two variants"), P.Variants.Num(), 2);
    if (P.Variants.Num() == 2)
    {
        TestEqual(TEXT("Brown"), P.Variants[0].Name, FString(TEXT("ブラウン")));
        TestEqual(TEXT("Blue gray"), P.Variants[1].Name, FString(TEXT("ブルーグレー")));
        TestFalse(TEXT("Brown source photo"), P.Variants[0].ImageUrl.IsEmpty());
        TestFalse(TEXT("Blue source photo"), P.Variants[1].ImageUrl.IsEmpty());
    }
    TestEqual(TEXT("SHA256 length"), P.ContentHash.Len(), 64);
    const FString Json = EncodeProduct(P);
    TestTrue(TEXT("Unverified status"), Json.Contains(TEXT("single-source-unverified")));
    TestTrue(TEXT("No generated model claim"), Json.Contains(TEXT("not-generated")));
    auto ParseText = [&](const FString &Html, FProductSource &Out) {
        FTCHARToUTF8 Data(*Html);
        TArray<uint8> Body;
        Body.Append(reinterpret_cast<const uint8 *>(Data.Get()), Data.Length());
        return ParseProduct(Body, Url, Out, Error);
    };
    const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR *>(Bytes.GetData()), Bytes.Num());
    const FString Html(Converted.Length(), Converted.Get());
    FProductSource Kept = P;
    TestFalse(TEXT("Conflicting overall dimensions"),
              ParseText(Html + TEXT("<div class='spec_area_list _size'><span class='title'>高さ</span><span "
                                    "class='text'>70cm</span></div>"),
                        P));
    TestEqual(TEXT("Failure retains previous output"), EncodeProduct(P), EncodeProduct(Kept));
    TestFalse(TEXT("Missing axis"), ParseText(Html.Replace(TEXT("82cm"), TEXT("不明")), P));
    TestTrue(TEXT("Missing matching color photo is allowed"),
             ParseText(Html.Replace(TEXT("張地：ブルーグレー"), TEXT("張地：未知色")), P));
    if (P.Variants.Num() == 2)
        TestTrue(TEXT("No other color substitution"), P.Variants[1].ImageUrl.IsEmpty());
    TestFalse(TEXT("Malformed quote"), ParseText(TEXT("<title x='unclosed>"), P));
    TArray<uint8> Large;
    Large.SetNumZeroed(ProductBodyLimit + 1);
    TestFalse(TEXT("Body limit"), ParseProduct(Large, Url, P, Error));
    TArray<uint8> InvalidUtf8{0xff, 0xfe};
    TestFalse(TEXT("Invalid UTF8"), ParseProduct(InvalidUtf8, Url, P, Error));
    return true;
}
#endif
