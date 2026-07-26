// Copyright 2026 RainButterfly. All Rights Reserved.

#include "UK2NodeTools.h"

#include "GUIDClass.h"

#define LOCTEXT_NAMESPACE "FUK2NodeToolsModule"

void FUK2NodeToolsModule::StartupModule()
{
	UGraphWeaverPerUserGuid* Settings =
		   GetMutableDefault<UGraphWeaverPerUserGuid>();

	if (!Settings->UserGuid.IsValid())
	{
		Settings->UserGuid = FGuid::NewGuid();
		Settings->SaveConfig();
	}
}

void FUK2NodeToolsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUK2NodeToolsModule, UK2NodeTools)