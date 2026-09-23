// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanRuntimeModule.h"

DEFINE_LOG_CATEGORY(LogDigitalHuman);

#define LOCTEXT_NAMESPACE "FDigitalHumanRuntimeModule"

void FDigitalHumanRuntimeModule::StartupModule()
{
	UE_LOG(LogDigitalHuman, Log, TEXT("DigitalHumanRuntime module started (Stage 1: skeleton)."));
}

void FDigitalHumanRuntimeModule::ShutdownModule()
{
	UE_LOG(LogDigitalHuman, Log, TEXT("DigitalHumanRuntime module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDigitalHumanRuntimeModule, DigitalHumanRuntime)
