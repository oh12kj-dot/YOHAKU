#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SceneDocument.h"
#include "SceneProjection.generated.h"
class UFurnitureDefinition;
class UBoxComponent;
class UMeshComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class ARectLight;
UCLASS() class YOHAKU_API AYFurnitureActor : public AActor
{
    GENERATED_BODY()
  public:
    AYFurnitureActor();
    void Build(UFurnitureDefinition *);
    bool BuildGenerated(const Yohaku::FAsset &);
    void SetPose(const Yohaku::FItem &);
    void SetSelected(bool);
    FGuid InstanceId;
    FString AssetId;
    UPROPERTY() TObjectPtr<UBoxComponent> PickBounds;
    UPROPERTY() TArray<TObjectPtr<UMeshComponent>> Parts;
};
UCLASS() class YOHAKU_API AYSceneProjection : public AActor
{
    GENERATED_BODY()
  public:
    AYSceneProjection();
    void Reconcile(const Yohaku::FScene &, const FGuid &);
    void PreviewPose(const Yohaku::FItem &);
    void ShowGhost(const Yohaku::FItem &);
    void HideGhost();
    void Cutaway(const FVector &);
    AYFurnitureActor *Find(const FGuid &) const;
    bool ResolveBuiltin(const FString &, const FString &, Yohaku::FAsset &, FString &);
    FGuid PickWall(const UPrimitiveComponent *) const;
    FGuid PickRoom(const UPrimitiveComponent *) const;
    UPROPERTY() TMap<FGuid, TObjectPtr<AYFurnitureActor>> Furniture;
    UPROPERTY() TObjectPtr<AYFurnitureActor> Ghost;
    UPROPERTY() TObjectPtr<UFurnitureDefinition> Brown;
    UPROPERTY() TObjectPtr<UFurnitureDefinition> BlueGray;

  private:
    void BuildArchitecture(const Yohaku::FScene &);
    UStaticMeshComponent *Cube(const FVector &, const FVector &, double, const FString &, FGuid, FGuid);
    UPROPERTY() TArray<TObjectPtr<UMeshComponent>> Architecture;
    UPROPERTY() TArray<TObjectPtr<ARectLight>> RoomLights;
    TMap<const UPrimitiveComponent *, FGuid> WallPicks, RoomPicks;
    TArray<Yohaku::FRoom> CachedRooms;
    TArray<Yohaku::FOpening> CachedOpenings;
    TArray<Yohaku::FWallSegment> CachedWalls;
    TMap<const UPrimitiveComponent *, FGuid> CutawayWalls;
    TArray<Yohaku::FAsset> CurrentAssets;
};
