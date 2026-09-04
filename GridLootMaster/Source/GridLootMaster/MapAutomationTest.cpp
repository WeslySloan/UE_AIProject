#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Map/MapManagerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMapV1Test,
    "GridLootMaster.Map.MapV1",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMapV1Test::RunTest(const FString& Parameters)
{
    UMapManagerComponent* MapManager = NewObject<UMapManagerComponent>();
    TestNotNull(TEXT("Map manager is created for Map v1"), MapManager);
    if (!MapManager)
    {
        return false;
    }

    const auto VerifyMap = [this, MapManager](FIntPoint StartPoint, const TArray<FIntPoint>& Candidates)
    {
        MapManager->SpawnPoint = StartPoint;
        MapManager->InitializeMap();
        TestEqual(TEXT("Map v1 has 81 tiles"), MapManager->MapGrid.Num(), 81);

        for (int32 Y = 0; Y < 9; ++Y)
        {
            for (int32 X = 0; X < 9; ++X)
            {
                FTileData CurrentTile;
                TestTrue(TEXT("Every Map v1 coordinate is valid"), MapManager->GetTileData(X, Y, CurrentTile));
                if (X < 8)
                {
                    FTileData EastTile;
                    TestTrue(TEXT("East neighbor coordinate is valid"), MapManager->GetTileData(X + 1, Y, EastTile));
                    TestEqual(TEXT("East/West wall state is symmetric"), CurrentTile.bOpenEast, EastTile.bOpenWest);
                }
                if (Y < 8)
                {
                    FTileData SouthTile;
                    TestTrue(TEXT("South neighbor coordinate is valid"), MapManager->GetTileData(X, Y + 1, SouthTile));
                    TestEqual(TEXT("South/North wall state is symmetric"), CurrentTile.bOpenSouth, SouthTile.bOpenNorth);
                }
            }
        }

        int32 ReachableCount = 0;
        TArray<FIntPoint> Pending;
        Pending.Add(StartPoint);
        TSet<FIntPoint> Visited;
        while (Pending.Num() > 0)
        {
            const FIntPoint Current = Pending.Pop();
            if (Visited.Contains(Current))
            {
                continue;
            }
            Visited.Add(Current);
            ++ReachableCount;

            const FIntPoint Neighbors[] = {
                FIntPoint(Current.X, Current.Y - 1), FIntPoint(Current.X, Current.Y + 1),
                FIntPoint(Current.X - 1, Current.Y), FIntPoint(Current.X + 1, Current.Y)
            };
            for (const FIntPoint Neighbor : Neighbors)
            {
                if (!Visited.Contains(Neighbor) && MapManager->CanMoveBetween(Current, Neighbor))
                {
                    Pending.Add(Neighbor);
                }
            }
        }
        TestEqual(TEXT("All Map v1 tiles are reachable"), ReachableCount, 81);

        for (const FIntPoint Candidate : Candidates)
        {
            const int32 ManhattanDistance = FMath::Abs(Candidate.X - StartPoint.X) +
                FMath::Abs(Candidate.Y - StartPoint.Y);
            const TArray<FIntPoint> Path = MapManager->FindPath(StartPoint, Candidate);
            TestTrue(TEXT("Extraction candidate meets Manhattan distance"), ManhattanDistance >= 14);
            TestTrue(TEXT("Extraction candidate has an A* path of at least 14"), Path.Num() >= 14);
        }

        TestEqual(TEXT("One valid extraction point is selected"), MapManager->ExtractionPoints.Num(), 1);
        TestTrue(TEXT("Selected extraction point is one of the Map v1 candidates"),
            Candidates.Contains(MapManager->ExtractionPoints[0]));
    };

    VerifyMap(FIntPoint(0, 0), { FIntPoint(8, 6), FIntPoint(8, 7), FIntPoint(8, 8) });
    VerifyMap(FIntPoint(8, 8), { FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(0, 2) });

    FTileData TileData;
    TestTrue(TEXT("Map v1 tile data exposes Enemy Spawn permission"),
        MapManager->GetTileData(0, 0, TileData));
    TestFalse(TEXT("Static forbidden tile is not a normal Enemy Spawn candidate"), TileData.bEnemySpawnAllowed);
    TestTrue(TEXT("Static allowed tile is a normal Enemy Spawn candidate"),
        MapManager->GetTileData(4, 0, TileData) && TileData.bEnemySpawnAllowed);
    return true;
}

#endif
