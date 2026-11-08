#include "SceneDocument.h"
namespace Yohaku
{
bool ValidateGeometry(const FScene &, FString &);
bool SplitSharedWalls(FScene &, FString &);
double Normalize(double V)
{
    return FMath::RoundToDouble(V * 1000.0) / 1000.0;
}
static double Angle(double V)
{
    double A = FMath::Fmod(V, UE_DOUBLE_TWO_PI);
    if (A > UE_DOUBLE_PI)
        A -= UE_DOUBLE_TWO_PI;
    if (A < -UE_DOUBLE_PI)
        A += UE_DOUBLE_TWO_PI;
    return A;
}
static bool Range(double V, double L, double H)
{
    return FMath::IsFinite(V) && V >= L && V <= H;
}
FString VariantId(EVariant V)
{
    return V == EVariant::Brown ? TEXT("study-psf-v3-brown") : TEXT("study-psf-v3-blue-gray");
}
bool BuiltinAsset(const FString &Id, const FString &Hash, FAsset &A, FString &Error)
{
    if ((Id != VariantId(EVariant::Brown) && Id != VariantId(EVariant::BlueGray)) ||
        (!Hash.IsEmpty() && Hash != TEXT("builtin-v3")))
    {
        Error = TEXT("未対応または内容が変わった家具版です");
        return false;
    }
    A = FAsset();
    A.Id = Id;
    A.ManifestHash = TEXT("builtin-v3");
    A.Name = TEXT("PSF SOFA 2P");
    A.SourceUrl = TEXT("https://flymee.jp/product/106294/");
    A.DimensionStatus = TEXT("single-source-unverified");
    A.MaterialStatus = TEXT("photo-referenced-study");
    A.Parts = {{{0, 0, 185}, {1500, 870, 130}},     {{0, -45, 330}, {1230, 700, 164}},
               {{-675, 0, 400}, {150, 870, 530}},   {{675, 0, 400}, {150, 870, 530}},
               {{-320, 285, 610}, {630, 250, 420}}, {{320, 285, 610}, {630, 250, 420}},
               {{0, 0, 65}, {1340, 730, 130}}};
    return true;
}
FRoom FRoom::Rectangle(double X, double Z, double W, double D, const FString &N)
{
    FRoom R;
    R.Id = FGuid::NewGuid();
    R.Name = N;
    R.Polygon = {{X, Z}, {X + W, Z}, {X + W, Z + D}, {X, Z + D}};
    R.Walls.SetNum(4);
    return R;
}
FVector2D FRoom::Min() const
{
    FVector2D P(DBL_MAX, DBL_MAX);
    for (auto V : Polygon)
    {
        P.X = FMath::Min(P.X, V.X);
        P.Y = FMath::Min(P.Y, V.Y);
    }
    return P;
}
FVector2D FRoom::Max() const
{
    FVector2D P(-DBL_MAX, -DBL_MAX);
    for (auto V : Polygon)
    {
        P.X = FMath::Max(P.X, V.X);
        P.Y = FMath::Max(P.Y, V.Y);
    }
    return P;
}
FScene::FScene()
{
    FRoom R = FRoom::Rectangle(0, 0, 5000, 4000, TEXT("部屋1"));
    R.Id = FGuid(0, 0, 0, 1);
    R.Walls[0].Id = FGuid(0, 0, 0, 3);
    R.Walls[1].Id = FGuid(0, 0, 0, 4);
    R.Walls[2].Id = FGuid(0, 0, 0, 2);
    R.Walls[3].Id = FGuid(0, 0, 0, 5);
    Rooms.Add(R);
    for (auto V : {EVariant::Brown, EVariant::BlueGray})
    {
        FAsset A;
        FString E;
        BuiltinAsset(VariantId(V), TEXT(""), A, E);
        Assets.Add(A);
    }
}
const FRoom *FScene::Room(const FGuid &Id) const
{
    return Rooms.FindByPredicate([&](const FRoom &R) { return R.Id == Id; });
}
const FAsset *FScene::Asset(const FString &Id) const
{
    return Assets.FindByPredicate([&](const FAsset &A) { return A.Id == Id; });
}
bool Validate(const FScene &S, FString &Error)
{
    Error = TEXT("部屋・家具・開口の値または参照が不正です");
    if (S.Assets.Num() < 2 || S.Assets.Num() > 100 || S.Items.Num() > 200)
        return false;
    TSet<FString> AssetIds;
    for (const auto &A : S.Assets)
    {
        if (A.Id.IsEmpty() || A.Id.Len() > 100 || AssetIds.Contains(A.Id) || A.ManifestHash.IsEmpty() ||
            A.Parts.IsEmpty() || A.Parts.Num() > 2048)
            return false;
        AssetIds.Add(A.Id);
        for (double V : {A.Size.X, A.Size.Y, A.Size.Z})
            if (!Range(V, 1, 30000))
                return false;
        for (const auto &P : A.Parts)
            for (double V : {P.Center.X, P.Center.Y, P.Center.Z, P.Size.X, P.Size.Y, P.Size.Z})
                if (!Range(V, -60000, 60000))
                    return false;
        for (const auto &P : A.Parts)
            if (P.Size.X <= 0 || P.Size.Y <= 0 || P.Size.Z <= 0)
                return false;
    }
    TSet<FGuid> Ids;
    for (const auto &I : S.Items)
    {
        if (!I.Id.IsValid() || Ids.Contains(I.Id) || !S.Room(I.RoomId) || !S.Asset(I.AssetId) ||
            !Range(I.X, -100000, 100000) || !Range(I.Z, -100000, 100000) || !FMath::IsFinite(I.Yaw))
            return false;
        Ids.Add(I.Id);
    }
    if (!ValidateGeometry(S, Error))
        return false;
    Error.Reset();
    return true;
}
bool Apply(const FScene &Before, const FEdit &E, FScene &After, FString &Error)
{
    After = Before;
    int32 At = After.Items.IndexOfByPredicate([&](const FItem &I) { return I.Id == E.Target; });
    auto *R = After.Rooms.FindByPredicate([&](const FRoom &X) { return X.Id == E.RoomId; });
    auto Fail = [&](const TCHAR *M) {
        Error = M;
        return false;
    };
    switch (E.Type)
    {
    case EEditType::Room: {
        if (!R || R->Polygon.Num() != 4)
            return Fail(TEXT("長方形の部屋を選択してください"));
        auto Min = R->Min();
        R->Polygon = {{Min.X, Min.Y},
                      {Min.X + Normalize(E.Width), Min.Y},
                      {Min.X + Normalize(E.Width), Min.Y + Normalize(E.Depth)},
                      {Min.X, Min.Y + Normalize(E.Depth)}};
        R->Height = Normalize(E.Height);
        break;
    }
    case EEditType::AddRoom:
        After.Rooms.Add(E.Room);
        if (!SplitSharedWalls(After, Error))
            return false;
        break;
    case EEditType::SetPolygon:
        if (!R)
            return false;
        {
            auto Old = *R;
            *R = E.Room;
            R->Id = Old.Id;
            if (R->Polygon == Old.Polygon)
                R->Walls = Old.Walls;
            else
            {
                R->Walls.Reset();
                for (int32 I = 0; I < R->Polygon.Num(); ++I)
                    R->Walls.Add(FWall());
            }
        }
        break;
    case EEditType::RemoveRoom:
        if (!R || After.Rooms.Num() == 1)
            return Fail(TEXT("最後の部屋は削除できません"));
        for (const auto &I : After.Items)
            if (I.RoomId == R->Id)
                return Fail(TEXT("家具のある部屋は削除できません"));
        {
            TSet<FGuid> Removed;
            for (auto W : R->Walls)
                Removed.Add(W.Id);
            for (auto O : After.Openings)
                if (Removed.Contains(O.WallId) || Removed.Contains(O.OtherWallId))
                    return Fail(TEXT("開口のある部屋は削除できません"));
            After.Rooms.RemoveAll([&](const FRoom &X) { return X.Id == E.RoomId; });
        }
        break;
    case EEditType::Surface:
        if (!R)
            return false;
        if (!E.WallId.IsValid() && E.Surface == ESurface::Floor)
            R->FloorFinish = E.Finish;
        else
        {
            FGuid W = E.WallId;
            if (!W.IsValid() && R->Walls.Num() == 4)
            {
                const int32 Map[] = {0, 2, 0, 1, 3};
                if (static_cast<uint8>(E.Surface) > 4)
                    return false;
                W = R->Walls[Map[static_cast<int32>(E.Surface)]].Id;
            }
            auto *Wall = R->Walls.FindByPredicate([&](const FWall &V) { return V.Id == W; });
            if (!Wall)
                return false;
            Wall->Finish = E.Finish;
        }
        break;
    case EEditType::Opening: {
        auto *O = After.Openings.FindByPredicate([&](const FOpening &V) { return V.Id == E.Opening.Id; });
        if (O)
            *O = E.Opening;
        else
            After.Openings.Add(E.Opening);
        break;
    }
    case EEditType::RemoveOpening:
        if (!After.Openings.RemoveAll([&](const FOpening &O) { return O.Id == E.Target; }))
            return false;
        break;
    case EEditType::AddAsset:
        if (After.Asset(E.Asset.Id))
            return Fail(TEXT("同じ家具版は登録済みです"));
        After.Assets.Add(E.Asset);
        break;
    case EEditType::Add:
        After.Items.Add(E.Item);
        break;
    case EEditType::Move:
        if (At == INDEX_NONE)
            return false;
        After.Items[At].X = E.Item.X;
        After.Items[At].Z = E.Item.Z;
        After.Items[At].Yaw = E.Item.Yaw;
        After.Items[At].RoomId = E.Item.RoomId;
        break;
    case EEditType::Remove:
        if (At == INDEX_NONE)
            return false;
        After.Items.RemoveAt(At);
        break;
    case EEditType::Duplicate:
        if (At == INDEX_NONE)
            return false;
        {
            auto I = After.Items[At];
            I.Id = E.Item.Id;
            I.X = E.Item.X;
            I.Z = E.Item.Z;
            After.Items.Add(I);
        }
        break;
    case EEditType::Variant:
    case EEditType::Replace: {
        if (At == INDEX_NONE || !After.Asset(E.Item.AssetId))
            return Fail(TEXT("交換先の家具版がありません"));
        auto &I = After.Items[At];
        FItem Old = I;
        I.AssetId = E.Item.AssetId;
        if (E.AnchorWallId.IsValid())
        {
            const auto Ws = Walls(After);
            const auto *W = Ws.FindByPredicate(
                [&](const FWallSegment &V) { return V.Id == E.AnchorWallId && V.RoomId == I.RoomId; });
            if (!W)
                return Fail(TEXT("基準壁が変更されています"));
            const auto T = (W->B - W->A).GetSafeNormal();
            const FVector2D N(-T.Y, T.X);
            auto MinDistance = [&](const FItem &V) {
                double D = DBL_MAX;
                for (auto P : Footprint(V, *After.Asset(V.AssetId)))
                    D = FMath::Min(D, FVector2D::DotProduct(N, P - W->A));
                return D;
            };
            double Delta = MinDistance(Old) - MinDistance(I);
            I.X += N.X * Delta;
            I.Z += N.Y * Delta;
        }
        break;
    }
    default:
        return Fail(TEXT("未知の編集コマンドです"));
    }
    for (auto &Room : After.Rooms)
    {
        Room.Height = Normalize(Room.Height);
        for (auto &P : Room.Polygon)
        {
            P.X = Normalize(P.X);
            P.Y = Normalize(P.Y);
        }
    }
    for (auto &O : After.Openings)
    {
        O.Offset = Normalize(O.Offset);
        O.Width = Normalize(O.Width);
        O.Height = Normalize(O.Height);
        O.Sill = Normalize(O.Sill);
    }
    for (auto &I : After.Items)
    {
        if (!FMath::IsFinite(I.Yaw))
            return Fail(TEXT("角度が不正です"));
        I.X = Normalize(I.X);
        I.Z = Normalize(I.Z);
        I.Yaw = Angle(I.Yaw);
    }
    return Validate(After, Error);
}
bool FDocument::Commit(const FEdit &E, FString &Error)
{
    FScene Next;
    if (!Apply(Scene, E, Next, Error))
        return false;
    if (Next == Scene)
        return true;
    Past.Add(Scene);
    if (Past.Num() > 100)
        Past.RemoveAt(0);
    Future.Reset();
    Scene = MoveTemp(Next);
    return true;
}
bool FDocument::Undo()
{
    if (Past.IsEmpty())
        return false;
    Future.Add(Scene);
    Scene = Past.Pop();
    return true;
}
bool FDocument::Redo()
{
    if (Future.IsEmpty())
        return false;
    Past.Add(Scene);
    Scene = Future.Pop();
    return true;
}
bool FDocument::ReplaceLoaded(const FString &Json, FString &Error)
{
    FScene Next;
    if (!Decode(Json, Next, Error, ResolveAsset))
        return false;
    Scene = MoveTemp(Next);
    Past.Reset();
    Future.Reset();
    MarkSaved();
    return true;
}
void FDocument::SetAssetResolver(FAssetResolver R)
{
    ResolveAsset = MoveTemp(R);
    for (auto &A : Scene.Assets)
    {
        FAsset B;
        FString E;
        if (ResolveAsset && ResolveAsset(A.Id, A.ManifestHash, B, E))
            A = MoveTemp(B);
    }
}
} // namespace Yohaku
