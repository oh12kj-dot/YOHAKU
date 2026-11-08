#include "RoomController.h"
#include "SceneProjection.h"
#include "RoomEditorWidget.h"
#include "LayoutStorage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/BoxComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "HAL/FileManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/App.h"
#include "InputKeyEventArgs.h"
#include "ProductResearch.h"
#include "LocalModels.h"
#include "Algo/Reverse.h"

ARoomController::ARoomController()
{
    bShowMouseCursor = true;
    PrimaryActorTick.bCanEverTick = true;
}
void ARoomController::BeginPlay()
{
    Super::BeginPlay();
    Research = MakeShared<FProductResearch, ESPMode::ThreadSafe>();
    Projection = GetWorld()->SpawnActor<AYSceneProjection>();
    const TWeakObjectPtr<AYSceneProjection> WeakProjection(Projection);
    Document.SetAssetResolver([WeakProjection](const FString &Id, const FString &Hash, Yohaku::FAsset &A, FString &E) {
        if (Id.StartsWith(TEXT("study-psf-")))
            return WeakProjection.IsValid() && WeakProjection->ResolveBuiltin(Id, Hash, A, E);
        return Yohaku::FLocalModels::Resolve(Id, Hash, A, E);
    });
    ViewCamera = GetWorld()->SpawnActor<ACameraActor>();
    ViewCamera->GetCameraComponent()->SetFieldOfView(48);
    SetViewTarget(ViewCamera);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    if (GEngine && GEngine->GameViewport)
    {
        SAssignNew(Widget, SRoomEditor).Controller(this);
        GEngine->GameViewport->AddViewportWidgetContent(Widget.ToSharedRef(), 10);
        GEngine->GameViewport->OnWindowCloseRequested().BindUObject(this, &ARoomController::ConfirmClose);
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("YohakuSmoke")) &&
        IFileManager::Get().FileExists(*Yohaku::FLayoutStorage::Path()))
    {
        FString Json, Error;
        if (Yohaku::FLayoutStorage::Load(Json, Error) && Document.ReplaceLoaded(Json, Error))
            Notice = TEXT("このPCに保存したレイアウトを復元しました");
        else
            Notice = Error;
    }
    ModelLibrary = Yohaku::FLocalModels::List();
    Refresh();
    UpdateCamera();
    bSmoke = FParse::Param(FCommandLine::Get(), TEXT("YohakuSmoke")) ||
             FParse::Param(FCommandLine::Get(), TEXT("YohakuReloadSmoke"));
    if (bSmoke)
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, this, &ARoomController::RunSmokeTest, 4, false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("YohakuProductSmoke")) ||
        FParse::Param(FCommandLine::Get(), TEXT("YohakuCandidateReloadSmoke")))
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, this, &ARoomController::RunProductSmokeTest, 4, false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("YohakuExpansionSmoke")) ||
        FParse::Param(FCommandLine::Get(), TEXT("YohakuExpansionReloadSmoke")))
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, this, &ARoomController::RunExpansionSmokeTest, 4, false);
    }
}
void ARoomController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Research)
        Research->Cancel();
    Research.Reset();
    if (GEngine && GEngine->GameViewport)
        GEngine->GameViewport->OnWindowCloseRequested().Unbind();
    if (Widget.IsValid() && GEngine && GEngine->GameViewport)
        GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
    Widget.Reset();
    Super::EndPlay(Reason);
}

