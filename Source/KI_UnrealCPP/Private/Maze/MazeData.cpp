// Fill out your copyright notice in the Description page of Project Settings.


#include "Maze/MazeData.h"

const FIntPoint FMazeData::Directions[DirectionCount] =
{
	// 북동남서 순서
	FIntPoint(0,1), FIntPoint(1,0), FIntPoint(0,-1), FIntPoint(-1,0)
};

FMazeData::~FMazeData()
{
	ClearMaze();
}

void FMazeData::MakeMaze(uint8 InWidth, uint8 InHeight, int32 InSeed)
{
	Width = InWidth;
	Height = InHeight;

	if (InSeed == -1)
	{
		RandomStream.GenerateNewSeed();		// 랜덤하게 씨드값 설정
	}
	else
	{
		RandomStream.Initialize(InSeed);	// 원하는데로 씨드값 설정
	}

	ClearMaze();					// 기존 미로 데이터 삭제
	Cells.SetNum(Width * Height);	// 미로 셀의 배열 생성

	WilSonAlgorithmExecute();		// 미로 알고리즘 수행(맴버 변수들만 사용)
}

void FMazeData::ClearMaze()
{
	Cells.Reset();
}

FCellData* FMazeData::GetCell(uint8 InX, uint8 InY)
{
	if (IsValidLocation(InX, InY))
	{	
		return &(Cells[LocationToIndex(InX, InY)]);
	}
	return nullptr;
}

void FMazeData::WilSonAlgorithmExecute()
{
	// 초기화 작업
	TArray<FCellData*> NotMazeCells;
	NotMazeCells.Reserve(Width * Height);
	for (int32 y = 0; y < Height; y++)
	{
		for (int32 x = 0; x < Width; x++)
		{
			FCellData* cell = GetCell(x, y);
			cell->X = x;			// 위치 초기화
			cell->Y = y;
			NotMazeCells.Add(cell);	// 모든 셀을 미로가 아닌 셀로 등록
		}
	}
	ShuffleArray(NotMazeCells);	// 랜덤으로 뽑는 것 처럼하기 위해 순서 섞기

	// 윌슨 알고리즘 시작
	FCellData* initCell = NotMazeCells.Pop();	// 미로가 아닌 셀 중 하나를 초기셀로 설정하고 셀 목록에서 제거
	initCell->bInMaze = true;	// 미로에 포함되었다고 표시

	while (NotMazeCells.Num() > 0)
	{
		FCellData* startCell = NotMazeCells.Pop();
		if(startCell->bInMaze)	// 이미 미로에 포함된 셀이라면 건너뛰기
		{
			continue;
		}
		
		FCellData* currentCell = startCell;
		do
		{
			FCellData* neighborCell = GetRandomNeighborCell(*currentCell);	// 이웃 셀 선택
			currentCell->NextCell = neighborCell;	// 이동할 이웃셀 기록
			currentCell = neighborCell;				// 이웃 셀로 이동
		} while (!currentCell->bInMaze);	// currentCell이 미로에 포함된 영역에 도달할 때까지 반복

		FCellData* path = startCell;
		while (path != currentCell)	// startCell에서 currentCell까지 순서대로 진행
		{
			path->bInMaze = true;				// 미로에 포함시키고
			ConnectCells(path, path->NextCell);	// 셀간에 벽 제거
			path = path->NextCell;	// 다음 셀로 넘어가기
		}
	}	// 미로에 포함되지 않은 셀이 없어질 때까지 반복
}

void FMazeData::ConnectCells(FCellData* InFrom, FCellData* InTo)
{
	// X는 오른쪽으로 갈 때 증가
	// Y는 아래쪽으로 갈 때 증가

	if (InFrom && InTo && InFrom != InTo)
	{
		if (InFrom->X < InTo->X)
		{
			// From의 동쪽에 To가 있다.
			InFrom->AddPath(EDirectionType::East);
			InTo->AddPath(EDirectionType::West);
		}
		else if (InFrom->X > InTo->X)
		{
			// From의 서쪽에 To가 있다.
			InFrom->AddPath(EDirectionType::West);
			InTo->AddPath(EDirectionType::East);
		}
		else if (InFrom->Y < InTo->Y)
		{
			// From의 남쪽에 To가 있다.
			InFrom->AddPath(EDirectionType::South);
			InTo->AddPath(EDirectionType::North);
		}
		else if (InFrom->Y > InTo->Y)
		{
			// From의 북쪽에 To가 있다.
			InFrom->AddPath(EDirectionType::North);
			InTo->AddPath(EDirectionType::South);
		}
	}
}

FCellData* FMazeData::GetRandomNeighborCell(const FCellData& InCell)
{
	FIntPoint neighborLocation;
	do
	{
		int32 index = RandomStream.RandRange(0, DirectionCount - 1);
		neighborLocation = InCell.GetLocation() + Directions[index];
	} while (!IsValidLocation(neighborLocation.X, neighborLocation.Y));	// 유효한 위치가 나올때까지 반복
	return GetCell(static_cast<uint8>(neighborLocation.X), static_cast<uint8>(neighborLocation.Y));
}

void FMazeData::ShuffleArray(TArray<FCellData*>& InOutArray)
{
	for (int32 i = InOutArray.Num() - 1; i > 0; i--)
	{
		int32 index = RandomStream.RandRange(0, i);
		InOutArray.Swap(i, index);
	}
}
