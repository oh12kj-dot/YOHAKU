#include "SceneDocument.h"
namespace Yohaku
{
bool SplitSharedWalls(FScene &S, FString &Error)
{
    const auto Original = Walls(S);
    auto Next = S.Rooms;
    for (int32 RI = 0; RI < S.Rooms.Num(); ++RI)
    {
        const auto &R = S.Rooms[RI];
        auto &Out = Next[RI];
        Out.Polygon.Reset();
        Out.Walls.Reset();
        for (int32 I = 0; I < R.Polygon.Num(); ++I)
        {
            auto A = R.Polygon[I], B = R.Polygon[(I + 1) % R.Polygon.Num()];
            double L = (B - A).Size();
            if (L < .001)
            {
                Error = TEXT("壁の長さが不正です");
                return false;
            }
            auto T = (B - A) / L;
            TArray<double> Cuts{0, L};
            for (const auto &Other : S.Rooms)
                if (Other.Id != R.Id)
                    for (auto P : Other.Polygon)
                    {
                        double D = FVector2D::DotProduct(P - A, T);
                        if (D > .001 && D < L - .001 && (P - (A + D * T)).Size() < .001)
                            Cuts.AddUnique(D);
                    }
            Cuts.Sort();
            for (int32 C = 0; C < Cuts.Num() - 1; ++C)
            {
                Out.Polygon.Add(A + T * Cuts[C]);
                FWall W = R.Walls[I];
                if (Cuts.Num() > 2)
                    W.Id = FGuid::NewGuid();
                Out.Walls.Add(W);
            }
        }
    }
    S.Rooms = MoveTemp(Next);
    const auto NewWalls = Walls(S);
    for (auto &O : S.Openings)
    {
        const auto *Old = Original.FindByPredicate([&](const auto &W) { return W.Id == O.WallId; });
        if (!Old)
        {
            Error = TEXT("変更前の開口参照が不正です");
            return false;
        }
        auto T = (Old->B - Old->A).GetSafeNormal();
        auto A = Old->A + T * O.Offset, B = A + T * O.Width;
        const auto *New = NewWalls.FindByPredicate([&](const auto &W) {
            auto D = (W.B - W.A).GetSafeNormal();
            double Start = FVector2D::DotProduct(A - W.A, D), End = FVector2D::DotProduct(B - W.A, D);
            return W.RoomId == Old->RoomId && D.Equals(T, .001) && Start >= -.001 && End <= (W.B - W.A).Size() + .001 &&
                   (A - (W.A + Start * D)).Size() < .001;
        });
        if (!New)
        {
            Error = TEXT("部屋境界が既存の開口を分割します。開口を変更してから追加してください");
            return false;
        }
        O.WallId = New->Id;
        O.Offset = Normalize(FVector2D::DotProduct(A - New->A, T));
        const auto *Pair = NewWalls.FindByPredicate([&](const auto &W) {
            return W.RoomId != New->RoomId && W.A.Equals(New->B, .001) && W.B.Equals(New->A, .001);
        });
        if (O.OtherWallId.IsValid() && !Pair)
        {
            Error = TEXT("共有開口の接続が失われる変更です");
            return false;
        }
        O.OtherWallId = Pair ? Pair->Id : FGuid();
    }
    return true;
}
} // namespace Yohaku
