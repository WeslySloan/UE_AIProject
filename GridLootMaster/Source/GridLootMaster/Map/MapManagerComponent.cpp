#include "MapManagerComponent.h"
#include "Math/UnrealMathUtility.h"
#include "Algo/Reverse.h"

UMapManagerComponent::UMapManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    MapWidth = 9;
    MapHeight = 9;
    SpawnPoint = FIntPoint(0, 0);
    ExtractionPointCount = 1;
    ExtractionMinDistance = 14;
}

void UMapManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeMap();
}

int32 UMapManagerComponent::GetIndex(int32 X, int32 Y) const
{
    if (X < 0 || X >= MapWidth || Y < 0 || Y >= MapHeight) return -1;
    return Y * MapWidth + X;
}

ETileZone UMapManagerComponent::DetermineZone(int32 X, int32 Y) const
{
    // 3x3 분할에 따른 Zone 계산
    int32 ZoneX = X / 3;
    int32 ZoneY = Y / 3;
    int32 ZoneIndex = ZoneY * 3 + ZoneX;
    return static_cast<ETileZone>(ZoneIndex);
}

void UMapManagerComponent::InitializeMap()
{
	MapWidth = 9;
	MapHeight = 9;
	SpawnPoint.X = FMath::Clamp(SpawnPoint.X, 0, MapWidth - 1);
	SpawnPoint.Y = FMath::Clamp(SpawnPoint.Y, 0, MapHeight - 1);
	ExtractionPoints.Empty();

	MapGrid.Empty();
	MapGrid.SetNum(MapWidth * MapHeight);

    for (int32 Y = 0; Y < MapHeight; ++Y)
    {
        for (int32 X = 0; X < MapWidth; ++X)
        {
            int32 Index = GetIndex(X, Y);
            MapGrid[Index].Coordinate = FIntPoint(X, Y);
            MapGrid[Index].Zone = DetermineZone(X, Y);
            MapGrid[Index].TileType = ETileType::Normal;
            MapGrid[Index].bIsExplored = false;
            MapGrid[Index].bEnemySpawnAllowed = true;

            // 맵 경계선 처리
            MapGrid[Index].bOpenNorth = (Y > 0);
            MapGrid[Index].bOpenSouth = (Y < MapHeight - 1);
            MapGrid[Index].bOpenEast  = (X < MapWidth - 1);
            MapGrid[Index].bOpenWest  = (X > 0);
        }
    }

    auto CloseEdge = [&](int32 X1, int32 Y1, int32 X2, int32 Y2) {
        int32 Idx1 = GetIndex(X1, Y1);
        int32 Idx2 = GetIndex(X2, Y2);
        if (Idx1 < 0 || Idx2 < 0) return;

        if (X1 < X2) { MapGrid[Idx1].bOpenEast = false; MapGrid[Idx2].bOpenWest = false; }
        else if (X1 > X2) { MapGrid[Idx1].bOpenWest = false; MapGrid[Idx2].bOpenEast = false; }
        else if (Y1 < Y2) { MapGrid[Idx1].bOpenSouth = false; MapGrid[Idx2].bOpenNorth = false; }
        else if (Y1 > Y2) { MapGrid[Idx1].bOpenNorth = false; MapGrid[Idx2].bOpenSouth = false; }
    };

    // GridLootMaster Map v1 ClosedEdges
    const FIntPoint ClosedEdges[][2] = {
        {{1,0},{1,1}}, {{2,0},{3,0}}, {{4,0},{4,1}}, {{5,0},{6,0}}, {{7,0},{7,1}},
        {{0,1},{1,1}}, {{3,1},{4,1}}, {{6,1},{7,1}}, {{0,2},{0,3}}, {{2,2},{3,2}},
        {{2,2},{2,3}}, {{5,2},{6,2}}, {{6,2},{6,3}}, {{8,2},{8,3}}, {{1,3},{1,4}},
        {{3,3},{4,3}}, {{5,3},{5,4}}, {{0,4},{1,4}}, {{3,4},{4,4}}, {{3,4},{3,5}},
        {{4,4},{5,4}}, {{7,4},{8,4}}, {{7,4},{7,5}}, {{0,5},{0,6}}, {{2,5},{2,6}},
        {{4,5},{5,5}}, {{6,5},{6,6}}, {{8,5},{8,6}}, {{2,6},{3,6}}, {{5,6},{6,6}},
        {{1,7},{2,7}}, {{1,7},{1,8}}, {{4,7},{5,7}}, {{4,7},{4,8}}, {{7,7},{8,7}},
        {{7,7},{7,8}}, {{2,8},{3,8}}, {{5,8},{6,8}}
    };
    for (const auto& Edge : ClosedEdges)
    {
        CloseEdge(Edge[0].X, Edge[0].Y, Edge[1].X, Edge[1].Y);
    }

    // MapTiles v1의 정적 일반 Enemy Spawn 금지 타일
    const FIntPoint EnemySpawnForbiddenTiles[] = {
        {0,0}, {0,1}, {0,2}, {3,3}, {5,3}, {3,5}, {5,5}, {4,4},
        {7,7}, {8,6}, {8,7}, {8,8}
    };
    for (const FIntPoint Coordinate : EnemySpawnForbiddenTiles)
    {
        const int32 Index = GetIndex(Coordinate.X, Coordinate.Y);
        if (MapGrid.IsValidIndex(Index))
        {
            MapGrid[Index].bEnemySpawnAllowed = false;
        }
    }

    const FIntPoint DeadEndTiles[] = {
        {3,3}, {5,3}, {3,5}, {5,5}
    };
    for (const FIntPoint Coordinate : DeadEndTiles)
    {
        const int32 Index = GetIndex(Coordinate.X, Coordinate.Y);
        if (MapGrid.IsValidIndex(Index))
        {
            MapGrid[Index].TileType = ETileType::DeadEnd;
        }
    }

    GenerateExtractionPoints();
}

