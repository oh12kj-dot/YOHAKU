#include "RoomController.h"
#include "LocalModels.h"
#include "LayoutStorage.h"
#include "SceneProjection.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealClient.h"
void ARoomController::RunExpansionSmokeTest()
{
    using namespace Yohaku;
    FString Root, Models, Job;
    bool Ok = FParse::Value(FCommandLine::Get(), TEXT("YohakuLayoutRoot="), Root) &&
              FParse::Value(FCommandLine::Get(), TEXT("YohakuModelRoot="), Models);
    TArray<FString> Checks;
    auto Check = [&](bool Pass, const TCHAR *Label) {
        Checks.Add(FString(Pass ? TEXT("PASS ") : TEXT("FAIL ")) + Label);
        Ok &= Pass;
    };
    if (!Ok)
    {
        UE_LOG(LogTemp, Error, TEXT("EXPANSION_REQUIRES_ISOLATED_ROOTS"));
        UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
        return;
    }
    FString Error;
    bool Reload = FParse::Param(FCommandLine::Get(), TEXT("YohakuExpansionReloadSmoke"));
    if (!Reload)
    {
        FEdit Room;
        Room.Type = EEditType::AddRoom;
        Room.Room = FRoom::Rectangle(5000, 0, 3000, 2000, TEXT("隣室"));
        Check(Execute(Room), TEXT("Second room added"));
        FGuid Second = Room.Room.Id;
        FEdit Shape;
        Shape.Type = EEditType::AddRoom;
        Shape.Room = FRoom::Rectangle(0, 5000, 3000, 3000, TEXT("L字の部屋"));
        Shape.Room.Polygon = {{0, 5000}, {3000, 5000}, {3000, 6500}, {1500, 6500}, {1500, 8000}, {0, 8000}};
        Shape.Room.Walls.SetNum(6);
        Check(Execute(Shape), TEXT("Concave room added"));
        FGuid Third = Shape.Room.Id;
        FEdit Door;
        Door.Type = EEditType::Opening;
        auto WallList = Walls(Document.Get());
        for (const auto &A : WallList)
            for (const auto &B : WallList)
                if (A.RoomId == FGuid(0, 0, 0, 1) && B.RoomId == Second && A.A.Equals(B.B, .001) &&
                    A.B.Equals(B.A, .001))
                {
                    Door.Opening.WallId = A.Id;
                    Door.Opening.OtherWallId = B.Id;
                }
        Door.Opening.Offset = 400;
        Door.Opening.Width = 1200;
        Check(Execute(Door), TEXT("Shared doorway added"));
        FEdit Window;
        Window.Type = EEditType::Opening;
        Window.Opening.Kind = EOpeningKind::Window;
        Window.Opening.WallId = FGuid(0, 0, 0, 2);
        Window.Opening.Offset = 1500;
        Window.Opening.Width = 1500;
        Window.Opening.Height = 1000;
        Window.Opening.Sill = 900;
        Check(Execute(Window), TEXT("Window added"));
        Check(FParse::Value(FCommandLine::Get(), TEXT("YohakuStudyJob="), Job),
              TEXT("Explicit real photo generated output"));
        FAsset Generated;
        const bool Published = FLocalModels::Publish(Job, Generated, Error);
        Check(Published, TEXT("Real generated mesh and provenance validated"));
        if (!Published)
            UE_LOG(LogTemp, Error, TEXT("MODEL_PUBLISH_FAILED %s"), *Error);
        if (Published)
        {
            FEdit Asset;
            Asset.Type = EEditType::AddAsset;
            Asset.Asset = Generated;
            Check(Execute(Asset), TEXT("Generated study registered"));
            FEdit Place;
            Place.Type = EEditType::Add;
            Place.Item.AssetId = Generated.Id;
            Place.Item.X = 2500;
            Place.Item.Z = 2200;
            Place.Item.Yaw = PI;
            Check(Execute(Place), TEXT("Generated photo shape placed"));
            auto *Actor = Projection->Find(Place.Item.Id);
            Check(Actor && Actor->Parts.Num() == 1, TEXT("Generated mesh rendered without PSF substitution"));
            const FString Before = Encode(Document.Get());
            FEdit Swap;
            Swap.Type = EEditType::Replace;
            Swap.Target = Place.Item.Id;
            Swap.Item.AssetId = VariantId(EVariant::BlueGray);
            Swap.AnchorWallId = FGuid(0, 0, 0, 2);
            Check(Execute(Swap), TEXT("Wall anchored asset exchange"));
            Undo();
            Check(Encode(Document.Get()) == Before, TEXT("Undo restores exact generated reference and pose"));
        }
        FEdit Place;
        Place.Type = EEditType::Add;
        Place.Item.RoomId = Second;
        Place.Item.X = 6500;
        Place.Item.Z = 1000;
        Place.Item.Yaw = PI;
        Check(Execute(Place), TEXT("Furniture in second room"));
        Place.Item = FItem();
        Place.Item.RoomId = Third;
        Place.Item.AssetId = VariantId(EVariant::BlueGray);
        Place.Item.X = 750;
        Place.Item.Z = 7000;
        Place.Item.Yaw = PI;
        Check(Execute(Place), TEXT("Furniture in concave room"));
        auto Route = Passage(Document.Get(), {4000, 1000}, {5300, 1000}, 700);
        Check(Route.bValid && Route.bFound, TEXT("Shared door passage evaluated"));
        Save();
        Check(!Document.Dirty(), TEXT("V2 scene saved explicitly"));
    }
    Check(Document.Get().Rooms.Num() == 3, TEXT("Three rooms retained"));
    Check(Document.Get().Openings.Num() == 2, TEXT("Door and window retained"));
    Check(Document.Get().Items.Num() == 3, TEXT("Three furniture instances retained"));
    int32 Studies = 0;
    for (const auto &I : Document.Get().Items)
        if (const auto *A = Document.Get().Asset(I.AssetId))
            if (A->bGeneratedStudy)
            {
                ++Studies;
                Check(A->MaterialStatus == TEXT("not-generated") &&
                          A->DimensionStatus == TEXT("single-source-unverified"),
                      TEXT("Generated draft status never promoted"));
                FGeneratedMesh Mesh;
                Check(FLocalModels::ReadMesh(*A, Mesh, Error), TEXT("Generated shape hash rechecked"));
            }
    Check(Studies == 1, TEXT("One actual generated study"));
    const FString Expected = Encode(Document.Get());
    FString Json;
    Check(FLayoutStorage::Load(Json, Error) && Document.ReplaceLoaded(Json, Error) &&
              Encode(Document.Get()) == Expected,
          TEXT("V2 storage exact roundtrip"));
    ActiveRoomId = Document.Get().Rooms[0].Id;
    Selected.Invalidate();
    Refresh();
    View(0);
    auto Evidence = MakeShared<FJsonObject>();
    Evidence->SetBoolField(TEXT("passed"), Ok);
    Evidence->SetBoolField(TEXT("restart"), Reload);
    Evidence->SetStringField(TEXT("scene"), Encode(Document.Get()));
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (auto S : Checks)
        Rows.Add(MakeShared<FJsonValueString>(S));
    Evidence->SetArrayField(TEXT("checks"), Rows);
    FString Out;
    FJsonSerializer::Serialize(Evidence, TJsonWriterFactory<>::Create(&Out));
    FFileHelper::SaveStringToFile(Out,
                                  *(Root / (Reload ? TEXT("expansion-restart.json") : TEXT("expansion-runtime.json"))),
                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("YOHAKU_EXPANSION %s %s"), Ok ? TEXT("PASS") : TEXT("FAIL"), *Error);
    FTimerHandle Shot;
    GetWorldTimerManager().SetTimer(
        Shot,
        FTimerDelegate::CreateWeakLambda(
            this,
            [this, Root, Reload]() {
                FScreenshotRequest::RequestScreenshot(
                    Root / (Reload ? TEXT("expansion-restart.png") : TEXT("expansion.png")), true, false);
                FTimerHandle Quit;
                GetWorldTimerManager().SetTimer(
                    Quit,
                    FTimerDelegate::CreateWeakLambda(
                        this, [this]() { UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); }),
                    3, false);
            }),
        12, false);
}
