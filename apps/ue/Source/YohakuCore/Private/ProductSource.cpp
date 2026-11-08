#include "ProductSource.h"
#include "Internationalization/Regex.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include <openssl/sha.h>

namespace Yohaku
{
FString Sha256(const TArray<uint8>& Bytes)
{
    uint8 Hash[SHA256_DIGEST_LENGTH];
    if (!SHA256(Bytes.GetData(), Bytes.Num(), Hash)) return TEXT("");
    return BytesToHex(Hash, sizeof(Hash)).ToLower();
}
namespace
{
bool Match(const FString &Value, const TCHAR *Pattern)
{
    FRegexMatcher M(FRegexPattern(Pattern), Value);
    return M.FindNext() && M.GetMatchBeginning() == 0 && M.GetMatchEnding() == Value.Len();
}
FString Decode(FString Value)
{
    Value.ReplaceInline(TEXT("&nbsp;"), TEXT(" "));
    Value.ReplaceInline(TEXT("&quot;"), TEXT("\""));
    Value.ReplaceInline(TEXT("&#39;"), TEXT("'"));
    Value.ReplaceInline(TEXT("&apos;"), TEXT("'"));
    Value.ReplaceInline(TEXT("&lt;"), TEXT("<"));
    Value.ReplaceInline(TEXT("&gt;"), TEXT(">"));
    Value.ReplaceInline(TEXT("&amp;"), TEXT("&"));
    return Value.TrimStartAndEnd();
}
struct FNode
{
    FString Tag, Text;
    TMap<FString, FString> Attr;
    TArray<int32> Children;
    bool Has(const TCHAR *Class) const
    {
        TArray<FString> Names;
        Attr.FindRef(TEXT("class")).ParseIntoArrayWS(Names);
        return Names.Contains(Class);
    }
};
// Bounded local tokenizer. No script execution, subresource loads, or HTML rendering.
bool Tokenize(const FString &Html, TArray<FNode> &Nodes)
{
    Nodes.AddDefaulted();
    TArray<int32> Stack{0};
    int32 P = 0;
    while (P < Html.Len())
    {
        if (Html[P] != '<')
        {
            const int32 Begin = P;
            while (P < Html.Len() && Html[P] != '<')
                ++P;
            Nodes[Stack.Last()].Text += Html.Mid(Begin, P - Begin);
            continue;
        }
        if (Html.Mid(P, 4) == TEXT("<!--"))
        {
            const int32 End = Html.Find(TEXT("-->"), ESearchCase::CaseSensitive, ESearchDir::FromStart, P + 4);
            if (End == INDEX_NONE)
                return false;
            P = End + 3;
            continue;
        }
        ++P;
        bool Closing = P < Html.Len() && Html[P] == '/';
        if (Closing)
            ++P;
        const int32 Begin = P;
        while (P < Html.Len() && (FChar::IsAlnum(Html[P]) || Html[P] == '-' || Html[P] == ':'))
            ++P;
        const FString Tag = Html.Mid(Begin, P - Begin).ToLower();
        if (Tag.IsEmpty())
        {
            const int32 End = Html.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, P);
            if (End == INDEX_NONE)
                return false;
            P = End + 1;
            continue;
        }
        FNode Node;
        Node.Tag = Tag;
        bool SelfClosing = false;
        while (P < Html.Len() && Html[P] != '>')
        {
            if (FChar::IsWhitespace(Html[P]))
            {
                ++P;
                continue;
            }
            if (Html[P] == '/')
            {
                SelfClosing = true;
                ++P;
                continue;
            }
            const int32 KeyStart = P;
            while (P < Html.Len() && !FChar::IsWhitespace(Html[P]) && Html[P] != '=' && Html[P] != '>' &&
                   Html[P] != '/')
                ++P;
            if (KeyStart == P)
                return false;
            const FString Key = Html.Mid(KeyStart, P - KeyStart).ToLower();
            while (P < Html.Len() && FChar::IsWhitespace(Html[P]))
                ++P;
            FString Value;
            if (P < Html.Len() && Html[P] == '=')
            {
                ++P;
                while (P < Html.Len() && FChar::IsWhitespace(Html[P]))
                    ++P;
                const TCHAR Quote = P < Html.Len() && (Html[P] == '\'' || Html[P] == '"') ? Html[P++] : 0;
                const int32 ValueStart = P;
                while (P < Html.Len() && (Quote ? Html[P] != Quote : !FChar::IsWhitespace(Html[P]) && Html[P] != '>'))
                    ++P;
                Value = Decode(Html.Mid(ValueStart, P - ValueStart));
                if (Quote)
                {
                    if (P == Html.Len())
                        return false;
                    ++P;
                }
            }
            Node.Attr.Add(Key, Value);
        }
        if (P == Html.Len())
            return false;
        ++P;
        if (Closing)
        {
            for (int32 I = Stack.Num() - 1; I > 0; --I)
                if (Nodes[Stack[I]].Tag == Tag)
                {
                    Stack.SetNum(I);
                    break;
                }
            continue;
        }
        if (Tag == TEXT("script") || Tag == TEXT("style"))
        {
            const int32 End = Html.Find(TEXT("</") + Tag, ESearchCase::IgnoreCase, ESearchDir::FromStart, P);
            if (End == INDEX_NONE)
                return false;
            P = End;
            continue;
        }
        const int32 Index = Nodes.Add(MoveTemp(Node));
        Nodes[Stack.Last()].Children.Add(Index);
        if (Nodes.Num() > 25000 || Stack.Num() > 128)
            return false;
        if (!SelfClosing &&
            !Match(Tag, TEXT("^(area|base|br|col|embed|hr|img|input|link|meta|param|source|track|wbr)$")))
            Stack.Add(Index);
    }
    return true;
}
FString Text(const TArray<FNode> &Nodes, int32 Index)
{
    FString Result = Nodes[Index].Text;
    for (const int32 Child : Nodes[Index].Children)
        Result += TEXT(" ") + Text(Nodes, Child);
    return Decode(Result);
}
FString ChildText(const TArray<FNode> &Nodes, int32 Index, const TCHAR *Class)
{
    for (int32 Child : Nodes[Index].Children)
    {
        if (Nodes[Child].Has(Class))
            return Text(Nodes, Child);
        const FString Found = ChildText(Nodes, Child, Class);
        if (!Found.IsEmpty())
            return Found;
    }
    return TEXT("");
}
} // namespace
bool ValidateProductUrl(const FString &Input, FString &Canonical, FString &Error)
{
    Canonical.Reset();
    if (Input.Len() <= 2048 && Match(Input, TEXT("^https://flymee\\.jp/product/[0-9]+/(\\?pi_id=[0-9]+)?$")))
    {
        Canonical = Input;
        return true;
    }
    Error = TEXT("FLYMEeの商品URLを入力してください。https://flymee.jp/product/番号/（pi_idのみ対応）");
    return false;
}
bool ParseProduct(const TArray<uint8> &Bytes, const FString &Url, FProductSource &Out, FString &Error)
{
    FProductSource Result;
    if (!ValidateProductUrl(Url, Result.SourceUrl, Error))
        return false;
    auto Fail = [&Error]() {
        Error = TEXT("掲載情報を特定できません。仕様別の追加調査が必要です。");
        return false;
    };
    if (Bytes.IsEmpty() || Bytes.Num() > ProductBodyLimit || Bytes.Contains(0))
        return Fail();
    uint8 Hash[SHA256_DIGEST_LENGTH];
    if (!SHA256(Bytes.GetData(), Bytes.Num(), Hash))
        return Fail();
    Result.ContentHash = BytesToHex(Hash, sizeof(Hash)).ToLower();
    const FUTF8ToTCHAR Utf8(reinterpret_cast<const ANSICHAR *>(Bytes.GetData()), Bytes.Num());
    const FString Html(Utf8.Length(), Utf8.Get());
    const FTCHARToUTF8 RoundTrip(*Html, Html.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0)
        return Fail();
    TArray<FNode> Nodes;
    if (!Tokenize(Html, Nodes))
        return Fail();
    TMap<FString, double> Dimensions;
    TMap<FString, FString> Photos;
    TSet<FString> VariantIds;
    for (int32 I = 1; I < Nodes.Num(); ++I)
    {
        const FNode &N = Nodes[I];
        if (N.Tag == TEXT("title") && Result.Name.IsEmpty())
        {
            FString Title = Text(Nodes, I), Rest;
            Title.Split(TEXT(" - インテリア"), &Title, &Rest);
            Title.Split(TEXT(" / "), &Title, &Rest);
            Result.Name = Title.TrimStartAndEnd();
            Result.Brand = Result.Name;
            Result.Name.Split(TEXT(" PSF"), &Result.Brand, &Rest);
        }
        if (N.Has(TEXT("spec_area_list")) && N.Has(TEXT("_size")))
        {
            const FString Label = ChildText(Nodes, I, TEXT("title"));
            if (Label != TEXT("幅") && Label != TEXT("奥行") && Label != TEXT("高さ"))
                continue;
            const FString Value = ChildText(Nodes, I, TEXT("text"));
            FRegexMatcher M(FRegexPattern(TEXT("^([0-9]+(?:\\.[0-9]+)?)\\s*(cm|mm)$")), Value);
            if (!M.FindNext())
                return Fail();
            const double Mm = FCString::Atod(*M.GetCaptureGroup(1)) * (M.GetCaptureGroup(2) == TEXT("cm") ? 10 : 1);
            if (!FMath::IsFinite(Mm) || Mm <= 0 || Mm > 30000 ||
                (Dimensions.Contains(Label) && Dimensions[Label] != Mm))
                return Fail();
            Dimensions.Add(Label, Mm);
        }
        if (N.Tag == TEXT("dd") && N.Has(TEXT("separate")) && Result.AlternateConfiguration.IsEmpty())
            Result.AlternateConfiguration = Text(Nodes, I);
        if (N.Has(TEXT("js-specHandler")))
        {
            FProductVariant V;
            V.Id = N.Attr.FindRef(TEXT("data-spec-item-id"));
            V.Name = ChildText(Nodes, I, TEXT("js-specItemName"));
            if (!Match(V.Id, TEXT("^[0-9]+$")) || V.Name.IsEmpty() || V.Name.Len() > 200 || VariantIds.Contains(V.Id))
                return Fail();
            VariantIds.Add(V.Id);
            Result.Variants.Add(V);
        }
        if (N.Tag == TEXT("img"))
        {
            const FString Caption = N.Attr.FindRef(TEXT("data-caption"));
            const FString Src = N.Attr.FindRef(TEXT("src"));
            if (Caption.StartsWith(TEXT("張地：")) && !Src.Contains(TEXT("..")) && !Photos.Contains(Caption.Mid(3)) &&
                Match(Src, TEXT("^https://static2\\.flymee\\.jp/product_images/[A-Za-z0-9_./-]+$")))
                Photos.FindOrAdd(Caption.Mid(3)) = Src;
        }
    }
    if (Dimensions.Num() != 3 || Result.Name.IsEmpty() || Result.Name.Len() > 500 || Result.Variants.IsEmpty() ||
        Result.Variants.Num() > 100)
        return Fail();
    Result.WidthMm = Dimensions[TEXT("幅")];
    Result.DepthMm = Dimensions[TEXT("奥行")];
    Result.HeightMm = Dimensions[TEXT("高さ")];
    Result.ProductId = Url.Mid(26);
    Result.ProductId = Result.ProductId.Left(Result.ProductId.Find(TEXT("/")));
    for (auto &V : Result.Variants)
        V.ImageUrl = Photos.FindRef(V.Name);
    Result.RetrievedAt = FDateTime::UtcNow().ToIso8601();
    Out = MoveTemp(Result);
    Error.Reset();
    return true;
}
TSharedRef<FJsonObject> ProductToJson(const FProductSource &P)
{
    const auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("productId"), P.ProductId);
    O->SetStringField(TEXT("name"), P.Name);
    O->SetStringField(TEXT("brand"), P.Brand);
    O->SetStringField(TEXT("sourceUrl"), P.SourceUrl);
    O->SetStringField(TEXT("retrievedAt"), P.RetrievedAt);
    O->SetStringField(TEXT("contentHash"), P.ContentHash);
    O->SetStringField(TEXT("dimensionStatus"), TEXT("single-source-unverified"));
    O->SetStringField(TEXT("modelStatus"), TEXT("not-generated"));
    O->SetStringField(TEXT("alternateConfiguration"), P.AlternateConfiguration);
    const auto D = MakeShared<FJsonObject>();
    D->SetNumberField(TEXT("widthMm"), P.WidthMm);
    D->SetNumberField(TEXT("depthMm"), P.DepthMm);
    D->SetNumberField(TEXT("heightMm"), P.HeightMm);
    O->SetObjectField(TEXT("dimensions"), D);
    TArray<TSharedPtr<FJsonValue>> Variants;
    for (const auto &V : P.Variants)
    {
        const auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("id"), V.Id);
        Row->SetStringField(TEXT("name"), V.Name);
        if (V.ImageUrl.IsEmpty())
            Row->SetField(TEXT("imageUrl"), MakeShared<FJsonValueNull>());
        else
            Row->SetStringField(TEXT("imageUrl"), V.ImageUrl);
        Variants.Add(MakeShared<FJsonValueObject>(Row));
    }
    O->SetArrayField(TEXT("variants"), Variants);
    return O;
}
FString EncodeProduct(const FProductSource &P)
{
    FString Json;
    FJsonSerializer::Serialize(ProductToJson(P), TJsonWriterFactory<>::Create(&Json));
    return Json;
}
bool DecodeProduct(const TSharedPtr<FJsonObject> &O, FProductSource &Out)
{
    if (!O || O->Values.Num() != 11)
        return false;
    FProductSource P;
    FString DimensionStatus, ModelStatus, Canonical, Error;
    if (!O->TryGetStringField(TEXT("productId"), P.ProductId) || !O->TryGetStringField(TEXT("name"), P.Name) ||
        !O->TryGetStringField(TEXT("brand"), P.Brand) || !O->TryGetStringField(TEXT("sourceUrl"), P.SourceUrl) ||
        !O->TryGetStringField(TEXT("retrievedAt"), P.RetrievedAt) ||
        !O->TryGetStringField(TEXT("contentHash"), P.ContentHash) ||
        !O->TryGetStringField(TEXT("alternateConfiguration"), P.AlternateConfiguration) ||
        !O->TryGetStringField(TEXT("dimensionStatus"), DimensionStatus) ||
        !O->TryGetStringField(TEXT("modelStatus"), ModelStatus))
        return false;
    FDateTime Time;
    if (P.Name.TrimStartAndEnd().IsEmpty() || P.Name.Len() > 500 || P.Brand.Len() > 200 ||
        P.AlternateConfiguration.Len() > 4000 || !Match(P.ContentHash, TEXT("^[0-9a-f]{64}$")) ||
        !P.RetrievedAt.EndsWith(TEXT("Z")) || !FDateTime::ParseIso8601(*P.RetrievedAt, Time) ||
        DimensionStatus != TEXT("single-source-unverified") || ModelStatus != TEXT("not-generated") ||
        !ValidateProductUrl(P.SourceUrl, Canonical, Error) || Canonical != P.SourceUrl ||
        !P.SourceUrl.StartsWith(TEXT("https://flymee.jp/product/") + P.ProductId + TEXT("/")) ||
        !Match(P.ProductId, TEXT("^[0-9]+$")))
        return false;
    const TSharedPtr<FJsonObject> *D;
    const TArray<TSharedPtr<FJsonValue>> *Rows;
    if (!O->TryGetObjectField(TEXT("dimensions"), D) || (*D)->Values.Num() != 3 ||
        !(*D)->TryGetNumberField(TEXT("widthMm"), P.WidthMm) || !(*D)->TryGetNumberField(TEXT("depthMm"), P.DepthMm) ||
        !(*D)->TryGetNumberField(TEXT("heightMm"), P.HeightMm) || !O->TryGetArrayField(TEXT("variants"), Rows) ||
        Rows->Num() < 1 || Rows->Num() > 100)
        return false;
    for (double V : {P.WidthMm, P.DepthMm, P.HeightMm})
        if (!FMath::IsFinite(V) || V <= 0 || V > 30000)
            return false;
    TSet<FString> Ids;
    for (const auto &Row : *Rows)
    {
        const TSharedPtr<FJsonObject> *V = nullptr;
        FProductVariant Item;
        if (!Row || !Row->TryGetObject(V) || (*V)->Values.Num() != 3 || !(*V)->TryGetStringField(TEXT("id"), Item.Id) ||
            !(*V)->TryGetStringField(TEXT("name"), Item.Name) || !Match(Item.Id, TEXT("^[0-9]+$")) ||
            Ids.Contains(Item.Id) || Item.Name.TrimStartAndEnd().IsEmpty() || Item.Name.Len() > 200)
            return false;
        const auto Image = (*V)->TryGetField(TEXT("imageUrl"));
        if (!Image ||
            (!Image->IsNull() &&
             (!Image->TryGetString(Item.ImageUrl) || Item.ImageUrl.Contains(TEXT("..")) ||
              !Match(Item.ImageUrl, TEXT("^https://static2\\.flymee\\.jp/product_images/[A-Za-z0-9_./-]+$")))))
            return false;
        Ids.Add(Item.Id);
        P.Variants.Add(MoveTemp(Item));
    }
    Out = MoveTemp(P);
    return true;
}
} // namespace Yohaku
