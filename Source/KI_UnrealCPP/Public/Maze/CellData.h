// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))	// 블루프린트에서 비트플래그로 사용 가능
enum class EDirectionType : uint8
{
	None	= 0			UMETA(Hidden),
	North	= 1 << 0	UMETA(DisplayName = "North"),
	East	= 1 << 1	UMETA(DisplayName = "East"),
	South	= 1 << 2	UMETA(DisplayName = "South"),
	West	= 1 << 3	UMETA(DisplayName = "West"),
};
ENUM_CLASS_FLAGS(EDirectionType)	// 비트 연산자 오버로딩

/**
 * 
 */
struct KI_UNREALCPP_API FCellData
{
public:
	// 셀의 X좌표
	uint8 X = 0;

	// 셀의 Y좌표
	uint8 Y = 0;

	// 이 셀에 열려있는 문의 방향
	EDirectionType Path = EDirectionType::None;

	// 이 셀이 미로에 포함되어 있는지 여부
	bool bInMaze = false;
		
	// TSharedPtr	: TObjectPtr과 비슷. 일반 C++ 포인터용. 레퍼런스 카운터 사용.
	// TWeakPtr		: TWeakObjectPtr과 비슷. 일반 C++ 포인터용. 레퍼런스 카운터 증가하지 않음.
	// TUniquePtr	: 주소가 한 변수에서만 유지됨. 복사 불가능. 이동은 가능.
	FCellData* NextCell = nullptr;


	// 셀의 좌표를 가져오는 함수
	inline FIntPoint GetLocation() const { return FIntPoint(X, Y); }

	// 셀에 길을 추가하는 함수
	inline void AddPath(EDirectionType InAddedPath) { Path |= InAddedPath; }

	// 특정 방향이 길인지 확인하는 함수
	inline bool IsPath(EDirectionType InCheck) const { return (Path & InCheck) == InCheck; }
	//inline bool IsPath(EDirectionType InCheck) { return EnumHasAnyFlags(Path, InCheck); }

	// 특정 방향이 벽인지 확인하는 함수
	inline bool IsWall(EDirectionType InCheck) const { return !IsPath(InCheck); }
	
};
