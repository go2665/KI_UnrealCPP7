// Fill out your copyright notice in the Description page of Project Settings.


#include "Maze/MazeActor.h"
#include "Maze/MazeData.h"
#include "Maze/CellActor.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"

// Sets default values
AMazeActor::AMazeActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AMazeActor::BeginPlay()
{
	Super::BeginPlay();

	OnPreMapGenerate();

	FMazeData* maze = MakeMaze();
	SpawnCells(maze);
	ClearMaze(maze);

	OnPostMapGenerate();
}

void AMazeActor::OnPreMapGenerate()
{
	UNavigationSystemV1* navSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (navSystem)
	{
		navSystem->SetNavigationAutoUpdateEnabled(false, navSystem);
	}
}

void AMazeActor::OnPostMapGenerate()
{
	UNavigationSystemV1* navSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (navSystem)
	{
		navSystem->Build();	// 수동으로 네비게이션 매시 생성하게 하기
		navSystem->SetNavigationAutoUpdateEnabled(true, navSystem);  // 자동 업데이트 다시 활성화(안해도 상관없음)
	}
}

FMazeData* AMazeActor::MakeMaze()
{
	FMazeData* maze = new FMazeData();
	maze->MakeMaze(Width, Height, RandomSeed);

	return maze;
}

void AMazeActor::SpawnCells(FMazeData* Maze)
{
	if (CellActorClass)
	{
		const ACellActor* DefaultCell = CellActorClass->GetDefaultObject<ACellActor>();
		
		float cellHalfSize = DefaultCell->GetCellHalfSize();;
		float cellSize = cellHalfSize * 2.0f;

		FVector startLocation = FVector(Height * cellHalfSize, -Width * cellHalfSize, 0)
			+ FVector((Height % 2) * -cellHalfSize, (Width % 2) * cellHalfSize, 0);	// 셀의 피봇이 가운데에 있기 때문에 그것만큼 더 움직이기(짝수일때는 어차피 가운데니 안해도 됨)
		//FVector startLocation(0, 0, 0);
		//UE_LOG(LogTemp, Log, TEXT("start : %s"), *startLocation.ToString());

		UWorld* world = GetWorld();
		for (uint8 y = 0; y < Height; y++)	// y는 남쪽으로 증가
		{
			for (uint8 x = 0; x < Width; x++)	// x는 동쪽으로 증가
			{
				FCellData* cell = Maze->GetCell(x, y);
				if (cell)
				{
					FVector cellLocation = startLocation + FVector(-y * cellSize, x * cellSize, 0.0f);
					ACellActor* cellActor = world->SpawnActor<ACellActor>(
						CellActorClass,
						cellLocation,
						FRotator::ZeroRotator
					);
					if (cellActor)
					{
						cellActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);	// 미로에 셀 추가
						cellActor->InitializeCell(cell);
					}
				}
			}
		}
	}
}

void AMazeActor::ClearMaze(FMazeData*& Maze)
{
	Maze->ClearMaze();
	delete Maze;
	Maze = nullptr;
}