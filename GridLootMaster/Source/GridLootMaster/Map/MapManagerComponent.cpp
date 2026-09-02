#include "MapManagerComponent.h"
#include "Math/UnrealMathUtility.h"
#include "Algo/Reverse.h"

UMapManagerComponent::UMapManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    MapWidth = 9;
    MapHeight = 9;
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

            // 맵 경계선 처리
            MapGrid[Index].bOpenNorth = (Y > 0);
            MapGrid[Index].bOpenSouth = (Y < MapHeight - 1);
            MapGrid[Index].bOpenEast  = (X < MapWidth - 1);
            MapGrid[Index].bOpenWest  = (X > 0);
        }
    }

    // [테스트용] 몇 개의 벽을 막아 봅니다 (고정된 맵 템플릿의 시작)
    // 예: (1,1)의 오른쪽과 (2,1)의 왼쪽을 막는다
    auto CloseEdge = [&](int32 X1, int32 Y1, int32 X2, int32 Y2) {
        int32 Idx1 = GetIndex(X1, Y1);
        int32 Idx2 = GetIndex(X2, Y2);
        if (Idx1 < 0 || Idx2 < 0) return;

        if (X1 < X2) { MapGrid[Idx1].bOpenEast = false; MapGrid[Idx2].bOpenWest = false; }
        else if (X1 > X2) { MapGrid[Idx1].bOpenWest = false; MapGrid[Idx2].bOpenEast = false; }
        else if (Y1 < Y2) { MapGrid[Idx1].bOpenSouth = false; MapGrid[Idx2].bOpenNorth = false; }
        else if (Y1 > Y2) { MapGrid[Idx1].bOpenNorth = false; MapGrid[Idx2].bOpenSouth = false; }
    };

    // 간단한 미로처럼 몇 군데 벽 생성
    CloseEdge(2, 0, 3, 0); CloseEdge(2, 1, 3, 1); CloseEdge(2, 2, 3, 2); // Zone A와 B 사이 막음
    CloseEdge(1, 2, 1, 3); CloseEdge(2, 2, 2, 3); // Zone A와 D 사이 막음 (통로 하나만 놔둠)
    CloseEdge(5, 5, 6, 5); CloseEdge(5, 6, 6, 6);
}

bool UMapManagerComponent::GetTileData(int32 X, int32 Y, FTileData& OutTileData) const
{
    int32 Index = GetIndex(X, Y);
    if (Index != -1)
    {
        OutTileData = MapGrid[Index];
        return true;
    }
    return false;
}

bool UMapManagerComponent::CanMoveBetween(FIntPoint From, FIntPoint To) const
{
    int32 FromIdx = GetIndex(From.X, From.Y);
    int32 ToIdx = GetIndex(To.X, To.Y);
    
    if (FromIdx == -1 || ToIdx == -1) return false;

    const FTileData& FromTile = MapGrid[FromIdx];
    const FTileData& ToTile = MapGrid[ToIdx];

    // 상하좌우 체크 (From 입장에서 열려있고, To 입장에서도 열려있어야 함)
    if (To.X == From.X + 1 && To.Y == From.Y) return FromTile.bOpenEast && ToTile.bOpenWest;
    if (To.X == From.X - 1 && To.Y == From.Y) return FromTile.bOpenWest && ToTile.bOpenEast;
    if (To.Y == From.Y + 1 && To.X == From.X) return FromTile.bOpenSouth && ToTile.bOpenNorth;
    if (To.Y == From.Y - 1 && To.X == From.X) return FromTile.bOpenNorth && ToTile.bOpenSouth;

    return false; // 대각선이나 멀리 떨어진 경우
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
