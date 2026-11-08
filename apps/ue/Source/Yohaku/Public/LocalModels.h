#pragma once
#include "CoreMinimal.h"
#include "SceneDocument.h"
#include "ProductSource.h"
#include "HAL/PlatformProcess.h"
namespace Yohaku
{
struct FGeneratedMesh
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
};
class FLocalModels
{
  public:
    static FString Root();
    static bool Resolve(const FString &Id, const FString &Hash, FAsset &Out, FString &Error);
    static bool ReadMesh(const FAsset &, FGeneratedMesh &, FString &Error);
    static bool Publish(const FString &JobDirectory, FAsset &, FString &Error);
    static TArray<FAsset> List();
};
class FLocalGeneration
{
  public:
    ~FLocalGeneration();
    bool Available() const;
    bool Start(const FProductSource &, const FGuid &Candidate, const FString &Variant, FString &Error);
    void Poll();
    void Cancel();
    bool Busy() const { return Handle.IsValid(); }
    FString Status;
    TOptional<FAsset> Result;

  private:
    FProcHandle Handle;
    FString JobDirectory;
    uint32 ProcessId = 0;
    double Started = 0;
};
} // namespace Yohaku
