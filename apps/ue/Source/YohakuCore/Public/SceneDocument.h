#pragma once
#include "CoreMinimal.h"
namespace Yohaku
{
enum class ESurface : uint8
{
    Floor,
    North,
    South,
    East,
    West
};
enum class EVariant : uint8
{
    Brown,
    BlueGray
};
enum class EOpeningKind : uint8
{
    Door,
    Window
};
struct FWall
{
    FGuid Id = FGuid::NewGuid();
    int32 Finish = 0;
    bool operator==(const FWall &B) const { return Id == B.Id && Finish == B.Finish; }
};
struct YOHAKUCORE_API FRoom
{
    FGuid Id;
    FString Name;
    TArray<FVector2D> Polygon;
    TArray<FWall> Walls;
    double Height = 2600;
    int32 FloorFinish = 0;
    FVector2D Min() const;
    FVector2D Max() const;
    double Width() const { return Max().X - Min().X; }
    double Depth() const { return Max().Y - Min().Y; }
    bool operator==(const FRoom &B) const
    {
        return Id == B.Id && Name == B.Name && Polygon == B.Polygon && Walls == B.Walls && Height == B.Height &&
               FloorFinish == B.FloorFinish;
    }
    static FRoom Rectangle(double X, double Z, double W, double D, const FString &Name);
};
struct FOpening
{
    FGuid Id = FGuid::NewGuid(), WallId, OtherWallId;
    EOpeningKind Kind = EOpeningKind::Door;
    double Offset = 500, Width = 800, Height = 2000, Sill = 0;
    bool HingeAtEnd = false, SwingInward = true;
    bool operator==(const FOpening &B) const
    {
        return Id == B.Id && WallId == B.WallId && OtherWallId == B.OtherWallId && Kind == B.Kind &&
               Offset == B.Offset && Width == B.Width && Height == B.Height && Sill == B.Sill &&
               HingeAtEnd == B.HingeAtEnd && SwingInward == B.SwingInward;
    }
};
struct FCollisionBox
{
    FVector Center = FVector::ZeroVector, Size = FVector::ZeroVector;
    bool operator==(const FCollisionBox &B) const { return Center == B.Center && Size == B.Size; }
};
struct FAsset
{
    FString Id, ManifestHash, MeshHash, MeshFile, Name, SourceUrl, DimensionStatus, MaterialStatus;
    FVector Size = FVector(1500, 870, 820);
    TArray<FCollisionBox> Parts;
    bool bGeneratedStudy = false;
    bool operator==(const FAsset &B) const { return Id == B.Id && ManifestHash == B.ManifestHash; }
};
using FAssetResolver = TFunction<bool(const FString &, const FString &, FAsset &, FString &)>;
struct FItem
{
    FGuid Id = FGuid::NewGuid(), RoomId = FGuid(0, 0, 0, 1);
    FString AssetId = TEXT("study-psf-v3-brown");
    double X = 2000, Z = 2000, Yaw = 0;
    bool operator==(const FItem &B) const
    {
        return Id == B.Id && RoomId == B.RoomId && AssetId == B.AssetId && X == B.X && Z == B.Z && Yaw == B.Yaw;
    }
};
struct YOHAKUCORE_API FScene
{
    FScene();
    TArray<FRoom> Rooms;
    TArray<FOpening> Openings;
    TArray<FAsset> Assets;
    TArray<FItem> Items;
    const FRoom *Room(const FGuid &Id) const;
    const FAsset *Asset(const FString &Id) const;
    bool operator==(const FScene &B) const
    {
        return Rooms == B.Rooms && Openings == B.Openings && Assets == B.Assets && Items == B.Items;
    }
};
enum class EEditType : uint8
{
    Room,
    Surface,
    Add,
    Move,
    Remove,
    Duplicate,
    Variant,
    AddRoom,
    SetPolygon,
    RemoveRoom,
    Opening,
    RemoveOpening,
    AddAsset,
    Replace
};
struct FEdit
{
    EEditType Type = EEditType::Room;
    FItem Item;
    FRoom Room;
    FOpening Opening;
    FAsset Asset;
    FGuid Target, RoomId = FGuid(0, 0, 0, 1), WallId, AnchorWallId;
    double Width = 5000, Depth = 4000, Height = 2600;
    ESurface Surface = ESurface::Floor;
    int32 Finish = 0;
};
struct FWarning
{
    FGuid First, Second;
    FString Message;
};
struct FWallSegment
{
    FGuid Id, RoomId;
    FVector2D A, B;
    double Height;
    int32 Finish;
};
struct FWallPiece
{
    FWallSegment Wall;
    FVector2D A, B;
    double Bottom, Height;
};
struct FPassageResult
{
    bool bFound = false, bValid = false;
    double GridMm = 100;
    double MinimumWidthMm = 0;
    TArray<FVector2D> Path;
    FString Message;
};
YOHAKUCORE_API FString VariantId(EVariant);
YOHAKUCORE_API bool BuiltinAsset(const FString &, const FString &, FAsset &, FString &);
YOHAKUCORE_API double Normalize(double);
YOHAKUCORE_API bool Validate(const FScene &, FString &);
YOHAKUCORE_API bool Apply(const FScene &, const FEdit &, FScene &, FString &);
YOHAKUCORE_API TArray<FWallSegment> Walls(const FScene &);
YOHAKUCORE_API TArray<FWallPiece> WallPieces(const FScene &);
YOHAKUCORE_API bool OpeningOnWall(const FScene &, const FOpening &, const FWallSegment &, double &Offset);
YOHAKUCORE_API bool PointInRoom(const FRoom &, const FVector2D &, bool Boundary = true);
YOHAKUCORE_API TArray<FVector2D> Footprint(const FItem &, const FAsset &);
YOHAKUCORE_API TArray<FWarning> Warnings(const FScene &);
YOHAKUCORE_API FPassageResult Passage(const FScene &, FVector2D, FVector2D, double);
YOHAKUCORE_API FString Encode(const FScene &);
YOHAKUCORE_API bool Decode(const FString &, FScene &, FString &, const FAssetResolver &Resolver = {});
class YOHAKUCORE_API FDocument
{
  public:
    const FScene &Get() const { return Scene; }
    bool Commit(const FEdit &, FString &);
    bool Undo();
    bool Redo();
    bool CanUndo() const { return Past.Num() > 0; }
    bool CanRedo() const { return Future.Num() > 0; }
    int32 UndoCount() const { return Past.Num(); }
    bool Dirty() const { return Encode(Scene) != SavedJson; }
    void MarkSaved() { SavedJson = Encode(Scene); }
    bool ReplaceLoaded(const FString &, FString &);
    void SetAssetResolver(FAssetResolver);

  private:
    FScene Scene;
    TArray<FScene> Past, Future;
    FString SavedJson;
    FAssetResolver ResolveAsset;
};
} // namespace Yohaku