bool UMapManagerComponent::GetTileData(int32 X, int32 Y, FTileData& OutTileData) const
{
    int32 Index = GetIndex(X, Y);
    if (Index != -1 && MapGrid.IsValidIndex(Index))
    {
        OutTileData = MapGrid[Index];
        return true;
    }
    return false;
}

bool UMapManagerComponent::IsExtractionPoint(FIntPoint Coordinate) const
{
    return ExtractionPoints.Contains(Coordinate);
}

void UMapManagerComponent::GenerateExtractionPoints()
{
    TArray<FIntPoint> Candidates;
    const int32 MinDistance = FMath::Max(0, ExtractionMinDistance);

    if (SpawnPoint == FIntPoint(0, 0))
    {
        Candidates.Add(FIntPoint(8, 6));
        Candidates.Add(FIntPoint(8, 7));
        Candidates.Add(FIntPoint(8, 8));
    }
    else if (SpawnPoint == FIntPoint(8, 8))
    {
        Candidates.Add(FIntPoint(0, 0));
        Candidates.Add(FIntPoint(0, 1));
        Candidates.Add(FIntPoint(0, 2));
    }

    for (int32 Index = Candidates.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        Candidates.Swap(Index, SwapIndex);
    }

    const int32 DesiredCount = FMath::Clamp(ExtractionPointCount, 1, Candidates.Num());
    for (const FIntPoint Candidate : Candidates)
    {
        const int32 Distance = FMath::Abs(Candidate.X - SpawnPoint.X) + FMath::Abs(Candidate.Y - SpawnPoint.Y);
        if (Distance < MinDistance || !FindPath(SpawnPoint, Candidate).Num())
        {
            continue;
        }

        ExtractionPoints.Add(Candidate);
        if (ExtractionPoints.Num() >= DesiredCount)
        {
            break;
        }
    }

    for (const FIntPoint ExtractionPoint : ExtractionPoints)
    {
        const int32 Index = GetIndex(ExtractionPoint.X, ExtractionPoint.Y);
        if (MapGrid.IsValidIndex(Index))
        {
            MapGrid[Index].TileType = ETileType::Extraction;
        }
    }
}

