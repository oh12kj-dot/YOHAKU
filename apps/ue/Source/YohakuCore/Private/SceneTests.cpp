#include "SceneDocument.h"
#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYohakuSceneContract, "Yohaku.Core.SceneContract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FYohakuSceneContract::RunTest(const FString &)
{
    using namespace Yohaku;
    FDocument D;
    FString Error;
    FEdit Add;
    Add.Type = EEditType::Add;
    Add.Item.X = 1234.56789;
    Add.Item.Z = 2000;
    Add.Item.Yaw = PI / 2;
    TestTrue(TEXT("Add"), D.Commit(Add, Error));
    TestEqual(TEXT("mm normalized"), D.Get().Items[0].X, 1234.568);
    const FScene Before = D.Get();
    FEdit Color;
    Color.Type = EEditType::Variant;
    Color.Target = Add.Item.Id;
    Color.Item.AssetId = VariantId(EVariant::BlueGray);
    TestTrue(TEXT("Color swap"), D.Commit(Color, Error));
    TestEqual(TEXT("Color preserves centre"), D.Get().Items[0].X, Before.Items[0].X);
    TestEqual(TEXT("Color preserves yaw"), D.Get().Items[0].Yaw, Before.Items[0].Yaw);
    TestTrue(TEXT("Undo"), D.Undo());
    TestTrue(TEXT("Redo"), D.Redo());
    D.Undo();
    FEdit Room;
    Room.Type = EEditType::Room;
    Room.Width = 6000;
    D.Commit(Room, Error);
    TestFalse(TEXT("Truncate redo branch"), D.CanRedo());
    for (int32 I = 0; I < 120; ++I)
    {
        Room.Width = 6001 + I;
        D.Commit(Room, Error);
    }
    TestEqual(TEXT("100 history cap"), D.UndoCount(), 100);
    const FString Saved = Encode(D.Get());
    FDocument Loaded;
    TestTrue(TEXT("Round trip"), Loaded.ReplaceLoaded(Saved, Error));
    TestTrue(TEXT("Exact values and IDs"), D.Get() == Loaded.Get());
    TestFalse(TEXT("Loaded history empty"), Loaded.CanUndo());
    const FScene Kept = Loaded.Get();
    TestFalse(TEXT("Corrupt load rejected"), Loaded.ReplaceLoaded(TEXT("{bad"), Error));
    TestTrue(TEXT("Corrupt load preserves scene"), Kept == Loaded.Get());
    TestFalse(TEXT("Unknown asset rejected"),
              Loaded.ReplaceLoaded(Saved.Replace(TEXT("study-psf-v3-brown"), TEXT("foreign-asset")), Error));
    TestTrue(TEXT("Unknown asset preserves scene"), Kept == Loaded.Get());
    TestFalse(
        TEXT("Extra field rejected"),
        Loaded.ReplaceLoaded(Saved.Replace(TEXT("\"version\": 2"), TEXT("\"unexpected\":0,\"version\":2")), Error));
    FEdit Bad;
    Bad.Type = EEditType::Room;
    Bad.Width = 0;
    TestFalse(TEXT("Invalid dimensions rejected"), D.Commit(Bad, Error));
    FEdit Unknown;
    Unknown.Type = static_cast<EEditType>(255);
    TestFalse(TEXT("Unknown command rejected"), D.Commit(Unknown, Error));
    FEdit Rotation;
    Rotation.Type = EEditType::Move;
    Rotation.Target = D.Get().Items[0].Id;
    Rotation.Item = D.Get().Items[0];
    Rotation.Item.Yaw = 1e100;
    TestTrue(TEXT("Huge finite angle finishes and normalizes"), D.Commit(Rotation, Error));
    TestTrue(TEXT("Angle remains canonical"), FMath::Abs(D.Get().Items[0].Yaw) <= UE_DOUBLE_PI);
    const FScene BeforeInvalid = D.Get();
    Rotation.Item.Yaw = std::numeric_limits<double>::infinity();
    TestFalse(TEXT("Infinite angle rejected before normalization"), D.Commit(Rotation, Error));
    TestTrue(TEXT("Infinite angle preserves scene"), D.Get() == BeforeInvalid);
    FEdit Surface;
    Surface.Type = EEditType::Surface;
    Surface.Surface = ESurface::East;
    Surface.Finish = 2;
    D.Commit(Surface, Error);
    TestEqual(TEXT("Stable east ID"), D.Get().Rooms[0].Walls[1].Finish, 2);
    TestEqual(TEXT("Other wall unchanged"), D.Get().Rooms[0].Walls[2].Finish, 0);
    Room.Width = 7000;
    D.Commit(Room, Error);
    TestEqual(TEXT("Room resize preserves wall finish"), D.Get().Rooms[0].Walls[1].Finish, 2);
    FScene WarningScene;
    FItem A;
    A.X = 0;
    A.Z = 0;
    WarningScene.Items.Add(A);
    FItem B = A;
    B.Id = FGuid::NewGuid();
    WarningScene.Items.Add(B);
    const auto Notices=Warnings(WarningScene);
    TestEqual(TEXT("Two outside warnings"), Notices.FilterByPredicate([](const auto& W){return !W.Second.IsValid();}).Num(),2);
    TestEqual(TEXT("One compound furniture collision"), Notices.FilterByPredicate([&](const auto& W){return W.Second==B.Id;}).Num(),1);
    return true;
}
#endif
