// Copyright 2026 RainButterfly. All Rights Reserved.

#pragma once

#define EMPTY_LOG_GW() UE_LOG(LogTemp, Error, TEXT("        "))
#define WAITING_MOD_LOG() \
do { \
UE_LOG(LogTemp, Error, TEXT("The code here is incomplete and needs to be fixed immediately.")); \
UE_LOG(LogTemp, Error, TEXT("File: %s, Func: %s, Line: %d"), TEXT(__FILE__), TEXT(__FUNCTION__), __LINE__); \
EMPTY_LOG_GW() \
} while(0)
