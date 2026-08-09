// Fill out your copyright notice in the Description page of Project Settings.


#include "Maze/MazeActor.h"
#include "Maze/MazeData.h"
#include "Maze/CellActor.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"

//-------------------- HISM 및 디버그 관련 헤더 추가 --------------------
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
//-------------------------------------------------------------

// Sets default values
AMazeActor::AMazeActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	//-------------------- RootComponent 및 HISM 컴포넌트 생성 및 초기화 --------------------
	USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootComp);

	FloorHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FloorHISM"));
	FloorHISM->SetupAttachment(RootComp);
	FloorHISM->SetCollisionProfileName(TEXT("BlockAll"));

	BaseWallHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("BaseWallHISM"));
	BaseWallHISM->SetupAttachment(RootComp);
	BaseWallHISM->SetCollisionProfileName(TEXT("BlockAll"));

	GateHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("GateHISM"));
	GateHISM->SetupAttachment(RootComp);
	GateHISM->SetCollisionProfileName(TEXT("BlockAll"));
	//-------------------------------------------------------------------------------------
}

void AMazeActor::GenerateMaze()
{
	OnPreMapGenerate();

	// 기존 생성된 셀 정리
	ClearMaze();

	TUniquePtr<FMazeData> maze = MakeMazeData();
	//-------------------- bUseHISM 듀얼 모드 분기 처리 --------------------
	if (bUseHISM)
	{
		BuildMazeHISM(maze.Get());
	}
	else
	{
		SpawnCells(maze.Get());
	}
	//--------------------------------------------------------------------
	//ClearMazeData(maze);
	OnPostMapGenerate();
}

void AMazeActor::ClearMaze()
{
	//-------------------- bUseHISM 듀얼 모드 분기 처리 --------------------
	if (bUseHISM)
	{
		ClearHISMInstances();
	}
	else
	{
		for (ACellActor* cell : SpawnedCells)
		{
			if (cell)
			{
				cell->Destroy();
			}
		}
		SpawnedCells.Empty();
	}
	//--------------------------------------------------------------------
}

// Called when the game starts or when spawned
void AMazeActor::BeginPlay()
{
	Super::BeginPlay();

	GenerateMaze();
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

TUniquePtr<FMazeData> AMazeActor::MakeMazeData()
{
	TUniquePtr<FMazeData> Maze = MakeUnique<FMazeData>();
	Maze->MakeMaze(Width, Height, RandomSeed);

	return Maze;
}

void AMazeActor::SpawnCells(FMazeData* Maze)
{
	if (CellActorClass)
	{
		SpawnedCells.Reserve(Width * Height);
		const ACellActor* DefaultCell = CellActorClass->GetDefaultObject<ACellActor>();		
		CellHalfSize = DefaultCell->GetCellHalfSize();
		float cellSize = CellHalfSize * 2.0f;

		FVector startLocation = FVector(Height * CellHalfSize, -Width * CellHalfSize, 0)
			+ FVector((Height % 2) * -CellHalfSize, (Width % 2) * CellHalfSize, 0);	// 셀의 피봇이 가운데에 있기 때문에 그것만큼 더 움직이기(짝수일때는 어차피 가운데니 안해도 됨)
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
					SpawnedCells.Add(cellActor);
				}
			}
		}
	}
}

//void AMazeActor::ClearMazeData(FMazeData*& Maze)
//{
//	Maze->ClearMaze();
//	delete Maze;
//	Maze = nullptr;
//}

//-------------------- HISM 생성 및 인스턴스 배치 로직 구현 --------------------
void AMazeActor::ClearHISMInstances()
{
	if (FloorHISM)
	{
		FloorHISM->ClearInstances();
	}
	if (BaseWallHISM)
	{
		BaseWallHISM->ClearInstances();
	}
	if (GateHISM)
	{
		GateHISM->ClearInstances();
	}
	FlushPersistentDebugLines(GetWorld());	// 디버그 박스도 지우기
}

void AMazeActor::BuildMazeHISM(FMazeData* Maze)
{
	if (!Maze)
	{
		return;
	}

	ClearHISMInstances();

	// for 안에서 중복 계산하지 않도록 미리 계산
	float cellSize = CellHalfSize * 2.0f;

	// 셀 시작 위치 계산 (기존 SpawnCells와 동일)
	FVector startLocation = FVector(Height * CellHalfSize, -Width * CellHalfSize, 0)
		+ FVector((Height % 2) * -CellHalfSize, (Width % 2) * CellHalfSize, 0);	// 홀수일 때는 가운데에 위치하도록 보정

	for (uint8 y = 0; y < Height; y++)	// y는 남쪽으로 증가
	{
		for (uint8 x = 0; x < Width; x++)	// x는 동쪽으로 증가
		{
			FCellData* cell = Maze->GetCell(x, y);
			if (!cell)
			{
				continue;
			}

			FVector cellLocation = startLocation + FVector(-y * cellSize, x * cellSize, 0.0f);
			//-------------------- MakeCellHISM 함수 호출 --------------------
			MakeCellHISM(cell, cellLocation);
			//----------------------------------------------------------------
		}
	}
}

