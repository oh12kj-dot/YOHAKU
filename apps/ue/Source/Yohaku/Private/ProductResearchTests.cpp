#include "ProductResearch.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductResearchContract, "Yohaku.Native.ProductResearchState",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductResearchContract::RunTest(const FString &)
{
    TSharedPtr<FProductResearch, ESPMode::ThreadSafe> R = MakeShared<FProductResearch, ESPMode::ThreadSafe>();
    R->SetInput(TEXT("https://localhost/product/106294/"));
    R->Start();
    TestFalse(TEXT("Invalid URL never starts worker"), R->IsBusy());
    TestFalse(TEXT("Invalid URL never returns product"), R->Result.IsSet());
    TestFalse(TEXT("Empty result cannot be saved"), R->CanSaveCandidate());
    const FString Url = TEXT("https://flymee.jp/product/106294/");
    R->SetInput(Url);
    Yohaku::FProductSource P;
    P.ProductId = TEXT("106294");
    P.SourceUrl = Url;
    R->bBusy = true;
    const uint64 First = ++R->Generation;
    R->Complete(First, Url, P, TEXT(""));
    TestTrue(TEXT("Current completed request is shown"), R->Result.IsSet());
    R->SetInput(TEXT("https://flymee.jp/product/106295/"));
    TestFalse(TEXT("URL edit clears old product immediately"), R->Result.IsSet());
    R->bBusy = true;
    R->Complete(First, Url, P, TEXT(""));
    TestFalse(TEXT("Stale response cannot restore old product"), R->Result.IsSet());
    TestFalse(TEXT("Stale completion cannot be saved"), R->CanSaveCandidate());
    R->bBusy = true;
    const uint64 Current = ++R->Generation;
    R->Cancelled = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
    auto Token = R->Cancelled;
    R->Cancel();
    TestTrue(TEXT("Cancellation reaches worker token"), Token->Load());
    R->Complete(Current, R->Input, P, TEXT(""));
    TestFalse(TEXT("Cancelled completion is discarded"), R->Result.IsSet());
    TestFalse(TEXT("Cancelled completion cannot be saved"), R->CanSaveCandidate());
    R->bBusy = true;
    const uint64 Before = R->Generation;
    R->Start();
    TestEqual(TEXT("One in-flight request only"), R->Generation, Before);
    R->bBusy = false;
    R->LastStart = FPlatformTime::Seconds();
    R->Start();
    TestFalse(TEXT("Immediate retry is bounded"), R->IsBusy());
    R->Complete(R->Generation, R->Input, {}, TEXT("通信失敗"));
    TestFalse(TEXT("Failure never displays old result"), R->Result.IsSet());
    TestEqual(TEXT("Failure is visible"), R->Status, FString(TEXT("通信失敗")));
    R->Cancelled = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
    Token = R->Cancelled;
    R.Reset();
    TestTrue(TEXT("Owner destruction cancels worker"), Token->Load());
    return true;
}
#endif
