// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Editor-only module for the Digital Human Runtime SDK.
 * Hosts editor tooling: connection monitor, curve debugger, latency monitor,
 * viseme/emotion viewers, and setup wizards. All added in later stages, once
 * the corresponding runtime systems exist to visualize.
 */
class FDigitalHumanEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
