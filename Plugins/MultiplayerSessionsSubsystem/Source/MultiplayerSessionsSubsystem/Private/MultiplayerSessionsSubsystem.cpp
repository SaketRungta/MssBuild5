// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "MultiplayerSessionsSubsystem.h"

#define LOCTEXT_NAMESPACE "FMultiplayerSessionsSubsystemModule"

void FMultiplayerSessionsSubsystemModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FMultiplayerSessionsSubsystemModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMultiplayerSessionsSubsystemModule, MultiplayerSessionsSubsystem)