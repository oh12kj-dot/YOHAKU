#include "RoomController.h"
#include "ProductResearch.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealClient.h"

void ARoomController::RunProductSmokeTest()
{
    FString Root, CandidateRoot;
    if (!FParse::Value(FCommandLine::Get(), TEXT("YohakuLayoutRoot="), Root) ||
        !FParse::Value(FCommandLine::Get(), TEXT("YohakuCandidateRoot="), CandidateRoot))
    {
        UE_LOG(LogTemp, Error, TEXT("PRODUCT_SMOKE_REQUIRES_ISOLATED_STORAGE"));
        UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
        return;
    }
    const FString Before = Yohaku::Encode(Document.Get());
    const bool DirtyBefore = Document.Dirty();
    const int32 HistoryBefore = Document.UndoCount();
    const TWeakObjectPtr<ARoomController> Weak(this);
    bShowResearch = true;
    const bool Reload = FParse::Param(FCommandLine::Get(), TEXT("YohakuCandidateReloadSmoke"));
    Research->OnFinished = [Weak, Before, DirtyBefore, HistoryBefore, Root, Reload]() {
        if (!Weak.IsValid())
            return;
        auto &C = *Weak.Get();
        const auto Evidence = MakeShared<FJsonObject>();
        bool CandidateOk = false;
        if (Reload)
            CandidateOk = C.Research->Result.IsSet() && !C.Research->IsBusy();
        else if (C.Research->Result.IsSet())
        {
            const FString Expected = Yohaku::EncodeProduct(C.Research->Result.GetValue());
            C.Research->SaveCandidate();
            const int32 Count = C.Research->SavedCandidates().Num();
            C.Research->SaveCandidate();
            C.Research->Cancel();
            if (Count > 0)
                C.Research->OpenCandidate(C.Research->SavedCandidates().Last().Id);
            CandidateOk = Count > 0 && Count == C.Research->SavedCandidates().Num() && C.Research->Result.IsSet() &&
                          Expected == Yohaku::EncodeProduct(C.Research->Result.GetValue());
        }
        const bool Unchanged = Before == Yohaku::Encode(C.Document.Get()) && DirtyBefore == C.Document.Dirty() &&
                               HistoryBefore == C.Document.UndoCount();
        const auto &P = C.Research->Result;
        const bool Ok = CandidateOk && Unchanged && P.IsSet() && P->ProductId == TEXT("106294") && P->WidthMm == 1500 &&
                        P->DepthMm == 870 && P->HeightMm == 820 && P->Variants.Num() == 2;
        Evidence->SetBoolField(TEXT("passed"), Ok);
        Evidence->SetBoolField(TEXT("sceneDirtyAndHistoryUnchanged"), Unchanged);
        Evidence->SetBoolField(TEXT("candidateSaveOrOfflineReloadPassed"), CandidateOk);
        Evidence->SetStringField(TEXT("status"), C.Research->Status);
        if (P.IsSet())
            Evidence->SetStringField(TEXT("product"), Yohaku::EncodeProduct(P.GetValue()));
        FString Json;
        FJsonSerializer::Serialize(Evidence, TJsonWriterFactory<>::Create(&Json));
        IFileManager::Get().MakeDirectory(*Root, true);
        FFileHelper::SaveStringToFile(
            Json, *(Root / (Reload ? TEXT("candidate-restart.json") : TEXT("product-evidence.json"))),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("YOHAKU_PRODUCT_SMOKE %s"), Ok ? TEXT("PASS") : TEXT("FAIL"));
        FTimerHandle Shot;
        C.GetWorldTimerManager().SetTimer(
            Shot,
            FTimerDelegate::CreateWeakLambda(
                &C,
                [&C, Root]() {
                    FScreenshotRequest::RequestScreenshot(Root / TEXT("product-research.png"), true, false);
                    FTimerHandle Quit;
                    C.GetWorldTimerManager().SetTimer(
                        Quit,
                        FTimerDelegate::CreateWeakLambda(
                            &C, [&C]() { UKismetSystemLibrary::QuitGame(&C, &C, EQuitPreference::Quit, false); }),
                        3, false);
                }),
            3, false);
    };
    if (Reload)
    {
        if (!Research->SavedCandidates().IsEmpty())
            Research->OpenCandidate(Research->SavedCandidates().Last().Id);
        Research->OnFinished();
    }
    else
        Research->Start();
}
