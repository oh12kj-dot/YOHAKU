#include "ProductCandidateStorage.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Windows/WindowsHWrapper.h"

namespace Yohaku
{
namespace
{
constexpr int64 Limit = 4 * 1024 * 1024;
struct FStoreLock
{
    HANDLE Handle = nullptr;
    bool Owned = false;
    explicit FStoreLock(const FString &Path)
    {
        const FString Name = TEXT("Local\\YohakuCandidates-") + FMD5::HashAnsiString(*Path.ToLower());
        Handle = ::CreateMutexW(nullptr, false, *Name);
        if (Handle)
        {
            const DWORD Result = ::WaitForSingleObject(Handle, 0);
            Owned = Result == WAIT_OBJECT_0 || Result == WAIT_ABANDONED;
        }
    }
    ~FStoreLock()
    {
        if (Owned)
            ::ReleaseMutex(Handle);
        if (Handle)
            ::CloseHandle(Handle);
    }
};
FString EncodeCandidates(const TArray<FProductCandidate> &Items)
{
    const auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("format"), TEXT("yohaku-product-candidates"));
    O->SetNumberField(TEXT("version"), 1);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const auto &Item : Items)
    {
        const auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("id"), Item.Id.ToString(EGuidFormats::DigitsWithHyphens));
        Row->SetStringField(TEXT("savedAt"), Item.SavedAt);
        Row->SetObjectField(TEXT("source"), ProductToJson(Item.Source));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    O->SetArrayField(TEXT("entries"), Rows);
    FString Json;
    FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Json));
    return Json;
}
} // namespace
FProductCandidateStorage::FProductCandidateStorage(const FString &Root)
{
    FString Folder = Root;
    if (Folder.IsEmpty() && !FParse::Value(FCommandLine::Get(), TEXT("YohakuCandidateRoot="), Folder))
        Folder = FString(FPlatformProcess::UserSettingsDir()) / TEXT("Yohaku/ProductCandidates");
    FilePath = FPaths::ConvertRelativePathToFull(Folder / TEXT("candidates.yohaku.json"));
    FPaths::CollapseRelativeDirectories(FilePath);
}
bool FProductCandidateStorage::Read(TArray<FProductCandidate> &Out, FString &Error) const
{
    auto Fail = [&]() {
        Error = TEXT("商品候補を読み込めません。元のファイルを保持しています。形式・保存先を確認してください。");
        return false;
    };
    auto &Files = IFileManager::Get();
    if (!Files.FileExists(*FilePath))
    {
        if (Files.DirectoryExists(*FilePath))
            return Fail();
        Out.Reset();
        return true;
    }
    const int64 Size = Files.FileSize(*FilePath);
    TArray<uint8> Bytes;
    if (Size < 1 || Size > Limit || !FFileHelper::LoadFileToArray(Bytes, *FilePath) || Bytes.Num() > Limit ||
        Bytes.Contains(0))
        return Fail();
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR *>(Bytes.GetData()), Bytes.Num());
    const FString Json(Decoded.Length(), Decoded.Get());
    const FTCHARToUTF8 Encoded(*Json);
    if (Encoded.Length() != Bytes.Num() || FMemory::Memcmp(Encoded.Get(), Bytes.GetData(), Bytes.Num()))
        return Fail();
    TSharedPtr<FJsonObject> O;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O || O->Values.Num() != 3)
        return Fail();
    FString Format;
    double Version;
    const TArray<TSharedPtr<FJsonValue>> *Rows;
    if (!O->TryGetStringField(TEXT("format"), Format) || Format != TEXT("yohaku-product-candidates") ||
        !O->TryGetNumberField(TEXT("version"), Version) || Version != 1 ||
        !O->TryGetArrayField(TEXT("entries"), Rows) || Rows->Num() > 100)
        return Fail();
    TArray<FProductCandidate> Loaded;
    TSet<FGuid> Ids;
    TSet<FString> Keys;
    for (const auto &Value : *Rows)
    {
        const TSharedPtr<FJsonObject> *Row, *Source;
        FProductCandidate Item;
        FString Id;
        FDateTime Time;
        if (!Value || !Value->TryGetObject(Row) || (*Row)->Values.Num() != 3 ||
            !(*Row)->TryGetStringField(TEXT("id"), Id) ||
            !FGuid::ParseExact(Id, EGuidFormats::DigitsWithHyphens, Item.Id) || !Item.Id.IsValid() ||
            Ids.Contains(Item.Id) || !(*Row)->TryGetStringField(TEXT("savedAt"), Item.SavedAt) ||
            !Item.SavedAt.EndsWith(TEXT("Z")) || !FDateTime::ParseIso8601(*Item.SavedAt, Time) ||
            !(*Row)->TryGetObjectField(TEXT("source"), Source) || !DecodeProduct(*Source, Item.Source))
            return Fail();
        const FString Key = Item.Source.SourceUrl + TEXT("\n") + Item.Source.ContentHash;
        if (Keys.Contains(Key))
            return Fail();
        Ids.Add(Item.Id);
        Keys.Add(Key);
        Loaded.Add(MoveTemp(Item));
    }
    Out = MoveTemp(Loaded);
    return true;
}
bool FProductCandidateStorage::Reload(FString &Error)
{
    TArray<FProductCandidate> Loaded;
    bReadable = Read(Loaded, Error);
    if (!bReadable)
        return false;
    Items = MoveTemp(Loaded);
    Error.Reset();
    return true;
}
bool FProductCandidateStorage::Save(const FProductSource &Source, FGuid &Id, bool &Existing, FString &Error)
{
    Existing = false;
    FProductSource Validated;
    if (!bReadable || !DecodeProduct(ProductToJson(Source), Validated))
    {
        Error = TEXT("保存できる調査結果がないか、候補ファイルを再読込する必要があります。");
        return false;
    }
    FStoreLock Lock(FilePath);
    if (!Lock.Owned)
    {
        Error = TEXT("別の処理が商品候補を保存中です。少し待ってから保存してください。");
        return false;
    }
    TArray<FProductCandidate> Next;
    if (!Read(Next, Error))
    {
        bReadable = false;
        return false;
    }
    for (const auto &Item : Next)
        if (Item.Source.SourceUrl == Source.SourceUrl && Item.Source.ContentHash == Source.ContentHash)
        {
            Id = Item.Id;
            Existing = true;
            Items = MoveTemp(Next);
            Error.Reset();
            return true;
        }
    if (Next.Num() >= 100)
    {
        Error = TEXT("保存できる商品候補は100件までです。");
        return false;
    }
    FProductCandidate Item{FGuid::NewGuid(), FDateTime::UtcNow().ToIso8601(), MoveTemp(Validated)};
    Next.Add(Item);
    const FString Json = EncodeCandidates(Next);
    const FTCHARToUTF8 Encoded(*Json);
    if (Encoded.Length() > Limit)
    {
        Error = TEXT("商品候補の保存容量4MiBを超えます。");
        return false;
    }
    const FString Temp = FilePath + TEXT(".") + FGuid::NewGuid().ToString() + TEXT(".tmp");
    auto &Files = IFileManager::Get();
    Files.MakeDirectory(*FPaths::GetPath(FilePath), true);
    if (!FFileHelper::SaveStringToFile(Json, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !::MoveFileExW(*Temp, *FilePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        Files.Delete(*Temp);
        Error = TEXT("商品候補を保存できません。元のファイルは保持しました。");
        return false;
    }
    Id = Item.Id;
    Items = MoveTemp(Next);
    Error.Reset();
    return true;
}
} // namespace Yohaku
