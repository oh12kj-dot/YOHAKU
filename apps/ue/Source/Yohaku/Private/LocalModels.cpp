#include "LocalModels.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Internationalization/Regex.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Windows/WindowsHWrapper.h"
namespace Yohaku
{
namespace
{
bool Match(const FString &S, const TCHAR *Pattern)
{
    FRegexMatcher M(FRegexPattern(Pattern), S);
    return M.FindNext() && M.GetMatchBeginning() == 0 && M.GetMatchEnding() == S.Len();
}
bool Hash(const FString &S)
{
    return Match(S, TEXT("^[0-9a-f]{64}$"));
}
bool AssetId(const FString &S)
{
    return Match(S, TEXT("^study-hy3d-[0-9a-f]{32}$"));
}
bool ObjectFile(const FString &Path, int64 Max, TSharedPtr<FJsonObject> &Out, FString *Digest = nullptr)
{
    TArray<uint8> Bytes;
    const int64 Size = IFileManager::Get().FileSize(*Path);
    if (Size < 1 || Size > Max || !FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() > Max || Bytes.Contains(0))
        return false;
    if (Digest)
        *Digest = Sha256(Bytes);
    FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR *>(Bytes.GetData()), Bytes.Num());
    FString S(Decoded.Length(), Decoded.Get());
    FTCHARToUTF8 Encoded(*S);
    if (Encoded.Length() != Bytes.Num() || FMemory::Memcmp(Encoded.Get(), Bytes.GetData(), Bytes.Num()))
        return false;
    return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S), Out) && Out.IsValid();
}
bool Vec(const TSharedPtr<FJsonValue> &Value, FVector &V)
{
    const TArray<TSharedPtr<FJsonValue>> *A;
    if (!Value || !Value->TryGetArray(A) || A->Num() != 3 || !(*A)[0]->TryGetNumber(V.X) ||
        !(*A)[1]->TryGetNumber(V.Y) || !(*A)[2]->TryGetNumber(V.Z))
        return false;
    return !V.ContainsNaN() && V.GetAbsMax() <= 60000;
}
bool Manifest(const FString &Directory, const FString &ExpectedId, const FString &ExpectedHash, FAsset &A,
              FString &Error)
{
    auto Fail = [&]() {
        Error = TEXT("生成試作の証拠または形状が欠けているか、内容が変更されています。現在の部屋は保持しました。");
        return false;
    };
    TSharedPtr<FJsonObject> O;
    FString Digest;
    if (!ObjectFile(Directory / TEXT("manifest.json"), 1024 * 1024, O, &Digest) ||
        (!ExpectedHash.IsEmpty() && Digest != ExpectedHash) || O->Values.Num() != 20)
        return Fail();
    FString Format, Quality, Coordinates, Variant, ImageUrl, ImageHash, Checkpoint, Time, Candidate;
    double Version;
    const TSharedPtr<FJsonObject> *Source = nullptr;
    const TArray<TSharedPtr<FJsonValue>> *Parts = nullptr, *Inferred = nullptr;
    if (!O->TryGetStringField(TEXT("format"), Format) || Format != TEXT("yohaku-generated-study") ||
        !O->TryGetNumberField(TEXT("version"), Version) || Version != 1 || !O->TryGetStringField(TEXT("id"), A.Id) ||
        !AssetId(A.Id) || (!ExpectedId.IsEmpty() && A.Id != ExpectedId) ||
        !O->TryGetStringField(TEXT("name"), A.Name) || A.Name.Len() > 800 ||
        !O->TryGetStringField(TEXT("meshHash"), A.MeshHash) || !Hash(A.MeshHash) ||
        !O->TryGetStringField(TEXT("candidateId"), Candidate) || !O->TryGetStringField(TEXT("imageHash"), ImageHash) ||
        !Hash(ImageHash) || !O->TryGetStringField(TEXT("checkpointHash"), Checkpoint) || !Hash(Checkpoint) ||
        !O->TryGetStringField(TEXT("generatedAt"), Time) ||
        !O->TryGetStringField(TEXT("dimensionStatus"), A.DimensionStatus) ||
        A.DimensionStatus != TEXT("single-source-unverified") ||
        !O->TryGetStringField(TEXT("materialStatus"), A.MaterialStatus) || A.MaterialStatus != TEXT("not-generated") ||
        !O->TryGetStringField(TEXT("qualityStatus"), Quality) || Quality != TEXT("unadopted-study") ||
        !O->TryGetStringField(TEXT("coordinates"), Coordinates) ||
        Coordinates != TEXT("mm-x-depth-height-centered-floor") || !O->TryGetObjectField(TEXT("source"), Source) ||
        !O->TryGetStringField(TEXT("variantId"), Variant) || !O->TryGetStringField(TEXT("imageUrl"), ImageUrl) ||
        !Vec(O->TryGetField(TEXT("sizeMm")), A.Size) || !O->TryGetArrayField(TEXT("parts"), Parts) ||
        Parts->IsEmpty() || Parts->Num() > 2048 || !O->TryGetArrayField(TEXT("inferred"), Inferred) ||
        Inferred->Num() != 3)
        return Fail();
    FGuid CandidateId;
    FDateTime Date;
    FProductSource Product;
    if (!FGuid::Parse(Candidate, CandidateId) || !CandidateId.IsValid() || !Time.EndsWith(TEXT("Z")) ||
        !FDateTime::ParseIso8601(*Time, Date) || !DecodeProduct(*Source, Product) ||
        !A.Size.Equals(FVector(Product.WidthMm, Product.DepthMm, Product.HeightMm), .001))
        return Fail();
    const auto *V =
        Product.Variants.FindByPredicate([&](const auto &P) { return P.Id == Variant && P.ImageUrl == ImageUrl; });
    if (!V || ImageUrl.IsEmpty())
        return Fail();
    const TSharedPtr<FJsonObject> *Settings = nullptr;
    double Steps, Seed, Octree, Grid, TargetFaces;
    if (!O->TryGetObjectField(TEXT("settings"), Settings) || (*Settings)->Values.Num() != 5 ||
        !(*Settings)->TryGetNumberField(TEXT("steps"), Steps) || Steps != 20 ||
        !(*Settings)->TryGetNumberField(TEXT("seed"), Seed) || !FMath::IsFinite(Seed) || Seed < 0 ||
        Seed > 2147483647 || Seed != FMath::FloorToDouble(Seed) ||
        !(*Settings)->TryGetNumberField(TEXT("octree"), Octree) || Octree != 256 ||
        !(*Settings)->TryGetNumberField(TEXT("collisionGrid"), Grid) || Grid != 16 ||
        !(*Settings)->TryGetNumberField(TEXT("meshTargetFaces"), TargetFaces) || TargetFaces != 50000)
        return Fail();
    TSet<FString> Labels;
    for (auto I : *Inferred)
    {
        FString S;
        if (!I->TryGetString(S) ||
            (S != TEXT("back-and-hidden-surfaces") && S != TEXT("front-direction") &&
             S != TEXT("single-image-shape")) ||
            Labels.Contains(S))
            return Fail();
        Labels.Add(S);
    }
    A.Parts.Reset();
    for (auto Value : *Parts)
    {
        const TSharedPtr<FJsonObject> *P = nullptr;
        FCollisionBox B;
        if (!Value->TryGetObject(P) || (*P)->Values.Num() != 2 || !Vec((*P)->TryGetField(TEXT("center")), B.Center) ||
            !Vec((*P)->TryGetField(TEXT("size")), B.Size) || B.Size.GetMin() <= 0 ||
            B.Size.GetAbsMax() > A.Size.GetAbsMax() || B.Center.GetAbsMax() > A.Size.GetAbsMax() * 2)
            return Fail();
        const FVector Low(-A.Size.X / 2, -A.Size.Y / 2, 0), High(A.Size.X / 2, A.Size.Y / 2, A.Size.Z);
        const auto Min = B.Center - B.Size / 2, Max = B.Center + B.Size / 2;
        if (Min.X < Low.X - .02 || Min.Y < Low.Y - .02 || Min.Z < Low.Z - .02 || Max.X > High.X + .02 ||
            Max.Y > High.Y + .02 || Max.Z > High.Z + .02)
            return Fail();
        A.Parts.Add(B);
    }
    A.ManifestHash = Digest;
    A.SourceUrl = Product.SourceUrl;
    A.MeshFile = Directory / TEXT("mesh.json");
    A.bGeneratedStudy = true;
    FGeneratedMesh Mesh;
    if (!FLocalModels::ReadMesh(A, Mesh, Error))
        return false;
    TArray<uint8> Image;
    if (IFileManager::Get().FileSize(*(Directory / TEXT("source-image.bin"))) > 16 * 1024 * 1024 ||
        !FFileHelper::LoadFileToArray(Image, *(Directory / TEXT("source-image.bin"))) || Sha256(Image) != ImageHash)
        return Fail();
    return true;
}
FString Profile()
{
    return FString(FPlatformProcess::UserSettingsDir()) / TEXT("Yohaku/LocalGeneration/runtime.json");
}
bool Runtime(FString &Python, FString &Comfy, FString &Checkpoint)
{
    TSharedPtr<FJsonObject> O;
    if (!ObjectFile(Profile(), 16384, O))
        return false;
    FString H;
    return O->TryGetStringField(TEXT("python"), Python) && O->TryGetStringField(TEXT("comfyRoot"), Comfy) &&
           O->TryGetStringField(TEXT("checkpoint"), Checkpoint) && O->TryGetStringField(TEXT("checkpointSha256"), H) &&
           Hash(H) && IFileManager::Get().FileExists(*Python) && IFileManager::Get().FileExists(*Checkpoint) &&
           IFileManager::Get().DirectoryExists(*Comfy);
}
} // namespace
FString FLocalModels::Root()
{
    FString R;
    if (!FParse::Value(FCommandLine::Get(), TEXT("YohakuModelRoot="), R))
        R = FString(FPlatformProcess::UserSettingsDir()) / TEXT("Yohaku/Models");
    return FPaths::ConvertRelativePathToFull(R);
}
bool FLocalModels::Resolve(const FString &Id, const FString &HashValue, FAsset &A, FString &Error)
{
    if (!AssetId(Id) || !Hash(HashValue))
    {
        Error = TEXT("未対応の生成家具版です");
        return false;
    }
    FAsset Candidate;
    if (!Manifest(Root() / Id, Id, HashValue, Candidate, Error))
        return false;
    A = MoveTemp(Candidate);
    return true;
}
bool FLocalModels::ReadMesh(const FAsset &A, FGeneratedMesh &Out, FString &Error)
{
    auto Fail = [&]() {
        Error = TEXT("生成形状の形式・ハッシュ・外寸が不正です");
        return false;
    };
    TSharedPtr<FJsonObject> O;
    FString Digest;
    if (!ObjectFile(A.MeshFile, 48 * 1024 * 1024, O, &Digest) || Digest != A.MeshHash || O->Values.Num() != 4)
        return Fail();
    double Version;
    FString Units;
    const TArray<TSharedPtr<FJsonValue>> *V = nullptr, *T = nullptr;
    if (!O->TryGetNumberField(TEXT("version"), Version) || Version != 1 ||
        !O->TryGetStringField(TEXT("units"), Units) || Units != TEXT("mm-x-depth-height") ||
        !O->TryGetArrayField(TEXT("vertices"), V) || V->Num() < 100 || V->Num() > 150000 ||
        !O->TryGetArrayField(TEXT("triangles"), T) || T->Num() < 300 || T->Num() > 900000 || T->Num() % 3)
        return Fail();
    FGeneratedMesh M;
    FBox Bounds(ForceInit);
    for (auto P : *V)
    {
        FVector Point;
        if (!Vec(P, Point))
            return Fail();
        M.Vertices.Add(Point);
        Bounds += Point;
    }
    if (!Bounds.GetSize().Equals(A.Size, .02) || FMath::Abs(Bounds.Min.Z) > .01 ||
        FMath::Abs(Bounds.GetCenter().X) > .01 || FMath::Abs(Bounds.GetCenter().Y) > .01)
        return Fail();
    for (auto P : *T)
    {
        double Index;
        if (!P->TryGetNumber(Index) || !FMath::IsFinite(Index) || Index < 0 || Index >= M.Vertices.Num() ||
            Index != FMath::FloorToDouble(Index))
            return Fail();
        M.Triangles.Add(int32(Index));
    }
    for (int32 I = 0; I < M.Triangles.Num(); I += 3)
        if (FVector::CrossProduct(M.Vertices[M.Triangles[I + 1]] - M.Vertices[M.Triangles[I]],
                                  M.Vertices[M.Triangles[I + 2]] - M.Vertices[M.Triangles[I]])
                .SizeSquared() < 1e-12)
            return Fail();
    Out = MoveTemp(M);
    return true;
}
bool FLocalModels::Publish(const FString &Job, FAsset &Out, FString &Error)
{
    FAsset A;
    if (!Manifest(Job, TEXT(""), TEXT(""), A, Error))
        return false;
    const FString Dest = Root() / A.Id,
                  Temp = Root() / (TEXT("pending-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    auto &Files = IFileManager::Get();
    if (Files.DirectoryExists(*Dest))
        return Resolve(A.Id, A.ManifestHash, Out, Error);
    Files.MakeDirectory(*Temp, true);
    ON_SCOPE_EXIT
    {
        if (FPaths::DirectoryExists(Temp))
            IFileManager::Get().DeleteDirectory(*Temp, false, true);
    };
    for (const TCHAR *Name : {TEXT("manifest.json"), TEXT("mesh.json"), TEXT("source-image.bin")})
        if (Files.Copy(*(Temp / Name), *(Job / Name), false) != COPY_OK)
        {
            Error = TEXT("生成試作を保存できません");
            return false;
        }
    if (!::MoveFileExW(*Temp, *Dest, MOVEFILE_WRITE_THROUGH))
    {
        Error = TEXT("生成試作を確定できません。元のデータは保持しました");
        return false;
    }
    return Resolve(A.Id, A.ManifestHash, Out, Error);
}
TArray<FAsset> FLocalModels::List()
{
    TArray<FString> Folders;
    IFileManager::Get().FindFiles(Folders, *(Root() / TEXT("*")), false, true);
    TArray<FAsset> Result;
    for (auto Folder : Folders)
    {
        if (!AssetId(Folder) || Result.Num() >= 100)
            continue;
        FAsset A;
        FString Error;
        if (Manifest(Root() / Folder, Folder, TEXT(""), A, Error))
            Result.Add(MoveTemp(A));
    }
    return Result;
}
FLocalGeneration::~FLocalGeneration()
{
    Cancel();
}
bool FLocalGeneration::Available() const
{
    FString P, C, M;
    return Runtime(P, C, M) &&
           IFileManager::Get().FileExists(*(FPaths::ProjectDir() / TEXT("Tools/local-3d-worker.py")));
}
bool FLocalGeneration::Start(const FProductSource &P, const FGuid &Candidate, const FString &Variant, FString &Error)
{
    if (Busy())
        return false;
    FString Python, Comfy, Checkpoint;
    if (!Runtime(Python, Comfy, Checkpoint) || !Available())
    {
        Error = TEXT("ローカル生成の準備が必要です。有料サービスは使用しません。");
        return false;
    }
    const auto *V = P.Variants.FindByPredicate([&](const auto &X) { return X.Id == Variant && !X.ImageUrl.IsEmpty(); });
    FProductSource Validated;
    if (!Candidate.IsValid() || !V || !DecodeProduct(ProductToJson(P), Validated))
    {
        Error = TEXT("写真に対応する色・仕様を選択してください");
        return false;
    }
    JobDirectory = FLocalModels::Root() / (TEXT("jobs/") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    IFileManager::Get().MakeDirectory(*JobDirectory, true);
    auto O = MakeShared<FJsonObject>();
    O->SetObjectField(TEXT("source"), ProductToJson(P));
    O->SetStringField(TEXT("candidateId"), Candidate.ToString(EGuidFormats::DigitsWithHyphens));
    O->SetStringField(TEXT("variantId"), Variant);
    O->SetStringField(TEXT("assetId"), TEXT("study-hy3d-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower());
    O->SetNumberField(TEXT("seed"), 42);
    FString Json;
    FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Json));
    if (!FFileHelper::SaveStringToFile(Json, *(JobDirectory / TEXT("job.json")),
                                       FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        Error = TEXT("生成入力を保存できません");
        return false;
    }
    const FString Worker = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/local-3d-worker.py"));
    const FString Args =
        FString::Printf(TEXT("\"%s\" \"%s\" \"%s\""), *Worker, *(JobDirectory / TEXT("job.json")), *Profile());
    Handle = FPlatformProcess::CreateProc(*Python, *Args, false, true, true, &ProcessId, 0, *Comfy, nullptr);
    if (!Handle.IsValid())
    {
        Error = TEXT("ローカル生成を起動できません");
        return false;
    }
    Result.Reset();
    Started = FPlatformTime::Seconds();
    Status = TEXT("ローカル生成を開始しました（形状試作・素材未生成）");
    return true;
}
void FLocalGeneration::Poll()
{
    if (!Busy())
        return;
    TSharedPtr<FJsonObject> O;
    if (ObjectFile(JobDirectory / TEXT("status.json"), 16384, O))
    {
        FString S;
        if (O->TryGetStringField(TEXT("message"), S) && S.Len() <= 1000)
            Status = S;
    }
    if (FPlatformTime::Seconds() - Started > 1800)
    {
        Cancel();
        Status = TEXT("ローカル生成の制限時間を超えました。部屋は保持しました");
        return;
    }
    if (FPlatformProcess::IsProcRunning(Handle))
        return;
    int32 Code = -1;
    FPlatformProcess::GetProcReturnCode(Handle, &Code);
    FPlatformProcess::CloseProc(Handle);
    Handle.Reset();
    FAsset A;
    FString E;
    if (Code == 0 && FLocalModels::Publish(JobDirectory, A, E))
    {
        Result = MoveTemp(A);
        Status = TEXT("掲載値に合わせた無地の3D試作です。背面・正面方向は推定、寸法・素材は未照合です。");
    }
    else
    {
        Result.Reset();
        Status = E.IsEmpty() ? TEXT("ローカル生成に失敗しました。部屋は保持しました。") : E;
    }
}
void FLocalGeneration::Cancel()
{
    if (Handle.IsValid())
    {
        FPlatformProcess::TerminateProc(Handle, true);
        FPlatformProcess::CloseProc(Handle);
        Handle.Reset();
        Status = TEXT("生成を中止しました。部屋は変更していません。");
    }
    Result.Reset();
}
} // namespace Yohaku
