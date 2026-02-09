// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

// Implementation of NativeAOT PAL inline functions for FreeRTOS

#include <errno.h>

FORCEINLINE void PalInterlockedOperationBarrier()
{
#if defined(TARGET_ARM) || defined(TARGET_ARM64)
    // On ARM, provide a memory barrier after interlocked operations
    // to prevent load reordering
    __sync_synchronize();
#endif
}

FORCEINLINE int32_t PalInterlockedIncrement(_Inout_ int32_t volatile *pDst)
{
    int32_t result = __sync_add_and_fetch(pDst, 1);
    PalInterlockedOperationBarrier();
    return result;
}

FORCEINLINE int64_t PalInterlockedIncrement64(_Inout_ int64_t volatile *pDst)
{
    int64_t result = __sync_add_and_fetch(pDst, 1);
    PalInterlockedOperationBarrier();
    return result;
}

FORCEINLINE int32_t PalInterlockedDecrement(_Inout_ int32_t volatile *pDst)
{
    int32_t result = __sync_sub_and_fetch(pDst, 1);
    PalInterlockedOperationBarrier();
    return result;
}

FORCEINLINE uint32_t PalInterlockedOr(_Inout_ uint32_t volatile *pDst, uint32_t iValue)
{
    int32_t result = __sync_or_and_fetch(pDst, iValue);
    PalInterlockedOperationBarrier();
    return result;
}

FORCEINLINE uint32_t PalInterlockedAnd(_Inout_ uint32_t volatile *pDst, uint32_t iValue)
{
    int32_t result = __sync_and_and_fetch(pDst, iValue);
    PalInterlockedOperationBarrier();
    return result;
}

FORCEINLINE int32_t PalInterlockedExchange(_Inout_ int32_t volatile *pDst, int32_t iValue)
{
    int32_t result =__atomic_exchange_n(pDst, iValue, __ATOMIC_ACQ_REL);
    PalInterlockedOperationBarrier();
    return result;
}

FORCEINLINE int64_t PalInterlockedExchange64(_Inout_ int64_t volatile *pDst, int64_t iValue)
{
    int64_t result =__atomic_exchange_n(pDst, iValue, __ATOMIC_ACQ_REL);
    PalInterlockedOperationBarrier();
    return result;
}

FORCEINLINE int32_t PalInterlockedCompareExchange(_Inout_ int32_t volatile *pDst, int32_t iValue, int32_t iComparand)
{
    int32_t result = __sync_val_compare_and_swap(pDst, iComparand, iValue);
    PalInterlockedOperationBarrier();
    return result;
}

FORCEINLINE int64_t PalInterlockedCompareExchange64(_Inout_ int64_t volatile *pDst, int64_t iValue, int64_t iComparand)
{
    int64_t result = __sync_val_compare_and_swap(pDst, iComparand, iValue);
    PalInterlockedOperationBarrier();
    return result;
}

#if defined(TARGET_ARM64)
FORCEINLINE uint8_t PalInterlockedCompareExchange128(_Inout_ int64_t volatile *pDst, int64_t iValueHigh, int64_t iValueLow, int64_t *pComparandAndResult)
{
    __int128_t iComparand = ((__int128_t)pComparandAndResult[1] << 64) + (uint64_t)pComparandAndResult[0];
    __int128_t iResult = __sync_val_compare_and_swap((__int128_t volatile*)pDst, iComparand, ((__int128_t)iValueHigh << 64) + (uint64_t)iValueLow);
    PalInterlockedOperationBarrier();
    pComparandAndResult[0] = (int64_t)iResult; pComparandAndResult[1] = (int64_t)(iResult >> 64);
    return iComparand == iResult;
}
#endif // TARGET_ARM64

#if defined(TARGET_ARM64)

#define PalInterlockedExchangePointer(_pDst, _pValue) \
    ((void *)PalInterlockedExchange64((int64_t volatile *)(_pDst), (int64_t)(size_t)(_pValue)))

#define PalInterlockedCompareExchangePointer(_pDst, _pValue, _pComparand) \
    ((void *)PalInterlockedCompareExchange64((int64_t volatile *)(_pDst), (int64_t)(size_t)(_pValue), (int64_t)(size_t)(_pComparand)))

#else // TARGET_ARM (32-bit)

#define PalInterlockedExchangePointer(_pDst, _pValue) \
    ((void *)PalInterlockedExchange((int32_t volatile *)(_pDst), (int32_t)(size_t)(_pValue)))

#define PalInterlockedCompareExchangePointer(_pDst, _pValue, _pComparand) \
    ((void *)PalInterlockedCompareExchange((int32_t volatile *)(_pDst), (int32_t)(size_t)(_pValue), (int32_t)(size_t)(_pComparand)))

#endif // TARGET_ARM64


FORCEINLINE void PalYieldProcessor()
{
#if defined(TARGET_ARM) || defined(TARGET_ARM64)
    // ARM yield instruction
    __asm__ __volatile__(
        "yield"
        );
#endif
}

FORCEINLINE void PalMemoryBarrier()
{
    __sync_synchronize();
}

#define PalDebugBreak() __builtin_trap()

FORCEINLINE int32_t PalGetLastError()
{
    return errno;
}

FORCEINLINE void PalSetLastError(int32_t error)
{
    errno = error;
}

FORCEINLINE int32_t PalOsPageSize()
{
    // FreeRTOS doesn't have traditional virtual memory pages,
    // but return a reasonable default for memory alignment
    return 0x1000; // 4KB
}
