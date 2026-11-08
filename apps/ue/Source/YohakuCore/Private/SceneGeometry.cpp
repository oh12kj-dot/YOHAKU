#include "SceneDocument.h"
#include "Algo/Reverse.h"
namespace Yohaku
{
namespace
{
constexpr double Eps = .001;
bool Range(double V, double L, double H)
{
    return FMath::IsFinite(V) && V >= L && V <= H;
}
double Cross(FVector2D A, FVector2D B)
{
    return A.X * B.Y - A.Y * B.X;
}
double Distance(FVector2D P, FVector2D A, FVector2D B)
{
    auto D = B - A;
    double T = FMath::Clamp(FVector2D::DotProduct(P - A, D) / D.SizeSquared(), 0., 1.);
    return (P - A - T * D).Size();
}
bool Touch(FVector2D A, FVector2D B, FVector2D C, FVector2D D)
{
    if (FMath::Max(A.X, B.X) + Eps < FMath::Min(C.X, D.X) || FMath::Max(C.X, D.X) + Eps < FMath::Min(A.X, B.X) ||
        FMath::Max(A.Y, B.Y) + Eps < FMath::Min(C.Y, D.Y) || FMath::Max(C.Y, D.Y) + Eps < FMath::Min(A.Y, B.Y))
        return false;
    return Cross(B - A, C - A) * Cross(B - A, D - A) <= Eps && Cross(D - C, A - C) * Cross(D - C, B - C) <= Eps;
}
bool PolygonValid(const FRoom &R)
{
    int32 N = R.Polygon.Num();
    if (N < 4 || N > 32 || R.Walls.Num() != N || R.Name.TrimStartAndEnd().IsEmpty() || R.Name.Len() > 100 ||
        !Range(R.Height, 1000, 6000) || R.FloorFinish < 0 || R.FloorFinish > 3)
        return false;
    double Area = 0;
    for (int32 I = 0; I < N; ++I)
    {
        auto A = R.Polygon[I], B = R.Polygon[(I + 1) % N], C = R.Polygon[(I + 2) % N];
        if (!Range(A.X, -30000, 30000) || !Range(A.Y, -30000, 30000) || (B - A).Size() < 100 ||
            !((A.X == B.X) ^ (A.Y == B.Y)) ||
            (FMath::Abs(Cross(B - A, C - B)) < Eps && FVector2D::DotProduct(B - A, C - B) <= 0))
            return false;
        Area += Cross(A, B);
        for (int32 J = I + 1; J < N; ++J)
            if (J != I + 1 && !(I == 0 && J == N - 1) && Touch(A, B, R.Polygon[J], R.Polygon[(J + 1) % N]))
                return false;
    }
    return Area > 0 && R.Width() >= 500 && R.Depth() >= 500 && R.Width() <= 30000 && R.Depth() <= 30000;
}
bool RoomsOverlap(const FRoom &A, const FRoom &B)
{
    double MinX = FMath::Max(A.Min().X, B.Min().X), MaxX = FMath::Min(A.Max().X, B.Max().X),
           MinY = FMath::Max(A.Min().Y, B.Min().Y), MaxY = FMath::Min(A.Max().Y, B.Max().Y);
    if (MaxX - MinX < Eps || MaxY - MinY < Eps)
        return false;
    TArray<double> X{MinX, MaxX}, Y{MinY, MaxY};
    for (const auto *R : {&A, &B})
        for (auto V : R->Polygon)
        {
            if (V.X > MinX && V.X < MaxX)
                X.AddUnique(V.X);
            if (V.Y > MinY && V.Y < MaxY)
                Y.AddUnique(V.Y);
        }
    X.Sort();
    Y.Sort();
    for (int32 I = 1; I < X.Num(); ++I)
        for (int32 J = 1; J < Y.Num(); ++J)
        {
            FVector2D P((X[I] + X[I - 1]) / 2, (Y[J] + Y[J - 1]) / 2);
            if (PointInRoom(A, P, false) && PointInRoom(B, P, false))
                return true;
        }
    return false;
}
struct FBox2
{
    TArray<FVector2D> Points;
    double Low, High;
};
FVector2D World(FVector2D V, const FItem &I)
{
    double C = FMath::Cos(I.Yaw), S = FMath::Sin(I.Yaw);
    return {I.X + C * V.X + S * V.Y, I.Z - S * V.X + C * V.Y};
}
FBox2 Box(const FItem &I, FVector Center, FVector Size)
{
    FBox2 B;
    B.Low = Center.Z - Size.Z / 2;
    B.High = Center.Z + Size.Z / 2;
    for (auto V : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1)})
        B.Points.Add(World({Center.X + V.X * Size.X / 2, Center.Y + V.Y * Size.Y / 2}, I));
    return B;
}
bool Intersects(const FBox2 &A, const FBox2 &B)
{
    if (A.High <= B.Low + Eps || B.High <= A.Low + Eps)
        return false;
    for (const auto *P : {&A.Points, &B.Points})
        for (int32 I = 0; I < 2; ++I)
        {
            auto Edge = (*P)[I + 1] - (*P)[I];
            FVector2D N(-Edge.Y, Edge.X);
            double AL = DBL_MAX, AH = -DBL_MAX, BL = DBL_MAX, BH = -DBL_MAX;
            for (auto V : A.Points)
            {
                double D = FVector2D::DotProduct(N, V);
                AL = FMath::Min(AL, D);
                AH = FMath::Max(AH, D);
            }
            for (auto V : B.Points)
            {
                double D = FVector2D::DotProduct(N, V);
                BL = FMath::Min(BL, D);
                BH = FMath::Max(BH, D);
            }
            if (AH <= BL + Eps || BH <= AL + Eps)
                return false;
        }
    return true;
}
bool PointInBox(FVector2D P, const FBox2 &B, double Radius)
{
    double Min = DBL_MAX;
    bool Inside = true;
    for (int32 I = 0; I < 4; ++I)
    {
        auto A = B.Points[I], C = B.Points[(I + 1) % 4];
        Inside &= Cross(C - A, P - A) >= -Eps;
        Min = FMath::Min(Min, Distance(P, A, C));
    }
    return Inside || Min < Radius;
}

} // namespace
bool PointInRoom(const FRoom &R, const FVector2D &P, bool Boundary)
{
    bool In = false;
    int32 N = R.Polygon.Num();
    for (int32 I = 0, J = N - 1; I < N; J = I++)
    {
        auto A = R.Polygon[J], B = R.Polygon[I];
        if (Distance(P, A, B) <= Eps)
            return Boundary;
        if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X)
            In = !In;
    }
    return In;
}
TArray<FWallSegment> Walls(const FScene &S)
{
    TArray<FWallSegment> R;
    for (const auto &Room : S.Rooms)
        for (int32 I = 0; I < Room.Polygon.Num(); ++I)
            R.Add({Room.Walls[I].Id, Room.Id, Room.Polygon[I], Room.Polygon[(I + 1) % Room.Polygon.Num()], Room.Height,
                   Room.Walls[I].Finish});
    return R;
}
TArray<FWallPiece> WallPieces(const FScene &S)
{
    TArray<FWallPiece> Out;
    for (const auto &W : Walls(S))
    {
        double L = (W.B - W.A).Size();
        auto T = (W.B - W.A) / L;
        TArray<double> X{0, L}, Y{0, W.Height};
        for (const auto &O : S.Openings)
        {
            double D;
            if (OpeningOnWall(S, O, W, D))
            {
                X.AddUnique(D);
                X.AddUnique(D + O.Width);
                Y.AddUnique(O.Sill);
                Y.AddUnique(O.Sill + O.Height);
            }
        }
        X.Sort();
        Y.Sort();
        for (int32 I = 1; I < X.Num(); ++I)
            for (int32 J = 1; J < Y.Num(); ++J)
            {
                double MX = (X[I] + X[I - 1]) / 2, MY = (Y[J] + Y[J - 1]) / 2;
                bool Hole = false;
                for (const auto &O : S.Openings)
                {
                    double D;
                    if (OpeningOnWall(S, O, W, D) && MX > D && MX < D + O.Width && MY > O.Sill &&
                        MY < O.Sill + O.Height)
                    {
                        Hole = true;
                        break;
                    }
                }
                if (!Hole)
                    Out.Add({W, W.A + T * X[I - 1], W.A + T * X[I], Y[J - 1], Y[J] - Y[J - 1]});
            }
    }
    return Out;
}
bool OpeningOnWall(const FScene &S, const FOpening &O, const FWallSegment &W, double &Offset)
{
    if (O.WallId == W.Id)
    {
        Offset = O.Offset;
        return true;
    }
    if (O.OtherWallId == W.Id)
    {
        Offset = (W.B - W.A).Size() - O.Offset - O.Width;
        return true;
    }
    return false;
}
TArray<FVector2D> Footprint(const FItem &I, const FAsset &A)
{
    return Box(I, {0, 0, A.Size.Z / 2}, A.Size).Points;
}
bool ValidateGeometry(const FScene &S, FString &Error)
{
    auto Fail = [&](const TCHAR *M) {
        Error = M;
        return false;
    };
    if (S.Rooms.IsEmpty() || S.Rooms.Num() > 16 || S.Openings.Num() > 128)
        return Fail(TEXT("部屋は1〜16室、開口は128件までです"));
    TSet<FGuid> Ids;
    for (int32 I = 0; I < S.Rooms.Num(); ++I)
    {
        const auto &R = S.Rooms[I];
        if (!R.Id.IsValid() || Ids.Contains(R.Id) || !PolygonValid(R))
            return Fail(TEXT("部屋は交差しない直交多角形（反時計回り）で指定してください"));
        Ids.Add(R.Id);
        for (auto W : R.Walls)
        {
            if (!W.Id.IsValid() || Ids.Contains(W.Id) || W.Finish < 0 || W.Finish > 4)
                return Fail(TEXT("壁のIDまたは仕上げが不正です"));
            Ids.Add(W.Id);
        }
        for (int32 J = 0; J < I; ++J)
            if (RoomsOverlap(R, S.Rooms[J]))
                return Fail(TEXT("部屋の内部が重なっています"));
    }
    const auto Ws = Walls(S);
    for (const auto &O : S.Openings)
    {
        const auto *W = Ws.FindByPredicate([&](const auto &V) { return V.Id == O.WallId; });
        if (!O.Id.IsValid() || Ids.Contains(O.Id) || !W || !Range(O.Offset, 0, 60000) || !Range(O.Width, 300, 5000) ||
            !Range(O.Height, 300, 5000) || !Range(O.Sill, 0, 5000) || O.Offset + O.Width > (W->B - W->A).Size() + Eps ||
            O.Sill + O.Height > W->Height + Eps || (O.Kind != EOpeningKind::Door && O.Kind != EOpeningKind::Window) ||
            (O.Kind == EOpeningKind::Door && O.Sill != 0))
            return Fail(TEXT("開口が壁の範囲外か、寸法が不正です"));
        Ids.Add(O.Id);
        if (O.OtherWallId.IsValid())
        {
            const auto *B = Ws.FindByPredicate([&](const auto &V) { return V.Id == O.OtherWallId; });
            if (!B || W->RoomId == B->RoomId || !W->A.Equals(B->B, Eps) || !W->B.Equals(B->A, Eps) ||
                O.Sill + O.Height > B->Height + Eps)
                return Fail(TEXT("共有開口の隣室・壁の対応が不正です"));
        }
        for (const auto &Other : S.Openings)
        {
            if (Other.Id == O.Id)
                continue;
            double D;
            if (OpeningOnWall(S, Other, *W, D) &&
                FMath::Min(O.Offset + O.Width, D + Other.Width) > FMath::Max(O.Offset, D) + Eps &&
                FMath::Min(O.Sill + O.Height, Other.Sill + Other.Height) > FMath::Max(O.Sill, Other.Sill) + Eps)
                return Fail(TEXT("同じ壁の開口が重なっています"));
        }
    }
    for (const auto &I : S.Items)
        if (Ids.Contains(I.Id))
            return Fail(TEXT("シーン内のIDが重複しています"));
    return true;
}
TArray<FWarning> Warnings(const FScene &S)
{
    TArray<FWarning> Out;
    const auto Ws = Walls(S);
    for (int32 I = 0; I < S.Items.Num(); ++I)
    {
        const auto &A = S.Items[I];
        const auto *Asset = S.Asset(A.AssetId);
        const auto *R = S.Room(A.RoomId);
        if (!Asset || !R)
            continue;
        auto Foot = Footprint(A, *Asset);
        bool Outside = false;
        for (auto P : Foot)
            Outside |= !PointInRoom(*R, P);
        for (int32 K = 0; K < 4; ++K)
        {
            auto A0 = Foot[K], B0 = Foot[(K + 1) % 4], D = B0 - A0;
            TArray<double> Cuts{0, 1};
            for (const auto &W : Ws)
                if (W.RoomId == R->Id)
                {
                    auto E = W.B - W.A;
                    double Den = Cross(D, E);
                    if (FMath::Abs(Den) > Eps)
                    {
                        double T = Cross(W.A - A0, E) / Den, U = Cross(W.A - A0, D) / Den;
                        if (T > 0 && T < 1 && U >= 0 && U <= 1)
                            Cuts.AddUnique(T);
                    }
                }
            Cuts.Sort();
            for (int32 N = 1; N < Cuts.Num(); ++N)
                if (!PointInRoom(*R, A0 + D * ((Cuts[N] + Cuts[N - 1]) / 2)))
                    Outside = true;
        }
        if (Outside)
            Out.Add({A.Id, {}, TEXT("所属する部屋からのはみ出し候補")});
        for (int32 J = I + 1; J < S.Items.Num(); ++J)
        {
            const auto &B = S.Items[J];
            const auto *Other = S.Asset(B.AssetId);
            if (!Other || !Intersects(Box(A, {0, 0, Asset->Size.Z / 2}, Asset->Size),
                                      Box(B, {0, 0, Other->Size.Z / 2}, Other->Size)))
                continue;
            bool Hit = false;
            for (auto P : Asset->Parts)
            {
                for (auto Q : Other->Parts)
                    if (Intersects(Box(A, P.Center, P.Size), Box(B, Q.Center, Q.Size)))
                    {
                        Hit = true;
                        break;
                    }
                if (Hit)
                    break;
            }
            if (Hit)
                Out.Add({A.Id, B.Id, TEXT("家具の部品形状が干渉する候補（保守的な近似）")});
        }
        TSet<FGuid> HitWalls;
        for (const auto &P : WallPieces(S))
        {
            const auto T = (P.B - P.A).GetSafeNormal();
            FItem Wall;
            Wall.X = (P.A.X + P.B.X) / 2;
            Wall.Z = (P.A.Y + P.B.Y) / 2;
            Wall.Yaw = -FMath::Atan2(T.Y, T.X);
            auto WB = Box(Wall, {0, 0, P.Bottom + P.Height / 2}, {(P.B - P.A).Size(), 100, P.Height});
            for (const auto &Part : Asset->Parts)
                if (Intersects(Box(A, Part.Center, Part.Size), WB))
                {
                    HitWalls.Add(P.Wall.Id);
                    break;
                }
        }
        for (auto Wall : HitWalls)
            Out.Add({A.Id, Wall, TEXT("壁との干渉候補（開口を考慮・近似）")});
        for (const auto &O : S.Openings)
            if (O.Kind == EOpeningKind::Door)
            {
                const auto *W = Ws.FindByPredicate([&](const auto &V) { return V.Id == O.WallId; });
                if (!W)
                    continue;
                auto T = (W->B - W->A).GetSafeNormal(), N = FVector2D(-T.Y, T.X) * (O.SwingInward ? 1. : -1.);
                auto H = W->A + T * (O.Offset + (O.HingeAtEnd ? O.Width : 0));
                auto Along = T * (O.HingeAtEnd ? -1. : 1.);
                FItem Sweep;
                auto C = H + (Along + N) * O.Width / 2;
                Sweep.X = C.X;
                Sweep.Z = C.Y;
                Sweep.Yaw = -FMath::Atan2(Along.Y, Along.X);
                if (Intersects(Box(A, {0, 0, Asset->Size.Z / 2}, Asset->Size),
                               Box(Sweep, {0, 0, O.Height / 2}, {O.Width, O.Width, O.Height})))
                    Out.Add({A.Id, O.Id, TEXT("ドア開閉スペースへの侵入候補（掃引の外接範囲）")});
            }
    }
    return Out;
}
FPassageResult Passage(const FScene &S, FVector2D Start, FVector2D End, double Width)
{
    FPassageResult R;
    if (!Range(Width, 300, 2000) || Start.ContainsNaN() || End.ContainsNaN() || S.Rooms.IsEmpty())
    {
        R.Message = TEXT("通路幅と端点が不正です");
        return R;
    }
    FVector2D Min = S.Rooms[0].Min(), Max = S.Rooms[0].Max();
    for (const auto &Room : S.Rooms)
    {
        Min.X = FMath::Min(Min.X, Room.Min().X);
        Min.Y = FMath::Min(Min.Y, Room.Min().Y);
        Max.X = FMath::Max(Max.X, Room.Max().X);
        Max.Y = FMath::Max(Max.Y, Room.Max().Y);
    }
    const double Grid = 100, Radius = Width / 2 + Grid * .708;
    const int32 NX = FMath::CeilToInt((Max.X - Min.X) / Grid) + 1, NY = FMath::CeilToInt((Max.Y - Min.Y) / Grid) + 1;
    if (NX * NY > 400000)
    {
        R.Message = TEXT("通路の検査範囲が大きすぎます");
        return R;
    }
    TArray<TPair<FVector2D, FVector2D>> Segments;
    for (const auto &P : WallPieces(S))
        if (P.Bottom < 1800 && P.Bottom + P.Height > 0)
            Segments.Add({P.A, P.B});
    TArray<FBox2> Parts;
    TMap<FIntPoint, TArray<int32>> Buckets;
    int32 Entries = 0;
    for (const auto &I : S.Items)
        if (const auto *A = S.Asset(I.AssetId))
            for (const auto &B : A->Parts)
                if (B.Center.Z - B.Size.Z / 2 < 1800 && B.Center.Z + B.Size.Z / 2 > 0)
                {
                    if (Parts.Num() >= 20000)
                    {
                        R.Message = TEXT("通路検査の形状数が上限を超えます。形状を軽量化してください");
                        return R;
                    }
                    auto Shape = Box(I, B.Center, B.Size);
                    FVector2D Low(DBL_MAX, DBL_MAX), High(-DBL_MAX, -DBL_MAX);
                    for (auto P : Shape.Points)
                    {
                        Low.X = FMath::Min(Low.X, P.X);
                        Low.Y = FMath::Min(Low.Y, P.Y);
                        High.X = FMath::Max(High.X, P.X);
                        High.Y = FMath::Max(High.Y, P.Y);
                    }
                    const int32 Id = Parts.Add(MoveTemp(Shape));
                    for (int32 X = FMath::FloorToInt((Low.X - Radius) / 1000);
                         X <= FMath::FloorToInt((High.X + Radius) / 1000); ++X)
                        for (int32 Y = FMath::FloorToInt((Low.Y - Radius) / 1000);
                             Y <= FMath::FloorToInt((High.Y + Radius) / 1000); ++Y)
                        {
                            if (++Entries > 2000000)
                            {
                                R.Message = TEXT("通路検査の形状予算を超えます");
                                return R;
                            }
                            Buckets.FindOrAdd(FIntPoint(X, Y)).Add(Id);
                        }
                }
    auto Free = [&](FVector2D P) {
        bool Inside = false;
        for (const auto &Room : S.Rooms)
            Inside |= PointInRoom(Room, P);
        if (!Inside)
            return false;
        for (auto Seg : Segments)
            if (Distance(P, Seg.Key, Seg.Value) < Radius + 50)
                return false;
        if (const auto *Near = Buckets.Find(FIntPoint(FMath::FloorToInt(P.X / 1000), FMath::FloorToInt(P.Y / 1000))))
            for (int32 Id : *Near)
                if (PointInBox(P, Parts[Id], Radius))
                    return false;
        return true;
    };
    if (!Free(Start) || !Free(End))
    {
        R.bValid = true;
        R.Message = TEXT("始点または終点に指定幅の余裕がありません（100mm格子の概算）");
        return R;
    }
    auto Index = [&](FVector2D P) {
        return FMath::Clamp(FMath::RoundToInt((P.Y - Min.Y) / Grid), 0, NY - 1) * NX +
               FMath::Clamp(FMath::RoundToInt((P.X - Min.X) / Grid), 0, NX - 1);
    };
    auto Point = [&](int32 Id) { return Min + FVector2D(Id % NX, Id / NX) * Grid; };
    int32 A = Index(Start), B = Index(End);
    TArray<int32> Parent, Queue;
    TArray<int8> FreeState;
    Parent.Init(-2, NX * NY);
    FreeState.Init(-1, NX * NY);
    if (!Free(Point(A)) || !Free(Point(B)))
    {
        R.bValid = true;
        R.Message = TEXT("始点・終点を壁から少し離してください（100mm格子の概算）");
        return R;
    }
    Parent[A] = -1;
    Queue.Add(A);
    for (int32 Head = 0; Head < Queue.Num() && Parent[B] == -2; ++Head)
    {
        int32 At = Queue[Head];
        for (auto D : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
        {
            int32 X = At % NX + D.X, Y = At / NX + D.Y;
            if (X < 0 || Y < 0 || X >= NX || Y >= NY)
                continue;
            int32 Next = Y * NX + X;
            if (Parent[Next] != -2)
                continue;
            if (FreeState[Next] < 0)
                FreeState[Next] = Free(Point(Next)) ? 1 : 0;
            if (!FreeState[Next])
                continue;
            Parent[Next] = At;
            Queue.Add(Next);
        }
    }
    R.bValid = true;
    R.bFound = Parent[B] != -2;
    if (R.bFound)
    {
        for (int32 Id = B; Id != -1; Id = Parent[Id])
            R.Path.Add(Point(Id));
        Algo::Reverse(R.Path);
        double Clearance = DBL_MAX;
        for (auto P : R.Path)
        {
            for (auto Seg : Segments)
                Clearance = FMath::Min(Clearance, Distance(P, Seg.Key, Seg.Value) - 50);
            for (const auto &Shape : Parts)
                for (int32 I = 0; I < 4; ++I)
                    Clearance = FMath::Min(Clearance, Distance(P, Shape.Points[I], Shape.Points[(I + 1) % 4]));
        }
        R.MinimumWidthMm = FMath::Max(0., 2 * Clearance - Grid * 1.416);
        R.Message =
            FString::Printf(TEXT("幅 %.0fmmの経路候補あり / 最小幅の概算 %.0fmm（100mm格子・近似・安全保証なし）"),
                            Width, R.MinimumWidthMm);
    }
    else
        R.Message = TEXT("指定幅の経路候補が見つかりません（100mm格子の保守的概算）");
    return R;
}
} // namespace Yohaku
