#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SceneDocument.h"
#include "RoomController.generated.h"
class AYSceneProjection;
class ACameraActor;
class SRoomEditor;
class FProductResearch;
enum class ERoomTool : uint8
{
    Select,
    Place,
    DrawRoom,
    AddRoom,
    Polygon,
    PassageStart,
    PassageEnd
};
UCLASS()
class YOHAKU_API ARoomController : public APlayerController
{
    GENERATED_BODY()
  public:
    ARoomController();
    virtual void BeginPlay() override;
    virtual bool InputKey(const FInputKeyEventArgs &Params) override;
    virtual void FlushPressedKeys() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    Yohaku::FDocument Document;
    FGuid Selected;
    ERoomTool Tool = ERoomTool::Select;
    bool bSnap = true;
    Yohaku::ESurface Surface = Yohaku::ESurface::Floor;
    FString Notice;
    TSharedPtr<FProductResearch, ESPMode::ThreadSafe> Research;
    bool bShowResearch = false;
    FGuid ActiveRoomId = FGuid(0, 0, 0, 1), SelectedWall, SelectedOpening;
    bool bKeepWallDistance = false, bReplacePolygon = false;
    double OpeningOffset = 500, OpeningWidth = 800, OpeningHeight = 2000, OpeningSill = 900, PassageWidth = 800;
    bool bHingeAtEnd = false, bSwingInward = true;
    TArray<FVector2D> PolygonPoints;
    TOptional<FVector2D> PassageStart, PassageEnd;
    Yohaku::FPassageResult PassageResult;
    TArray<Yohaku::FWarning> CachedWarnings;
    TArray<Yohaku::FAsset> ModelLibrary;
    void PlaceStudyAsset(const Yohaku::FAsset &);
    const Yohaku::FRoom &CurrentRoom() const;
    void ChooseRoom(FGuid);
    void StartPolygon(bool Replace);
    void FinishPolygon();
    void RemoveRoom();
    void AddOpening(Yohaku::EOpeningKind);
    void SelectOpening(FGuid);
    void RemoveOpening();
    void CheckPassage();
    void PlaceGenerated();
    void StartPlacementAsset(const FString &);
    void ReplaceAsset(const FString &);
    bool Execute(const Yohaku::FEdit &Edit);
    void StartPlacement(Yohaku::EVariant Variant);
    void SetTool(ERoomTool NewTool);
    void SetRoom(double W, double D, double H);
    void SetFinish(int32 Finish);
    void EditPose(double X, double Z, double YawDegrees);
    void ChangeVariant(Yohaku::EVariant Variant);
    void Rotate();
    void Duplicate();
    void DeleteSelected();
    void Undo();
    void Redo();
    void Cancel();
    void Save();
    void Load();
    void View(int32 Preset);
    const Yohaku::FItem *SelectedItem() const;
    FString Status() const;
    bool IsInteracting() const { return bDragging || bDrawing || Tool == ERoomTool::Place; }
    void Refresh();
    UPROPERTY() TObjectPtr<AYSceneProjection> Projection;
    UPROPERTY() TObjectPtr<ACameraActor> ViewCamera;
    bool FloorPoint(FVector2D &Out) const;
    const Yohaku::FItem* DisplayPose() const {return Tool==ERoomTool::Place||bDragging?&Preview:SelectedItem();}
    bool PendingRectangle(FVector2D& A,FVector2D& B) const {if(!bDrawing)return false;A=DrawStart;return FloorPoint(B);}

  private:
    void LeftDown();
    void LeftUp();
    void RightDown();
    void RightUp();
    void ZoomIn();
    void ZoomOut();
    void UpdatePointerPose();
    void UpdateOrbit();
    void CommitDrag();
    void UpdateCamera();
    bool ConfirmClose();
    void RunSmokeTest();
    void RunProductSmokeTest();
    void RunExpansionSmokeTest();
    TSharedPtr<SRoomEditor> Widget;
    bool bDragging = false, bDrawing = false, bOrbit = false, bDragMoved = false;
    TSet<FKey> ModifierKeys;
    Yohaku::FItem Preview;
    FVector2D GrabOffset, DragPointerStart, DrawStart, LastMouse;
    double OrbitYaw = -90, OrbitPitch = 54, Distance = 950;
    FVector CameraTarget = FVector(250, 200, 45);
    bool bSmoke = false;
    double LoadArmedUntil = 0;
};
