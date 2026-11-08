#include "SceneDocument.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpansionContract, "Yohaku.Core.Expansion",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FExpansionContract::RunTest(const FString &)
{
    using namespace Yohaku;
    FDocument D;
    FString Error;
    const FString Legacy =
        TEXT("{\"format\":\"yohaku-local-study\",\"version\":1,\"room\":{\"widthMm\":5000,\"depthMm\":4000,"
             "\"heightMm\":2600},\"finishes\":[2,1,2,3,4],\"furniture\":[{\"id\":\"12345678-1234-1234-1234-"
             "123456789012\",\"assetVersionId\":\"study-psf-v3-brown\",\"xMm\":2600,\"zMm\":3400,\"yawRad\":0}]} ");
    TestTrue(TEXT("Legacy v1 reads without writing"), D.ReplaceLoaded(Legacy, Error));
    TestFalse(TEXT("Converted scene starts clean"), D.Dirty());
    TestEqual(TEXT("Legacy north finish"), D.Get().Rooms[0].Walls[2].Finish, 1);
    TestEqual(TEXT("Legacy west finish"), D.Get().Rooms[0].Walls[3].Finish, 4);
    TestEqual(TEXT("Legacy item position"), D.Get().Items[0].Z, 3400.);
    TestTrue(TEXT("Explicit new serialization is v2"), Encode(D.Get()).Contains(TEXT("\"version\": 2")));
    FDocument Plan;
    FEdit Add;
    Add.Type = EEditType::AddRoom;
    Add.Room = FRoom::Rectangle(5000, 0, 3000, 2000, TEXT("隣室"));
    TestTrue(TEXT("Adjacent partial boundary room"), Plan.Commit(Add, Error));
    const auto WallList = Walls(Plan.Get());
    int32 Shared = 0;
    for (const auto &A : WallList)
        for (const auto &B : WallList)
            if (A.RoomId != B.RoomId && A.A.Equals(B.B, .001) && A.B.Equals(B.A, .001))
                ++Shared;
    TestEqual(TEXT("Partial shared boundary is split and paired"), Shared, 2);
    FEdit Door;
    Door.Type = EEditType::Opening;
    for (const auto &A : WallList)
        for (const auto &B : WallList)
            if (A.RoomId == FGuid(0, 0, 0, 1) && B.RoomId != A.RoomId && A.A.Equals(B.B, .001) && A.B.Equals(B.A, .001))
            {
                Door.Opening.WallId = A.Id;
                Door.Opening.OtherWallId = B.Id;
            }
    Door.Opening.Width = 1200;
    Door.Opening.Offset = 400;
    TestTrue(TEXT("Shared door"), Plan.Commit(Door, Error));
    FDocument Restored;
    TestTrue(TEXT("V2 roundtrip with rooms and opening"), Restored.ReplaceLoaded(Encode(Plan.Get()), Error));
    TestTrue(TEXT("Exact room/wall/opening IDs"), Plan.Get() == Restored.Get());
    auto Before = Plan.Get();
    FEdit Invalid = Door;
    Invalid.Opening.Id = FGuid::NewGuid();
    TestFalse(TEXT("Overlapping door rejected"), Plan.Commit(Invalid, Error));
    TestTrue(TEXT("Rejected opening preserves scene"), Before == Plan.Get());
    Invalid.Opening.Offset = 1990;
    TestFalse(TEXT("Wall end overflow rejected"), Plan.Commit(Invalid, Error));
    Invalid = Add;
    Invalid.Room = FRoom::Rectangle(1000, 1000, 2000, 2000, TEXT("重複"));
    TestFalse(TEXT("Interior room overlap rejected"), Plan.Commit(Invalid, Error));
    FEdit L;
    L.Type = EEditType::AddRoom;
    L.Room = FRoom::Rectangle(0, 5000, 3000, 3000, TEXT("L字"));
    L.Room.Polygon = {{0, 5000}, {3000, 5000}, {3000, 6500}, {1500, 6500}, {1500, 8000}, {0, 8000}};
    L.Room.Walls.SetNum(6);
    TestTrue(TEXT("Concave orthogonal room"), Plan.Commit(L, Error));
    TestTrue(TEXT("L interior"), PointInRoom(L.Room, {500, 7500}));
    TestFalse(TEXT("L missing corner"), PointInRoom(L.Room, {2500, 7500}));
    FEdit Bow = L;
    Bow.Room.Id = FGuid::NewGuid();
    Bow.Room.Polygon = {{0, 10000}, {2000, 10000}, {2000, 12000}, {1000, 12000}, {1000, 9000}, {0, 9000}};
    Bow.Room.Walls.SetNum(6);
    TestFalse(TEXT("Self intersection rejected"), Plan.Commit(Bow, Error));
    FDocument Swap;
    FEdit Asset;
    Asset.Type = EEditType::AddAsset;
    Asset.Asset = Swap.Get().Assets[0];
    Asset.Asset.Id = TEXT("test-wide-study");
    Asset.Asset.ManifestHash = TEXT("test-hash");
    Asset.Asset.Size.X = 2100;
    Asset.Asset.Parts = {{{0, 0, 410}, {2100, 870, 820}}};
    TestTrue(TEXT("Register immutable test study"), Swap.Commit(Asset, Error));
    FEdit Place;
    Place.Type = EEditType::Add;
    Place.Item.X = 2000;
    Place.Item.Z = 1500;
    Place.Item.Yaw = PI / 4;
    TestTrue(TEXT("Place for distance test"), Swap.Commit(Place, Error));
    FEdit Replace;
    Replace.Type = EEditType::Replace;
    Replace.Target = Place.Item.Id;
    Replace.Item.AssetId = Asset.Asset.Id;
    Replace.AnchorWallId = Swap.Get().Rooms[0].Walls[3].Id;
    auto Old = Swap.Get().Items[0];
    auto Distance = [&](const FScene &S) {
        double V = DBL_MAX;
        for (auto P : Footprint(S.Items[0], *S.Asset(S.Items[0].AssetId)))
            V = FMath::Min(V, P.X);
        return V;
    };
    double Gap = Distance(Swap.Get());
    TestTrue(TEXT("Wall anchored replacement"), Swap.Commit(Replace, Error));
    TestTrue(TEXT("Wall gap retained at 45 degrees"), FMath::Abs(Distance(Swap.Get()) - Gap) < .002);
    TestEqual(TEXT("Along wall retained"), Swap.Get().Items[0].Z, Old.Z);
    TestEqual(TEXT("Instance id retained"), Swap.Get().Items[0].Id, Old.Id);
    TestTrue(TEXT("Undo restores previous asset and pose"), Swap.Undo());
    TestTrue(TEXT("Undo exact item"), Swap.Get().Items[0] == Old);
    Replace.AnchorWallId = FGuid::NewGuid();
    TestFalse(TEXT("Stale wall rejected"), Swap.Commit(Replace, Error));
    FScene Empty;
    auto Route = Passage(Empty, {1000, 1000}, {4000, 3000}, 800);
    TestTrue(TEXT("Empty room route"), Route.bValid && Route.bFound);
    FItem Block;
    Block.X = 2500;
    Block.Z = 2000;
    FAsset Wall = Empty.Assets[0];
    Wall.Id = TEXT("test-blocker");
    Wall.Size = {200, 4000, 2000};
    Wall.Parts = {{{0, 0, 1000}, {200, 4000, 2000}}};
    Empty.Assets.Add(Wall);
    Block.AssetId = Wall.Id;
    Empty.Items.Add(Block);
    Route = Passage(Empty, {1000, 1000}, {4000, 3000}, 800);
    TestTrue(TEXT("Furniture closed passage"), Route.bValid && !Route.bFound);
    return true;
}
#endif
