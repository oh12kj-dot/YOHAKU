#pragma once
#include "CoreMinimal.h"
#include "ProductSource.h"
#include "ProductCandidateStorage.h"
#include "LocalModels.h"

// Transient research is deliberately separate from the saved room document.
class FProductResearch : public TSharedFromThis<FProductResearch, ESPMode::ThreadSafe>
{
  public:
    FString Input = TEXT("https://flymee.jp/product/106294/?pi_id=326211");
    FString Status;
    TOptional<Yohaku::FProductSource> Result;
    TFunction<void()> OnFinished;
    FProductResearch();
    ~FProductResearch();
    void SetInput(const FString &Value);
    void Start();
    void Cancel();
    bool IsBusy() const { return bBusy || LocalGeneration.Busy(); }
    FString Summary() const;
    void SaveCandidate();
    void OpenCandidate(const FGuid &Id);
    void ReloadCandidates();
    bool CanSaveCandidate() const { return !IsBusy() && Result.IsSet() && Candidates.IsReadable(); }
    Yohaku::FLocalGeneration LocalGeneration;
    FString GenerationVariantId;
    void Generate(const FString &VariantId);
    const TArray<Yohaku::FProductCandidate> &SavedCandidates() const { return Candidates.Entries(); }
    FString CandidateStatus;

  private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FProductResearchContract;
#endif
    void Complete(uint64 RequestId, const FString &Url, TOptional<Yohaku::FProductSource> Product,
                  const FString &Error);
    TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> Cancelled;
    bool bBusy = false;
    uint64 Generation = 0;
    double LastStart = -100;
    Yohaku::FProductCandidateStorage Candidates;
};
