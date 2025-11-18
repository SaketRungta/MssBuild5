// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#include "SteamIntegrationKit.h"

#include "ISettingsModule.h"
#include "MyClass.h"

#define LOCTEXT_NAMESPACE "FSteamIntegrationKitModule"

void FSteamIntegrationKitModule::StartupModule()
{
}

void FSteamIntegrationKitModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSteamIntegrationKitModule, SteamIntegrationKit)