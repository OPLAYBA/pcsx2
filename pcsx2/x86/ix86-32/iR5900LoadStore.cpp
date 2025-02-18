/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "PrecompiledHeader.h"

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900LoadStore.h"
#include "iR5900.h"

using namespace x86Emitter;

#define REC_STORES
#define REC_LOADS

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/
#ifndef LOADSTORE_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(LB,  _Rt_);
REC_FUNC_DEL(LBU, _Rt_);
REC_FUNC_DEL(LH,  _Rt_);
REC_FUNC_DEL(LHU, _Rt_);
REC_FUNC_DEL(LW,  _Rt_);
REC_FUNC_DEL(LWU, _Rt_);
REC_FUNC_DEL(LWL, _Rt_);
REC_FUNC_DEL(LWR, _Rt_);
REC_FUNC_DEL(LD,  _Rt_);
REC_FUNC_DEL(LDR, _Rt_);
REC_FUNC_DEL(LDL, _Rt_);
REC_FUNC_DEL(LQ,  _Rt_);
REC_FUNC(SB);
REC_FUNC(SH);
REC_FUNC(SW);
REC_FUNC(SWL);
REC_FUNC(SWR);
REC_FUNC(SD);
REC_FUNC(SDL);
REC_FUNC(SDR);
REC_FUNC(SQ);
REC_FUNC(LWC1);
REC_FUNC(SWC1);
REC_FUNC(LQC2);
REC_FUNC(SQC2);

#else

alignas(16) u64 retValues[2];

void _eeOnLoadWrite(u32 reg)
{
	int regt;

	if (!reg)
		return;

	_eeOnWriteReg(reg, 1);
	regt = _checkXMMreg(XMMTYPE_GPRREG, reg, MODE_READ);

	if (regt >= 0)
	{
		if (xmmregs[regt].mode & MODE_WRITE)
		{
			if (reg != _Rs_)
			{
				xPUNPCK.HQDQ(xRegisterSSE(regt), xRegisterSSE(regt));
				xMOVQ(ptr[&cpuRegs.GPR.r[reg].UL[2]], xRegisterSSE(regt));
			}
			else
				xMOVH.PS(ptr[&cpuRegs.GPR.r[reg].UL[2]], xRegisterSSE(regt));
		}
		xmmregs[regt].inuse = 0;
	}
}

using namespace Interpreter::OpcodeImpl;

alignas(16) u32 dummyValue[4];

//////////////////////////////////////////////////////////////////////////////////////////
//
static void recLoadQuad(u32 bits, bool sign)
{
	pxAssume(bits == 128);

	// Load arg2 with the destination.
	// 64/128 bit modes load the result directly into the cpuRegs.GPR struct.

	int gprreg = _Rt_ ? _Rt_ : -1;
	int reg;

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 srcadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		if (bits == 128)
			srcadr &= ~0x0f;

		_eeOnLoadWrite(_Rt_);
		_deleteEEreg(_Rt_, 0);

		reg = vtlb_DynGenReadQuad_Const(bits, srcadr, gprreg);
	}
	else
	{
		// Load ECX with the source memory address that we're reading from.
		_eeMoveGPRtoR(arg1reg, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);
		if (bits == 128) // force 16 byte alignment on 128 bit reads
			xAND(arg1regd, ~0x0F);

		_eeOnLoadWrite(_Rt_);
		_deleteEEreg(_Rt_, 0);

		iFlushCall(FLUSH_FULLVTLB);
		reg = vtlb_DynGenReadQuad(bits, gprreg);
	}

	if (gprreg == -1)
	{
		if (_Rt_)
			xMOVQ(ptr64[&cpuRegs.GPR.r[_Rt_].UL[0]], xRegisterSSE(reg));

		_freeXMMreg(reg);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
static void recLoadNonQuad(u32 bits, bool sign)
{
	pxAssume(bits <= 64);

	// 8/16/32 bit modes return the loaded value in EAX.

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 srcadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;

		_eeOnLoadWrite(_Rt_);
		_deleteEEreg(_Rt_, 0);

		vtlb_DynGenReadNonQuad_Const(bits, sign, srcadr);
	}
	else
	{
		// Load arg1 with the source memory address that we're reading from.
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		_eeOnLoadWrite(_Rt_);
		_deleteEEreg(_Rt_, 0);

		iFlushCall(FLUSH_FULLVTLB);
		vtlb_DynGenReadNonQuad(bits, sign);
	}

	if (_Rt_)
		xMOV(ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);
}

