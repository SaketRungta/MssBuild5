// Copyright Epic Games, Inc. All Rights Reserved.

#include "MssBuild5GameMode.h"
#include "MssBuild5Character.h"
#include "UObject/ConstructorHelpers.h"

AMssBuild5GameMode::AMssBuild5GameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
