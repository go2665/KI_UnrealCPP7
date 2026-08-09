// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MazeActor.generated.h"

class FMazeData;
//-------------------- HISM 컴포넌트 전방 선언 --------------------
class UHierarchicalInstancedStaticMeshComponent;
//-----------------------------------------------------------------

UCLASS()
class KI_UNREALCPP_API AMazeActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMazeActor();

	// 에디터 디테일 패널에 [Generate Maze] 버튼 생성
	UFUNCTION(CallInEditor, Category = "Maze")
	void GenerateMaze();

	// 에디터 디테일 패널에 [Clear Maze] 버튼 생성
	UFUNCTION(CallInEditor, Category = "Maze")
	void ClearMaze();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void OnPreMapGenerate();
	void OnPostMapGenerate();

private:	
	TUniquePtr<FMazeData> MakeMazeData();
	void SpawnCells(FMazeData* Maze);
	//void ClearMazeData(FMazeData*& Maze);

	//-------------------- HISM 생성 및 초기화 함수 --------------------
	void BuildMazeHISM(FMazeData* Maze);
	void ClearHISMInstances();
	void MakeCellHISM(struct FCellData* Cell, const FVector& CellLocation);
	//------------------------------------------------------------------

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Maze", meta = (ClampMin = "3", ClampMax = "100"))
	int32 Width = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Maze", meta = (ClampMin = "3", ClampMax = "100"))
	int32 Height = 3;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Maze")
	int32 RandomSeed = -1;

	//-------------------- CellActor로 스폰할 경우에 필요한 변수들 -----------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Maze")
	TSubclassOf<class ACellActor> CellActorClass = nullptr;

	// 에디터/게임 중 스폰된 셀 액터들을 보관하고 삭제 관리하는 배열
	UPROPERTY(VisibleInstanceOnly, Category = "Maze")
	TArray<TObjectPtr<ACellActor>> SpawnedCells;
	//-----------------------------------------------------------------------

	//-------------------- HISM 리팩토링 & 듀얼 모드 설정 --------------------
	// HISM 사용 여부 (true: HISM 인스턴스 렌더링, false: 기존 ACellActor 스폰 방식)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Maze|Performance")
	bool bUseHISM = true;

	// 셀 한 변 길이의 절반
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Maze|Dimensions")
	float CellHalfSize = 1000.0f;

	// HISM 컴포넌트 3종류 (Floor, BaseWall, Gate)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FloorHISM = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BaseWallHISM = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Maze|Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GateHISM = nullptr;
	//-----------------------------------------------------------------------
};

