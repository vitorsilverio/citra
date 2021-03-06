// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/make_unique.h"

#include "core/arm/skyeye_common/armstate.h"
#include "core/arm/skyeye_common/armsupp.h"
#include "core/arm/skyeye_common/vfp/vfp.h"

#include "core/arm/dyncom/arm_dyncom.h"
#include "core/arm/dyncom/arm_dyncom_interpreter.h"
#include "core/arm/dyncom/arm_dyncom_run.h"

#include "core/core.h"
#include "core/core_timing.h"

ARM_DynCom::ARM_DynCom(PrivilegeMode initial_mode) {
    state = Common::make_unique<ARMul_State>(initial_mode);
}

ARM_DynCom::~ARM_DynCom() {
}

void ARM_DynCom::SetPC(u32 pc) {
    state->Reg[15] = pc;
}

u32 ARM_DynCom::GetPC() const {
    return state->Reg[15];
}

u32 ARM_DynCom::GetReg(int index) const {
    return state->Reg[index];
}

void ARM_DynCom::SetReg(int index, u32 value) {
    state->Reg[index] = value;
}

u32 ARM_DynCom::GetVFPReg(int index) const {
    return state->ExtReg[index];
}

void ARM_DynCom::SetVFPReg(int index, u32 value) {
    state->ExtReg[index] = value;
}

u32 ARM_DynCom::GetVFPSystemReg(VFPSystemRegister reg) const {
    return state->VFP[reg];
}

void ARM_DynCom::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    state->VFP[reg] = value;
}

u32 ARM_DynCom::GetCPSR() const {
    return state->Cpsr;
}

void ARM_DynCom::SetCPSR(u32 cpsr) {
    state->Cpsr = cpsr;
}

u32 ARM_DynCom::GetCP15Register(CP15Register reg) {
    return state->CP15[reg];
}

void ARM_DynCom::SetCP15Register(CP15Register reg, u32 value) {
    state->CP15[reg] = value;
}

void ARM_DynCom::AddTicks(u64 ticks) {
    down_count -= ticks;
    if (down_count < 0)
        CoreTiming::Advance();
}

void ARM_DynCom::ExecuteInstructions(int num_instructions) {
    state->NumInstrsToExecute = num_instructions;

    // Dyncom only breaks on instruction dispatch. This only happens on every instruction when
    // executing one instruction at a time. Otherwise, if a block is being executed, more
    // instructions may actually be executed than specified.
    unsigned ticks_executed = InterpreterMainLoop(state.get());
    AddTicks(ticks_executed);
}

void ARM_DynCom::ResetContext(Core::ThreadContext& context, u32 stack_top, u32 entry_point, u32 arg) {
    memset(&context, 0, sizeof(Core::ThreadContext));

    context.cpu_registers[0] = arg;
    context.pc = entry_point;
    context.sp = stack_top;
    context.cpsr = 0x1F; // Usermode
}

void ARM_DynCom::SaveContext(Core::ThreadContext& ctx) {
    memcpy(ctx.cpu_registers, state->Reg.data(), sizeof(ctx.cpu_registers));
    memcpy(ctx.fpu_registers, state->ExtReg.data(), sizeof(ctx.fpu_registers));

    ctx.sp = state->Reg[13];
    ctx.lr = state->Reg[14];
    ctx.pc = state->Reg[15];
    ctx.cpsr = state->Cpsr;

    ctx.fpscr = state->VFP[1];
    ctx.fpexc = state->VFP[2];
}

void ARM_DynCom::LoadContext(const Core::ThreadContext& ctx) {
    memcpy(state->Reg.data(), ctx.cpu_registers, sizeof(ctx.cpu_registers));
    memcpy(state->ExtReg.data(), ctx.fpu_registers, sizeof(ctx.fpu_registers));

    state->Reg[13] = ctx.sp;
    state->Reg[14] = ctx.lr;
    state->Reg[15] = ctx.pc;
    state->Cpsr = ctx.cpsr;

    state->VFP[1] = ctx.fpscr;
    state->VFP[2] = ctx.fpexc;
}

void ARM_DynCom::PrepareReschedule() {
    state->NumInstrsToExecute = 0;
}
