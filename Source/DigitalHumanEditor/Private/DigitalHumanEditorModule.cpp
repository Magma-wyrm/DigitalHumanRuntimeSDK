// DigitalHumanRuntimeSDK - Copyright (c) 2026 Parth Sarwade. All Rights Reserved.
// https://github.com/Magma-wyrm/DigitalHumanRuntimeSDK

#include "DigitalHumanEditorModule.h"
#include "DigitalHumanRuntimeModule.h"

#define LOCTEXT_NAMESPACE "FDigitalHumanEditorModule"

void FDigitalHumanEditorModule::StartupModule()
{
	UE_LOG(LogDigitalHuman, Log, TEXT("DigitalHumanEditor module started (Stage 1: skeleton)."));
}

void FDigitalHumanEditorModule::ShutdownModule()
{
	UE_LOG(LogDigitalHuman, Log, TEXT("DigitalHumanEditor module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDigitalHumanEditorModule, DigitalHumanEditor)