bool ARoomController::ConfirmClose()
{
    if (!Document.Dirty())
        return true;
    if (FApp::IsUnattended())
        return false;
    const EAppReturnType::Type Choice =
        FMessageDialog::Open(EAppMsgType::YesNoCancel,
                             FText::FromString(TEXT("未保存の変更があります。保存して終了しますか？\nはい：保存して終了"
                                                    "\nいいえ：変更を破棄\nキャンセル：編集に戻る")),
                             FText::FromString(TEXT("余白 — レイアウトの保存")));
    if (Choice == EAppReturnType::Yes)
    {
        Save();
        return !Document.Dirty();
    }
    return Choice == EAppReturnType::No;
}
bool ARoomController::InputKey(const FInputKeyEventArgs &Params)
{
    const bool Handled = Super::InputKey(Params);
    const FKey Key = Params.Key;
    if (Key == EKeys::LeftControl || Key == EKeys::RightControl || Key == EKeys::LeftShift || Key == EKeys::RightShift)
    {
        if (Params.Event == IE_Pressed)
            ModifierKeys.Add(Key);
        else if (Params.Event == IE_Released)
            ModifierKeys.Remove(Key);
        return Handled;
    }
    if (!Projection || !ViewCamera)
        return Handled;
    // Dispatch while the viewport still holds this event's cursor and modifier state.
    // Tick-based bindings lose both when press/move/release arrive in one frame.
    if (Params.Event == IE_Released)
    {
        if (Key == EKeys::LeftMouseButton)
        {
            LeftUp();
            return true;
        }
        if (Key == EKeys::RightMouseButton)
        {
            RightUp();
            return true;
        }
        return Handled;
    }
    if (Params.Event != IE_Pressed && Params.Event != IE_DoubleClick)
        return Handled;
    if (Key == EKeys::LeftMouseButton)
    {
        LeftDown();
        return true;
    }
    if (Key == EKeys::RightMouseButton)
    {
        RightDown();
        return true;
    }
    if (Key == EKeys::MouseScrollUp)
    {
        ZoomIn();
        return true;
    }
    if (Key == EKeys::MouseScrollDown)
    {
        ZoomOut();
        return true;
    }
    if (Key == EKeys::Escape)
    {
        Cancel();
        return true;
    }
    const bool Ctrl = ModifierKeys.Contains(EKeys::LeftControl) || ModifierKeys.Contains(EKeys::RightControl);
    const bool Shift = ModifierKeys.Contains(EKeys::LeftShift) || ModifierKeys.Contains(EKeys::RightShift);
    if (Ctrl)
    {
        if (Key == EKeys::Z)
        {
            if (Shift)
                Redo();
            else
                Undo();
            return true;
        }
        if (Key == EKeys::Y)
        {
            Redo();
            return true;
        }
        if (Key == EKeys::S)
        {
            Save();
            return true;
        }
        if (Key == EKeys::O)
        {
            Load();
            return true;
        }
    }
    else
    {
        if (Key == EKeys::R)
        {
            Rotate();
            return true;
        }
        if (Key == EKeys::Delete)
        {
            DeleteSelected();
            return true;
        }
    }
    return Handled;
}
void ARoomController::FlushPressedKeys()
{
    Super::FlushPressedKeys();
    ModifierKeys.Reset();
    bOrbit = false;
    Cancel();
}
bool ARoomController::FloorPoint(FVector2D &Out) const
{
    FVector Origin, Direction;
    if (!DeprojectMousePositionToWorld(Origin, Direction) || FMath::Abs(Direction.Z) < .00001)
        return false;
    const double T = -Origin.Z / Direction.Z;
    if (T < 0)
        return false;
    const FVector P = Origin + Direction * T;
    Out = FVector2D(P.X * 10, P.Y * 10);
    return true;
}
const Yohaku::FItem *ARoomController::SelectedItem() const
{
    return Document.Get().Items.FindByPredicate([&](const auto &I) { return I.Id == Selected; });
}
void ARoomController::Refresh()
{
    if (!Document.Get().Room(ActiveRoomId))
        ActiveRoomId = Document.Get().Rooms[0].Id;
    Projection->Reconcile(Document.Get(), Selected);
    CachedWarnings = Yohaku::Warnings(Document.Get());
    PassageResult = Yohaku::FPassageResult();
    if (!SelectedItem())
        Selected.Invalidate();
}
bool ARoomController::Execute(const Yohaku::FEdit &E)
{
    LoadArmedUntil = 0;
    FString Error;
    if (!Document.Commit(E, Error))
    {
        Notice = Error.IsEmpty() ? TEXT("操作を適用できません") : Error;
        return false;
    }
    Notice.Reset();
    Refresh();
    return true;
}
void ARoomController::StartPlacement(Yohaku::EVariant V)
{
    Cancel();
    Tool = ERoomTool::Place;
    Preview = Yohaku::FItem();
    Preview.AssetId = Yohaku::VariantId(V);
    Preview.RoomId = ActiveRoomId;
    Preview.Yaw = PI;
    Notice = TEXT("床をクリックして配置 / Escで中止");
}
void ARoomController::SetTool(ERoomTool T)
{
    Cancel();
    Tool = T;
    if (T == ERoomTool::DrawRoom || T == ERoomTool::AddRoom || T == ERoomTool::Polygon)
    {
        View(1);
        Notice = TEXT("床面を対角にドラッグして部屋を作成。寸法は入力値です");
    }
}
void ARoomController::LeftDown()
{
    if (bOrbit)
        return;
    FVector2D P;
    if (!FloorPoint(P))
        return;
    if (Tool == ERoomTool::PassageStart || Tool == ERoomTool::PassageEnd)
    {
        if (Tool == ERoomTool::PassageStart)
            PassageStart = P;
        else
            PassageEnd = P;
        Tool = ERoomTool::Select;
        Notice = TEXT("通路の端点を設定しました");
        return;
    }
    if (Tool == ERoomTool::Polygon)
    {
        if (!PolygonPoints.IsEmpty())
        {
            auto Last = PolygonPoints.Last();
            if (FMath::Abs(P.X - Last.X) > FMath::Abs(P.Y - Last.Y))
                P.Y = Last.Y;
            else
                P.X = Last.X;
        }
        P.X = FMath::GridSnap(P.X, 100.);
        P.Y = FMath::GridSnap(P.Y, 100.);
        if (PolygonPoints.IsEmpty() || !P.Equals(PolygonPoints.Last()))
            PolygonPoints.Add(P);
        return;
    }
    if (Tool == ERoomTool::Place)
    {
        UpdatePointerPose();
        Yohaku::FEdit E;
        E.Type = Yohaku::EEditType::Add;
        E.Item = Preview;
        if (Execute(E))
        {
            Selected = Preview.Id;
            Tool = ERoomTool::Select;
            Projection->HideGhost();
            Refresh();
        }
        return;
    }
    if (Tool == ERoomTool::DrawRoom || Tool == ERoomTool::AddRoom)
    {
        bDrawing = true;
        DrawStart = P;
        return;
    }
    FHitResult Hit;
    GetHitResultUnderCursor(ECC_Visibility, false, Hit);
    auto *Actor = Cast<AYFurnitureActor>(Hit.GetActor());
    Selected = Actor ? Actor->InstanceId : FGuid();
    if (Actor)
    {
        if (const auto *I = SelectedItem())
            ActiveRoomId = I->RoomId;
    }
    else
    {
        const FGuid Room = Projection->PickRoom(Hit.GetComponent());
        if (Room.IsValid())
            ActiveRoomId = Room;
        SelectedWall = Projection->PickWall(Hit.GetComponent());
        SelectedOpening.Invalidate();
    }
    Refresh();
    if (const auto *I = SelectedItem())
    {
        Preview = *I;
        GrabOffset = FVector2D(I->X, I->Z) - P;
        float X = 0, Y = 0;
        GetMousePosition(X, Y);
        DragPointerStart = FVector2D(X, Y);
        bDragMoved = false;
        bDragging = true;
    }
}
void ARoomController::LeftUp()
{
    if (bDragging)
    {
        UpdatePointerPose();
        CommitDrag();
    }
    if (bDrawing)
    {
        bDrawing = false;
        FVector2D P;
        if (FloorPoint(P))
        {
            if (Tool == ERoomTool::AddRoom)
            {
                Yohaku::FEdit E;
                E.Type = Yohaku::EEditType::AddRoom;
                E.Room =
                    Yohaku::FRoom::Rectangle(FMath::GridSnap(FMath::Min(P.X, DrawStart.X), 100.),
                                             FMath::GridSnap(FMath::Min(P.Y, DrawStart.Y), 100.),
                                             FMath::GridSnap(FMath::Max(500., FMath::Abs(P.X - DrawStart.X)), 100.),
                                             FMath::GridSnap(FMath::Max(500., FMath::Abs(P.Y - DrawStart.Y)), 100.),
                                             FString::Printf(TEXT("部屋%d"), Document.Get().Rooms.Num() + 1));
                if (Execute(E))
                    ActiveRoomId = E.Room.Id;
            }
            else
                SetRoom(FMath::Clamp(FMath::Abs(P.X - DrawStart.X), 500., 30000.),
                        FMath::Clamp(FMath::Abs(P.Y - DrawStart.Y), 500., 30000.), CurrentRoom().Height);
        }
        Tool = ERoomTool::Select;
    }
}
void ARoomController::CommitDrag()
{
    bDragging = false;
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Move;
    E.Target = Selected;
    E.Item = Preview;
    Execute(E);
}
void ARoomController::RightDown()
{
    bOrbit = true;
    float X, Y;
    GetMousePosition(X, Y);
    LastMouse = FVector2D(X, Y);
}
void ARoomController::RightUp()
{
    UpdateOrbit();
    bOrbit = false;
}
void ARoomController::UpdateOrbit()
{
    if (bOrbit)
    {
        float X, Y;
        if (GetMousePosition(X, Y))
        {
            const FVector2D Delta = FVector2D(X, Y) - LastMouse;
            LastMouse = FVector2D(X, Y);
            if (ModifierKeys.Contains(EKeys::LeftShift) || ModifierKeys.Contains(EKeys::RightShift))
            {
                const FVector Right = ViewCamera->GetActorRightVector(),
                              Forward = FVector::CrossProduct(FVector::UpVector, Right);
                CameraTarget -= Right * Delta.X * Distance * .001;
                CameraTarget += Forward * Delta.Y * Distance * .001;
            }
            else
            {
                OrbitYaw -= Delta.X * .25;
                OrbitPitch = FMath::Clamp(OrbitPitch + Delta.Y * .2, 15.0, 89.0);
            }
            UpdateCamera();
        }
    }
}
void ARoomController::UpdatePointerPose()
{
    FVector2D P;
    if ((Tool == ERoomTool::Place || bDragging) && !bOrbit && FloorPoint(P))
    {
        if (bDragging)
        {
            float X = 0, Y = 0;
            if (!GetMousePosition(X, Y))
                return;
            if (!bDragMoved && FVector2D::DistSquared(DragPointerStart, FVector2D(X, Y)) < 9.0)
                return;
            bDragMoved = true;
            P += GrabOffset;
        }
        const double Step = bSnap ? 100 : 0.001;
        Preview.X = FMath::GridSnap(P.X, Step);
        Preview.Z = FMath::GridSnap(P.Y, Step);
        for (const auto &R : Document.Get().Rooms)
            if (Yohaku::PointInRoom(R, {Preview.X, Preview.Z}))
            {
                Preview.RoomId = R.Id;
                break;
            }
        if (bDragging)
            Projection->PreviewPose(Preview);
        else
            Projection->ShowGhost(Preview);
    }
}
void ARoomController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);
    if (Research)
    {
        Research->LocalGeneration.Poll();
        if (Research->LocalGeneration.Result.IsSet())
        {
            const auto &A = Research->LocalGeneration.Result.GetValue();
            if (!ModelLibrary.ContainsByPredicate([&](const auto &V) { return V.Id == A.Id; }))
                ModelLibrary.Add(A);
        }
    }
    UpdateOrbit();
    UpdatePointerPose();
    Projection->Cutaway(ViewCamera->GetActorLocation());
}
void ARoomController::UpdateCamera()
{
    const double Y = FMath::DegreesToRadians(OrbitYaw), P = FMath::DegreesToRadians(OrbitPitch);
    const FVector At =
        CameraTarget + Distance * FVector(FMath::Cos(P) * FMath::Cos(Y), FMath::Cos(P) * FMath::Sin(Y), FMath::Sin(P));
    ViewCamera->SetActorLocation(At);
    ViewCamera->SetActorRotation((CameraTarget - At).Rotation());
}
void ARoomController::ZoomIn()
{
    Distance = FMath::Clamp(Distance * .9, 120.0, 4000.0);
    UpdateCamera();
}
void ARoomController::ZoomOut()
{
    Distance = FMath::Clamp(Distance / .9, 120.0, 4000.0);
    UpdateCamera();
}
void ARoomController::View(int32 P)
{
    auto Minimum = CurrentRoom().Min(), Maximum = CurrentRoom().Max();
    if (P != 2)
        for (const auto &R : Document.Get().Rooms)
        {
            Minimum.X = FMath::Min(Minimum.X, R.Min().X);
            Minimum.Y = FMath::Min(Minimum.Y, R.Min().Y);
            Maximum.X = FMath::Max(Maximum.X, R.Max().X);
            Maximum.Y = FMath::Max(Maximum.Y, R.Max().Y);
        }
    const auto Center = (Minimum + Maximum) / 20;
    CameraTarget = FVector(Center.X, Center.Y, 45);
    OrbitPitch = P == 1 ? 89 : P == 2 ? 18 : 54;
    Distance = P == 2 ? 480 : FMath::Max(Maximum.X - Minimum.X, Maximum.Y - Minimum.Y) / 10 * 1.7;
    OrbitYaw = -90;
    UpdateCamera();
}
void ARoomController::SetRoom(double W, double D, double H)
{
    Cancel();
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Room;
    E.RoomId = ActiveRoomId;
    E.Width = W;
    E.Depth = D;
    E.Height = H;
    if (CurrentRoom().Polygon.Num() != 4)
    {
        E.Type = Yohaku::EEditType::SetPolygon;
        E.Room = CurrentRoom();
        E.Room.Height = H;
    }
    if (Execute(E))
        View(0);
}
void ARoomController::SetFinish(int32 F)
{
    Cancel();
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Surface;
    E.RoomId = ActiveRoomId;
    E.WallId = SelectedWall;
    E.Surface = Surface;
    E.Finish = F;
    Execute(E);
}
void ARoomController::EditPose(double X, double Z, double Yaw)
{
    if (!SelectedItem())
        return;
    Cancel();
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Move;
    E.Target = Selected;
    E.Item = *SelectedItem();
    E.Item.X = X;
    E.Item.Z = Z;
    E.Item.Yaw = FMath::DegreesToRadians(Yaw);
    Execute(E);
}
void ARoomController::ChangeVariant(Yohaku::EVariant V)
{
    if (bKeepWallDistance && !SelectedWall.IsValid())
    {
        Notice = TEXT("基準壁を選択してください");
        return;
    }
    if (!SelectedItem())
        return;
    Cancel();
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Replace;
    if (bKeepWallDistance)
        E.AnchorWallId = SelectedWall;
    E.Target = Selected;
    E.Item.AssetId = Yohaku::VariantId(V);
    Execute(E);
}
void ARoomController::Rotate()
{
    if (Tool == ERoomTool::Place)
    {
        Preview.Yaw += PI / 4;
        return;
    }
    if (const auto *I = SelectedItem())
        EditPose(I->X, I->Z, FMath::RadiansToDegrees(I->Yaw) + 45);
}
void ARoomController::Duplicate()
{
    if (!SelectedItem())
        return;
    Cancel();
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Duplicate;
    E.Target = Selected;
    E.Item.Id = FGuid::NewGuid();
    E.Item.X = SelectedItem()->X + 200;
    E.Item.Z = SelectedItem()->Z + 200;
    if (Execute(E))
    {
        Selected = E.Item.Id;
        Refresh();
    }
}
void ARoomController::DeleteSelected()
{
    if (!SelectedItem())
        return;
    Cancel();
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Remove;
    E.Target = Selected;
    Execute(E);
    Selected.Invalidate();
}
void ARoomController::Undo()
{
    Cancel();
    Document.Undo();
    Refresh();
}
void ARoomController::Redo()
{
    Cancel();
    Document.Redo();
    Refresh();
}
void ARoomController::Cancel()
{
    bDragging = false;
    bDrawing = false;
    PolygonPoints.Reset();
    Tool = ERoomTool::Select;
    if (Projection)
    {
        Projection->HideGhost();
        Refresh();
    }
    Notice.Reset();
}
void ARoomController::Save()
{
    Cancel();
    FString Error;
    if (Yohaku::FLayoutStorage::Save(Yohaku::Encode(Document.Get()), Error))
    {
        Document.MarkSaved();
        Notice = TEXT("このPCに保存しました");
    }
    else
        Notice = Error;
}
void ARoomController::Load()
{
    if (Document.Dirty() && LoadArmedUntil < GetWorld()->GetTimeSeconds())
    {
        LoadArmedUntil = GetWorld()->GetTimeSeconds() + 6;
        Notice = TEXT("未保存の変更があります。6秒以内にもう一度「読込」で破棄して読み込みます");
        return;
    }
    LoadArmedUntil = 0;
    Cancel();
    FString Json, Error;
    if (Yohaku::FLayoutStorage::Load(Json, Error) && Document.ReplaceLoaded(Json, Error))
    {
        Selected.Invalidate();
        Refresh();
        View(0);
        Notice = TEXT("保存したレイアウトを読み込みました");
    }
    else
        Notice = Error;
}
FString ARoomController::Status() const
{
    const auto &W = CachedWarnings;
    const FString Message = !Notice.IsEmpty() ? Notice
                            : W.IsEmpty()     ? TEXT("検証用レイアウト / 掲載値・形状・素材は正式採用前")
                                              : W[0].Message;
    return FString::Printf(TEXT("%s  |  %d点  |  %d件の形状警告（近似）\n%s"),
                           Document.Dirty() ? TEXT("未保存") : TEXT("このPCに保存済み"), Document.Get().Items.Num(),
                           W.Num(), *Message);
}