//////////////////////////////////////////////////////////////////////////////////////////
//

static void recStore(u32 bits)
{
	// Performance note: Const prop for the store address is good, always.
	// Constprop for the value being stored is not really worthwhile (better to use register
	// allocation -- simpler code and just as fast)

	// Load EDX first with the value being written, or the address of the value
	// being written (64/128 bit modes).

	if (bits < 128)
	{
		_eeMoveGPRtoR(arg2reg, _Rt_);
	}
	else
	{
		_flushEEreg(_Rt_); // flush register to mem

		const xRegisterSSE& dreg = xRegisterSSE::GetArgRegister(1, 0);
		_freeXMMreg(dreg.GetId());
		xMOVAPS(dreg, ptr128[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	}

	// Load ECX with the destination address, or issue a direct optimized write
	// if the address is a constant propagation.

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 dstadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		if (bits == 128)
			dstadr &= ~0x0f;

		vtlb_DynGenWrite_Const(bits, dstadr);
	}
	else
	{
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);
		if (bits == 128)
			xAND(arg1regd, ~0x0F);

		iFlushCall(FLUSH_FULLVTLB);

		vtlb_DynGenWrite(bits);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
//
void recLB()  { recLoadNonQuad(  8, true);  EE::Profiler.EmitOp(eeOpcode::LB); }
void recLBU() { recLoadNonQuad(  8, false); EE::Profiler.EmitOp(eeOpcode::LBU); }
void recLH()  { recLoadNonQuad( 16, true);  EE::Profiler.EmitOp(eeOpcode::LH); }
void recLHU() { recLoadNonQuad( 16, false); EE::Profiler.EmitOp(eeOpcode::LHU); }
void recLW()  { recLoadNonQuad( 32, true);  EE::Profiler.EmitOp(eeOpcode::LW); }
void recLWU() { recLoadNonQuad( 32, false); EE::Profiler.EmitOp(eeOpcode::LWU); }
void recLD()  { recLoadNonQuad( 64, false); EE::Profiler.EmitOp(eeOpcode::LD); }
void recLQ()  { recLoadQuad(128, false); EE::Profiler.EmitOp(eeOpcode::LQ); }

void recSB()  { recStore(  8); EE::Profiler.EmitOp(eeOpcode::SB); }
void recSH()  { recStore( 16); EE::Profiler.EmitOp(eeOpcode::SH); }
void recSW()  { recStore( 32); EE::Profiler.EmitOp(eeOpcode::SW); }
void recSD()  { recStore( 64); EE::Profiler.EmitOp(eeOpcode::SD); }
void recSQ()  { recStore(128); EE::Profiler.EmitOp(eeOpcode::SQ); }

////////////////////////////////////////////////////

void recLWL()
{
#ifdef REC_LOADS
	iFlushCall(FLUSH_FULLVTLB);
	_deleteEEreg(_Rt_, 1);

	_eeMoveGPRtoR(arg1regd, _Rs_);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);

	// calleeSavedReg1 = bit offset in word
	xMOV(calleeSavedReg1d, arg1regd);
	xAND(calleeSavedReg1d, 3);
	xSHL(calleeSavedReg1d, 3);

	xAND(arg1regd, ~3);
	vtlb_DynGenReadNonQuad(32, false);

	if (!_Rt_)
		return;

	// mask off bytes loaded
	xMOV(ecx, calleeSavedReg1d);
	xMOV(edx, 0xffffff);
	xSHR(edx, cl);
	xAND(edx, ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);

	// OR in bytes loaded
	xNEG(ecx);
	xADD(ecx, 24);
	xSHL(eax, cl);
	xOR(eax, edx);

	eeSignExtendTo(_Rt_);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);

	recCall(LWL);
#endif

	EE::Profiler.EmitOp(eeOpcode::LWL);
}