bool UMapManagerComponent::CanMoveBetween(FIntPoint From, FIntPoint To) const
{
    int32 FromIdx = GetIndex(From.X, From.Y);
    int32 ToIdx = GetIndex(To.X, To.Y);
    
    if (FromIdx == -1 || ToIdx == -1 ||
        !MapGrid.IsValidIndex(FromIdx) || !MapGrid.IsValidIndex(ToIdx))
    {
        return false;
    }

    const FTileData& FromTile = MapGrid[FromIdx];
    const FTileData& ToTile = MapGrid[ToIdx];

    // 상하좌우 체크 (From 입장에서 열려있고, To 입장에서도 열려있어야 함)
    if (To.X == From.X + 1 && To.Y == From.Y) return FromTile.bOpenEast && ToTile.bOpenWest;
    if (To.X == From.X - 1 && To.Y == From.Y) return FromTile.bOpenWest && ToTile.bOpenEast;
    if (To.Y == From.Y + 1 && To.X == From.X) return FromTile.bOpenSouth && ToTile.bOpenNorth;
    if (To.Y == From.Y - 1 && To.X == From.X) return FromTile.bOpenNorth && ToTile.bOpenSouth;

    return false; // 대각선이나 멀리 떨어진 경우
}

int32 UMapManagerComponent::GetTileDistance(FIntPoint From, FIntPoint To) const
{
    if (GetIndex(From.X, From.Y) == -1 || GetIndex(To.X, To.Y) == -1)
    {
        return MAX_int32;
    }

    return FMath::Max(FMath::Abs(From.X - To.X), FMath::Abs(From.Y - To.Y));
}

bool UMapManagerComponent::HasLineOfSight(FIntPoint From, FIntPoint To) const
{
    if (GetIndex(From.X, From.Y) == -1 || GetIndex(To.X, To.Y) == -1)
    {
        return false;
    }

    if (From == To)
    {
        return true;
    }

    int32 X = From.X;
    int32 Y = From.Y;
    const int32 DeltaX = FMath::Abs(To.X - From.X);
    const int32 DeltaY = FMath::Abs(To.Y - From.Y);
    const int32 StepX = From.X < To.X ? 1 : -1;
    const int32 StepY = From.Y < To.Y ? 1 : -1;
    int32 Error = DeltaX - DeltaY;

    while (X != To.X || Y != To.Y)
    {
        const int32 PreviousX = X;
        const int32 PreviousY = Y;
        const int32 DoubleError = Error * 2;
        const bool bStepX = DoubleError > -DeltaY;
        const bool bStepY = DoubleError < DeltaX;
        if (bStepX)
        {
            Error -= DeltaY;
            X += StepX;
        }
        if (bStepY)
        {
            Error += DeltaX;
            Y += StepY;
        }

        const FIntPoint Previous(PreviousX, PreviousY);
        const FIntPoint Next(X, Y);
        if (bStepX && bStepY)
        {
            const FIntPoint Horizontal(X, PreviousY);
            const FIntPoint Vertical(PreviousX, Y);
            if (!CanMoveBetween(Previous, Horizontal) || !CanMoveBetween(Previous, Vertical))
            {
                return false;
            }
        }
        else if (!CanMoveBetween(Previous, Next))
        {
            return false;
        }
    }

    return true;
}

struct FAStarNode
{
    FIntPoint Coord;
    int32 GCost; // 시작점부터의 이동 비용
    int32 HCost; // 목적지까지의 추정 비용 (휴리스틱)
    FIntPoint ParentCoord;

    int32 GetFCost() const { return GCost + HCost; }

    bool operator==(const FAStarNode& Other) const { return Coord == Other.Coord; }
};

