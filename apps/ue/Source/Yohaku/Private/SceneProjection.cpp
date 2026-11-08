#include "SceneProjection.h"
#include "FurnitureDefinition.h"
#include "LocalModels.h"
#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Engine/RectLight.h"
#include "Components/RectLightComponent.h"
namespace
{
UMaterialInterface *Finish(bool Floor, int32 Index)
{
    const TCHAR *Floors[] = {TEXT("Oak"), TEXT("Walnut"), TEXT("WhiteOak"), TEXT("Stone")};
    const TCHAR *Walls[] = {TEXT("Ivory"), TEXT("Sage"), TEXT("Clay"), TEXT("Chalk"), TEXT("Slate")};
    const FString Name = Floor ? Floors[FMath::Clamp(Index, 0, 3)] : Walls[FMath::Clamp(Index, 0, 4)];
    return LoadObject<UMaterialInterface>(nullptr, *(TEXT("/Game/Finishes/M_") + Name + TEXT(".M_") + Name));
}
TArray<int32> Triangulate(const TArray<FVector2D> &P)
{
    TArray<int32> Remaining, Out;
    for (int32 I = 0; I < P.Num(); ++I)
        Remaining.Add(I);
    auto Cross = [](FVector2D A, FVector2D B) { return A.X * B.Y - A.Y * B.X; };
    for (int32 I = Remaining.Num() - 1; I >= 0 && Remaining.Num() > 3; --I)
    {
        auto A = P[Remaining[(I + Remaining.Num() - 1) % Remaining.Num()]], B = P[Remaining[I]],
             C = P[Remaining[(I + 1) % Remaining.Num()]];
        if (FMath::Abs(Cross(B - A, C - B)) < .001)
            Remaining.RemoveAt(I);
    }
    while (Remaining.Num() > 2)
    {
        bool Found = false;
        for (int32 I = 0; I < Remaining.Num(); ++I)
        {
            int32 A = Remaining[(I + Remaining.Num() - 1) % Remaining.Num()], B = Remaining[I],
                  C = Remaining[(I + 1) % Remaining.Num()];
            if (Cross(P[B] - P[A], P[C] - P[B]) <= 0)
                continue;
            bool Inside = false;
            for (int32 V : Remaining)
                if (V != A && V != B && V != C && Cross(P[B] - P[A], P[V] - P[A]) >= 0 &&
                    Cross(P[C] - P[B], P[V] - P[B]) >= 0 && Cross(P[A] - P[C], P[V] - P[C]) >= 0)
                {
                    Inside = true;
                    break;
                }
            if (Inside)
                continue;
            Out.Append({A, C, B});
            Remaining.RemoveAt(I);
            Found = true;
            break;
        }
        if (!Found)
        {
            Out.Reset();
            break;
        }
    }
    return Out;
}
} // namespace
AYFurnitureActor::AYFurnitureActor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    PickBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("PickBounds"));
    PickBounds->SetupAttachment(RootComponent);
    PickBounds->SetBoxExtent({75, 43.5, 41});
    PickBounds->SetRelativeLocation({0, 0, 41});
    PickBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