////////////////////////////////////////////////////
void recLWR()
{
#ifdef REC_LOADS
	iFlushCall(FLUSH_FULLVTLB);
	_deleteEEreg(_Rt_, 1);

	_eeMoveGPRtoR(arg1regd, _Rs_);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);

	// edi = bit offset in word
	xMOV(calleeSavedReg1d, arg1regd);

	xAND(arg1regd, ~3);
	vtlb_DynGenReadNonQuad(32, false);

	if (!_Rt_)
		return;

	xAND(calleeSavedReg1d, 3);
	xForwardJE8 nomask;
		xSHL(calleeSavedReg1d, 3);
		// mask off bytes loaded
		xMOV(ecx, 24);
		xSUB(ecx, calleeSavedReg1d);
		xMOV(edx, 0xffffff00);
		xSHL(edx, cl);
		xAND(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], edx);

		// OR in bytes loaded
		xMOV(ecx, calleeSavedReg1d);
		xSHR(eax, cl);
		xOR(ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]], eax);

		xForwardJump8 end;
	nomask.SetTarget();
		eeSignExtendTo(_Rt_);
	end.SetTarget();
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);

	recCall(LWR);
#endif

	EE::Profiler.EmitOp(eeOpcode::LWR);
}

////////////////////////////////////////////////////
void recSWL()
{
#ifdef REC_STORES
	iFlushCall(FLUSH_FULLVTLB);

	_eeMoveGPRtoR(arg1regd, _Rs_);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);

	// edi = bit offset in word
	xMOV(calleeSavedReg1d, arg1regd);
	xAND(arg1regd, ~3);
	xAND(calleeSavedReg1d, 3);
	xCMP(calleeSavedReg1d, 3);
	xForwardJE8 skip;
		xSHL(calleeSavedReg1d, 3);

		vtlb_DynGenReadNonQuad(32, false);

		// mask read -> arg2
		xMOV(ecx, calleeSavedReg1d);
		xMOV(arg2regd, 0xffffff00);
		xSHL(arg2regd, cl);
		xAND(arg2regd, eax);

		if (_Rt_)
		{
			// mask write and OR -> edx
			xNEG(ecx);
			xADD(ecx, 24);
			_eeMoveGPRtoR(eax, _Rt_);
			xSHR(eax, cl);
			xOR(arg2regd, eax);
		}

		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);
		xAND(arg1regd, ~3);

		xForwardJump8 end;
	skip.SetTarget();
		_eeMoveGPRtoR(arg2regd, _Rt_);
	end.SetTarget();

	vtlb_DynGenWrite(32);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(SWL);
#endif

	EE::Profiler.EmitOp(eeOpcode::SWL);
}

////////////////////////////////////////////////////
void recSWR()
{
#ifdef REC_STORES
	iFlushCall(FLUSH_FULLVTLB);

	_eeMoveGPRtoR(arg1regd, _Rs_);
	if (_Imm_ != 0)
		xADD(arg1regd, _Imm_);

	// edi = bit offset in word
	xMOV(calleeSavedReg1d, arg1regd);
	xAND(arg1regd, ~3);
	xAND(calleeSavedReg1d, 3);
	xForwardJE8 skip;
		xSHL(calleeSavedReg1d, 3);

		vtlb_DynGenReadNonQuad(32, false);

		// mask read -> edx
		xMOV(ecx, 24);
		xSUB(ecx, calleeSavedReg1d);
		xMOV(arg2regd, 0xffffff);
		xSHR(arg2regd, cl);
		xAND(arg2regd, eax);

		if (_Rt_)
		{
			// mask write and OR -> edx
			xMOV(ecx, calleeSavedReg1d);
			_eeMoveGPRtoR(eax, _Rt_);
			xSHL(eax, cl);
			xOR(arg2regd, eax);
		}

		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);
		xAND(arg1regd, ~3);

		xForwardJump8 end;
	skip.SetTarget();
		_eeMoveGPRtoR(arg2regd, _Rt_);
	end.SetTarget();

	vtlb_DynGenWrite(32);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(SWR);
#endif

	EE::Profiler.EmitOp(eeOpcode::SWR);
}

