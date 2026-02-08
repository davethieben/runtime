// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

//
// FreeRTOS Platform Abstraction Layer (PAL) header
//
// This file provides the declarations for the FreeRTOS PAL implementation.
// The PAL maps NativeAOT runtime requirements to FreeRTOS APIs.
//

#ifndef __PAL_FREERTOS_H__
#define __PAL_FREERTOS_H__

#ifdef TARGET_FREERTOS

// FreeRTOS headers
// Note: User must configure FreeRTOS include paths in their build
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#include "queue.h"

// Memory allocation wrappers
// FreeRTOS doesn't have virtual memory, so these map to heap allocation
void* PalVirtualAlloc_FreeRTOS(size_t size);
void PalVirtualFree_FreeRTOS(void* pAddress);

// Threading primitives
// Map to FreeRTOS tasks
bool PalCreateThread_FreeRTOS(void* pCallback, void* pContext, TaskHandle_t* pHandle);
void PalSleep_FreeRTOS(uint32_t milliseconds);
uint64_t PalGetCurrentThreadId_FreeRTOS(void);

// Synchronization
// Map to FreeRTOS semaphores and event groups
SemaphoreHandle_t PalCreateEvent_FreeRTOS(bool manualReset, bool initialState);
bool PalSetEvent_FreeRTOS(SemaphoreHandle_t handle);
bool PalResetEvent_FreeRTOS(SemaphoreHandle_t handle);
uint32_t PalWaitForSingleObject_FreeRTOS(SemaphoreHandle_t handle, uint32_t timeout);

// System information
uint32_t PalGetPageSize_FreeRTOS(void);
uint64_t PalGetTickCount_FreeRTOS(void);

// Console output (implementation depends on board - typically UART)
void PalDebugPrint_FreeRTOS(const char* message);

#endif // TARGET_FREERTOS

#endif // __PAL_FREERTOS_H__
