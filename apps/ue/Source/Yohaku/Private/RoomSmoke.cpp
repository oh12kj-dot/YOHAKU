#include "RoomController.h"
#include "SceneProjection.h"
#include "LayoutStorage.h"
#include "FurnitureDefinition.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputKeyEventArgs.h"

void ARoomController::RunSmokeTest()
{
    using namespace Yohaku;
    TArray<FString> Checks;
    bool Ok = true;
    auto Check = [&](bool Value, const TCHAR *Label) {
        Checks.Add(FString(Value ? TEXT("PASS ") : TEXT("FAIL ")) + Label);
        Ok &= Value;
    };
    FString TestRoot;
    Check(FParse::Value(FCommandLine::Get(), TEXT("YohakuLayoutRoot="), TestRoot),
          TEXT("Explicit isolated test storage"));
    if (!Ok)
    {
        UE_LOG(LogTemp, Error, TEXT("YOHAKU_SMOKE_REQUIRES_ISOLATED_STORAGE"));
        return;
    }
    FString Error;
    const bool Reload = FParse::Param(FCommandLine::Get(), TEXT("YohakuReloadSmoke"));
    if (Reload)
    {
        FString Json;
        Check(FLayoutStorage::Load(Json, Error) && Document.ReplaceLoaded(Json, Error), TEXT("Load in second process"));
        Refresh();
        View(0);
        Check(Document.Get().Rooms[0].Width() == 6200 && Document.Get().Items.Num() == 1, TEXT("Restart restores room and count"));
        if (Document.Get().Items.Num() == 1)
        {
            Selected = Document.Get().Items[0].Id;
            Check(SelectedItem()->AssetId == VariantId(EVariant::BlueGray) && SelectedItem()->X == 2600 &&
                      SelectedItem()->Z == 2400,
                  TEXT("Restart restores IDs, variant and pose"));
        }
    }
    else
    {
        Check(!ConfirmClose(), TEXT("Unattended dirty close cannot silently discard"));
        SetRoom(6200, 4600, 2600);
        Check(Document.Get().Rooms[0].Width() == 6200, TEXT("Native room command"));
        for (int32 I = 0; I < 5; ++I)
        {
            Surface = static_cast<ESurface>(I);
            SetFinish(I == 0 ? 0 : 1);
        }
        Surface = ESurface::Floor;
        Check(Document.Get().Rooms[0].FloorFinish == 0 && Document.Get().Rooms[0].Walls[0].Finish == 1 && Document.Get().Rooms[0].Walls[1].Finish == 1 && Document.Get().Rooms[0].Walls[2].Finish == 1 && Document.Get().Rooms[0].Walls[3].Finish == 1, TEXT("Independent floor and all four walls"));
        FEdit Add;
        Add.Type = EEditType::Add;
        Add.Item.X = 2200;
        Add.Item.Z = 2200;
        Add.Item.Yaw = PI;
        Check(Execute(Add), TEXT("Place study candidate"));
        Selected = Add.Item.Id;
        Refresh();
        auto *Actor = Projection->Find(Selected);
        Check(Actor && Actor->Parts.Num() == 36, TEXT("36 runtime sofa mesh parts"));
        Check(Projection->Brown && Projection->Brown->bStudyCandidate, TEXT("Candidate provenance retained"));
        bool MaterialsReady = true;
        for (const auto *Definition : {Projection->Brown.Get(), Projection->BlueGray.Get()})
        {
            if (!Definition || Definition->Parts.Num() != 36)
            {
                MaterialsReady = false;
                continue;
            }
            for (const auto &Part : Definition->Parts)
                MaterialsReady &= Part.Material && Part.Material->GetMaterial() &&
                                  Part.Material->GetMaterial()->GetUsageByFlag(MATUSAGE_Nanite) &&
                                  Part.Material->CheckMaterialUsage_Concurrent(MATUSAGE_Nanite);
        }
        Check(MaterialsReady, TEXT("Both furniture variants retain Nanite material usage in cooked build"));
        const int32 BeforeDrag = Document.UndoCount();
        const Yohaku::FScene Before = Document.Get();
        bDragging = true;
        Preview = *SelectedItem();
        Preview.X = 2600;
        Preview.Z = 2400;
        Projection->PreviewPose(Preview);
        Check(Document.Get() == Before, TEXT("Gesture preview does not mutate document"));
        Cancel();
        Check(Document.Get() == Before && Document.UndoCount() == BeforeDrag,
              TEXT("Cancel discards preview without history"));
        bDragging = true;
        Preview = *SelectedItem();
        Preview.X = 2600;
        Preview.Z = 2400;
        Projection->PreviewPose(Preview);
        CommitDrag();
        Check(Document.UndoCount() == BeforeDrag + 1, TEXT("Gesture commit creates exactly one history entry"));
        const Yohaku::FScene BeforeKeys = Document.Get();
        auto Key = [this](FKey K, EInputEvent Event) { InputKey(FInputKeyEventArgs::CreateSimulated(K, Event, 1.0f)); };
        Key(EKeys::R, IE_Pressed);
        Key(EKeys::R, IE_Released);
        const Yohaku::FScene Rotated = Document.Get();
        Check(Rotated != BeforeKeys, TEXT("Ordered key press rotates immediately"));
        Key(EKeys::LeftControl, IE_Pressed);
        Key(EKeys::Z, IE_Pressed);
        Key(EKeys::Z, IE_Released);
        Key(EKeys::LeftControl, IE_Released);
        Check(Document.Get() == BeforeKeys, TEXT("Same-frame Ctrl+Z restores scene"));
        Key(EKeys::RightControl, IE_Pressed);
        Key(EKeys::RightShift, IE_Pressed);
        Key(EKeys::Z, IE_Pressed);
        Key(EKeys::Z, IE_Released);
        Key(EKeys::RightShift, IE_Released);
        Key(EKeys::RightControl, IE_Released);
        Check(Document.Get() == Rotated, TEXT("Same-frame right Ctrl+Shift+Z redoes scene"));
        Undo();
        Key(EKeys::LeftControl, IE_Pressed);
        bDragging = true;
        Preview = *SelectedItem();
        Preview.X += 100;
        FlushPressedKeys();
        Check(!IsInteracting() && Document.Get() == BeforeKeys && ModifierKeys.IsEmpty(),
              TEXT("Focus flush cancels preview and clears modifiers"));
        const double X = SelectedItem()->X, Z = SelectedItem()->Z, Yaw = SelectedItem()->Yaw;
        ChangeVariant(EVariant::BlueGray);
        Check(SelectedItem()->X == X && SelectedItem()->Z == Z && SelectedItem()->Yaw == Yaw,
              TEXT("Variant swap preserves pose"));
        Actor = Projection->Find(Selected);
        FBox Bounds(ForceInit);
        for (auto Part : Actor->Parts)
            Bounds += Part->Bounds.GetBox();
        const FVector Size = Bounds.GetSize();
        Check(FMath::IsNearlyEqual(Size.X, 150.0, .02) && FMath::IsNearlyEqual(Size.Y, 87.0, .02) &&
                  FMath::IsNearlyEqual(Size.Z, 82.0, .02) && FMath::Abs(Bounds.Min.Z) < .01,
              TEXT("Runtime fixed centimetre bounds and floor"));
        const Yohaku::FScene CameraBefore = Document.Get();
        View(1);
        View(0);
        Check(Document.Get() == CameraBefore, TEXT("Camera presets do not change furniture"));
        Duplicate();
        Check(Document.Get().Items.Num() == 2, TEXT("Duplicate"));
        DeleteSelected();
        Check(Document.Get().Items.Num() == 1, TEXT("Delete"));
        Undo();
        Check(Document.Get().Items.Num() == 2, TEXT("Undo delete"));
        Redo();
        Check(Document.Get().Items.Num() == 1, TEXT("Redo delete"));
        Selected = Document.Get().Items[0].Id;
        Save();
        Check(!Document.Dirty(), TEXT("Atomic local save"));
        Check(ConfirmClose(), TEXT("Saved document permits close"));
        const Yohaku::FScene Kept = Document.Get();
        FString Saved;
        FLayoutStorage::Load(Saved, Error);
        Check(!Document.ReplaceLoaded(TEXT("{bad"), Error) && Document.Get() == Kept,
              TEXT("Corrupt input retains scene"));
        const FString BadAsset = Saved.Replace(*VariantId(EVariant::BlueGray), TEXT("unknown-version"));
        Check(!Document.ReplaceLoaded(BadAsset, Error) && Document.Get() == Kept, TEXT("Unknown asset retains scene"));
        Check(Document.ReplaceLoaded(Saved, Error) && Document.Get() == Kept, TEXT("Local save/load exact round trip"));
        Refresh();
    }
    Selected.Invalidate();
    Refresh();
    View(0);
    Notice = Ok ? TEXT("ネイティブ動作検証完了 / このPCに保存済み") : TEXT("検証で失敗があります");
    const auto Evidence = MakeShared<FJsonObject>();
    Evidence->SetBoolField(TEXT("passed"), Ok);
    Evidence->SetBoolField(TEXT("restart"), Reload);
    Evidence->SetStringField(TEXT("storage"), FLayoutStorage::Path());
    Evidence->SetStringField(TEXT("scene"), Encode(Document.Get()));
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (auto &S : Checks)
        Rows.Add(MakeShared<FJsonValueString>(S));
    Evidence->SetArrayField(TEXT("checks"), Rows);
    FString Text;
    auto Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Evidence, Writer);
    const FString EvidencePath = TestRoot / (Reload ? TEXT("restart-evidence.json") : TEXT("runtime-evidence.json"));
    FFileHelper::SaveStringToFile(Text, *EvidencePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("YOHAKU_NATIVE_SMOKE %s %s"), Ok ? TEXT("PASS") : TEXT("FAIL"), *EvidencePath);
    FTimerHandle Shot;
    GetWorldTimerManager().SetTimer(
        Shot,
        FTimerDelegate::CreateWeakLambda(
            this,
            [this, TestRoot, Reload]() {
                FScreenshotRequest::RequestScreenshot(
                    TestRoot / (Reload ? TEXT("native-restart.png") : TEXT("native-editor.png")), true, false);
                FTimerHandle Quit;
                GetWorldTimerManager().SetTimer(
                    Quit,
                    FTimerDelegate::CreateWeakLambda(
                        this, [this]() { UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); }),
                    3, false);
            }),
        30, false);
}