////////////////////////////////////////////////////

/// Masks rt with (0xffffffffffffffff maskshift maskamt), merges with (value shift amt), leaves result in value
static void ldlrhelper_const(int maskamt, const xImpl_Group2& maskshift, int amt, const xImpl_Group2& shift, const xRegister64& value, const xRegister64& rt)
{
	pxAssert(rt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());
	_freeX86reg(ecx);
	xMOV(rcx, -1);
	maskshift(rcx, maskamt);
	xAND(rt, rcx);

	shift(value, amt);
	xOR(value, rt);
}

/// Masks rt with (0xffffffffffffffff maskshift maskamt), merges with (value shift amt), leaves result in value
static void ldlrhelper(const xRegister32& maskamt, const xImpl_Group2& maskshift, const xRegister32& amt, const xImpl_Group2& shift, const xRegister64& value, const xRegister64& rt)
{
	pxAssert(rt.GetId() != ebx.GetId() && value.GetId() != ebx.GetId() && rt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());
	pxAssert(maskamt.GetId() != ebx.GetId() && amt.GetId() != ebx.GetId() && rt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());

	// TODO: Use actual register allocator here. Don't assume ebx is free.
	_freeX86reg(ebx);
	_freeX86reg(ecx);

	xMOV(rbx, -1);
	xMOV(ecx, maskamt);
	maskshift(rbx, cl);
	xAND(rt, rbx);

	xMOV(ecx, amt);
	shift(value, cl);
	xOR(value, rt);
}

void recLDL()
{
	if (!_Rt_)
		return;

#ifdef REC_LOADS
	if (GPR_IS_CONST1(_Rt_))
	{
		_flushConstReg(_Rt_);
		_eeOnWriteReg(_Rt_, 0);
	}

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 srcadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;

		// If _Rs_ is equal to _Rt_ we need to put the shift in to eax since it won't take the CONST path
		if (_Rs_ == _Rt_)
			xMOV(calleeSavedReg1d, srcadr);

		srcadr &= ~0x07;

		vtlb_DynGenReadNonQuad_Const(64, false, srcadr);
	}
	else
	{
		// Load ECX with the source memory address that we're reading from.
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		xMOV(calleeSavedReg1d, arg1regd);
		xAND(arg1regd, ~0x07);

		iFlushCall(FLUSH_FULLVTLB);

		vtlb_DynGenReadNonQuad(64, false);
	}

	_freeX86reg(calleeSavedReg2d);
	_eeMoveGPRtoR(calleeSavedReg2, _Rt_);

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 shift = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		shift = ((shift & 0x7) + 1) * 8;
		if (shift != 64)
		{
			ldlrhelper_const(shift, xSHR, 64 - shift, xSHL, rax, calleeSavedReg2);
		}
	}
	else
	{
		_freeX86reg(edx);

		xAND(calleeSavedReg1d, 0x7);
		xCMP(calleeSavedReg1d, 7);
		xForwardJE8 skip;
			// Calculate the shift from top bit to lowest
			xADD(calleeSavedReg1d, 1);
			xMOV(edx, 64);
			xSHL(calleeSavedReg1d, 3);
			xSUB(edx, calleeSavedReg1d);

			ldlrhelper(calleeSavedReg1d, xSHR, edx, xSHL, rax, calleeSavedReg2);
		skip.SetTarget();
	}

	xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(LDL);
#endif

	EE::Profiler.EmitOp(eeOpcode::LDL);
}