//-------------------- 단일 셀 HISM 인스턴스 생성 및 디버그 박스 표시 --------------------
void AMazeActor::MakeCellHISM(FCellData* Cell, const FVector& CellLocation)
{
	if (!Cell)
	{
		return;
	}

	FTransform cellTransform(FRotator::ZeroRotator, CellLocation);	

	// 1. 바닥 HISM 인스턴스 추가
	FloorHISM->AddInstance(cellTransform, true);	// true = 월드 공간 기준

	// 2. 기본 벽/기둥 HISM 인스턴스 추가-----------------------------------------------------------
	// 기본 벽
	FVector NorthWallLoc = CellLocation + FVector::ForwardVector * CellHalfSize;
	BaseWallHISM->AddInstance(FTransform(FRotator::ZeroRotator, NorthWallLoc));

	FVector WestWallLoc = CellLocation + FVector::LeftVector * CellHalfSize;
	FRotator WestWallRot = FRotator(0.0f, 90.0f, 0.0f);
	BaseWallHISM->AddInstance(FTransform(WestWallRot, WestWallLoc));

	// 동쪽 끝과 남쪽 끝 경계선에 대한 HISM 인스턴스 추가 (이중 중복 배치 방지)
	if(Cell->X == Width - 1)	// 동쪽 끝
	{
		FVector EastWallLoc = CellLocation + FVector::RightVector * CellHalfSize;
		FRotator EastWallRot = FRotator(0.0f, 90.0f, 0.0f);
		BaseWallHISM->AddInstance(FTransform(EastWallRot, EastWallLoc));
	}
	if (Cell->Y == Height - 1)	// 남쪽 끝
	{
		FVector SouthWallLoc = CellLocation + FVector::BackwardVector * CellHalfSize;
		FRotator SouthWallRot = FRotator(0.0f, 180.0f, 0.0f);
		BaseWallHISM->AddInstance(FTransform(SouthWallRot, SouthWallLoc));
	}
	//--------------------------------------------------------------------------------------------

	// 3. 문 HISM 인스턴스 추가 -------------------------------------------------------------------
	// 북쪽 벽 체크
	if (Cell->IsWall(EDirectionType::North))
	{
		FVector wallLoc = CellLocation + FVector::ForwardVector * CellHalfSize;
		FRotator wallRot = FRotator(0.0f, 0.0f, 0.0f);
		GateHISM->AddInstance(FTransform(wallRot, wallLoc));
	}

	// 서쪽 벽 체크
	if (Cell->IsWall(EDirectionType::West))
	{
		FVector wallLoc = CellLocation + FVector::LeftVector * CellHalfSize;
		FRotator wallRot = FRotator(0.0f, 270.0f, 0.0f);
		GateHISM->AddInstance(FTransform(wallRot, wallLoc));
	}

	// 동쪽 벽 체크 (가장 동쪽 경계인 X == Width - 1 일 때만 직접 배치하여 이중 렌더링 방지, 동쪽은 항상 막혀있다.)
	if (Cell->X == Width - 1 /*&& Cell->IsWall(EDirectionType::East)*/)
	{
		FVector wallLoc = CellLocation + FVector::RightVector * CellHalfSize;
		FRotator wallRot = FRotator(0.0f, 90.0f, 0.0f);
		GateHISM->AddInstance(FTransform(wallRot, wallLoc));
	}

	// 남쪽 벽 체크 (가장 남쪽 경계인 Y == Height - 1 일 때만 직접 배치하여 이중 렌더링 방지, 남쪽은 항상 막혀있다.)
	if (Cell->Y == Height - 1 /*&& Cell->IsWall(EDirectionType::South)*/)
	{
		FVector wallLoc = CellLocation + FVector::BackwardVector * CellHalfSize;
		FRotator wallRot = FRotator(0.0f, 180.0f, 0.0f);
		GateHISM->AddInstance(FTransform(wallRot, wallLoc));
	}		
	//--------------------------------------------------------------------------------------------

#if WITH_EDITOR
	// 4. 셀 영역 디버그 박스 (DrawDebugBox) 그리기
	DrawDebugBox(
		GetWorld(),
		CellLocation,
		FVector(CellHalfSize, CellHalfSize, 400.0f),
		FColor::Green,
		true,        // bPersistentLines (영구 표시)
		-1.0f,       // LifeTime
		0,           // DepthPriority
		3.0f         // Thickness
	);
#endif
}
//---------------------------------------------------------------------------------

