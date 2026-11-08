#include "ProductCandidateStorage.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Windows/WindowsHWrapper.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductCandidateContract, "Yohaku.Native.ProductCandidates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductCandidateContract::RunTest(const FString &)
{
    using namespace Yohaku;
    const FString Root = FPaths::ProjectSavedDir() / TEXT("Automation/Candidates") / FGuid::NewGuid().ToString();
    FProductCandidateStorage Store(Root);
    FString Error;
    TestTrue(TEXT("Missing library starts empty"), Store.Reload(Error));
    TArray<uint8> Bytes;
    TestTrue(TEXT("Load shared fixture"),
             FFileHelper::LoadFileToArray(Bytes,
                                          *(FPaths::ProjectDir() / TEXT("../../tests/fixtures/flymee-product.html"))));
    FProductSource P;
    TestTrue(TEXT("Parse fixture"),
             ParseProduct(Bytes, TEXT("https://flymee.jp/product/106294/?pi_id=326211"), P, Error));
    FGuid First, Id;
    bool Existing;
    TestTrue(TEXT("Save valid evidence"), Store.Save(P, First, Existing, Error));
    TestFalse(TEXT("New snapshot"), Existing);
    FString Original;
    FFileHelper::LoadFileToString(Original, *Store.Path());
    TestTrue(TEXT("Exact resave"), Store.Save(P, Id, Existing, Error));
    TestTrue(TEXT("Same identity"), Existing && Id == First);
    FString After;
    FFileHelper::LoadFileToString(After, *Store.Path());
    TestEqual(TEXT("Dedup does not rewrite file"), After, Original);
    FProductCandidateStorage Other(Root);
    TestTrue(TEXT("New owner loads offline"), Other.Reload(Error));
    TestEqual(TEXT("Full evidence roundtrip"), EncodeProduct(Other.Entries()[0].Source), EncodeProduct(P));
    P.ContentHash = FString::ChrN(64, 'a');
    TestTrue(TEXT("Changed content is distinct"), Other.Save(P, Id, Existing, Error));
    TestFalse(TEXT("New hash is not dedup"), Existing);
    P.SourceUrl = TEXT("https://flymee.jp/product/106294/?pi_id=326212");
    TestTrue(TEXT("Stale owner merges latest library"), Store.Save(P, Id, Existing, Error));
    TestEqual(TEXT("Both owners preserved"), Store.Entries().Num(), 3);
    FFileHelper::LoadFileToString(Original, *Store.Path());
    P.WidthMm = 0;
    TestFalse(TEXT("Bad dimension cannot save"), Store.Save(P, Id, Existing, Error));
    P.WidthMm = 1500;
    P.ContentHash = FString::ChrN(64, 'b');
    HANDLE Held = ::CreateFileW(*Store.Path(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    TestTrue(TEXT("Hold destination to exercise replacement failure"), Held != INVALID_HANDLE_VALUE);
    TestFalse(TEXT("Failed atomic replace"), Store.Save(P, Id, Existing, Error));
    if (Held != INVALID_HANDLE_VALUE)
        ::CloseHandle(Held);
    FFileHelper::LoadFileToString(After, *Store.Path());
    TestEqual(TEXT("Failed replace retains bytes"), After, Original);
    TestEqual(TEXT("Failed replace retains memory"), Store.Entries().Num(), 3);
    const TArray<FString> Invalid = {TEXT("{"),
                                     Original.Replace(TEXT("\"version\": 1"), TEXT("\"version\": 9")),
                                     Original.Replace(TEXT("single-source-unverified"), TEXT("verified")),
                                     Original.Replace(TEXT("not-generated"), TEXT("ready")),
                                     Original.Replace(TEXT("\"widthMm\": 1500"), TEXT("\"widthMm\": 0")),
                                     Original.Replace(TEXT("\"format\":"), TEXT("\"unknown\":")),
                                     FString::ChrN(4 * 1024 * 1024 + 1, 'x')};
    for (int32 I = 0; I < Invalid.Num(); ++I)
    {
        TestTrue(TEXT("Negative fixture changes document"), Invalid[I] != Original);
        FFileHelper::SaveStringToFile(Invalid[I], *Store.Path(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        TestFalse(FString::Printf(TEXT("Reject invalid fixture %d"), I), Store.Reload(Error));
        TestFalse(TEXT("No overwrite of unreadable file"), Store.Save(P, Id, Existing, Error));
        TestEqual(TEXT("No partial memory import"), Store.Entries().Num(), 3);
        FFileHelper::LoadFileToString(After, *Store.Path());
        TestEqual(TEXT("Bad file preserved"), After, Invalid[I]);
    }
    FFileHelper::SaveStringToFile(Original, *Store.Path(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    TestTrue(TEXT("Explicit reload recovers after file repair"), Store.Reload(Error));
    // This GUID directory was created by this test and contains no user data.
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}
#endif