////////////////////////////////////////////////////
void recLDR()
{
	if (!_Rt_)
		return;

#ifdef REC_LOADS
	if (GPR_IS_CONST1(_Rt_))
	{
		_flushConstReg(_Rt_);
		_eeOnWriteReg(_Rt_, 0);
	}

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 srcadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;

		// If _Rs_ is equal to _Rt_ we need to put the shift in to eax since it won't take the CONST path
		if(_Rs_ == _Rt_)
			xMOV(calleeSavedReg1d, srcadr);

		srcadr &= ~0x07;

		vtlb_DynGenReadNonQuad_Const(64, false, srcadr);
	}
	else
	{
		// Load ECX with the source memory address that we're reading from.
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		xMOV(calleeSavedReg1d, arg1regd);
		xAND(arg1regd, ~0x07);

		iFlushCall(FLUSH_FULLVTLB);

		vtlb_DynGenReadNonQuad(64, false);
	}

	_freeX86reg(calleeSavedReg2d);
	_eeMoveGPRtoR(calleeSavedReg2, _Rt_);

	if (GPR_IS_CONST1(_Rs_))
	{
		u32 shift = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		shift = (shift & 0x7) * 8;
		if (shift != 0)
		{
			ldlrhelper_const(64 - shift, xSHL, shift, xSHR, rax, calleeSavedReg2);
		}
	}
	else
	{
		_freeX86reg(edx);

		xAND(calleeSavedReg1d, 0x7);
		xForwardJE8 skip;
			// Calculate the shift from top bit to lowest
			xMOV(edx, 64);
			xSHL(calleeSavedReg1d, 3);
			xSUB(edx, calleeSavedReg1d);

			ldlrhelper(edx, xSHL, calleeSavedReg1d, xSHR, rax, calleeSavedReg2);
		skip.SetTarget();
	}

	xMOV(ptr[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(LDR);
#endif

	EE::Profiler.EmitOp(eeOpcode::LDR);
}

////////////////////////////////////////////////////

/// Masks value with (0xffffffffffffffff maskshift maskamt), merges with (rt shift amt), saves to dummyValue
static void sdlrhelper_const(int maskamt, const xImpl_Group2& maskshift, int amt, const xImpl_Group2& shift, const xRegister64& value, const xRegister64& rt)
{
	pxAssert(rt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());
	_freeX86reg(ecx);
	xMOV(rcx, -1);
	maskshift(rcx, maskamt);
	xAND(rcx, value);

	shift(rt, amt);
	xOR(rt, rcx);
}

/// Masks value with (0xffffffffffffffff maskshift maskamt), merges with (rt shift amt), saves to dummyValue
static void sdlrhelper(const xRegister32& maskamt, const xImpl_Group2& maskshift, const xRegister32& amt, const xImpl_Group2& shift, const xRegister64& value, const xRegister64& rt)
{
	pxAssert(rt.GetId() != ebx.GetId() && value.GetId() != ebx.GetId() && rt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());
	pxAssert(maskamt.GetId() != ebx.GetId() && amt.GetId() != ebx.GetId() && rt.GetId() != ecx.GetId() && value.GetId() != ecx.GetId());

	// TODO: Use actual register allocator here. Don't assume ebx is free.
	_freeX86reg(ebx);
	_freeX86reg(ecx);

	// Generate mask 128-(shiftx8)
	xMOV(rbx, -1);
	xMOV(ecx, maskamt);
	maskshift(rbx, cl);
	xAND(rbx, value);

	// Shift over reg value
	xMOV(ecx, amt);
	shift(rt, cl);
	xOR(rt, rbx);
}

void recSDL()
{
#ifdef REC_STORES
	_flushEEreg(_Rt_); // flush register to mem
	if (GPR_IS_CONST1(_Rs_))
	{
		u32 adr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		u32 aligned = adr & ~0x07;
		u32 shift = ((adr & 0x7) + 1) * 8;
		if (shift == 64)
		{
			xMOV(arg2reg, ptr[&cpuRegs.GPR.r[_Rt_].UD[0]]);
		}
		else
		{
			vtlb_DynGenReadNonQuad_Const(64, false, aligned);
			_eeMoveGPRtoR(arg2reg, _Rt_);
			sdlrhelper_const(shift, xSHL, 64 - shift, xSHR, rax, arg2reg);
		}
		vtlb_DynGenWrite_Const(64, aligned);
	}
	else
	{
		// Load ECX with the source memory address that we're reading from.
		_freeX86reg(arg1regd);
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		_freeX86reg(edx);
		_freeX86reg(calleeSavedReg1d);
		_freeX86reg(calleeSavedReg2d);
		_freeX86reg(arg2regd);
		_eeMoveGPRtoR(arg2reg, _Rt_);
		iFlushCall(FLUSH_FULLVTLB);

		xMOV(calleeSavedReg1d, arg1regd);
		xMOV(calleeSavedReg2, arg2reg);
		xAND(arg1regd, ~0x07);
		xAND(calleeSavedReg1d, 0x7);
		xCMP(calleeSavedReg1d, 7);
		xForwardJE8 skip;
			xADD(calleeSavedReg1d, 1);
			vtlb_DynGenReadNonQuad(64, false);

			//Calculate the shift from top bit to lowest
			xMOV(edx, 64);
			xSHL(calleeSavedReg1d, 3);
			xSUB(edx, calleeSavedReg1d);

			sdlrhelper(calleeSavedReg1d, xSHL, edx, xSHR, rax, calleeSavedReg2);

			_eeMoveGPRtoR(arg1regd, _Rs_);
			if (_Imm_ != 0)
				xADD(arg1regd, _Imm_);
			xAND(arg1regd, ~0x7);
			xMOV(arg2reg, calleeSavedReg2);
		skip.SetTarget();

		iFlushCall(FLUSH_FULLVTLB);

		vtlb_DynGenWrite(64);
	}
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(SDL);
#endif
	EE::Profiler.EmitOp(eeOpcode::SDL);
}

////////////////////////////////////////////////////
void recSDR()
{
#ifdef REC_STORES
	_flushEEreg(_Rt_); // flush register to mem
	if (GPR_IS_CONST1(_Rs_))
	{
		u32 adr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		u32 aligned = adr & ~0x07;
		u32 shift = (adr & 0x7) * 8;
		if (shift == 0)
		{
			xMOV(arg2reg, ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);
		}
		else
		{
			vtlb_DynGenReadNonQuad_Const(64, false, aligned);
			_eeMoveGPRtoR(arg2reg, _Rt_);
			sdlrhelper_const(64 - shift, xSHR, shift, xSHL, rax, arg2reg);
		}

		vtlb_DynGenWrite_Const(64, aligned);
	}
	else
	{
		// Load ECX with the source memory address that we're reading from.
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		_freeX86reg(edx);
		_freeX86reg(calleeSavedReg1d);
		_freeX86reg(calleeSavedReg2d);
		_freeX86reg(arg2regd);
		_eeMoveGPRtoR(arg2reg, _Rt_);
		iFlushCall(FLUSH_FULLVTLB);

		xMOV(calleeSavedReg1d, arg1regd);
		xMOV(calleeSavedReg2, arg2reg);
		xAND(arg1regd, ~0x07);
		xAND(calleeSavedReg1d, 0x7);
		xForwardJE8 skip;
			vtlb_DynGenReadNonQuad(64, false);

			xMOV(edx, 64);
			xSHL(calleeSavedReg1d, 3);
			xSUB(edx, calleeSavedReg1d);

			sdlrhelper(edx, xSHR, calleeSavedReg1d, xSHL, rax, calleeSavedReg2);

			_eeMoveGPRtoR(arg1regd, _Rs_);
			if (_Imm_ != 0)
				xADD(arg1regd, _Imm_);
			xAND(arg1regd, ~0x7);
			xMOV(arg2reg, calleeSavedReg2);
		skip.SetTarget();

		iFlushCall(FLUSH_FULLVTLB);

		vtlb_DynGenWrite(64);
	}
#else
	iFlushCall(FLUSH_INTERPRETER);
	_deleteEEreg(_Rs_, 1);
	_deleteEEreg(_Rt_, 1);
	recCall(SDR);
#endif
	EE::Profiler.EmitOp(eeOpcode::SDR);
}

//////////////////////////////////////////////////////////////////////////////////////////
/*********************************************************
* Load and store for COP1                                *
* Format:  OP rt, offset(base)                           *
*********************************************************/

////////////////////////////////////////////////////

void recLWC1()
{
#ifndef FPU_RECOMPILE
	recCall(::R5900::Interpreter::OpcodeImpl::LWC1);
#else
	_deleteFPtoXMMreg(_Rt_, 2);

	if (GPR_IS_CONST1(_Rs_))
	{
		int addr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		vtlb_DynGenReadNonQuad_Const(32, false, addr);
	}
	else
	{
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		iFlushCall(FLUSH_FULLVTLB);

		vtlb_DynGenReadNonQuad(32, false);
	}

	xMOV(ptr32[&fpuRegs.fpr[_Rt_].UL], eax);

	EE::Profiler.EmitOp(eeOpcode::LWC1);
#endif
}

//////////////////////////////////////////////////////

void recSWC1()
{
#ifndef FPU_RECOMPILE
	recCall(::R5900::Interpreter::OpcodeImpl::SWC1);
#else
	_deleteFPtoXMMreg(_Rt_, 1);

	xMOV(arg2regd, ptr32[&fpuRegs.fpr[_Rt_].UL]);

	if (GPR_IS_CONST1(_Rs_))
	{
		int addr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		vtlb_DynGenWrite_Const(32, addr);
	}
	else
	{
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		iFlushCall(FLUSH_FULLVTLB);

		vtlb_DynGenWrite(32);
	}

	EE::Profiler.EmitOp(eeOpcode::SWC1);
#endif
}

////////////////////////////////////////////////////

/*********************************************************
* Load and store for COP2 (VU0 unit)                     *
* Format:  OP rt, offset(base)                           *
*********************************************************/

#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_



void recLQC2()
{
	_freeX86reg(eax);
	xMOV(eax, ptr32[&cpuRegs.cycle]);
	xADD(eax, scaleblockcycles_clear());
	xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

	xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
	xForwardJZ32 skipvuidle;
	xSUB(eax, ptr32[&VU0.cycle]);
	xSUB(eax, ptr32[&VU0.nextBlockCycles]);
	xCMP(eax, 4);
	xForwardJL32 skip;
	_cop2BackupRegs();
	xLoadFarAddr(arg1reg, CpuVU0);
	xMOV(arg2reg, s_nBlockInterlocked);
	xFastCall((void*)BaseVUmicroCPU::ExecuteBlockJIT, arg1reg, arg2reg);
	_cop2RestoreRegs();
	skip.SetTarget();
	skipvuidle.SetTarget();

	int gpr;

	if (GPR_IS_CONST1(_Rs_))
	{
		int addr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;

		gpr = vtlb_DynGenReadQuad_Const(128, addr, -1);
	}
	else
	{
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		iFlushCall(FLUSH_FULLVTLB);

		gpr = vtlb_DynGenReadQuad(128, -1);
	}

	if (_Rt_)
		xMOVAPS(ptr128[&VU0.VF[_Ft_].UD[0]], xRegisterSSE(gpr));

	_freeXMMreg(gpr);

	EE::Profiler.EmitOp(eeOpcode::LQC2);
}

////////////////////////////////////////////////////

void recSQC2()
{
	_freeX86reg(eax);
	xMOV(eax, ptr32[&cpuRegs.cycle]);
	xADD(eax, scaleblockcycles_clear());
	xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

	xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
	xForwardJZ32 skipvuidle;
	xSUB(eax, ptr32[&VU0.cycle]);
	xSUB(eax, ptr32[&VU0.nextBlockCycles]);
	xCMP(eax, 4);
	xForwardJL32 skip;
	_cop2BackupRegs();
	xLoadFarAddr(arg1reg, CpuVU0);
	xMOV(arg2reg, s_nBlockInterlocked);
	xFastCall((void*)BaseVUmicroCPU::ExecuteBlockJIT, arg1reg, arg2reg);
	_cop2RestoreRegs();
	skip.SetTarget();
	skipvuidle.SetTarget();

	const xRegisterSSE& dreg = xRegisterSSE::GetArgRegister(1, 0);
	_freeXMMreg(dreg.GetId());
	xMOVAPS(dreg, ptr128[&VU0.VF[_Ft_].UD[0]]);

	if (GPR_IS_CONST1(_Rs_))
	{
		int addr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		vtlb_DynGenWrite_Const(128, addr);
	}
	else
	{
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);

		iFlushCall(FLUSH_FULLVTLB);

		vtlb_DynGenWrite(128);
	}

	EE::Profiler.EmitOp(eeOpcode::SQC2);
}

#endif

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
