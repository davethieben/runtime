// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#ifndef __NATIVE_CONTEXT_H__
#define __NATIVE_CONTEXT_H__

// FreeRTOS bare-metal doesn't have ucontext_t, so we use CONTEXT directly
#include "pal.h"

// Convert FreeRTOS native context to PAL_LIMITED_CONTEXT
void NativeContextToPalContext(const void* context, PAL_LIMITED_CONTEXT* palContext);
// Redirect FreeRTOS native context to the PAL_LIMITED_CONTEXT and also set the first two argument registers
void RedirectNativeContext(void* context, const PAL_LIMITED_CONTEXT* palContext, uintptr_t arg0Reg, uintptr_t arg1Reg);

struct NATIVE_CONTEXT
{
    CONTEXT ctx;

#ifdef TARGET_ARM
    uint32_t& Pc() { return ctx.Pc; }
    uint32_t& Sp() { return ctx.Sp; }
    uint32_t& Lr() { return ctx.Lr; }
    uint32_t& R0() { return ctx.R0; }
    uint32_t& R1() { return ctx.R1; }
    uint32_t& R2() { return ctx.R2; }
    uint32_t& R3() { return ctx.R3; }
    uint32_t& R4() { return ctx.R4; }
    uint32_t& R5() { return ctx.R5; }
    uint32_t& R6() { return ctx.R6; }
    uint32_t& R7() { return ctx.R7; }
    uint32_t& R8() { return ctx.R8; }
    uint32_t& R9() { return ctx.R9; }
    uint32_t& R10() { return ctx.R10; }
    uint32_t& R11() { return ctx.R11; }
    uint32_t& R12() { return ctx.R12; }

    uintptr_t GetIp() { return (uintptr_t)Pc(); }
    uintptr_t GetSp() { return (uintptr_t)Sp(); }

    void SetIp(uintptr_t ip) { Pc() = (uint32_t)ip; }
    void SetSp(uintptr_t sp) { Sp() = (uint32_t)sp; }

    // Argument register setters (ARM calling convention: R0-R3 are argument registers)
    void SetArg0Reg(uintptr_t val) { R0() = (uint32_t)val; }
    void SetArg1Reg(uintptr_t val) { R1() = (uint32_t)val; }

    template <typename F>
    void ForEachPossibleObjectRef(F lambda)
    {
        lambda((size_t*)&R0());
        lambda((size_t*)&R1());
        lambda((size_t*)&R2());
        lambda((size_t*)&R3());
        lambda((size_t*)&R4());
        lambda((size_t*)&R5());
        lambda((size_t*)&R6());
        lambda((size_t*)&R7());
        lambda((size_t*)&R8());
        lambda((size_t*)&R9());
        lambda((size_t*)&R10());
        lambda((size_t*)&R11());
        lambda((size_t*)&R12());
    }
#else
#error Unsupported architecture for FreeRTOS
#endif
};

#endif // __NATIVE_CONTEXT_H__
