#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"

/** Defined in UMssSubsystem.cpp */
DECLARE_LOG_CATEGORY_EXTERN(MultiplayerSessionSubsystemLog, Log, All);

// --------------------------------------------------------------------------
//  INTERNAL (DO NOT CALL DIRECTLY)
// --------------------------------------------------------------------------

inline void Internal_Log(const ELogVerbosity::Type Verbosity,
                         const char* FunctionName,
                         const FColor ScreenColor,
                         const FString& UserMessage)
{
    const FString FinalMessage = FString::Printf(TEXT("[%s] %s"), ANSI_TO_TCHAR(FunctionName), *UserMessage);

    switch (Verbosity)
    {
        case ELogVerbosity::Error:
            UE_LOG(MultiplayerSessionSubsystemLog, Error, TEXT("%s"), *FinalMessage);
            break;

        case ELogVerbosity::Warning:
            UE_LOG(MultiplayerSessionSubsystemLog, Warning, TEXT("%s"), *FinalMessage);
            break;

        case ELogVerbosity::Display:
        case ELogVerbosity::Log:
        default:
            UE_LOG(MultiplayerSessionSubsystemLog, Log, TEXT("%s"), *FinalMessage);
            break;
    }

#if !UE_BUILD_SHIPPING
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(INDEX_NONE, 8.f, ScreenColor, FinalMessage);
    }
#endif
}

// --------------------------------------------------------------------------
//  MACROS FOR EASY USAGE
// --------------------------------------------------------------------------

#define LOG_INFO(Format, ...) \
    Internal_Log(ELogVerbosity::Log, __FUNCTION__, FColor::Cyan, FString::Printf(Format, ##__VA_ARGS__))

#define LOG_WARNING(Format, ...) \
    Internal_Log(ELogVerbosity::Warning, __FUNCTION__, FColor::Yellow, FString::Printf(Format, ##__VA_ARGS__))

#define LOG_ERROR(Format, ...) \
    Internal_Log(ELogVerbosity::Error, __FUNCTION__, FColor::Red, FString::Printf(Format, ##__VA_ARGS__))