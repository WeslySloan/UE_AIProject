#pragma once

#include "CoreMinimal.h"
#include "MapData.generated.h"

// 3x3 단위의 9개 구역 (이미지 기준 A~I)
UENUM(BlueprintType)
enum class ETileZone : uint8
{
    Zone_A,
    Zone_B,
    Zone_C,
    Zone_D,
    Zone_E,
    Zone_F,
    Zone_G,
    Zone_H,
    Zone_I
};

// 특수 타일 이벤트 처리를 위한 타입
UENUM(BlueprintType)
enum class ETileType : uint8
{
    Normal UMETA(DisplayName = "일반 방"),
    DeadEnd UMETA(DisplayName = "밀실"),
    Stairs UMETA(DisplayName = "지하 계단"),
    Secret UMETA(DisplayName = "비밀 방"),
    Extraction UMETA(DisplayName = "탈출 지점")
};

// 개별 타일의 데이터를 정의하는 구조체
USTRUCT(BlueprintType)
struct FTileData
{
    GENERATED_BODY()

    // 맵에서의 그리드 좌표 (X: 가로, Y: 세로)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    FIntPoint Coordinate;

    // 타일이 속한 3x3 구역
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    ETileZone Zone;

    // 타일의 특성
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    ETileType TileType;

    // --- 4방향 이동 가능 여부 (벽/열린 문) ---
    // true면 해당 방향으로 길이 뚫려있음, false면 벽으로 막힘
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    bool bOpenNorth; // 위쪽 (Y-1)
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    bool bOpenSouth; // 아래쪽 (Y+1)
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    bool bOpenEast;  // 오른쪽 (X+1)
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    bool bOpenWest;  // 왼쪽 (X-1)

    // 플레이어가 한 번이라도 방문(혹은 시야 확보) 했는지 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    bool bIsExplored;

    // Map v1 정적 데이터: 일반 Enemy Spawn Candidate 허용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data|Enemy Spawn")
    bool bEnemySpawnAllowed;

    FTileData()
        : Coordinate(FIntPoint(0, 0))
        , Zone(ETileZone::Zone_A)
        , TileType(ETileType::Normal)
        , bOpenNorth(true)
        , bOpenSouth(true)
        , bOpenEast(true)
        , bOpenWest(true)
        , bIsExplored(false)
        , bEnemySpawnAllowed(true)
    {}
};
