#pragma once
#include "CoreMinimal.h"
#include "ProductSource.h"

namespace Yohaku
{
struct FProductCandidate
{
    FGuid Id;
    FString SavedAt;
    FProductSource Source;
};
// Candidate evidence is independent of room saves and never represents an adopted model.
class FProductCandidateStorage
{
  public:
    explicit FProductCandidateStorage(const FString &Root = TEXT(""));
    bool Reload(FString &Error);
    bool Save(const FProductSource &Source, FGuid &Id, bool &Existing, FString &Error);
    const TArray<FProductCandidate> &Entries() const { return Items; }
    bool IsReadable() const { return bReadable; }
    const FString &Path() const { return FilePath; }

  private:
    bool Read(TArray<FProductCandidate> &Out, FString &Error) const;
    FString FilePath;
    TArray<FProductCandidate> Items;
    bool bReadable = false;
};
} // namespace Yohaku