void AYFurnitureActor::Build(UFurnitureDefinition *D)
{
    for (auto P : Parts)
        P->DestroyComponent();
    Parts.Reset();
    if (!D)
        return;
    for (const auto &Part : D->Parts)
    {
        auto *C = NewObject<UStaticMeshComponent>(this);
        C->SetupAttachment(RootComponent);
        C->SetStaticMesh(Part.Mesh);
        C->SetMaterial(0, Part.Material);
        C->SetRelativeTransform(Part.Transform);
        C->SetMobility(EComponentMobility::Movable);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->RegisterComponent();
        Parts.Add(C);
    }
    PickBounds->SetBoxExtent({75, 43.5, 41});
    PickBounds->SetRelativeLocation({0, 0, 41});
}
bool AYFurnitureActor::BuildGenerated(const Yohaku::FAsset &A)
{
    Yohaku::FGeneratedMesh M;
    FString Error;
    if (!Yohaku::FLocalModels::ReadMesh(A, M, Error))
        return false;
    for (auto P : Parts)
        P->DestroyComponent();
    Parts.Reset();
    auto *C = NewObject<UProceduralMeshComponent>(this);
    C->SetupAttachment(RootComponent);
    TArray<FVector> V, N;
    TArray<FVector2D> UV;
    TArray<int32> T;
    V.Reserve(M.Vertices.Num());
    for (auto P : M.Vertices)
    {
        V.Add(P / 10);
        N.Add(FVector::ZeroVector);
        UV.Add({P.X / A.Size.X + .5, P.Y / A.Size.Y + .5});
    }
    for (int32 I = 0; I < M.Triangles.Num(); I += 3)
    {
        int32 A0 = M.Triangles[I], B = M.Triangles[I + 1], D = M.Triangles[I + 2];
        auto Normal = FVector::CrossProduct(V[B] - V[A0], V[D] - V[A0]);
        N[A0] += Normal;
        N[B] += Normal;
        N[D] += Normal;
        T.Append({A0, D, B});
    }
    for (auto &Normal : N)
        Normal.Normalize();
    C->CreateMeshSection_LinearColor(0, V, T, N, UV, {}, {}, false);
    C->SetMaterial(0,
                   LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Finishes/M_GeneratedStudy.M_GeneratedStudy")));
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->RegisterComponent();
    Parts.Add(C);
    PickBounds->SetBoxExtent(A.Size / 20);
    PickBounds->SetRelativeLocation({0, 0, A.Size.Z / 20});
    return true;
}
void AYFurnitureActor::SetPose(const Yohaku::FItem &I)
{
    InstanceId = I.Id;
    AssetId = I.AssetId;
    SetActorLocationAndRotation({I.X / 10, I.Z / 10, 0}, FRotator(0, -FMath::RadiansToDegrees(I.Yaw), 0));
    SetActorScale3D(FVector::OneVector);
}
void AYFurnitureActor::SetSelected(bool S)
{
    for (auto P : Parts)
    {
        P->SetRenderCustomDepth(S);
        P->SetCustomDepthStencilValue(S ? 1 : 0);
    }
}
AYSceneProjection::AYSceneProjection()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
bool AYSceneProjection::ResolveBuiltin(const FString &Id, const FString &Hash, Yohaku::FAsset &A, FString &Error)
{
    if (!Yohaku::BuiltinAsset(Id, Hash, A, Error))
        return false;
    if (!Brown)
        Brown = LoadObject<UFurnitureDefinition>(nullptr, TEXT("/Game/Catalog/PSFBrown.PSFBrown"));
    if (!BlueGray)
        BlueGray = LoadObject<UFurnitureDefinition>(nullptr, TEXT("/Game/Catalog/PSFBlueGray.PSFBlueGray"));
    auto *D = Id == Yohaku::VariantId(Yohaku::EVariant::Brown) ? Brown.Get() : BlueGray.Get();
    if (!D)
    {
        Error = TEXT("家具の参照資産がありません");
        return false;
    }
    A.Parts.Reset();
    for (const auto &P : D->Parts)
        if (P.Mesh)
        {
            auto Bounds = P.Mesh->GetBoundingBox().TransformBy(P.Transform);
            A.Parts.Add({Bounds.GetCenter() * 10, Bounds.GetSize() * 10});
        }
    return !A.Parts.IsEmpty();
}
UStaticMeshComponent *AYSceneProjection::Cube(const FVector &Position, const FVector &Size, double Yaw,
                                              const FString &Material, FGuid Wall, FGuid Room)
{
    auto *C = NewObject<UStaticMeshComponent>(this);
    C->SetupAttachment(RootComponent);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    C->SetRelativeLocation(Position);
    C->SetRelativeRotation(FRotator(0, Yaw, 0));
    C->SetRelativeScale3D(Size / 100);
    C->SetMobility(EComponentMobility::Movable);
    C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    C->SetCollisionResponseToAllChannels(ECR_Ignore);
    C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    if (!Material.IsEmpty())
        C->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, *Material));
    C->RegisterComponent();
    Architecture.Add(C);
    RoomPicks.Add(C, Room);
    if (Wall.IsValid())
    {
        WallPicks.Add(C, Wall);
        CutawayWalls.Add(C, Wall);
    }
    return C;
}
void AYSceneProjection::BuildArchitecture(const Yohaku::FScene &S)
{
    for (auto L : RoomLights)
        if (L)
            L->Destroy();
    RoomLights.Reset();
    for (auto C : Architecture)
        C->DestroyComponent();
    Architecture.Reset();
    WallPicks.Reset();
    RoomPicks.Reset();
    CutawayWalls.Reset();
    CachedWalls = Yohaku::Walls(S);
    for (const auto &R : S.Rooms)
    {
        auto *Floor = NewObject<UProceduralMeshComponent>(this);
        Floor->SetupAttachment(RootComponent);
        TArray<FVector> V, N;
        TArray<FVector2D> UV;
        for (auto P : R.Polygon)
        {
            V.Add({P.X / 10, P.Y / 10, 0});
            N.Add(FVector::UpVector);
            UV.Add(P / 1000);
        }
        Floor->CreateMeshSection_LinearColor(0, V, Triangulate(R.Polygon), N, UV, {}, {}, true);
        Floor->bUseComplexAsSimpleCollision = true;
        Floor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Floor->SetCollisionResponseToAllChannels(ECR_Ignore);
        Floor->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Floor->SetMaterial(0, Finish(true, R.FloorFinish));
        Floor->RegisterComponent();
        Architecture.Add(Floor);
        RoomPicks.Add(Floor, R.Id);
    }
    for (const auto &Piece : Yohaku::WallPieces(S))
    {
        const auto &W = Piece.Wall;
        auto Delta = Piece.B - Piece.A;
        auto T = Delta.GetSafeNormal();
        auto N = FVector2D(-T.Y, T.X);
        bool Shared = false;
        for (auto Other : CachedWalls)
            if (Other.RoomId != W.RoomId && W.A.Equals(Other.B, .001) && W.B.Equals(Other.A, .001))
            {
                Shared = true;
                break;
            }
        auto Center = (Piece.A + Piece.B) / 2 + (Shared ? N * 25 : FVector2D::ZeroVector);
        auto *C = Cube({Center.X / 10, Center.Y / 10, (Piece.Bottom + Piece.Height / 2) / 10},
                       {Delta.Size() / 10, Shared ? 5. : 10., Piece.Height / 10},
                       FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X)), TEXT(""), W.Id, W.RoomId);
        C->SetMaterial(0, Finish(false, W.Finish));
    }
    for (const auto &O : S.Openings)
    {
        const auto *W = CachedWalls.FindByPredicate([&](const auto &V) { return V.Id == O.WallId; });
        if (!W)
            continue;
        auto T = (W->B - W->A).GetSafeNormal(), N = FVector2D(-T.Y, T.X);
        auto A = W->A + T * O.Offset, B = A + T * O.Width;
        double Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));
        FString Frame = TEXT("/Game/Finishes/M_WhiteOak.M_WhiteOak");
        for (auto P : {A, B})
            Cube({P.X / 10, P.Y / 10, (O.Sill + O.Height / 2) / 10}, {6, 12, O.Height / 10}, Yaw, Frame, W->Id,
                 W->RoomId);
        auto Mid = (A + B) / 2;
        Cube({Mid.X / 10, Mid.Y / 10, (O.Sill + O.Height) / 10}, {O.Width / 10, 12, 6}, Yaw, Frame, W->Id, W->RoomId);
        if (O.Kind == Yohaku::EOpeningKind::Window)
        {
            Cube({Mid.X / 10, Mid.Y / 10, O.Sill / 10}, {O.Width / 10, 12, 6}, Yaw, Frame, W->Id, W->RoomId);
            Cube({Mid.X / 10, Mid.Y / 10, (O.Sill + O.Height / 2) / 10}, {O.Width / 10, 1, O.Height / 10}, Yaw,
                 TEXT("/Game/Finishes/M_WindowGlass.M_WindowGlass"), W->Id, W->RoomId);
        }
        else
        {
            auto H = O.HingeAtEnd ? B : A;
            auto Out = N * (O.SwingInward ? 1. : -1.);
            auto C = H + Out * O.Width / 2;
            Cube({C.X / 10, C.Y / 10, O.Height / 20}, {O.Width / 10, 4, O.Height / 10},
                 FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X)), TEXT("/Game/Finishes/M_Walnut.M_Walnut"), W->Id,
                 W->RoomId);
        }
    }
    for (const auto &Room : S.Rooms)
    {
        auto Center = (Room.Min() + Room.Max()) / 2;
        if (!Yohaku::PointInRoom(Room, Center))
        {
            auto Tri = Triangulate(Room.Polygon);
            if (Tri.Num() >= 3)
                Center = (Room.Polygon[Tri[0]] + Room.Polygon[Tri[1]] + Room.Polygon[Tri[2]]) / 3;
        }
        double Area = 0;
        for (int32 K = 0; K < Room.Polygon.Num(); ++K)
        {
            auto A = Room.Polygon[K], B = Room.Polygon[(K + 1) % Room.Polygon.Num()];
            Area += A.X * B.Y - A.Y * B.X;
        }
        double Power = FMath::Clamp(Area / 40000000., .3, 2.5);
        for (int32 K = 0; K < 2; ++K)
        {
            FVector Position((Center.X + (K ? .2 : -.18) * Room.Width()) / 10,
                             (Center.Y + (K ? .1 : -.15) * Room.Depth()) / 10, Room.Height / 10 + (K ? 80 : 120));
            FVector Target(Center.X / 10, Center.Y / 10, 40);
            auto *L = GetWorld()->SpawnActor<ARectLight>(Position, (Target - Position).Rotation());
            auto *Light = Cast<URectLightComponent>(L->GetLightComponent());
            Light->SetMobility(EComponentMobility::Movable);
            Light->SetIntensityUnits(ELightUnits::Lumens);
            Light->SetIntensity((K ? 8000 : 16000) * Power);
            Light->SetSourceWidth(FMath::Clamp(Room.Width() / 18, 100., 700.));
            Light->SetSourceHeight(FMath::Clamp(Room.Depth() / 20, 100., 600.));
            Light->SetAttenuationRadius(3000);
            RoomLights.Add(L);
        }
    }
    CachedRooms = S.Rooms;
    CachedOpenings = S.Openings;
}
void AYSceneProjection::Reconcile(const Yohaku::FScene &S, const FGuid &Selected)
{
    if (!Brown)
        Brown = LoadObject<UFurnitureDefinition>(nullptr, TEXT("/Game/Catalog/PSFBrown.PSFBrown"));
    if (!BlueGray)
        BlueGray = LoadObject<UFurnitureDefinition>(nullptr, TEXT("/Game/Catalog/PSFBlueGray.PSFBlueGray"));
    if (CachedRooms != S.Rooms || CachedOpenings != S.Openings)
        BuildArchitecture(S);
    CurrentAssets = S.Assets;
    TSet<FGuid> Keep;
    for (const auto &I : S.Items)
    {
        const auto *A = S.Asset(I.AssetId);
        if (!A)
            continue;
        Keep.Add(I.Id);
        auto *Actor = Find(I.Id);
        if (!Actor)
        {
            Actor = GetWorld()->SpawnActor<AYFurnitureActor>();
            Furniture.Add(I.Id, Actor);
        }
        if (Actor->AssetId != I.AssetId)
        {
            if (A->bGeneratedStudy)
                Actor->BuildGenerated(*A);
            else
                Actor->Build(I.AssetId == Yohaku::VariantId(Yohaku::EVariant::Brown) ? Brown : BlueGray);
        }
        Actor->SetPose(I);
        Actor->SetSelected(I.Id == Selected);
    }
    TArray<FGuid> Removed;
    for (auto Pair : Furniture)
        if (!Keep.Contains(Pair.Key))
        {
            Pair.Value->Destroy();
            Removed.Add(Pair.Key);
        }
    for (auto Id : Removed)
        Furniture.Remove(Id);
}
AYFurnitureActor *AYSceneProjection::Find(const FGuid &Id) const
{
    auto *A = Furniture.Find(Id);
    return A ? A->Get() : nullptr;
}
void AYSceneProjection::PreviewPose(const Yohaku::FItem &I)
{
    if (auto *A = Find(I.Id))
        A->SetPose(I);
}
void AYSceneProjection::ShowGhost(const Yohaku::FItem &I)
{
    const auto *A = CurrentAssets.FindByPredicate([&](const auto &V) { return V.Id == I.AssetId; });
    if (!A)
        return;
    if (!Ghost)
    {
        Ghost = GetWorld()->SpawnActor<AYFurnitureActor>();
        Ghost->PickBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (Ghost->AssetId != I.AssetId)
    {
        if (A->bGeneratedStudy)
            Ghost->BuildGenerated(*A);
        else
            Ghost->Build(I.AssetId == Yohaku::VariantId(Yohaku::EVariant::Brown) ? Brown : BlueGray);
    }
    Ghost->SetPose(I);
    Ghost->SetActorHiddenInGame(false);
    Ghost->SetSelected(true);
}
void AYSceneProjection::HideGhost()
{
    if (Ghost)
        Ghost->SetActorHiddenInGame(true);
}
void AYSceneProjection::Cutaway(const FVector &C)
{
    for (auto Pair : CutawayWalls)
    {
        const auto *W = CachedWalls.FindByPredicate([&](const auto &V) { return V.Id == Pair.Value; });
        if (!W)
            continue;
        auto T = (W->B - W->A).GetSafeNormal();
        bool Visible = FVector2D::DotProduct(FVector2D(C.X * 10, C.Y * 10) - W->A, {-T.Y, T.X}) >= 0;
        const_cast<UPrimitiveComponent *>(Pair.Key)->SetVisibility(Visible);
        const_cast<UPrimitiveComponent *>(Pair.Key)->SetCollisionResponseToChannel(ECC_Visibility,
                                                                                   Visible ? ECR_Block : ECR_Ignore);
    }
}
FGuid AYSceneProjection::PickWall(const UPrimitiveComponent *C) const
{
    const auto *Id = WallPicks.Find(C);
    return Id ? *Id : FGuid();
}
FGuid AYSceneProjection::PickRoom(const UPrimitiveComponent *C) const
{
    const auto *Id = RoomPicks.Find(C);
    return Id ? *Id : FGuid();
}
