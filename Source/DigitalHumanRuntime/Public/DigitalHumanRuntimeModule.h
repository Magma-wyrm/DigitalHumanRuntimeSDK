// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

/**
 * Central log category for the entire Digital Human Runtime plugin.
 * Every subsystem (networking, audio, lip sync, behaviour) logs through this
 * category so log filtering in the Output Log stays manageable as the plugin grows.
 */
DIGITALHUMANRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogDigitalHuman, Log, All);

/**
 * Runtime module for the Digital Human Runtime SDK.
 *
 * Stage 1 responsibility: module lifecycle only (startup/shutdown). Subsystems
 * (WebSocket client, audio engine, lip sync engine, behaviour engine) are added
 * as engine Subsystems / Components in later stages rather than being owned
 * directly by this module class, keeping this file stable as the plugin grows.
 */
class FDigitalHumanRuntimeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Whether this module supports being dynamically reloaded without restarting
	 * the engine. Kept false for now since later stages will register native
	 * classes that are not safe to hot-reload cleanly.
	 */
	virtual bool SupportsDynamicReloading() override { return false; }
};