const Yohaku::FRoom &ARoomController::CurrentRoom() const
{
    const auto *R = Document.Get().Room(ActiveRoomId);
    return R ? *R : Document.Get().Rooms[0];
}
void ARoomController::ChooseRoom(FGuid Id)
{
    if (!Document.Get().Room(Id))
        return;
    Cancel();
    ActiveRoomId = Id;
    SelectedWall.Invalidate();
    SelectedOpening.Invalidate();
    Surface = Yohaku::ESurface::Floor;
    View(0);
}
void ARoomController::StartPolygon(bool Replace)
{
    Cancel();
    bReplacePolygon = Replace;
    Tool = ERoomTool::Polygon;
    View(1);
    Notice = TEXT("床をクリックして直交する頂点を追加し、「間取りを確定」で閉じます。Escで取消");
}
void ARoomController::FinishPolygon()
{
    if (Tool != ERoomTool::Polygon)
        return;
    Yohaku::FRoom Room = CurrentRoom();
    Room.Polygon = PolygonPoints;
    if (Room.Polygon.Num() > 1 && Room.Polygon.Last().Equals(Room.Polygon[0]))
        Room.Polygon.Pop();
    double Area = 0;
    for (int32 I = 0; I < Room.Polygon.Num(); ++I)
    {
        const auto A = Room.Polygon[I], B = Room.Polygon[(I + 1) % Room.Polygon.Num()];
        Area += A.X * B.Y - A.Y * B.X;
    }
    if (Area < 0)
        Algo::Reverse(Room.Polygon);
    Room.Walls.Reset();
    for (int32 I = 0; I < Room.Polygon.Num(); ++I)
        Room.Walls.Add(Yohaku::FWall());
    Yohaku::FEdit E;
    E.RoomId = ActiveRoomId;
    E.Type = bReplacePolygon ? Yohaku::EEditType::SetPolygon : Yohaku::EEditType::AddRoom;
    if (!bReplacePolygon)
    {
        Room.Id = FGuid::NewGuid();
        Room.Name = FString::Printf(TEXT("部屋%d"), Document.Get().Rooms.Num() + 1);
    }
    E.Room = Room;
    if (Execute(E))
    {
        PolygonPoints.Reset();
        Tool = ERoomTool::Select;
        ChooseRoom(Room.Id);
    }
}
void ARoomController::RemoveRoom()
{
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::RemoveRoom;
    E.RoomId = ActiveRoomId;
    Cancel();
    if (Execute(E))
        ChooseRoom(Document.Get().Rooms[0].Id);
}
void ARoomController::AddOpening(Yohaku::EOpeningKind Kind)
{
    if (!SelectedWall.IsValid())
    {
        Notice = TEXT("開口を作る壁を選択してください");
        return;
    }
    Yohaku::FOpening O;
    if (SelectedOpening.IsValid())
        O.Id = SelectedOpening;
    O.WallId = SelectedWall;
    O.Kind = Kind;
    O.Offset = OpeningOffset;
    O.Width = OpeningWidth;
    O.Height = OpeningHeight;
    O.Sill = Kind == Yohaku::EOpeningKind::Door ? 0 : OpeningSill;
    O.HingeAtEnd = bHingeAtEnd;
    O.SwingInward = bSwingInward;
    auto Ws = Yohaku::Walls(Document.Get());
    const auto *W = Ws.FindByPredicate([&](const auto &V) { return V.Id == SelectedWall; });
    if (!W)
        return;
    for (const auto &Other : Ws)
        if (Other.RoomId != W->RoomId && Other.A.Equals(W->B, .001) && Other.B.Equals(W->A, .001))
        {
            O.OtherWallId = Other.Id;
            break;
        }
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Opening;
    E.Opening = O;
    Cancel();
    if (Execute(E))
        SelectedOpening = O.Id;
}
void ARoomController::SelectOpening(FGuid Id)
{
    const auto *O = Document.Get().Openings.FindByPredicate([&](const auto &V) { return V.Id == Id; });
    if (!O)
    {
        SelectedOpening.Invalidate();
        return;
    }
    SelectedOpening = Id;
    SelectedWall = O->WallId;
    OpeningOffset = O->Offset;
    OpeningWidth = O->Width;
    OpeningHeight = O->Height;
    OpeningSill = O->Sill;
    bHingeAtEnd = O->HingeAtEnd;
    bSwingInward = O->SwingInward;
    for (const auto &W : Yohaku::Walls(Document.Get()))
        if (W.Id == O->WallId)
            ActiveRoomId = W.RoomId;
}
void ARoomController::RemoveOpening()
{
    if (!SelectedOpening.IsValid())
        return;
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::RemoveOpening;
    E.Target = SelectedOpening;
    Cancel();
    if (Execute(E))
        SelectedOpening.Invalidate();
}
void ARoomController::CheckPassage()
{
    if (!PassageStart.IsSet() || !PassageEnd.IsSet())
    {
        Notice = TEXT("床をクリックして通路の始点と終点を指定してください");
        return;
    }
    Cancel();
    PassageResult = Yohaku::Passage(Document.Get(), PassageStart.GetValue(), PassageEnd.GetValue(), PassageWidth);
    Notice = PassageResult.Message;
}
void ARoomController::StartPlacementAsset(const FString &Id)
{
    if (!Document.Get().Asset(Id))
    {
        Notice = TEXT("配置する家具版がありません");
        return;
    }
    Cancel();
    Tool = ERoomTool::Place;
    Preview = Yohaku::FItem();
    Preview.AssetId = Id;
    Preview.RoomId = ActiveRoomId;
    Preview.Yaw = PI;
    Notice = TEXT("床をクリックして試作を配置 / 掲載寸法に合わせた形状・素材と背面は未検証");
}
void ARoomController::PlaceGenerated()
{
    if (!Research || !Research->LocalGeneration.Result.IsSet())
        return;
    const auto A = Research->LocalGeneration.Result.GetValue();
    if (!Document.Get().Asset(A.Id))
    {
        Yohaku::FEdit E;
        E.Type = Yohaku::EEditType::AddAsset;
        E.Asset = A;
        if (!Execute(E))
            return;
    }
    StartPlacementAsset(A.Id);
    bShowResearch = false;
}
void ARoomController::ReplaceAsset(const FString &Id)
{
    if (!SelectedItem())
        return;
    if (bKeepWallDistance && !SelectedWall.IsValid())
    {
        Notice = TEXT("距離を保つ基準壁を選択してください");
        return;
    }
    Cancel();
    Yohaku::FEdit E;
    E.Type = Yohaku::EEditType::Replace;
    E.Target = Selected;
    E.Item.AssetId = Id;
    if (bKeepWallDistance)
        E.AnchorWallId = SelectedWall;
    Execute(E);
}

void ARoomController::PlaceStudyAsset(const Yohaku::FAsset &A)
{
    if (!Document.Get().Asset(A.Id))
    {
        Yohaku::FEdit E;
        E.Type = Yohaku::EEditType::AddAsset;
        E.Asset = A;
        if (!Execute(E))
            return;
    }
    StartPlacementAsset(A.Id);
    bShowResearch = false;
}
