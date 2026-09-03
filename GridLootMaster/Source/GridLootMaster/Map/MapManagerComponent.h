#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MapData.h"
#include "MapManagerComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GRIDLOOTMASTER_API UMapManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    UMapManagerComponent();

    // 맵 데이터 (X, Y 좌표로 1D 배열 매핑: Index = Y * MapWidth + X)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    TArray<FTileData> MapGrid;

    // 맵 가로/세로 크기 (기본 9x9)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    int32 MapWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Data")
    int32 MapHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extraction")
    FIntPoint SpawnPoint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extraction")
    int32 ExtractionPointCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extraction")
    int32 ExtractionMinDistance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Extraction")
    TArray<FIntPoint> ExtractionPoints;

    // 맵을 초기화하고 타일 엣지 정보를 설정합니다.
    UFUNCTION(BlueprintCallable, Category = "Map Data")
    void InitializeMap();

    // 특정 좌표의 타일 정보 반환
    UFUNCTION(BlueprintCallable, Category = "Map Data")
    bool GetTileData(int32 X, int32 Y, FTileData& OutTileData) const;

    UFUNCTION(BlueprintCallable, Category = "Extraction")
    bool IsExtractionPoint(FIntPoint Coordinate) const;
    
    // 두 타일 사이의 이동 가능 여부 체크
    UFUNCTION(BlueprintCallable, Category = "Map Data")
    bool CanMoveBetween(FIntPoint From, FIntPoint To) const;

    // A* (에이스타) 길찾기 알고리즘을 사용하여 최단 경로 반환 (막힌 벽 회피)
    UFUNCTION(BlueprintCallable, Category = "Map Data")
    TArray<FIntPoint> FindPath(FIntPoint StartPoint, FIntPoint TargetPoint);

protected:
    virtual void BeginPlay() override;
    
    // 내부 헬퍼 함수
    int32 GetIndex(int32 X, int32 Y) const;
    ETileZone DetermineZone(int32 X, int32 Y) const;
    void GenerateExtractionPoints();
};