TArray<FIntPoint> UMapManagerComponent::FindPath(FIntPoint StartPoint, FIntPoint TargetPoint)
{
    TArray<FIntPoint> Path;
    
    // 시작점과 끝점이 유효한지 체크
    if (GetIndex(StartPoint.X, StartPoint.Y) == -1 || GetIndex(TargetPoint.X, TargetPoint.Y) == -1)
        return Path;

    if (StartPoint == TargetPoint)
        return Path;

    TArray<FAStarNode> OpenList;
    TArray<FAStarNode> ClosedList;

    FAStarNode StartNode;
    StartNode.Coord = StartPoint;
    StartNode.GCost = 0;
    StartNode.HCost = FMath::Abs(StartPoint.X - TargetPoint.X) + FMath::Abs(StartPoint.Y - TargetPoint.Y);
    StartNode.ParentCoord = StartPoint;
    
    OpenList.Add(StartNode);

    while (OpenList.Num() > 0)
    {
        // F비용이 가장 낮은 노드 찾기
        int32 LowestIndex = 0;
        for (int32 i = 1; i < OpenList.Num(); ++i)
        {
            if (OpenList[i].GetFCost() < OpenList[LowestIndex].GetFCost() || 
               (OpenList[i].GetFCost() == OpenList[LowestIndex].GetFCost() && OpenList[i].HCost < OpenList[LowestIndex].HCost))
            {
                LowestIndex = i;
            }
        }

        FAStarNode CurrentNode = OpenList[LowestIndex];
        OpenList.RemoveAt(LowestIndex);
        ClosedList.Add(CurrentNode);

        // 목적지 도착
        if (CurrentNode.Coord == TargetPoint)
        {
            FIntPoint CurrentTrace = TargetPoint;
            while (CurrentTrace != StartPoint)
            {
                Path.Add(CurrentTrace);
                // ClosedList에서 부모 찾기
                for (const FAStarNode& Node : ClosedList)
                {
                    if (Node.Coord == CurrentTrace)
                    {
                        CurrentTrace = Node.ParentCoord;
                        break;
                    }
                }
            }
            Algo::Reverse(Path);
            return Path;
        }

        // 인접한 4방향 노드 탐색
        FIntPoint Neighbors[4] = {
            FIntPoint(CurrentNode.Coord.X, CurrentNode.Coord.Y - 1), // 북
            FIntPoint(CurrentNode.Coord.X, CurrentNode.Coord.Y + 1), // 남
            FIntPoint(CurrentNode.Coord.X - 1, CurrentNode.Coord.Y), // 서
            FIntPoint(CurrentNode.Coord.X + 1, CurrentNode.Coord.Y)  // 동
        };

        for (int32 i = 0; i < 4; ++i)
        {
            FIntPoint NeighborCoord = Neighbors[i];

            // 맵 범위 및 이동 가능(벽) 여부 체크
            if (!CanMoveBetween(CurrentNode.Coord, NeighborCoord))
                continue;

            // 이미 ClosedList에 있으면 무시
            bool bInClosed = false;
            for (const FAStarNode& Node : ClosedList)
            {
                if (Node.Coord == NeighborCoord) { bInClosed = true; break; }
            }
            if (bInClosed) continue;

            int32 NewCostToNeighbor = CurrentNode.GCost + 1; // 1칸 이동 비용은 1

            // OpenList에 있는지 확인
            bool bInOpen = false;
            int32 OpenIndex = -1;
            for (int32 j = 0; j < OpenList.Num(); ++j)
            {
                if (OpenList[j].Coord == NeighborCoord)
                {
                    bInOpen = true;
                    OpenIndex = j;
                    break;
                }
            }

            if (!bInOpen || NewCostToNeighbor < OpenList[OpenIndex].GCost)
            {
                FAStarNode NeighborNode;
                NeighborNode.Coord = NeighborCoord;
                NeighborNode.GCost = NewCostToNeighbor;
                NeighborNode.HCost = FMath::Abs(NeighborCoord.X - TargetPoint.X) + FMath::Abs(NeighborCoord.Y - TargetPoint.Y);
                NeighborNode.ParentCoord = CurrentNode.Coord;

                if (!bInOpen)
                {
                    OpenList.Add(NeighborNode);
                }
                else
                {
                    OpenList[OpenIndex] = NeighborNode;
                }
            }
        }
    }

    // 길을 못 찾음
    return Path;
}
