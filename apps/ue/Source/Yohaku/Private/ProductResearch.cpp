#include "ProductResearch.h"
#include "Async/Async.h"
#include "HAL/PlatformTime.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winhttp.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
#if PLATFORM_WINDOWS
struct FInternetHandle
{
    HINTERNET Value = nullptr;
    explicit FInternetHandle(HINTERNET In) : Value(In) {}
    ~FInternetHandle()
    {
        if (Value)
            WinHttpCloseHandle(Value);
    }
    FInternetHandle(const FInternetHandle &) = delete;
};
bool Fetch(const FString &Url, const TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> &Cancelled,
           Yohaku::FProductSource &Product, FString &Error)
{
    FString Canonical;
    if (!Yohaku::ValidateProductUrl(Url, Canonical, Error))
        return false;
    const double Deadline = FPlatformTime::Seconds() + 12;
    auto Active = [&]() {
        if (Cancelled->Load())
        {
            Error = TEXT("取得を中止しました");
            return false;
        }
        if (FPlatformTime::Seconds() >= Deadline)
        {
            Error = TEXT("取得がタイムアウトしました。後でもう一度お試しください。");
            return false;
        }
        return true;
    };
    auto Failure = [&]() {
        if (Active())
            Error = TEXT("商品ページを取得できません。通信環境またはアクセス制限をご確認ください。");
        return false;
    };
    FInternetHandle Session(WinHttpOpen(L"YOHAKU product research/0.3", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!Session.Value || !WinHttpSetTimeouts(Session.Value, 2000, 2000, 2000, 2000))
        return Failure();
    FInternetHandle Connection(WinHttpConnect(Session.Value, L"flymee.jp", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!Connection.Value || !Active())
        return Failure();
    const FString Path = Canonical.Mid(17);
    const WCHAR *Accept[] = {L"text/html", nullptr};
    FInternetHandle Request(
        WinHttpOpenRequest(Connection.Value, L"GET", *Path, nullptr, WINHTTP_NO_REFERER, Accept, WINHTTP_FLAG_SECURE));
    if (!Request.Value)
        return Failure();
    DWORD Policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD Disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_COOKIES;
    DWORD Retries = 0;
    if (!WinHttpSetOption(Request.Value, WINHTTP_OPTION_REDIRECT_POLICY, &Policy, sizeof(Policy)) ||
        !WinHttpSetOption(Request.Value, WINHTTP_OPTION_DISABLE_FEATURE, &Disabled, sizeof(Disabled)) ||
        !WinHttpSetOption(Request.Value, WINHTTP_OPTION_CONNECT_RETRIES, &Retries, sizeof(Retries)))
        return Failure();
    // Certificate and hostname verification remain WinHTTP defaults. No credentials/cookies or redirects.
    if (!Active() ||
        !WinHttpSendRequest(Request.Value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !Active() || !WinHttpReceiveResponse(Request.Value, nullptr))
        return Failure();
    DWORD Code = 0, Size = sizeof(Code);
    if (!WinHttpQueryHeaders(Request.Value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &Code, &Size, WINHTTP_NO_HEADER_INDEX))
        return Failure();
    if (Code < 200 || Code >= 300)
    {
        Error = TEXT("商品ページが取得を受け付けませんでした。転送やアクセス制限は回避しません。");
        return false;
    }
    WCHAR Mime[256]{};
    Size = sizeof(Mime);
    if (!WinHttpQueryHeaders(Request.Value, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX, Mime, &Size,
                             WINHTTP_NO_HEADER_INDEX))
        return Failure();
    FString MediaType = FString(Mime).ToLower();
    int32 Separator;
    if (MediaType.FindChar(';', Separator))
        MediaType.LeftInline(Separator);
    if (MediaType.TrimStartAndEnd() != TEXT("text/html"))
    {
        Error = TEXT("商品ページの形式がHTMLではありません。");
        return false;
    }
    TArray<uint8> Bytes;
    uint8 Buffer[16384];
    for (;;)
    {
        if (!Active())
            return false;
        DWORD Remaining = FMath::Clamp(int32((Deadline - FPlatformTime::Seconds()) * 1000), 1, 2000);
        if (!WinHttpSetOption(Request.Value, WINHTTP_OPTION_RECEIVE_TIMEOUT, &Remaining, sizeof(Remaining)))
            return Failure();
        DWORD Read = 0;
        if (!WinHttpReadData(Request.Value, Buffer, sizeof(Buffer), &Read))
            return Failure();
        if (!Active())
            return false;
        if (Read == 0)
            break;
        if (Bytes.Num() + Read > Yohaku::ProductBodyLimit)
        {
            Error = TEXT("商品ページが取得上限の2MiBを超えました。");
            return false;
        }
        Bytes.Append(Buffer, Read);
    }
    return Yohaku::ParseProduct(Bytes, Canonical, Product, Error) && Active();
}
#else
bool Fetch(const FString &, const TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> &, Yohaku::FProductSource &,
           FString &Error)
{
    Error = TEXT("この通信機能はWindows版に対応しています。");
    return false;
}
#endif
} // namespace
FProductResearch::FProductResearch()
{
    ReloadCandidates();
}
void FProductResearch::ReloadCandidates()
{
    FString Error;
    CandidateStatus = Candidates.Reload(Error) ? FString::Printf(TEXT("保存済み候補 %d件（未照合・3Dモデル未作成）"),
                                                                 Candidates.Entries().Num())
                                               : Error;
}
void FProductResearch::SaveCandidate()
{
    if (!CanSaveCandidate())
        return;
    FGuid Id;
    bool Existing = false;
    FString Error;
    CandidateStatus = Candidates.Save(Result.GetValue(), Id, Existing, Error)
                          ? (Existing ? TEXT("同じ掲載内容は保存済みです。重複は作りませんでした。")
                                      : TEXT("出典付きの商品候補を保存しました。"))
                          : Error;
}
void FProductResearch::OpenCandidate(const FGuid &Id)
{
    if (IsBusy())
        return;
    LocalGeneration.Cancel();
    GenerationVariantId.Reset();
    for (const auto &Item : Candidates.Entries())
        if (Item.Id == Id)
        {
            SetInput(Item.Source.SourceUrl);
            Result = Item.Source;
            Status = TEXT("保存済みの掲載情報（未照合）・保存日時 UTC：") + Item.SavedAt;
            return;
        }
}
FProductResearch::~FProductResearch()
{
    if (Cancelled)
        Cancelled->Store(true);
}
void FProductResearch::SetInput(const FString &Value)
{
    if (Input == Value)
        return;
    Cancel();
    Input = Value;
    Status.Reset();
    Result.Reset();
}
void FProductResearch::Cancel()
{
    GenerationVariantId.Reset();
    LocalGeneration.Cancel();
    ++Generation;
    if (Cancelled)
        Cancelled->Store(true);
    if (bBusy)
        Status = TEXT("取得を中止しています…");
    Result.Reset();
}
void FProductResearch::Start()
{
    GenerationVariantId.Reset();
    if (IsBusy())
        return;
    LocalGeneration.Cancel();
    Result.Reset();
    FString Url, Error;
    if (!Yohaku::ValidateProductUrl(Input.TrimStartAndEnd(), Url, Error))
    {
        Status = Error;
        return;
    }
    if (FPlatformTime::Seconds() - LastStart < 1)
    {
        Status = TEXT("少し待ってから取得してください。");
        return;
    }
    LastStart = FPlatformTime::Seconds();
    Input = Url;
    const uint64 Id = ++Generation;
    bBusy = true;
    Status = TEXT("商品ページを取得中… 部屋の編集は続けられます");
    Cancelled = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
    const auto Token = Cancelled.ToSharedRef();
    const TWeakPtr<FProductResearch, ESPMode::ThreadSafe> Weak = AsShared();
    Async(EAsyncExecution::ThreadPool, [Weak, Token, Id, Url]() {
        Yohaku::FProductSource P;
        FString Error;
        TOptional<Yohaku::FProductSource> Product;
        if (Fetch(Url, Token, P, Error))
            Product = MoveTemp(P);
        AsyncTask(ENamedThreads::GameThread, [Weak, Id, Url, Product = MoveTemp(Product), Error]() mutable {
            if (const auto Self = Weak.Pin())
                Self->Complete(Id, Url, MoveTemp(Product), Error);
        });
    });
}
void FProductResearch::Complete(uint64 Id, const FString &Url, TOptional<Yohaku::FProductSource> Product,
                                const FString &Error)
{
    bBusy = false;
    Cancelled.Reset();
    if (Id != Generation || Url != Input)
    {
        Status = TEXT("取得を中止しました");
        return;
    }
    Result = MoveTemp(Product);
    Status = Result.IsSet() ? TEXT("掲載情報を取得しました（未照合）") : Error;
    if (OnFinished)
        OnFinished();
}
FString FProductResearch::Summary() const
{
    if (!Result.IsSet())
        return TEXT("");
    const auto &P = Result.GetValue();
    FString Text = FString::Printf(TEXT("%s\n\n掲載外寸  幅 %.1f × 奥行 %.1f × 高さ %.1f "
                                        "mm\n寸法は単一の掲載元から取得・実物や公式資料との照合前\n\n色・仕様候補\n"),
                                   *P.Name, P.WidthMm, P.DepthMm, P.HeightMm);
    for (const auto &V : P.Variants)
        Text += FString::Printf(TEXT("・%s（仕様ID %s）\n%s\n"), *V.Name, *V.Id,
                                V.ImageUrl.IsEmpty() ? TEXT("対応写真を特定できません") : *V.ImageUrl);
    if (!P.AlternateConfiguration.IsEmpty())
        Text += TEXT("\n別仕様の記載：") + P.AlternateConfiguration + TEXT("\n");
    Text += TEXT("\n生成試作・品質確認・正式採用は別に管理します\nURLで指定された仕様との対応は未確認\n"
                 "既存のPSF試作モデル・部屋の保存データは変更しません\n\n出典：") +
            P.SourceUrl + TEXT("\n取得日時（UTC）：") + P.RetrievedAt + TEXT("\n掲載内容SHA256：") + P.ContentHash;
    return Text;
}

void FProductResearch::Generate(const FString &Variant)
{
    if (!CanSaveCandidate())
        return;
    FGuid Id;
    bool Existing = false;
    FString Error;
    if (!Candidates.Save(Result.GetValue(), Id, Existing, Error) ||
        !LocalGeneration.Start(Result.GetValue(), Id, Variant, Error))
        CandidateStatus = Error;
}
