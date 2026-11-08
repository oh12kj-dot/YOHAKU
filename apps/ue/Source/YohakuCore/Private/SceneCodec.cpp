#include "SceneDocument.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
namespace Yohaku
{
namespace
{
using Object = TSharedPtr<FJsonObject>;
bool Keys(const Object &O, std::initializer_list<const TCHAR *> Names)
{
    if (!O || O->Values.Num() != int32(Names.size()))
        return false;
    for (auto K : Names)
        if (!O->HasField(K))
            return false;
    return true;
}
FString Guid(FGuid Id)
{
    return Id.IsValid() ? Id.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("");
}
bool Id(const Object &O, const TCHAR *Key, FGuid &Out, bool Optional = false)
{
    FString V;
    if (!O->TryGetStringField(Key, V))
        return false;
    if (Optional && V.IsEmpty())
    {
        Out.Invalidate();
        return true;
    }
    return FGuid::ParseExact(V, EGuidFormats::DigitsWithHyphens, Out) && Out.IsValid();
}
bool Integer(const Object &O, const TCHAR *Key, int32 &Out, int32 Max)
{
    double V;
    if (!O->TryGetNumberField(Key, V) || !FMath::IsFinite(V) || V < 0 || V > Max || V != FMath::FloorToDouble(V))
        return false;
    Out = int32(V);
    return true;
}
bool Array(const Object &O, const TCHAR *Key, const TArray<TSharedPtr<FJsonValue>> *&Out, int32 Max)
{
    return O->TryGetArrayField(Key, Out) && Out->Num() <= Max;
}
bool Row(const TSharedPtr<FJsonValue> &V, Object &Out)
{
    if (!V || V->Type != EJson::Object)
        return false;
    Out = V->AsObject();
    return Out.IsValid();
}
TSharedRef<FJsonValue> Value(const Object &O)
{
    return MakeShared<FJsonValueObject>(O);
}
bool Resolve(const FAssetResolver &Resolver, const FString &Id, const FString &Hash, FAsset &A, FString &E)
{
    return Resolver ? Resolver(Id, Hash, A, E) : BuiltinAsset(Id, Hash, A, E);
}
bool Legacy(const Object &Root, FScene &S, FString &E, const FAssetResolver &Resolver)
{
    if (!Keys(Root, {TEXT("format"), TEXT("version"), TEXT("room"), TEXT("finishes"), TEXT("furniture")}))
        return false;
    const Object *Room;
    double W, D, H;
    if (!Root->TryGetObjectField(TEXT("room"), Room) ||
        !Keys(*Room, {TEXT("widthMm"), TEXT("depthMm"), TEXT("heightMm")}) ||
        !(*Room)->TryGetNumberField(TEXT("widthMm"), W) || !(*Room)->TryGetNumberField(TEXT("depthMm"), D) ||
        !(*Room)->TryGetNumberField(TEXT("heightMm"), H))
        return false;
    const TArray<TSharedPtr<FJsonValue>> *F, *Items;
    if (!Array(Root, TEXT("finishes"), F, 5) || F->Num() != 5 || !Array(Root, TEXT("furniture"), Items, 200))
        return false;
    auto &R = S.Rooms[0];
    R.Polygon = {{0, 0}, {W, 0}, {W, D}, {0, D}};
    R.Height = H;
    int32 Map[] = {-1, 2, 0, 1, 3};
    for (int32 I = 0; I < 5; ++I)
    {
        double V;
        if (!(*F)[I]->TryGetNumber(V) || !FMath::IsFinite(V) || V < 0 || V > (I == 0 ? 3 : 4) ||
            V != FMath::FloorToDouble(V))
            return false;
        if (!I)
            R.FloorFinish = int32(V);
        else
            R.Walls[Map[I]].Finish = int32(V);
    }
    for (const auto &V : *Items)
    {
        Object O;
        FItem I;
        if (!Row(V, O) || !Keys(O, {TEXT("id"), TEXT("assetVersionId"), TEXT("xMm"), TEXT("zMm"), TEXT("yawRad")}) ||
            !Id(O, TEXT("id"), I.Id) || !O->TryGetStringField(TEXT("assetVersionId"), I.AssetId) ||
            !O->TryGetNumberField(TEXT("xMm"), I.X) || !O->TryGetNumberField(TEXT("zMm"), I.Z) ||
            !O->TryGetNumberField(TEXT("yawRad"), I.Yaw) || !S.Asset(I.AssetId))
            return false;
        S.Items.Add(I);
    }
    for (auto &A : S.Assets)
    {
        FAsset Loaded;
        if (!Resolve(Resolver, A.Id, A.ManifestHash, Loaded, E))
            return false;
        A = MoveTemp(Loaded);
    }
    return true;
}
} // namespace
FString Encode(const FScene &S)
{
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("format"), TEXT("yohaku-local-study"));
    O->SetNumberField(TEXT("version"), 2);
    TArray<TSharedPtr<FJsonValue>> Rooms, Openings, Assets, Items;
    for (const auto &R : S.Rooms)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("id"), Guid(R.Id));
        Row->SetStringField(TEXT("name"), R.Name);
        Row->SetNumberField(TEXT("heightMm"), R.Height);
        Row->SetNumberField(TEXT("floorFinish"), R.FloorFinish);
        TArray<TSharedPtr<FJsonValue>> Points, WallRows;
        for (auto P : R.Polygon)
        {
            TArray<TSharedPtr<FJsonValue>> Pair{MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y)};
            Points.Add(MakeShared<FJsonValueArray>(Pair));
        }
        for (auto W : R.Walls)
        {
            auto Wall = MakeShared<FJsonObject>();
            Wall->SetStringField(TEXT("id"), Guid(W.Id));
            Wall->SetNumberField(TEXT("finish"), W.Finish);
            WallRows.Add(Value(Wall));
        }
        Row->SetArrayField(TEXT("polygon"), Points);
        Row->SetArrayField(TEXT("walls"), WallRows);
        Rooms.Add(Value(Row));
    }
    for (const auto &P : S.Openings)
    {
        auto R = MakeShared<FJsonObject>();
        R->SetStringField(TEXT("id"), Guid(P.Id));
        R->SetStringField(TEXT("wallId"), Guid(P.WallId));
        R->SetStringField(TEXT("otherWallId"), Guid(P.OtherWallId));
        R->SetStringField(TEXT("kind"), P.Kind == EOpeningKind::Door ? TEXT("door") : TEXT("window"));
        R->SetNumberField(TEXT("offsetMm"), P.Offset);
        R->SetNumberField(TEXT("widthMm"), P.Width);
        R->SetNumberField(TEXT("heightMm"), P.Height);
        R->SetNumberField(TEXT("sillMm"), P.Sill);
        R->SetBoolField(TEXT("hingeAtEnd"), P.HingeAtEnd);
        R->SetBoolField(TEXT("swingInward"), P.SwingInward);
        Openings.Add(Value(R));
    }
    for (const auto &A : S.Assets)
    {
        auto R = MakeShared<FJsonObject>();
        R->SetStringField(TEXT("id"), A.Id);
        R->SetStringField(TEXT("manifestHash"), A.ManifestHash);
        Assets.Add(Value(R));
    }
    for (const auto &I : S.Items)
    {
        auto R = MakeShared<FJsonObject>();
        R->SetStringField(TEXT("id"), Guid(I.Id));
        R->SetStringField(TEXT("roomId"), Guid(I.RoomId));
        R->SetStringField(TEXT("assetVersionId"), I.AssetId);
        R->SetNumberField(TEXT("xMm"), I.X);
        R->SetNumberField(TEXT("zMm"), I.Z);
        R->SetNumberField(TEXT("yawRad"), I.Yaw);
        Items.Add(Value(R));
    }
    O->SetArrayField(TEXT("rooms"), Rooms);
    O->SetArrayField(TEXT("openings"), Openings);
    O->SetArrayField(TEXT("assets"), Assets);
    O->SetArrayField(TEXT("furniture"), Items);
    FString Json;
    FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Json));
    return Json;
}
bool Decode(const FString &Json, FScene &Out, FString &E, const FAssetResolver &Resolver)
{
    E = TEXT("保存形式・寸法・家具版が不正です。現在のシーンを保持しました");
    if (Json.Len() > 4 * 1024 * 1024)
        return false;
    Object O;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O)
        return false;
    FString Format;
    double Version;
    if (!O->TryGetStringField(TEXT("format"), Format) || Format != TEXT("yohaku-local-study") ||
        !O->TryGetNumberField(TEXT("version"), Version))
        return false;
    FScene S;
    if (Version == 1)
    {
        if (!Legacy(O, S, E, Resolver))
            return false;
    }
    else if (Version == 2)
    {
        if (!Keys(O, {TEXT("format"), TEXT("version"), TEXT("rooms"), TEXT("openings"), TEXT("assets"),
                      TEXT("furniture")}))
            return false;
        const TArray<TSharedPtr<FJsonValue>> *Rooms, *Openings, *Assets, *Items;
        if (!Array(O, TEXT("rooms"), Rooms, 16) || !Array(O, TEXT("openings"), Openings, 128) ||
            !Array(O, TEXT("assets"), Assets, 100) || !Array(O, TEXT("furniture"), Items, 200))
            return false;
        S.Rooms.Reset();
        S.Assets.Reset();
        for (const auto &V : *Rooms)
        {
            Object R;
            FRoom Room;
            const TArray<TSharedPtr<FJsonValue>> *Points, *WallRows;
            if (!Row(V, R) ||
                !Keys(R, {TEXT("id"), TEXT("name"), TEXT("heightMm"), TEXT("floorFinish"), TEXT("polygon"),
                          TEXT("walls")}) ||
                !Id(R, TEXT("id"), Room.Id) || !R->TryGetStringField(TEXT("name"), Room.Name) ||
                !R->TryGetNumberField(TEXT("heightMm"), Room.Height) ||
                !Integer(R, TEXT("floorFinish"), Room.FloorFinish, 3) || !Array(R, TEXT("polygon"), Points, 32) ||
                !Array(R, TEXT("walls"), WallRows, 32))
                return false;
            for (const auto &P : *Points)
            {
                const TArray<TSharedPtr<FJsonValue>> *Pair;
                FVector2D Point;
                if (!P->TryGetArray(Pair) || Pair->Num() != 2 || !(*Pair)[0]->TryGetNumber(Point.X) ||
                    !(*Pair)[1]->TryGetNumber(Point.Y))
                    return false;
                Room.Polygon.Add(Point);
            }
            for (const auto &W : *WallRows)
            {
                Object Wall;
                FWall Item;
                if (!Row(W, Wall) || !Keys(Wall, {TEXT("id"), TEXT("finish")}) || !Id(Wall, TEXT("id"), Item.Id) ||
                    !Integer(Wall, TEXT("finish"), Item.Finish, 4))
                    return false;
                Room.Walls.Add(Item);
            }
            S.Rooms.Add(Room);
        }
        for (const auto &V : *Openings)
        {
            Object R;
            FOpening P;
            FString Kind;
            if (!Row(V, R) ||
                !Keys(R,
                      {TEXT("id"), TEXT("wallId"), TEXT("otherWallId"), TEXT("kind"), TEXT("offsetMm"), TEXT("widthMm"),
                       TEXT("heightMm"), TEXT("sillMm"), TEXT("hingeAtEnd"), TEXT("swingInward")}) ||
                !Id(R, TEXT("id"), P.Id) || !Id(R, TEXT("wallId"), P.WallId) ||
                !Id(R, TEXT("otherWallId"), P.OtherWallId, true) || !R->TryGetStringField(TEXT("kind"), Kind) ||
                (Kind != TEXT("door") && Kind != TEXT("window")) || !R->TryGetNumberField(TEXT("offsetMm"), P.Offset) ||
                !R->TryGetNumberField(TEXT("widthMm"), P.Width) || !R->TryGetNumberField(TEXT("heightMm"), P.Height) ||
                !R->TryGetNumberField(TEXT("sillMm"), P.Sill) ||
                !R->TryGetBoolField(TEXT("hingeAtEnd"), P.HingeAtEnd) ||
                !R->TryGetBoolField(TEXT("swingInward"), P.SwingInward))
                return false;
            P.Kind = Kind == TEXT("door") ? EOpeningKind::Door : EOpeningKind::Window;
            S.Openings.Add(P);
        }
        for (const auto &V : *Assets)
        {
            Object R;
            FString Id, Hash;
            FAsset A;
            if (!Row(V, R) || !Keys(R, {TEXT("id"), TEXT("manifestHash")}) || !R->TryGetStringField(TEXT("id"), Id) ||
                !R->TryGetStringField(TEXT("manifestHash"), Hash) || !Resolve(Resolver, Id, Hash, A, E) || A.Id != Id ||
                A.ManifestHash != Hash)
                return false;
            S.Assets.Add(A);
        }
        for (const auto &V : *Items)
        {
            Object R;
            FItem I;
            if (!Row(V, R) ||
                !Keys(R,
                      {TEXT("id"), TEXT("roomId"), TEXT("assetVersionId"), TEXT("xMm"), TEXT("zMm"), TEXT("yawRad")}) ||
                !Id(R, TEXT("id"), I.Id) || !Id(R, TEXT("roomId"), I.RoomId) ||
                !R->TryGetStringField(TEXT("assetVersionId"), I.AssetId) || !R->TryGetNumberField(TEXT("xMm"), I.X) ||
                !R->TryGetNumberField(TEXT("zMm"), I.Z) || !R->TryGetNumberField(TEXT("yawRad"), I.Yaw))
                return false;
            S.Items.Add(I);
        }
    }
    else
        return false;
    if (!Validate(S, E))
        return false;
    for (auto &R : S.Rooms)
    {
        R.Height = Normalize(R.Height);
        for (auto &P : R.Polygon)
        {
            P.X = Normalize(P.X);
            P.Y = Normalize(P.Y);
        }
    }
    for (auto &I : S.Items)
    {
        I.X = Normalize(I.X);
        I.Z = Normalize(I.Z);
        I.Yaw = FMath::Fmod(I.Yaw, UE_DOUBLE_TWO_PI);
        if (I.Yaw > UE_DOUBLE_PI)
            I.Yaw -= UE_DOUBLE_TWO_PI;
        if (I.Yaw < -UE_DOUBLE_PI)
            I.Yaw += UE_DOUBLE_TWO_PI;
    }
    if (!Validate(S, E))
        return false;
    Out = MoveTemp(S);
    E.Reset();
    return true;
}
} // namespace Yohaku
